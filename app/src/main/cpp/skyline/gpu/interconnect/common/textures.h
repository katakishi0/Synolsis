// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <tsl/robin_map.h>
#include <unordered_map>
#include <shader_compiler/shader_info.h>
#include <gpu/texture/texture.h>
#include "common.h"
#include "tic.h"

namespace skyline::gpu::interconnect {
    class TexturePoolState : dirty::CachedManualDirty {
      public:
        struct EngineRegisters {
            const engine_common::TexHeaderPool &texHeaderPool;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource<EngineRegisters> engine;

      public:
        span<TextureImageControl> textureHeaders;

        TexturePoolState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        void Flush(InterconnectContext &ctx);

        void PurgeCaches();
    };

    class Textures {
      private:
        std::shared_ptr<TextureView> nullTextureView{};
        // Fallback views must themselves have VK_IMAGE_USAGE_STORAGE_BIT and
        // an identity-component view, unlike the sampled-image null view.
        std::unordered_map<u64, std::shared_ptr<TextureView>> nullStorageViews;
        dirty::ManualDirtyState<TexturePoolState> texturePool;

        tsl::robin_map<TextureImageControl, std::shared_ptr<TextureView>, util::ObjectHash<TextureImageControl>> textureHeaderStore;

        struct CacheEntry {
            TextureImageControl tic;
            TextureView *view;
            u64 sequenceNumber;
        };
        std::vector<CacheEntry> textureHeaderCache;

      public:
        Textures(DirtyManager &manager, const TexturePoolState::EngineRegisters &engine);

        void MarkAllDirty();

        TextureView *GetTexture(InterconnectContext &ctx, u32 index, Shader::TextureType shaderType);

        TextureView *GetNullStorageTexture(InterconnectContext &ctx, Shader::TextureType shaderType,
                                           texture::Format requestedFormat);

        Shader::TextureType GetTextureType(InterconnectContext &ctx, u32 index);
    };
}
