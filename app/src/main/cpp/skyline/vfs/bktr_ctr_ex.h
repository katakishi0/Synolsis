// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cstring>
#include <crypto/aes_cipher.h>
#include <crypto/key_store.h>
#include "backing.h"
#include "bktr_relocation.h"
#include "bktr_ctr_counter.h"

namespace skyline::vfs::bktr {
    // On-demand *physical* CTR-Ex decryption. The caller must separately
    // overlay the base RomFS using RelocationTable. Never expose this as a
    // complete patched RomFS or count it as an installed update by itself.
    class BktrCtrExBacking final : public Backing {
        std::shared_ptr<Backing> ciphertext; // entire physical NCA section
        std::shared_ptr<const SubsectionTable> subsections;
        std::array<std::uint8_t, 16> iv{};
        crypto::AesCipher cipher;
        std::mutex mutex;
        std::uint64_t absoluteSectionOffset;

      protected:
        size_t ReadImpl(span<u8> output, size_t offset) override {
            if (offset > size || output.size() > size - offset)
                return 0;
            size_t written{};
            // Cap temporary data at 128 KiB: safe for 6 GiB Android phones.
            constexpr size_t ChunkMax{128 * 1024};
            while (written < output.size()) {
                const auto current = static_cast<std::uint64_t>(offset + written);
                const auto section = subsections->Resolve(current);
                if (!section || !section->size)
                    return written;

                // Each CTR region must begin at an AES block boundary. If
                // malformed metadata disagrees, refuse to decrypt using an IV
                // from another subsection, rather than returning bad bytes.
                const auto blockStart = current & ~std::uint64_t{0xf};
                const auto start = subsections->Resolve(blockStart);
                if (!start || start->counter != section->counter)
                    return written;
                const auto skip = static_cast<size_t>(current - blockStart);
                const auto available = std::min<std::uint64_t>(
                    section->size, output.size() - written);
                const auto length = static_cast<size_t>(
                    std::min<std::uint64_t>(available, ChunkMax - skip));
                if (!length || blockStart > ciphertext->size ||
                    length + skip > ciphertext->size - blockStart ||
                    absoluteSectionOffset >
                        std::numeric_limits<std::uint64_t>::max() - blockStart)
                    return written;

                std::vector<u8> buffer(skip + length);
                if (ciphertext->ReadUnchecked(span<u8>(buffer), static_cast<size_t>(blockStart)) != buffer.size())
                    return written;
                {
                    std::scoped_lock guard{mutex};
                    cipher.SetIV(Counter(iv, section->counter, absoluteSectionOffset + blockStart));
                    cipher.Decrypt(span<u8>(buffer));
                }
                std::memcpy(output.data() + written, buffer.data() + skip, length);
                written += length;
            }
            return written;
        }

      public:
        BktrCtrExBacking(
            std::shared_ptr<Backing> rawSection,
            std::shared_ptr<const SubsectionTable> table,
            crypto::KeyStore::Key128 key,
            std::array<std::uint8_t, 16> initialIv,
            std::uint64_t sectionAbsoluteOffset)
            : Backing({true, false, false}, table ? static_cast<size_t>(table->Size()) : 0),
              ciphertext(std::move(rawSection)),
              subsections(std::move(table)),
              iv(initialIv),
              cipher(key, MBEDTLS_CIPHER_AES_128_CTR),
              absoluteSectionOffset(sectionAbsoluteOffset) {
            if (!ciphertext || !subsections ||
                subsections->Size() > ciphertext->size ||
                subsections->Size() > std::numeric_limits<size_t>::max() ||
                (sectionAbsoluteOffset & 0xf))
                throw exception("Invalid BKTR physical subsection backing");
        }
    };
}
