# Model tools

`pack_animated.py` (Python 3 with `numpy` and `Pillow`) prepares a rigged character for Umbra (S8):

```text
python pack_animated.py rigged.glb out.glb [--turn 0] [--texture 1024]
       [--clip idle=idle.glb] [--clip walk=walk.glb#0] [--keep run=Gallop ...]
       [--limit Head=35 --limit RightHand=45 ...] [--rig-reversed]
```

1. A new root node `UmbraRoot` turns the model by `--turn` degrees about Y (Umbra faces −Z;
   Meshy rigs already do, the Quaternius wolf needed 180), centers it and scales its largest
   half-extent to 1, measured on the skinned mesh at its rest pose.
2. All primitives become one skinned mesh. An untextured model gets a small palette texture made
   from its material colours.
3. Clips come from the file itself (`--keep new=old`) or from other GLBs with the same joint names
   (`--clip name=file.glb` or `file.glb#index`).
4. The base-colour texture becomes a JPEG of at most `--texture` pixels.

## A rig fitted backward

Automatic rigging (Meshy, and most others) expects the model to **face +Z**, the glTF front. A
model handed over facing -Z still gets a skeleton, but one that believes the back is the front:
the left arm's joints sit in the right arm, the toe joints in the heels, and every clip then bends
knees, wrists and the neck the wrong way. Standing still it looks fine, which is why it is easy to
miss. This happened to the hero and the bandit (their files had been turned to Umbra's -Z first).

Check: in the rigged file the joint `LeftHand` must be on the model's own left. If the face looks
along -Z, the model's left is -X; if `LeftHand` is at +X the rig is backward.

`--rig-reversed` repairs it without rigging again:

1. every vertex is turned half around the vertical line through `Hips`: `(x, z) -> (2hx - x, 2hz - z)`
   (normals too), so the mesh now faces the way the skeleton thinks;
2. every vertex is handed to the joint of the other side (`Left...` <-> `Right...`), because that is
   the joint it now lies in; toe vertices follow the foot (the toe joints were fitted to the heels);
3. use `--turn 180` with it, so the finished model faces -Z again.

It relies on the body being nearly the same left and right, which holds for a humanoid. For new
characters, turn the model to +Z before rigging instead.

## Joint limits

Library clips are made for a standard skeleton. On an automatically made rig the same clip can
turn a head almost backward or fold a wrist over (the hero's idle turned the head 88 degrees from
the chest, the bandit's attack bent a wrist 155 degrees). `--limit Joint=degrees` fixes this in the
packed file, for every clip:

```
d      = rest^-1 * q                 the joint's turn away from its rest pose (angle, axis)
angle' = limit * tanh(angle / limit) small turns stay as they are, big ones flatten under the limit
q'     = rest * (angle' about the same axis)
```

`tanh` is used instead of a hard cut so the motion slows into the limit and never snaps. A limit
of 35 leaves a 10 degree nod at 9.7 and brings a 90 degree twist to 34.

The engine's side (skins, clips, skinning math) is in `engine/core/include/Skeleton.md`.
Commands used for the reference characters: `tools/meshy/README.md`.
