// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/gpu/texture/storage_image_policy.h"
using skyline::gpu::texture::storage_image_policy::CanUseStorage;
static_assert(CanUseStorage(true, false, true));
static_assert(!CanUseStorage(true, true, true));
static_assert(!CanUseStorage(false, false, true));
static_assert(!CanUseStorage(true, false, false));
int main() { return CanUseStorage(true, false, true) ? 0 : 1; }
