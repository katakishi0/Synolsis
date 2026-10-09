// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/common/debug_log_sample.h"

using skyline::diagnostics::ShouldSample;
static_assert(!ShouldSample(0));
static_assert(ShouldSample(1) && ShouldSample(8));
static_assert(!ShouldSample(9) && !ShouldSample(15));
static_assert(ShouldSample(16));
static_assert(!ShouldSample(17) && !ShouldSample(31));
static_assert(ShouldSample(32));
static_assert(ShouldSample(1024));
static_assert(!ShouldSample(1025));
static_assert(!ShouldSample(1000000));
int main() { return ShouldSample(65536) ? 0 : 1; }
