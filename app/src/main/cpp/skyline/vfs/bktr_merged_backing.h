// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <algorithm>
#include <memory>
#include "backing.h"
#include "bktr_relocation.h"

namespace skyline::vfs::bktr {
    // A read-only, virtual updated RomFS image. The input Backings must
    // already describe the correct *physical section* coordinate spaces:
    // base decrypts with normal NCA CTR and patch with subsection CTR-Ex.
    // This class alone must not be connected to guest storage until the
    // caller validates both title IDs and RomFS IVFC section offsets.
    class BktrMergedBacking final : public Backing {
        std::shared_ptr<Backing> base;
        std::shared_ptr<Backing> patch;
        std::shared_ptr<const RelocationTable> relocations;

      protected:
        size_t ReadImpl(span<u8> output, size_t offset) override {
            if (offset > size || output.size() > size - offset)
                return 0;
            size_t written{};
            while (written < output.size()) {
                const auto position{static_cast<std::uint64_t>(offset + written)};
                const auto segment{relocations->Resolve(
                    position, base->size, patch->size)};
                if (!segment || !segment->size)
                    return written;
                const auto part{static_cast<size_t>(std::min<std::uint64_t>(
                    segment->size, output.size() - written))};
                auto backing{segment->fromPatch ? patch : base};
                const auto read{backing->ReadUnchecked(
                    output.subspan(written, part),
                    static_cast<size_t>(segment->sourceOffset))};
                if (read != part)
                    return written + read;
                written += read;
            }
            return written;
        }

      public:
        BktrMergedBacking(std::shared_ptr<Backing> baseSection,
                          std::shared_ptr<Backing> patchSection,
                          std::shared_ptr<const RelocationTable> table)
            : Backing({true, false, false}, table ? static_cast<size_t>(table->Size()) : 0),
              base(std::move(baseSection)),
              patch(std::move(patchSection)),
              relocations(std::move(table)) {
            if (!base || !patch || !relocations ||
                relocations->Size() > std::numeric_limits<size_t>::max())
                throw exception("Invalid BKTR merged RomFS source");
        }
    };
}
