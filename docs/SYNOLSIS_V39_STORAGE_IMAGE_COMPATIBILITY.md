# Synolsis V39 — guest shader storage images (Maxwell 3D / Kepler Compute)

## Evidence and V38 outcome

User compared V38 GPU-fence anti-flicker OFF/ON in NieR:Automata. **Both**
conditions still render large black, blue, and magenta rectangles. FPS
screenshots were 17/16 and memory 3088/2848 MiB, not controlled benchmark
samples. The earlier guest startup regression was fixed at V35; neither the
V37 Vulkan texture transfer ordering nor the V38 deferred callbacks
has been validated as fixing the corruption.

The code-level blocker is in Strato's Maxwell3D / KeplerCompute descriptor
managers. Shader `image_descriptors` were declared as
`VK_DESCRIPTOR_TYPE_STORAGE_IMAGE`, but were **never populated** in
`SyncDescriptors`. Guest textures were created without
`VK_IMAGE_USAGE_STORAGE_BIT`. The implementation therefore could not
execute correctly for shaders that read/write `imageLoad/imageStore`.

Relevant Eden reference:
https://github.com/eden-emulator/mirror/commit/8ed05425dddf17aeafa4daa751cb3a3a13c5a31b
(typed/typeless storage-image views must use *identity* component mapping;
that Eden patch helped NieR rendering in its own GPU architecture).

## V39 implementation

- Determine whether each guest texture's native Vulkan format supports
  `VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT` (optimal tiling) and is an
  uncompressed color image before adding the storage usage bit.
- Fill storage-image Vulkan descriptors at their original shader binding
  indices in both Maxwell 3D and Kepler Compute pipelines.
- Read bindless handles from guest constant buffers; support typed storage
  formats R8_UINT/SINT, R16_UINT/SINT, R32_UINT, R32G32_UINT and
  R32G32B32A32_UINT plus Typeless, matching the compiler's declared
  `Shader::ImageFormat` enum.
- Storage image views always use identity component swizzles; verify guest
  source format/view compatibility and the required GENERAL image layout.
- If the backing cannot be used for storage, allocate a minimal typed
  fallback image with valid storage usage instead of passing an invalid
  Vulkan descriptor to Turnip. Fallback does **not** reproduce guest data.
- Mark written guest storage textures dirty and add conservative all-command
  producer -> shader consumer dependencies for the stage.
- Update Maxwell per-constant-buffer quick-bind metadata, so rebinding a
  constant buffer also rebases storage-image descriptors.
- Avoid dereferencing absent `guest` on internally created fallback texture
  objects.

## Remaining caveats

This first-pass fix **does not prove full image/storage coherency** for
draws targeting the same texture as a render attachment, multisampled
storage resources, exotic swizzles, or unsupported GPU formats. A fallback
dummy is intended to keep Vulkan descriptors valid; affected shader outputs
remain inaccurate. We must not claim universal game compatibility.

Prefer evaluating after a complete Kotlin/NDK compile and full Vulkan
shader asset verification. Only describe NieR/Nine Sols flicker as
**fixed** if screenshots and emulation.log confirm it on the user's device.
Performance can change when storage flags are added to more images; monitor
GPU memory and frame times.

## A/B comparison

Use same device/driver, portable mode, frame skip 0, original presentation
scale 100%. Keep V35 and V38 backups. Compare V38 vs V39 with the
same game save and capture UI overlay separately from GPU output. Report
`Storage image fallback` and Vulkan validation messages.
