# DearPyGui — external-GPU-memory fork

This is a fork of [hoffstadt/DearPyGui](https://github.com/hoffstadt/DearPyGui)
that adds **zero-copy texture interop** with external GPU APIs (Vulkan,
slangpy, etc.) on the Linux/OpenGL backend.

Stock DearPyGui's only public texture API is `add_raw_texture`, which uploads
a CPU buffer (`numpy` array) to GL each frame. That's a bandwidth ceiling: a
1024² RGBA32F frame is 16 MB CPU→GPU per frame, plus a GPU→CPU readback if
the producer is also on the GPU.

This fork lets you skip the round-trip. A producer (e.g. slangpy) allocates a
Vulkan image with `VK_KHR_external_memory_fd` and exports an opaque fd; you
hand that fd to `add_raw_texture(..., external_memory_fd=...)` and DPG
imports it via `GL_EXT_memory_object_fd`. The GL texture and the Vulkan
storage image are now **the same physical GPU allocation** — the producer
writes, ImGui samples, no copies.

## What changed vs. upstream

- `mvRawTexture` accepts two new keyword args: `external_memory_fd` (int) and
  `external_memory_size` (int, bytes). When `external_memory_fd >= 0`, the GL
  texture is created once via `glImportMemoryFdEXT` + `glTexStorageMem2DEXT`
  and the per-frame CPU upload path is skipped entirely.
- New utility `LoadTextureFromExternalMemoryFd` in `mvUtilities_linux.cpp`.
- Stubs on Windows/macOS (return `ImTextureID_Invalid`). The same idea ports
  to D3D11 (`OpenSharedHandle`) and Metal (`IOSurface`); patches welcome.

Everything else is unchanged — existing `add_raw_texture` calls keep their
old behavior.

## Requirements

- Linux with `GL_EXT_memory_object` and `GL_EXT_memory_object_fd` (Mesa,
  recent NVIDIA).
- A producer that exports opaque-fd handles (slangpy ≥ 0.41, raw Vulkan,
  CUDA's `cudaExternalMemory`, etc.).

## Demo: rendering into DPG from slangpy with no copies

```python
import dearpygui.dearpygui as dpg
import slangpy as spy

W, H = 1024, 1024

# 1. slangpy makes a Vulkan storage image marked as exportable.
device = spy.create_device(type=spy.DeviceType.vulkan, include_paths=["shaders"])
texture = device.create_texture(
    format=spy.Format.rgba8_unorm,
    width=W, height=H,
    usage=(spy.TextureUsage.unordered_access
           | spy.TextureUsage.shader_resource
           | spy.TextureUsage.shared),
)
fd = int(texture.shared_handle.value)            # opaque-fd from VK_KHR_external_memory_fd
size_bytes = int(texture.memory_usage.device)    # driver-reported allocation size

# 2. DPG imports the same memory as a GL texture.
dpg.create_context(); dpg.create_viewport(width=W+32, height=H+80); dpg.setup_dearpygui()
with dpg.texture_registry():
    dpg.add_raw_texture(W, H, (),                          # default_value ignored
                        format=dpg.mvFormat_Float_rgba,
                        external_memory_fd=fd,
                        external_memory_size=size_bytes,
                        tag="shared_tex")

with dpg.window(label="zero-copy", tag="w"):
    dpg.add_image("shared_tex")
dpg.set_primary_window("w", True); dpg.show_viewport()

# 3. slangpy writes; DPG samples. Same memory.
kernel = device.create_compute_kernel(device.load_program("render", ["main"]))
import time; t0 = time.perf_counter()
while dpg.is_dearpygui_running():
    kernel.dispatch(thread_count=spy.uint3(W, H, 1),
                    vars={"output": texture, "uniforms": {"resolution": spy.uint2(W, H),
                                                          "time": time.perf_counter() - t0}})
    device.wait_for_idle()              # CPU sync; replace with VK<->GL semaphores for max speed
    dpg.render_dearpygui_frame()
dpg.destroy_context()
```

The companion `render.slang` here can be any compute shader writing
`RWTexture2D<float4>` — see the `spy-imgui/` example repo.

## Building

```bash
git clone --recursive <this fork>
cd DearPyGui
mkdir cmake-build-local && cd cmake-build-local
cmake .. -DMVDIST_ONLY=True -DMVDPG_VERSION=2.3 -DMV_PY_VERSION=3.12 -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
# drop the resulting _dearpygui.so over the one in your venv:
cp DearPyGui/_dearpygui.so /path/to/venv/lib/python3.12/site-packages/dearpygui/
```

## Caveats

1. **Sync is the user's job.** The example above CPU-syncs with
   `device.wait_for_idle()`. For real performance use external semaphores —
   slangpy exposes `Fence.shared_handle`, GL has `glGenSemaphoresEXT` /
   `glWaitSemaphoreEXT`. Without that, the Vulkan queue stalls each frame.
2. **Pass the driver-reported allocation size, not `W*H*bpp`.** Tiled images
   get padded for alignment. With slangpy use
   `texture.memory_usage.device` (Vulkan reports the same number via
   `VkMemoryRequirements.size`). If you hit `GL_INVALID_VALUE` (`0x501`)
   from `glTexStorageMem2DEXT`, that's what's wrong.
3. **Format must match exactly.** The fork imports as `GL_RGBA8` (or
   `GL_RGB8` for 3-component). Other formats need to be added to
   `LoadTextureFromExternalMemoryFd`.
4. **Linux/OpenGL only.** Windows and macOS fall back to a no-op stub.

## License

MIT, same as upstream.
