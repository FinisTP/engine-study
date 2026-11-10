# Model tools

`pack_animated.py` (Python 3 with `numpy` and `Pillow`) prepares a rigged character for Umbra (S8):

```text
python pack_animated.py rigged.glb out.glb [--turn 0] [--texture 1024]
       [--clip idle=idle.glb] [--clip walk=walk.glb#0] [--keep run=Gallop ...]
       [--limit Head=35 --limit RightHand=45 ...]
```

1. A new root node `UmbraRoot` turns the model by `--turn` degrees about Y (Umbra faces −Z;
   Meshy rigs already do, the Quaternius wolf needed 180), centers it and scales its largest
   half-extent to 1, measured on the skinned mesh at its rest pose.
2. All primitives become one skinned mesh. An untextured model gets a small palette texture made
   from its material colours.
3. Clips come from the file itself (`--keep new=old`) or from other GLBs with the same joint names
   (`--clip name=file.glb` or `file.glb#index`).
4. The base-colour texture becomes a JPEG of at most `--texture` pixels.

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
