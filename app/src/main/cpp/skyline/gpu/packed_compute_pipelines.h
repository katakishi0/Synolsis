// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <vulkan/vulkan_raii.hpp>
#include <vfs/filesystem.h>
#include "packed_compute_manifest.h"

namespace skyline::gpu::packed_compute {
    // These are real VkComputePipelines and VkDescriptorSetLayouts, but are
    // deliberately NOT dispatched by the presenter until descriptor writes,
    // memory barriers, intermediate storage images and fences exist.
    // Building a pipeline is NOT working FSR/frame generation.
    class Pass {
        vk::raii::ShaderModule shader;
        vk::raii::DescriptorSetLayout descriptorLayout;
        vk::raii::PipelineLayout layout;
        vk::raii::Pipeline pipeline;

        static std::vector<std::uint32_t> ReadSpirv(vfs::FileSystem &assets,
                                                     std::string_view name) {
            const auto backing{assets.OpenFile(std::string(name))};
            if (!backing || !backing->size || (backing->size % 4) ||
                backing->size > 1024 * 1024)
                throw exception("Invalid compute shader asset '{}'", name);

            std::vector<std::uint32_t> words(backing->size / 4);
            if (backing->ReadUnchecked(
                    span<u8>(reinterpret_cast<u8 *>(words.data()), backing->size)) != backing->size ||
                !ValidSpirv(std::span<const std::uint32_t>(words.data(), words.size())))
                throw exception("Invalid SPIR-V compute shader '{}'", name);
            return words;
        }

        static std::array<vk::DescriptorSetLayoutBinding, 4> Bindings(Kind kind) {
            constexpr auto stage{vk::ShaderStageFlagBits::eCompute};
            const auto sampled{vk::DescriptorType::eCombinedImageSampler};
            const auto storage{vk::DescriptorType::eStorageImage};
            if (kind == Kind::Temporal) {
                return {{
                    {0, sampled, 1, stage},
                    {1, sampled, 1, stage},
                    {2, storage, 1, stage},
                    {3, sampled, 1, stage},
                }};
            }

            return {{
                {0, vk::DescriptorType::eUniformBuffer, 1, stage},
                {1, vk::DescriptorType::eSampledImage, 1, stage},
                {2, storage, 1, stage},
                {3, vk::DescriptorType::eSampler, 1, stage},
            }};
        }

      public:
        Pass(vk::raii::Device &device, vfs::FileSystem &assets, const Asset &asset)
            : shader{[&]() {
                  auto words{ReadSpirv(assets, asset.path)};
                  return vk::raii::ShaderModule{device, vk::ShaderModuleCreateInfo{
                      .codeSize = words.size() * sizeof(std::uint32_t),
                      .pCode = words.data(),
                  }};
              }()},
              descriptorLayout{[&]() {
                  const auto bindings{Bindings(asset.kind)};
                  return vk::raii::DescriptorSetLayout{device, vk::DescriptorSetLayoutCreateInfo{
                      .bindingCount = static_cast<std::uint32_t>(bindings.size()),
                      .pBindings = bindings.data(),
                  }};
              }()},
              layout{[&]() {
                  const auto handle{*descriptorLayout};
                  const vk::PushConstantRange range{
                      vk::ShaderStageFlagBits::eCompute, 0, 16
                  };
                  return vk::raii::PipelineLayout{device, vk::PipelineLayoutCreateInfo{
                      .setLayoutCount = 1,
                      .pSetLayouts = &handle,
                      .pushConstantRangeCount = asset.usesPushConstants ? 1U : 0U,
                      .pPushConstantRanges = asset.usesPushConstants ? &range : nullptr,
                  }};
              }()},
              pipeline{device, nullptr, vk::ComputePipelineCreateInfo{
                  .stage = vk::PipelineShaderStageCreateInfo{
                      .stage = vk::ShaderStageFlagBits::eCompute,
                      .module = *shader,
                      .pName = "main",
                  },
                  .layout = *layout,
              }} {}

        vk::Pipeline Handle() const { return *pipeline; }
        vk::PipelineLayout Layout() const { return *layout; }
        vk::DescriptorSetLayout DescriptorLayout() const { return *descriptorLayout; }
    };

    class PipelineSet {
        std::array<std::unique_ptr<Pass>, Assets.size()> passes{};

      public:
        PipelineSet(vk::raii::Device &device, vfs::FileSystem &assets) {
            // Fail atomically: partial initialization is not a feature and
            // Vulkan handles are destroyed by RAII on exception.
            for (std::size_t i{}; i < Assets.size(); ++i)
                passes[i] = std::make_unique<Pass>(device, assets, Assets[i]);
        }

        const Pass &Get(Kind kind) const {
            return *passes[static_cast<std::size_t>(kind)];
        }
    };
}
