# Post-V5 render-scale foundation

The validated V5 APK was built from b54b92efd0eb9129c3243da755daaa0e446fa909.

Do not enable resolution scaling until Vulkan render targets, viewport/scissor, texture aliases, readbacks, and presentation are handled consistently. Preserve native 1x default to avoid FPS regression.
