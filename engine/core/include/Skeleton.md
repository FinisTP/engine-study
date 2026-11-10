# Skeletons, clips and skinning (`Skeleton.hpp`, `Animator.hpp`, S8)

A rigged model is one mesh plus a skeleton. Each vertex names up to four joints and how much each
one pulls it (`SkinVertex`: joint indices + weights that add up to 1). Moving the joints moves the
skin.

## Spaces

```text
bind space      the mesh as it was modelled (the vertices in the file)
joint space     relative to one joint
model space     the whole character, before the entity's transform
world space     after the entity's transform (the scene)
```

The vertices of a skinned mesh stay in **bind space** on the GPU. Every frame each joint gets one
matrix, the **palette** entry, that takes a bind-space point to model space:

```text
palette[j] = jointModel[j] * inverseBind[j]
```

- `inverseBind[j]` (from the glTF skin) takes a bind-space point into joint `j`'s own space as it
  was when the mesh was bound.
- `jointModel[j]` takes joint space to model space for **this frame's pose**.

So a point that sits still relative to its joint follows the joint. At the rest pose
`jointModel * inverseBind` is (almost) identity: the mesh looks as modelled.

## jointModel from a pose

A pose is one `JointPose` (translation, rotation, scale) per joint, relative to its parent.
`ComputePalette` walks the joints parents first (`Skeleton::order`):

```text
local[j]      = T(translation) * R(rotation) * S(scale)
jointModel[j] = (parent >= 0 ? jointModel[parent] : I) * base[j] * local[j]
```

`base[j]` holds the fixed (not animated) nodes between a joint and its nearest joint ancestor, or
up to the scene root for the top joint. `pack_animated.py`'s `UmbraRoot` node (turn, center,
scale to a half-extent of 1) lives there, so the normalised size applies to the skinned mesh too.
The mesh node's own transform is ignored, as glTF requires for skinned meshes.

## The vertex shader

```text
skin     = w.x * palette[first + j.x] + w.y * palette[first + j.y] + ... (4 joints)
position = model * skin * bindPosition
normal   = model * skin * bindNormal    (w = 0, then normalized)
```

`first` is where this item's joints start in the frame's joint buffer (`DrawItem::jointOffset`,
`DrawList::joints`). Vulkan reads them from a storage buffer (`Scene.hlsl`, set 0 binding 2);
WebGPU from `joints` in Scene.wgsl (group 0 binding 4). One frame holds at most 16384 matrices
(1 MiB); an item past that is drawn unskinned.

`RestPositions` does the same sum on the CPU at the rest pose: for a rigged model's bounds,
feet height and colliders (the stored vertices are in bind space, so they can't be used directly).
Every entity drawing a rigged model gets an animator, a still one if it has no animation
component, so nothing is ever drawn in bind space.

## Clips

A clip is a list of channels: one joint (by **name**), one property (translation, rotation or
scale), key times and values. Matching by name means a clip from another file plays on any
skeleton with the same joint names; a `prefix:` namespace is ignored if needed (`mixamorig:Hips`
plays on `Hips`). Joints a clip doesn't move keep their rest value.

`SampleClip` finds the two keys around the time and blends them:

```text
f     = (time - t0) / (t1 - t0)
value = mix(v0, v1, f)               translation, scale
value = slerp(q0, q1, f)             rotation (shortest way round)
```

A looping clip uses `time mod duration`; a one-shot holds its last key. Step keys jump.
Cubic-spline glTF clips use their key values only (the tangents are dropped): fine for the dense
keys exporters write.

## Blending two poses (crossfade)

When the animator changes state it blends from the old pose to the new one over `fade` seconds:

```text
a = 1 - fadeLeft / fade              0 -> 1
translation, scale = mix(old, new, a)
rotation           = slerp(old, new, a)
```

The blend is per joint, in parent space, before `ComputePalette`.

## The animator

`Animator` picks one state of its graph each frame: the **first** state (in file order) whose
condition holds. Conditions: always; a flag (`dead`); a number above a value (`speed > 0.3`);
or a trigger (`attack`) for a one-shot state, which then plays to its end even after the trigger
is gone. A trigger waits half a second for its state. The playback rate can follow a number
(`rate = clamp(speed / per, 0.5, 2)`), so walk cycles keep up with the character. The graph file
format is in `engine/scene/include/AnimGraph.md`.
