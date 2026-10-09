// SPDX-License-Identifier: MPL-2.0
#include <cassert>
#include "soc/host1x/command_packet_bounds.h"
using skyline::soc::host1x::command_packet::Fits;
using skyline::soc::host1x::command_packet::MaskWords;
static_assert(Fits(0,0));
static_assert(Fits(5,5));
static_assert(!Fits(5,6));
static_assert(MaskWords(0U)==0);
static_assert(MaskWords(0b101101U)==4);
static_assert(MaskWords(0xFFFFU)==16);
int main() {
    for (std::size_t remaining = 0; remaining < 512; ++remaining) {
        for (std::size_t required = 0; required < 512; ++required)
            assert(Fits(remaining, required) == (required <= remaining));
    }
}
