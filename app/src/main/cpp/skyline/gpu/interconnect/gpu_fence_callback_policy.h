// SPDX-License-Identifier: MPL-2.0
#pragma once

namespace skyline::gpu::interconnect::fence_policy {
    // On Strato's immediate path, guest synchronization callbacks may
    // complete before their Vulkan command buffer fence is signaled.
    // Direct memory import already requires fence-completion ordering.
    // Opt-in antiflicker adds the same ordering to the non-DMI path.
    constexpr bool DelayCallbacks(bool directMemoryImport, bool antiFlicker) {
        return directMemoryImport || antiFlicker;
    }
}
