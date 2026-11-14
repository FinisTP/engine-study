# The pawns' models (first pass, 2026-11-15)

Made with Meshy (the user's Pro plan) to try the game feel: one model for all seven guests and
one for every Taken, each tinted with the guest's `color` (`guests.csv`). Both are light grey so a
tint shows. 128 credits in all.

| File | From | Clips |
|---|---|---|
| `guest.glb` | Hoang Gia Huy's concept (text-to-image), image-to-3d, remesh 8,000 tris, rig 1.7 m | idle (library "Idle 1"), walk, run, attack ("Kung Fu Punch"), hit ("Face Punch Reaction"), death ("Dead"), talk ("Talk with Hands Open") |
| `taken.glb` | a tall masked figure in a torn stage costume, same steps, rig 1.9 m | idle ("Idle", a fighting stance), walk, attack, power ("Charged Spell Cast"), hit, death |

Both were packed from Meshy's rigged GLB and clip GLBs (they face +Z, so `--turn 180`):

```
python tools/models/pack_animated.py rigged.glb guest.glb --turn 180 --texture 1024 \
  --clip idle=idle.glb --clip walk=walking.glb ... \
  --limit Head=35 --limit neck=25 --limit LeftHand=45 --limit RightHand=45
```

The Meshy task ids and the source files are kept outside the repository (`meshy_output/...lastcurtain-characters`).
