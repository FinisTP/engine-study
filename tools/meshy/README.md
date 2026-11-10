# Meshy models for Umbra

Generated 3D models (https://www.meshy.ai) are prepared for the engine with two small scripts
(Python 3 with `numpy` and `Pillow`).

| Script | What it does |
|---|---|
| `slim_glb.py in.glb out.glb` | Keeps one mesh and the base-color texture only (JPEG, 1024 px; the engine ignores metal/roughness maps). Turns the model 180° (Meshy faces +Z, Umbra faces −Z). Centers it and scales its largest half-extent to 1, the same convention as the −1..1 sample cube, so a scene `scale` or an actor `size` is the half-size in meters. |
| `render_glb.py a.glb b.glb ...` | Writes `render.png`, a rough front view from −Z, to check facing and texture mapping without the engine. |

## Steps that produced `assets/models/{hero,elder,wolf,bandit}.glb` (2026-11-07)

1. Text to 3D preview (Meshy-6) with stylized low-poly prompts. Humanoids used `--pose-mode a-pose`
   so they can be rigged later. 20 credits each.
2. Refine (texture): 2k, no PBR maps, lighting removed. 10 credits each.
3. Remesh to ~8,000 triangles. 5 credits each. Meshy's output had 680k–960k triangles despite
   "low-poly" in the prompt.
4. `slim_glb.py`: 23–30 MB → about 0.7 MB per model.
5. `UmbraTool validate` on a scene using them (the engine's importer accepts them).

Total: 140 credits. Each model is one mesh with one texture, the format the engine's importer
supports.

| File | Prompt (short) | Meshy task ids (preview / refine / remesh) |
|---|---|---|
| hero.glb | young adventurer, leather armor over a blue tunic, short sword | 01a1b557-0138… / 01a1b558-248f… / 01a1b55b-7c83… |
| elder.glb | old village elder woman, purple robes, staff | 01a1b557-22f4… / 01a1b558-31f4… / 01a1b55b-81a4… |
| wolf.glb | grey wolf on all fours | 01a1b557-2d18… / 01a1b558-36e9… / 01a1b55b-86c8… |
| bandit.glb | hooded bandit, scarf, leather clothes, dagger | 01a1b557-3788… / 01a1b558-3c5b… / 01a1b55b-8c2a… |

## Rigging and clips (S8, 2026-11-10)

`hero.glb` and `bandit.glb` were replaced by rigged versions with six clips each; `wolf.glb` by a
free (CC0) rigged wolf; `elder.glb` stays unrigged.

1. Meshy auto-rigging of the remeshed humanoids (`rig`, height 1.7 m): a 24-joint skeleton plus
   walking and running clips. 5 credits each. The elder failed ("Pose estimation failed": her robe
   hides the legs) and keeps her procedural look.
2. Meshy animations on each rig (3 credits each): idle (action 0), attack (hero 219 "Right-hand
   Sword Slash", bandit 4 "Attack"), hit (178), death (189).
3. `tools/models/pack_animated.py` merged each rig and its clips into one GLB (one skinned mesh,
   clips named idle, walk, run, attack, hit, death; `--rig-reversed --turn 180`: the models were
   uploaded facing −Z and Meshy fitted the skeletons backward, `tools/models/README.md`
   "A rig fitted backward" (turn a model to +Z before rigging the next one);
   `--limit Head=35 --limit neck=25 --limit LeftHand=45 --limit RightHand=45`, because the library
   clips turned the head and wrists too far on these rigs, `tools/models/README.md` "Joint limits").
4. The wolf: Quaternius' "Wolf" from the Ultimate Animated Animal Pack (CC0, poly.pizza/m/P1gU3Qkr9r;
   file sha256 16ac9633…8f84), packed with `--turn 180` and its own clips renamed
   (Idle, Walk, Gallop → run, Attack, Idle_HitReact_Left → hit, Death).

Total: 34 credits (rig task ids 01a1c1c0-a7fc… hero, 01a1c1c0-b70e… bandit).

Where they are used:

- Scenes: the `player` entity draws `hero.glb` in the settlement, dungeon and arena, with the
  `animation` component `animations/humanoid.anim.json`. Elder Mara draws `elder.glb`.
- RPG content: actors carry `model`, `animation` (humanoid or wolf graph), `size` and an optional `color` tint. Enemies in the world and
  in the battle arena are drawn with these. The bandit leader is `bandit.glb`, a bit larger, with
  a purple tint.

License: models generated on a paid Meshy plan come with commercial-use rights; models from the
free plan are CC BY 4.0 (Meshy's pricing page).
