# Transforms and the hierarchy

## Conventions

- Right-handed coordinates, **+Y up**, **-Z forward**, units in **meters**. This matches glTF and the renderer's `lookAtRH` / `perspectiveRH_ZO`.
- Rotations are unit quaternions (`glm::quat`, constructor order `w, x, y, z`).
- Matrices are column-major (glm default) and multiply column vectors: `p_world = M * p_local`.

## Local matrix

A `Transform` stores position `t`, rotation `r` and scale `s`. Its matrix is

```text
M_local = T(t) * R(r) * S(s)
```

Applied to a point right-to-left: scale it, then rotate it, then move it. In code we skip building three matrices: take the rotation matrix, multiply column 0/1/2 by `s.x`/`s.y`/`s.z`, and write `t` into column 3.

## World matrix

```text
M_world(root)  = M_local(root)
M_world(child) = M_world(parent) * M_local(child)
```

So a child at local `(1,0,0)` under a parent at `(5,0,0)` rotated 90° about +Y ends up at `(5,0,-1)`. The rotation turns +X into -Z, then the parent's position is added.

## Dirty propagation

Each entity has a `dirty` flag, set by `SetLocalTransform`, `SetParent` and creation. `UpdateTransforms()` walks every root depth-first:

```text
update(node, parentMatrix, parentChanged):
    changed = node.dirty or parentChanged
    if changed: node.world = parentMatrix * node.local ; node.dirty = false
    for child in node.children: update(child, node.world, changed)
```

A moved parent therefore refreshes its whole subtree, while untouched subtrees keep their cached matrices. The walk still visits every node (O(n)) but skips the matrix math for clean ones. A dirty-list that avoids the visit is a later optimization, only if profiling asks for it.

## Hierarchy policies

| Operation | Policy |
|---|---|
| `SetParent(child, parent)` | Rejects a dead child or parent, self-parenting, and any cycle (the child may not be an ancestor of the parent). Keeps the **local** transform, so the child may jump in world space. |
| `SetParent(child, {})` | Makes the child a root. |
| `Destroy(parent)` | Destroys the whole subtree. Children are never silently re-rooted. |
| Handles from another world | Rejected like stale handles. |

Cycle check: walk from the proposed parent up through its parents. Reaching the child means a cycle. Cost is O(depth).
