// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/interconnect/command_executor.h>
#include <gpu/texture/format.h>
#include "samplers.h"
#include "textures.h"

namespace skyline::gpu::interconnect {
    union BindlessHandle {
        u32 raw;

        struct {
            u32 textureIndex : 20;
            u32 samplerIndex : 12;
        };
    };

    static DynamicBufferBinding GetConstantBufferBinding(InterconnectContext &ctx,
                                                         span<const u32, Shader::Info::MAX_CBUFS> cbufSizes,
                                                         BufferView view, size_t idx,
                                                         vk::PipelineStageFlagBits dstStage,
                                                         vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        if (!view) // Return a dummy buffer if the constant buffer isn't bound
            return BufferBinding{ctx.gpu.megaBufferAllocator.Allocate(ctx.executor.cycle, PAGE_SIZE).buffer, 0, PAGE_SIZE};

        ctx.executor.AttachBuffer(view);
        view.GetBuffer()->PopulateReadBarrier(dstStage, srcStageMask, dstStageMask);

        size_t sizeOverride{std::min<size_t>(cbufSizes[idx], view.size)};
        if (auto megaBufferBinding{view.TryMegaBuffer(ctx.executor.cycle, ctx.gpu.megaBufferAllocator, ctx.executor.executionTag, sizeOverride)}) {
            return megaBufferBinding;
        } else {
            view.GetBuffer()->BlockSequencedCpuBackingWrites();
            return view;
        }
    }

    static DynamicBufferBinding GetStorageBufferBinding(InterconnectContext &ctx, const auto &desc,
                                                        ConstantBuffer &cbuf, CachedMappedBufferView &cachedView,
                                                        vk::PipelineStageFlagBits dstStage,
                                                        vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        struct SsboDescriptor {
            u64 address;
            u32 size;
        };
        auto ssbo{cbuf.Read<SsboDescriptor>(ctx.executor, desc.cbuf_offset)};
        if (ssbo.size == 0)
            return BufferBinding{ctx.gpu.megaBufferAllocator.Allocate(ctx.executor.cycle, PAGE_SIZE).buffer, 0, PAGE_SIZE};

        size_t padding{ssbo.address & (ctx.gpu.traits.minimumStorageBufferAlignment - 1)};
        cachedView.Update(ctx, ssbo.address - padding, util::AlignUp(ssbo.size + padding, ctx.gpu.traits.minimumStorageBufferAlignment));
        if (!cachedView.view) // Return a dummy buffer if the SSBO isn't bound
            return BufferBinding{ctx.gpu.megaBufferAllocator.Allocate(ctx.executor.cycle, PAGE_SIZE).buffer, 0, PAGE_SIZE};

        auto view{cachedView.view};
        ctx.executor.AttachBuffer(view);
        view.GetBuffer()->PopulateReadBarrier(dstStage, srcStageMask, dstStageMask);

        if (desc.is_written) {
            if (view.GetBuffer()->SequencedCpuBackingWritesBlocked()) {
                srcStageMask |= vk::PipelineStageFlagBits::eAllCommands;
                dstStageMask |= dstStage;
            }

            view.GetBuffer()->MarkGpuDirty(ctx.executor.usageTracker);
        } else {
            if (auto megaBufferBinding{view.TryMegaBuffer(ctx.executor.cycle, ctx.gpu.megaBufferAllocator, ctx.executor.executionTag)})
                return megaBufferBinding;
        }

        view.GetBuffer()->BlockSequencedCpuBackingWrites();

        return view;
    }

    static BindlessHandle ReadBindlessHandle(InterconnectContext &ctx, auto &constantBuffers, const auto &desc, size_t arrayIdx) {
        ConstantBuffer &primaryCbuf{constantBuffers[desc.cbuf_index]};
        if (!primaryCbuf.view)
            return { .raw = 0 };

        size_t elemOffset{arrayIdx << desc.size_shift};
        size_t primaryCbufOffset{desc.cbuf_offset + elemOffset};
        u32 primaryVal{primaryCbuf.Read<u32>(ctx.executor, primaryCbufOffset)};

        if constexpr (requires { desc.has_secondary; }) {
            if (desc.has_secondary) {
                ConstantBuffer &secondaryCbuf{constantBuffers[desc.secondary_cbuf_index]};
                size_t secondaryCbufOffset{desc.secondary_cbuf_offset + elemOffset};
                u32 secondaryVal{secondaryCbuf.Read<u32>(ctx.executor, secondaryCbufOffset)};
                return {.raw = (primaryVal << desc.shift_left) | (secondaryVal << desc.secondary_shift_left)};
            }
        }

        return {.raw = primaryVal};
    }

    // Maxwell storage-image formats are independent of the TIC's component
    // swizzle: Vulkan storage image views MUST have identity components.
    static texture::Format ResolveStorageImageFormat(Shader::ImageFormat formatType) {
        switch (formatType) {
            case Shader::ImageFormat::Typeless: return {};
            case Shader::ImageFormat::R8_UINT: return format::R8Uint;
            case Shader::ImageFormat::R8_SINT: return format::R8Sint;
            case Shader::ImageFormat::R16_UINT: return format::R16Uint;
            case Shader::ImageFormat::R16_SINT: return format::R16Sint;
            case Shader::ImageFormat::R32_UINT: return format::R32Uint;
            case Shader::ImageFormat::R32G32_UINT: return format::R32G32Uint;
            case Shader::ImageFormat::R32G32B32A32_UINT: return format::R32G32B32A32Uint;
        }
        return {};
    }

    static vk::DescriptorImageInfo GetStorageImageBinding(
        InterconnectContext &ctx, const auto &desc, Textures &textures,
        BindlessHandle handle, vk::PipelineStageFlagBits dstStage,
        vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        const auto requestedFormat{ResolveStorageImageFormat(desc.format)};
        auto *view{textures.GetTexture(ctx, handle.textureIndex, desc.type)};

        auto &texture{*view->texture};
        const bool supportsStorage{static_cast<bool>(texture.usage & vk::ImageUsageFlagBits::eStorage)};
        const bool correctLayout{texture.layout == vk::ImageLayout::eGeneral};
        const bool colorAspect{static_cast<bool>(texture.format->vkAspect & vk::ImageAspectFlagBits::eColor)};
        const bool compatibleView{!requestedFormat || requestedFormat == texture.format ||
            (static_cast<bool>(texture.flags & vk::ImageCreateFlagBits::eMutableFormat) &&
             requestedFormat->IsCompatible(*texture.format))};

        if (!supportsStorage || !correctLayout || !colorAspect || !compatibleView) {
            // Never pass an image without VK_IMAGE_USAGE_STORAGE_BIT to a
            // storage-image descriptor: this is invalid Vulkan on Turnip.
            // Preserve descriptor validity with a typed, identity-mapped
            // fallback instead of crashing the host driver.
            LOGW("Storage image fallback: TIC={} format={} requested={} storage={} layout={}",
                 handle.textureIndex, vk::to_string(*texture.format),
                 requestedFormat ? vk::to_string(*requestedFormat) : "Typeless",
                 supportsStorage, vk::to_string(texture.layout));
            view = textures.GetNullStorageTexture(ctx, desc.type, requestedFormat);
        }

        ctx.executor.AttachTexture(view);
        auto &selectedTexture{*view->texture};
        // Null placeholder textures have no guest metadata; supply an empty
        // format there so Texture::GetView uses the native backing format.
        const auto viewFormat{selectedTexture.guest ? requestedFormat : texture::Format{}};
        auto storageView{selectedTexture.GetView(view->type, view->range, viewFormat,
                                               vk::ComponentMapping{})};

        // The executor records a memory barrier before the shader stage.
        // Storage-image writers also dirty the guest texture's mapped ranges
        // so future guest reads cannot reuse stale framebuffer contents.
        srcStageMask |= vk::PipelineStageFlagBits::eAllCommands;
        dstStageMask |= dstStage;
        if (selectedTexture.guest) {
            selectedTexture.PopulateReadBarrier(dstStage, srcStageMask, dstStageMask);
            if (desc.is_written)
                selectedTexture.MarkGpuDirty(ctx.executor.usageTracker);
        }

        return vk::DescriptorImageInfo{
            .imageView = storageView->GetView(),
            .imageLayout = vk::ImageLayout::eGeneral,
        };
    }

    static std::pair<vk::DescriptorImageInfo, TextureView *> GetTextureBinding(InterconnectContext &ctx, const auto &desc,
                                                                               Samplers &samplers, Textures &textures,
                                                                               BindlessHandle handle,
                                                                               vk::PipelineStageFlagBits dstStage,
                                                                               vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        auto sampler{samplers.GetSampler(ctx, handle.samplerIndex, handle.textureIndex)};
        auto texture{textures.GetTexture(ctx, handle.textureIndex, desc.type)};
        ctx.executor.AttachTexture(texture);
        auto view{texture->GetView()};
        texture->texture->PopulateReadBarrier(dstStage, srcStageMask, dstStageMask);

        return {
            vk::DescriptorImageInfo{
                .sampler = **sampler,
                .imageView = view,
                .imageLayout = texture->texture->layout
            },
            texture
        };
    }
}
