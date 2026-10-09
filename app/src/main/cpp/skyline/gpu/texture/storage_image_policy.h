// SPDX-License-Identifier: MPL-2.0
#pragma once

namespace skyline::gpu::texture::storage_image_policy {
    // VK_IMAGE_USAGE_STORAGE_BIT may only be requested for supported
    // optimal-tiling *color* images. Compressed and depth/stencil formats
    // cannot be used for shader storage writes in this path.
    constexpr bool CanUseStorage(bool colorAspect, bool compressed,
                                 bool optimalTilingStorageFeature) {
        return colorAspect && !compressed && optimalTilingStorageFeature;
    }
}
