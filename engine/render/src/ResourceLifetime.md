# GPU resource lifetime

Meshes and textures live in `SceneResources`, stored in `SlotMap`s and reached through generational `MeshHandle` / `TextureHandle` values (see `engine/core/include/SlotMap.md`).

## States

| State | How you see it |
|---|---|
| Ready | `Create*` returned a valid handle; `GetMesh` / `GetTexture` return non-null. |
| Failed | `Create*` returned an invalid handle and logged why (bad data from `ValidateMesh` / `ValidateTexture`, or a Vulkan error). |
| Gone | The handle was destroyed; it stays stale forever, even after its slot is reused. |

Uploads are synchronous (the queue waits), so there is no "loading" state yet. When asynchronous uploads arrive they add one.

What draws when something is missing:

- **Stale or invalid mesh**: that draw item is skipped.
- **Stale or invalid texture**: the item is drawn with the 1x1 white fallback texture.

## Why not destroy immediately?

The CPU records frame N+1 while the GPU may still be executing frames N and N-1 (two frames in flight). If a buffer is freed while one of those frames still reads it, the GPU reads freed memory. That's a use-after-free you can't catch on the CPU side.

So `DestroyMesh` / `ReplaceMesh` (and the texture versions) do two things:

1. Make the handle stale (destroy) or point it at the new data (replace). From the next recorded frame on, nothing references the old resource.
2. Move the old GPU objects into a `RetireQueue`, tagged with the current frame serial.

## The serial math

`frameSerial` counts submitted frames. It's incremented by `OnFrameSubmitted()` after each successful submit.

```text
retire at serial S   ->  frames 0..S-1 are submitted; any of them may use the resource
                         frames >= S never see it (the handle is already stale/replaced)
after submitting frame M, frameSerial = M+1
the fence for frame M's slot was waited at the start of M, so frames <= M-2 are done
```

The resource is safe to free when its last possible user (`S-1`) is done:

```text
S - 1 <= M - 2   <=>   S <= M - 1 = frameSerial - framesInFlight
```

That's exactly `retired.Collect(frameSerial - framesInFlight)`. If a frame fails to submit, the serial isn't bumped, so collection only gets later (safe), never earlier.

When the device is known to be idle (swapchain recreation, shutdown), `FlushRetired()` frees everything at once.

## Descriptor sets

Each texture owns a descriptor set (set 1) allocated from a pool with `FREE_DESCRIPTOR_SET_BIT`. `TextureResource::Release()` frees the set, then the sampler, then the image. A retired texture therefore keeps its descriptor set valid until the frames that bound it are done.
