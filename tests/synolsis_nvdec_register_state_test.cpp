// SPDX-License-Identifier: MPL-2.0
// Host-only deterministic NVDEC register indexing tests.
// No codec bytes, console keys or copyrighted firmware are required.
#include "../app/src/main/cpp/skyline/soc/host1x/classes/nvdec_register_state.h"
#include <cassert>
#include <cstdint>

using skyline::soc::host1x::NvdecRegisterState;

constexpr bool RegisterTransitions() {
    NvdecRegisterState regs{};
    if (!regs.Write(NvdecRegisterState::SetCodec, 3)) return false;
    if (regs.CodecId() != 3) return false;
    if (!regs.Write(NvdecRegisterState::PictureInfo, 0x4000)) return false;
    if (!regs.Write(NvdecRegisterState::BitstreamAddress, 0xA000)) return false;
    if (regs.GuestPictureInfoAddress() != 0x400000ULL) return false;
    if (regs.GuestBitstreamAddress() != 0xA00000ULL) return false;
    if (regs.Write(NvdecRegisterState::Count, 1)) return false;
    if (regs.Write(UINT32_MAX, 1)) return false;
    if (regs.Raw(NvdecRegisterState::Count) != 0) return false;
    if (regs.AddressScaled(UINT32_MAX) != 0) return false;
    if (!regs.Write(NvdecRegisterState::BitstreamAddress, UINT32_MAX)) return false;
    if (regs.GuestBitstreamAddress() != (static_cast<std::uint64_t>(UINT32_MAX) << 8)) return false;
    regs.Reset();
    return regs.CodecId() == 0 && regs.GuestPictureInfoAddress() == 0 &&
           regs.GuestBitstreamAddress() == 0;
}
static_assert(RegisterTransitions());
int main() {
    NvdecRegisterState regs{};
    assert(regs.Write(NvdecRegisterState::FrameNumber, 42));
    assert(regs.Raw(NvdecRegisterState::FrameNumber) == 42);
    assert(RegisterTransitions());
}
