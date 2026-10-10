// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace skyline::soc::host1x::video_probe {
    // Inspection of guest Host1x pushbuffers, with no syncpoint side effects.
    // This is intentionally separate from executing/decompressing video:
    // until VIC/MediaCodec and guest fences are wired we MUST NOT acknowledge
    // a decoded frame, mutate guest memory, or claim that cutscenes work.
    struct Summary {
        std::uint64_t videoMethodWrites{};
        std::uint64_t nvdecCodecWrites{};
        std::uint64_t nvdecExecutions{};
        std::uint64_t vicExecutions{};
        std::uint32_t lastNvdecCodec{};
        std::uint32_t lastVicExecute{};
        bool truncatedOrUnsupported{};
    };

    class VideoCommandProbe final {
        static constexpr std::uint32_t NvdecClassId{0xF0};
        static constexpr std::uint32_t VicClassId{0x5D};
        static constexpr std::uint32_t ThiMethod0{0x10};
        static constexpr std::uint32_t ThiMethod1{0x11};
        static constexpr std::uint32_t NvdecSetCodec{0x80};
        static constexpr std::uint32_t NvdecExecute{0xC0};
        static constexpr std::uint32_t VicExecute{0xC0};

        std::uint32_t nvdecMethod{};
        std::uint32_t vicMethod{};

        void Write(std::uint32_t targetClass, std::uint32_t method,
                   std::uint32_t value, Summary &out) {
            if (targetClass != NvdecClassId && targetClass != VicClassId)
                return;
            ++out.videoMethodWrites;
            auto &stored = targetClass == NvdecClassId ? nvdecMethod : vicMethod;
            if (method == ThiMethod0) {
                stored = value;
            } else if (method == ThiMethod1) {
                if (targetClass == NvdecClassId) {
                    if (stored == NvdecSetCodec) {
                        ++out.nvdecCodecWrites;
                        out.lastNvdecCodec = value;
                    } else if (stored == NvdecExecute) {
                        ++out.nvdecExecutions;
                    }
                } else if (stored == VicExecute) {
                    ++out.vicExecutions;
                    out.lastVicExecute = value;
                }
            }
        }

      public:
        // This intentionally does not follow Restart/Gather GPU addresses.
        // Such opcodes require separate validated SMMU resolution.
        bool Inspect(std::span<const std::uint32_t> words, Summary &out) {
            std::size_t pos{};
            std::uint32_t targetClass{1};
            const auto take = [&](std::size_t count) -> bool {
                return count <= words.size() - pos;
            };

            while (pos < words.size()) {
                const std::uint32_t raw{words[pos++]};
                const std::uint32_t opcode{raw >> 28};
                const std::uint32_t method{(raw >> 16) & 0xFFF};
                const std::uint32_t low{raw & 0xFFFF};
                switch (opcode) {
                    case 0: { // SetClass with up to 6 masked payloads
                        targetClass = (raw >> 6) & 0x3FF;
                        const auto mask{raw & 0x3F};
                        if (!take(std::popcount(mask))) {
                            out.truncatedOrUnsupported = true;
                            return false;
                        }
                        for (std::uint32_t bit{}; bit < 6; ++bit)
                            if ((mask >> bit) & 1)
                                Write(targetClass, method + bit, words[pos++], out);
                        break;
                    }
                    case 1: // Incr
                    case 2: { // NonIncr
                        if (!take(low)) {
                            out.truncatedOrUnsupported = true;
                            return false;
                        }
                        for (std::uint32_t j{}; j < low; ++j)
                            Write(targetClass, method + (opcode == 1 ? j : 0),
                                  words[pos++], out);
                        break;
                    }
                    case 3: { // Mask with up to 16 masked payloads
                        if (!take(std::popcount(low))) {
                            out.truncatedOrUnsupported = true;
                            return false;
                        }
                        for (std::uint32_t bit{}; bit < 16; ++bit)
                            if ((low >> bit) & 1)
                                Write(targetClass, method + bit, words[pos++], out);
                        break;
                    }
                    case 4: // Imm; low 12 bits are immediate data
                        Write(targetClass, method, raw & 0xFFF, out);
                        break;
                    default:
                        out.truncatedOrUnsupported = true;
                        return false;
                }
            }
            return true;
        }
    };
}
