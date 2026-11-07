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

Where they are used:

- Scenes: the `player` entity draws `hero.glb` in the settlement, dungeon and arena. Elder Mara
  draws `elder.glb`.
- RPG content: actors carry `model`, `size` and an optional `color` tint. Enemies in the world and
  in the battle arena are drawn with these. The bandit leader is `bandit.glb`, a bit larger, with
  a purple tint.

License: models generated on a paid Meshy plan come with commercial-use rights; models from the
free plan are CC BY 4.0 (Meshy's pricing page).
