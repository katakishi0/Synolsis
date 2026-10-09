// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/gpu/interconnect/gpu_fence_callback_policy.h"

using skyline::gpu::interconnect::fence_policy::DelayCallbacks;
static_assert(!DelayCallbacks(false, false));
static_assert(DelayCallbacks(true, false));
static_assert(DelayCallbacks(false, true));
static_assert(DelayCallbacks(true, true));
int main() { return DelayCallbacks(false, true) && !DelayCallbacks(false, false) ? 0 : 1; }
