# The Last Curtain: art requests (2026-11-22, refined 2026-11-23 00:10)

**For:** an AI art agent working in ComfyUI on the user's PC (SDXL base 1.0 only; birefnet,
inpainting and ExpressionEditor installed; no LoRA, ControlNet, IP-Adapter or upscaler; about 85 s a
picture).
**Source designs:** `campaign-content-2026-11-22.md` (recruits, zodiacs, stages) and
`cast-2-2026-11-22.md` (heroes, boon givers, ghosts, player, Le Thanh).
**Recipe:** the existing one in `plugins/last-curtain/apps/LastCurtain/assets/art/README.md`
(how Huy, Bảo, Khang, Mai and Lan were made). Follow it unless this list says otherwise.
**Total:** about **186** images (was 216). Make the **concept art** (section 00) first, then the
**first slice** (section 0).

**Refined (the user, 2026-11-22 23:43):**
- Keep the **dark oil pastel** style.
- Figures in **three-quarter view, turned to the right**.
- Eyes under a **khăn tang**, not a paper strip.
- Keep the existing sizes (body 768 px tall, portrait 512 × 627) and the existing cut-out workflow.
- **Forms are made in the engine** (tint, glow, strings); recruits are marked by a **gold knot**.
- The zodiac spirits keep their **animal shape**, made intimidating.
- The café painting is the **back wall** of the walkable 3D room.

**Refined (the user, 2026-11-23 00:00):**
- Heroes, freed spirits, the café wall and the good ending use the same medium with **calm, warm,
  dignified** mood words.
- **Allies glow light green.** The ten recruits are the official allies; Bảo and Mai are not
  allies (their art is on hold).
- The zodiac props are a guide, not a rule.
- Heroes' art needs no adviser review first.
- **Concept art comes first**, to preview every style.

---

## Style guide (applies to every image)

- **Look:** Vietnamese oil pastel on toned paper; silent, morbid, uncanny, melancholic. **No blood,
  wounds, skulls or dead bodies anywhere.**
- **Pose:** every figure stands in **three-quarter view turned to the right** (the game mirrors for
  left). Full length, feet visible, small in the frame with empty space around.
- **The dead** wear a white **khăn tang** (mourning cloth) tied over the eyes, with dark hollows
  soaked through where the eyes are. This is the rite of grief.
- **The living player** has no cloth, and eyes in soft shadow.
- **Le Thanh** has no cloth: she is the one who ties them.
- **Freed recruits (the allies), heroes and freed boon givers** have no cloth.
- **Paint only what the engine can't change.** Glow, translucency, colour tints and red strings are
  drawn by the engine (section 12). Never paint strings or coloured glow into a body sprite; strings
  in the picture stretch the cut-out box and shrink the figure. The **eyes** (cloth or no cloth) are
  the only thing that needs a second painting.
- **Real people:** heroes and recruits are *inspired by* real people. Do not copy any photograph or
  produce a likeness: stylized, invented faces.
- **Nothing recognizable:** no brand names, logos, real posters or real song sheets. Newspapers,
  tickets and contracts show no readable text.

**Technical specs (the existing ones):**

| Kind | Generate at | Ships as | Background |
|---|---|---|---|
| Board sprite (all figures, bosses too) | 832 × 1216 (long animals 1216 × 832) | `body_<face>.png`, 768 px tall | Plain dim grey paper, then birefnet cut-out |
| Portrait | Crop around the head from the body, scale to 1024, img2img 0.25 | `face_<face>.png`, 512 × 627 | Cut out |
| Expressions | ExpressionEditor (`blindfolded` values for the dead) | 7 faces: calm, angry, neardead, happy, scheming, afraid, talk | Cut out |
| Fight backdrop, cutscene still, key art | 1344 × 768 | jpg, as is | Opaque |
| Café back wall | 1536 × 640, outpainted at both sides to about 2480 × 640 | One texture over the 12.4 × 3.2 m wall | Opaque |
| Café decorations | 832 × 1216 or 1024 × 1024 | Cut-out prop sprites standing in the room | Cut out |
| Icon | 1024 × 1024 | 128 × 128, matching the existing 12 icons | Cut out |

Size on the board is set by the engine (tiles covered, height scale), never by the picture.

**Mood words for the warm subjects** (heroes, freed forms, the café wall, `cut/ending_good`, the
cards): replace "silent, morbid, uncanny, eerie stillness, unsettling, melancholic" with "calm,
warm, dignified, quiet, tender, soft lamplight". The medium words stay the same.

**Prompt, figures:** "full length figure of <subject>, standing, feet visible, small in the frame
with empty space around, seen in three-quarter view turned to the right, plain dim greyish paper
background" + **the style**.

**The style:** "Vietnamese oil pastel drawing on toned paper, thick waxy smudged strokes, visible
paper grain, muted desaturated earth colours, raw umber, ash grey, faded olive, dull deep red
accents, dim light, silent, morbid, uncanny, eerie stillness, unsettling, melancholic".

**Negative:** "blood, gore, wounds, corpse, skull, close-up, portrait crop, cropped legs, cropped
feet, facing the viewer, front view, photo, 3d render, photorealistic face, likeness of a real
person, anime, cartoon, chibi, bright saturated colours, text, letters, logo, watermark, frame,
border, multiple figures, scenery, jump-scare face, deformed".

**The khăn tang:** SDXL won't draw it from the prompt. Paint the band and the eye shadows, then repaint
that area with inpainting (README: "Blindfold").

**Same person in two paintings:** make the second by img2img or inpainting from the first, never
from text alone. For example, the freed form is the bound one with the cloth painted out.

**File names:** ComfyUI output `<group>/<id>_<variant>`. `pack_sprites.py` (figures) or
`pack_ghost.py` (animals) writes `art/sprites/<id>/`.

**String anchors:** for every figure that can be strung, write `art/sprites/<id>/anchors.json`: the
wrists, shoulders and (for Le Thanh) fingertips, as fractions of the packed body. The engine draws
the strings from there.

---

## 00. Concept art: the style preview (14, do these first)

**Why:** to see every look in the game once, before any set is made in bulk, and to approve or
change it cheaply. One of each kind of subject, made with the recipe above. Then a **style board** is
put together from them in the cloud (Python, no engine work). It places the cut-outs on a backdrop
and applies the planned engine forms as a mock-up: the tints, the light-green ally glow, the red
strings, the gold knot and the warm gold rim. About 1-1.5 hours of GPU time.

| # | Picture | Shows the style of |
|---|---|---|
| 1 | `player/male` | The living: solid, eyes in shadow, the fan |
| 2 | `lethanh/hostess` | The hostess: no cloth, a faint glow |
| 3 | `recruit/ti_bound` | The dead with the khăn tang: recruits, boon givers, ghosts |
| 4 | `recruit/ti_freed` | The same person freed (the cloth painted out), warm mood words |
| 5 | `recruit/ti_portrait` | A portrait, plus 2 of its 7 expressions |
| 6 | `zodiac/rat` | An animal-shaped zodiac spirit with its prop |
| 7 | `zodiac/rat_portrait` | A zodiac portrait |
| 8 | `hero/daughter_of_datdo` | A hero: warm, calm, dignified |
| 9 | `ghost/attacker` | An ordinary ghost: smaller, softer |
| 10 | `lethanh/ember_bride` | A boss transformation: embers and smoke |
| 11 | `backdrop/box_office` | A fight backdrop |
| 12 | `cafe/back_wall` | The café wall (the 1536 × 640 centre only; outpaint after approval) |
| 13 | `cut/rescue` | A cutscene still |
| 14 | `card/dawn_song` + one icon (`icon/ability_slingshot`) | Card art and icons |

**The style board** (one image) shows:
- Tí as a **specter** (red tint, strings, gold knot), as a **café ghost** (pale), as an **ally**
  (light green) and **freed** (gold rim).
- The **ghost** in its three natures (green, blue-grey, red with strings).
- The **Rat** with its strong red glow and thick strings.
- The **player** and **Le Thanh** on the box office.
- The portraits, the card and the icon at their sizes in the game.

**After approval:** pictures 1-9 and 11-12 count toward the first slice and the full list. Nothing is
made twice.

---

## 0. First slice (20, after the concept art)

**Player:**
- `player/male`, `player/female` and their portraits

**Le Thanh:**
- `lethanh/hostess`
- `lethanh/portrait_kind`

**Bé Tí:**
- `recruit/ti_bound`
- `recruit/ti_freed`
- `recruit/ti_portrait`

**The Rat:**
- `zodiac/rat`
- `zodiac/rat_portrait`

**Ordinary ghosts:**
- `ghost/attacker`, `ghost/defender`, `ghost/supporter`, `ghost/speedster`, `ghost/allrounder`

**Backdrops:**
- `backdrop/foyer_today`
- `backdrop/portrait_corridor`
- `backdrop/box_office`

**The café:**
- `cafe/back_wall`, outpainted

The concept art already covers 11 of these. Engine work for the slice (section 12): the form tints,
the strings and the gold knot, and the café wall texture.

---

## 1. The player (6)

The player is an **All-Rounder** and chooses **male or female** (the user). Both share the outfit:
- A living person in their twenties, from today.
- Jeans, sneakers, and a dark jacket over a T-shirt.
- A lanyard holding a theatre-tour ticket, and a phone with a cracked screen in a pocket.

No cloth, **eyes in soft shadow**, solid (the engine gives them a real shadow and no glow). Le
Thanh's gift: a **folded fan of ghostly red-gold light**.

| ID | Description |
|---|---|
| `player/male` | The man, standing ready, the folded fan in one hand. |
| `player/female` | The woman, same outfit and pose (img2img from the man for the same stance and scale). |
| `player/male_fan_open` | The man striking, the fan open into a blade of red-gold light (img2img from `player/male`). |
| `player/female_fan_open` | The woman, the same. |
| `player/male_portrait` | From the body: eyes in shadow, uneasy but determined. Plus the 7 expressions. |
| `player/female_portrait` | The same, the woman. |

## 1b. Bảo and Mai (on hold, not counted)

The ten recruits are the official allies (the user, 2026-11-23); Bảo and Mai are not allies. Their
new look waits until their role is decided (café characters, or dropped):
- **Bác sĩ Bảo:** about 50, a 1940s white doctor's coat, a stethoscope, round spectacles, a scalpel.
- **Bà Mai:** about 65, a brown áo bà ba, a grey bun, keys at her belt, a bowl of rice.

## 2. Le Thanh (8)

| ID | Description |
|---|---|
| `lethanh/hostess` | A woman of 25, small (1.56 m): a **red bridal áo dài**, a red hair ribbon, silver bracelets, hands folded, a gentle formal smile, eyes a little too still. No cloth. The engine gives her a faint glow. |
| `lethanh/dance` | The same, mid-dance, sleeves flowing (img2img from the hostess). |
| `lethanh/portrait_kind` | Portrait, soft smile (the expressions come from it). |
| `lethanh/portrait_uneasy` | Portrait, smile slipping, glancing aside. |
| `lethanh/portrait_revealed` | Portrait, calm and cold, red strings at her fingertips. Strings may be painted here, because a portrait is UI. |
| `lethanh/conductor` | Boss phase 1: the same dress, arms raised like a conductor, **no strings painted**. The engine runs one string from her fingertips to every specter in the room, and cuts them one by one. |
| `lethanh/ember_bride` | Boss phase 2: **the bride in embers**. The red dress is glowing embers and ash, the ribbon a trail of smoke, the bracelets glowing. Bowing gracefully. Sorrowful and terrible, not monstrous. Large on the board through the engine. |
| `lethanh/ember_portrait` | Portrait of the ember bride. |

## 3. The ten recruits (30: 3 each)

One painting serves every form, because the engine changes the rest:
- **Specter:** red tint, strings, the **gold knot**.
- **Café ghost** (before joining): a pale glow.
- **Ally** (joined, in fights and the café): a **light green** glow (the user).
- **The moment of freeing**, and the ending: a warm gold rim.

| Painting | What it is |
|---|---|
| `_bound` | The recruit with the **khăn tang over the eyes**. It is used for the specter and the café ghost. |
| `_freed` | The same picture with the cloth painted out (inpaint from `_bound`), with the warm mood words. It is used for the ally. |
| `_portrait` | From `_freed`, plus the 7 expressions. |

| ID | Who | Look |
|---|---|---|
| `recruit/ti_*` | **Bé Tí**, 12, Speedster | A barefoot boy, an oversized faded shirt, a conical hat hanging on his back, a folded paper message behind his ear, a ticket stub pinned to his shirt. |
| `recruit/nhan_*` | **Nhân**, 19, All-Rounder | A student in a white short-sleeved school shirt and dark trousers, a canvas satchel, ink-stained fingers, a torn strip of banner tied as a scarf. |
| `recruit/diep_*` | **Diệp**, 27, Supporter | A young woman doctor in a faded green field shirt, a red-cross armband, hair in a low bun, round glasses, a worn notebook held to her chest with loose pages escaping, a canvas medical bag. |
| `recruit/quang_*` | **Quang**, 20, Attacker | A young soldier with mud to the knees, a faded uniform, a leaf-camouflaged helmet, a slide rule in his breast pocket, a rifle held low. |
| `recruit/tai_*` | **Ba Tài**, 34, Speedster | A Saigon tailor in a crisp white shirt, a measuring tape round his neck, tailor's chalk behind his ear, sandals, shears. |
| `recruit/tulanh_*` | **Chị Tư Lành**, 37, Attacker | A Mekong woman in a black áo bà ba, a checked khăn rằn scarf (black-and-white chequered), a rifle across her back, a tiny silver baby bracelet on her wrist. |
| `recruit/phong_*` | **Thầy Phong**, 28, Defender | A thin teacher in glasses and worn khaki, a poetry notebook in his pocket, standing very straight. |
| `recruit/khoi_*` | **Khôi**, 19, All-Rounder | A thin young man with big ears, a coil of field-telephone wire over one shoulder, a pencil and a small diary. |
| `recruit/hien_*` | **Hiền**, 39, Supporter | A composer in a white shirt with rolled sleeves, a small violin, sheet music charred at the edges. |
| `recruit/bay_*` | **Thủ trưởng Bảy**, ~50, Defender | A grey-haired man with a cropped cut, a plain uniform with **no rank badges**, a pith helmet in his hand, a bamboo walking stick, a rice ball wrapped in a leaf at his belt. |

Three specter-only details from the first list now live in one painting:
- **Tí:** the ticket stub is always on his shirt.
- **Diệp:** the escaping pages are always there; the engine's red tint colours them in the specter.
- **Tài:** the tangled red thread became the engine's ordinary strings.

## 4. The twelve zodiac spirits (25)

**The animal shape stays (the user), made intimidating:**
- A theatrical, imposing pose: looming, rearing or coiled to strike.
- Gaunt and wrong in proportion, ghost-pale with smoke at the edges, eyes dull and hollow.
- Unsettling, never gory: **no skulls, no blood, no dripping red**.

**From the engine:** the red glow (stronger than the ghosts') and thick red strings.

**Each animal carries one thing from the person it was**, so the player can tie it to Le Thanh's
story and the history stills. The props below are a guide, not a rule (the user): keep whatever reads
well.

**Remake the twelve from `#UMBRA-299`.** The old set breaks these rules: the snake's skull head, the
rat's skull-like head, the monkey's red streaks, all in strict profile, none imposing. The old set
is retired; the later quick match uses the new one.

| ID | Zodiac | Look | From the person |
|---|---|---|---|
| `zodiac/rat` | Tý, the Rat | A hunched, gaunt rat as big as a man, long clawed hands | Brass spectacles and a green eyeshade; tickets and jewellery spilling from a pouch |
| `zodiac/rooster` | Dậu, the Rooster | A tall rooster with ragged feathers like burnt paper, long spurs | A straw boater; a fountain pen bound to one spur |
| `zodiac/cat` | Mão, the Cat | A tall thin cat sitting upright, neck too long | A pearl necklace and long gloves; a hand mirror at its paws |
| `zodiac/buffalo` | Sửu, the Buffalo | A huge emaciated water buffalo with chipped horns | A great hammer chained to its horns |
| `zodiac/dog` | Tuất, the Dog | A starved hound with one torn ear | A leash wound round its foreleg; a fedora at its feet |
| `zodiac/goat` | Mùi, the Goat | A goat standing on its hind legs like a stern woman, a thin beard | A dark áo dài over its shoulders; an abacus on a cord |
| `zodiac/pig` | Hợi, the Pig | A bloated boar | Dice in its trotter, coins scattered at its feet, a cigarette at its lip |
| `zodiac/monkey` | Thân, the Monkey | A crouching monkey with long thin arms, as if to leap | A red ringmaster's tailcoat and top hat; a whip and a tin cup |
| `zodiac/horse` | Ngọ, the Horse | A gaunt pale horse, mane of drifting smoke | A train ticket tucked in its bridle |
| `zodiac/snake` | Tỵ, the Snake | A great cobra rearing, hood spread, jade eyes | Coiled round a porcelain teacup |
| `zodiac/tiger` | Dần, the Tiger | A huge lean tiger, stripes like smeared ink | A groom's khăn đóng (round black turban) on its head; a red wedding candle in its jaws |
| `zodiac/dragon` | Thìn, the Dragon | A wingless serpentine Vietnamese dragon, gold-tinged scales | A cigar glowing like a coal in its mouth; a gold ring on one claw |
| `zodiac/dragon_smoke` | Dragon, phase 2 | A great dragon of smoke and embers coiled above a theatre stage, snapped red strings trailing (painted: this one is a scene) | — |

**Portraits:** `zodiac/<id>_portrait` × 12, a close-up of the animal's head with its prop.

**Recipe:** the zodiac prompts in `tools/art/lastcurtain_zodiac.json`, changed to three-quarter view
turned right, the imposing pose, the prop, and the negative above (skull, blood). Pack them with
`pack_ghost.py`.

## 5. The two heroes (6)

No cloth, never strings; the engine adds a gold rim. **Invented faces, not likenesses.** In the game
they are known **by titles only**: "the Young Electrician of Công Lý" and "the Girl of Đất Đỏ".
Use the warm mood words. No adviser review is needed before making them (the user).

| ID | Look |
|---|---|
| `hero/electrician` + `_portrait` | A young man of 24 in a white short-sleeved work shirt, dark trousers, an electrician's tool pouch, a calm steady gaze, seated at a café table. |
| `hero/daughter_of_datdo` + `_portrait` | A girl of 18 in a plain áo bà ba, hair in a simple bun with a small flower, barefoot, mid-song, bright and unafraid. |
| `card/homeland` | Power card art: a city at dawn seen from a bridge, a single figure standing, gold light. |
| `card/dawn_song` | Power card art: a sea island at first light, a flower on a stone, sound rippling as light. |

## 6. The three boon givers (12)

Each needs:
- `_bound`: the cloth over the eyes. It is used for the specter; the engine adds the red tint and
  the strings.
- `_freed`: the cloth painted out.
- `_portrait`
- `boon/<id>`: an icon

| ID | Look | Icon |
|---|---|---|
| `boon_giver/ferry_mother` | A woman of 60 in a dark áo bà ba and conical hat, a long oar, trousers rolled and wet. | An oar across a boat. |
| `boon_giver/girl_with_hoe` | A girl of 19 in a volunteer's green shirt and faded hat, a hoe on her shoulder, dust to her elbows. | A hoe and a road. |
| `boon_giver/wife_four_days` | A young woman of 22 in a simple wedding áo dài faded to pink, holding a small embroidered pillow. | An embroidered pillow. |

## 7. The ordinary ghosts (5)

**Softer than any zodiac:** rounder shapes, no masks or claws, the cloth over the eyes. One painting
per costume. **The nature is the engine's tint** (section 12): friendly, neutral or aggressive (with
strings). Their smaller height (about 80% of a recruit) is an engine setting.

| ID | Costume |
|---|---|
| `ghost/attacker` | A soldier in a faded uniform and pith helmet, a rifle held low. |
| `ghost/defender` | A farmer-militiawoman in a conical hat with a woven bamboo shield. |
| `ghost/supporter` | A nurse with a red-cross armband and a cloth bag. |
| `ghost/speedster` | A small child in an oversized shirt, barefoot, holding a paper message. |
| `ghost/allrounder` | A student in a white shirt with a satchel. |

## 8. Fight backdrops (16) and the café (1 + 12)

Wide and empty in the centre, 1940s Saigon Opera House interiors, ghostly, dim and beautiful;
1344 × 768.

| ID | Room |
|---|---|
| `backdrop/foyer_today` | The real foyer today at night: a tour group's phone torches, dust sheets on furniture, then old gaslight bleeding through. |
| `backdrop/portrait_corridor` | A corridor of old portraits with eyes that seem to follow, red carpet, gaslight. |
| `backdrop/box_office` | Brass ticket grilles, ceiling fans, drifts of old tickets, an iron safe. |
| `backdrop/balcony` | Gilded balcony boxes, velvet curtains, the stage far below. |
| `backdrop/dressing_rooms` | Bulb-ringed mirrors, powder dust, wigs on stands. |
| `backdrop/workshop` | Half-painted scenery flats, sawdust, pulleys, a long workbench. |
| `backdrop/stage_door` | Narrow back corridors, a porter's lodge, a rain-wet alley door. |
| `backdrop/costume_store` | Racks of costumes, masks on hooks, moth-light. |
| `backdrop/wine_cellar` | Barrels, a card table, swinging lamps, dice. |
| `backdrop/fly_tower` | Catwalks in three levels, ropes, sandbags, darkness below. |
| `backdrop/grand_staircase` | Marble stairs, a crystal chandelier, moonlight through tall windows. |
| `backdrop/orchestra_pit` | Music stands half under water, a sunken piano, flood light. |
| `backdrop/stalls` | Rows of red velvet seats filled with a faint ghostly audience, the great painted ceiling. |
| `backdrop/stage` | The main stage, a painted backdrop, footlights; a fire variant `backdrop/stage_fire`. |
| `backdrop/last_curtain` | The burnt dressing room opening onto the stage, ash, a red bridal veil, the whole house watching. |

**`cafe/back_wall`: the hub's back wall (the user).** The café is the walkable 3D room (12 × 8 m),
and this painting covers its back wall (12.4 × 3.2 m). It shows the far wall of a 1940s Saigon café
inside the theatre foyer:
- At the centre, a small stage with a red curtain (Le Thanh dances in front of it).
- Shelves with phin filters, a gramophone, ceiling fans, warm amber light.

**No tables, chairs or counter at floor level:** the room's 3D furniture stands in front. Generate
1536 × 640 and outpaint both sides to about 2480 × 640.

**`cafe/area_<n>` × 12:** cut-out prop sprites that stand in the room when an area opens [call]:
1. An old ticket rack
2. A balcony seat
3. A bulb mirror
4. A painted flat
5. A porter's lamp
6. A costume rack
7. A wine barrel table
8. A hanging sandbag lamp
9. A small chandelier
10. A music stand
11. A row of velvet seats
12. A stage footlight

## 9. Cutscene stills (19)

1344 × 768.

| ID | Scene |
|---|---|
| `cut/tour` | The night tour of the opera house today, phones up, laughing. |
| `cut/lights_die` | The lights die; the player alone; red threads in the dark. |
| `cut/rescue` | Le Thanh in red pulls the player away from a red-strung specter. |
| `cut/cafe_first` | The café appears, warm and impossible, Le Thanh bowing. |
| `cut/history_<1..12>` | One still per zodiac's fall, the piece of Le Thanh's story revealed after that zodiac (the user). They show the **people**, before they became animals. 1 her pay counted short at the window; 2 a cruel newspaper review; 3 a slashed costume; 4 a young girl backstage, frightened; 5 a caught runaway on a rainy night; 6 a girl of nine handed over at a theatre door; 7 a contract on a gambling table; 8 a ringmaster's whip; 9 an empty station platform; 10 a teacup and a voice lost; 11 a wedding; 12 a dressing-room door, smoke beneath it, 1944 searchlights outside. *(No violence shown, only before and after.)* |
| `cut/reveal` | Le Thanh on her café stage, red strings unspooling from her fingers to every ghost. |
| `cut/ending_good` | Dawn in the empty theatre, spirits leaving through the doors in gold light, Le Thanh in white looking back. |
| `cut/ending_bad` | A full house of ghosts watching Le Thanh sing, the player among them, a khăn tang tied over their eyes. |

## 10. UI icons (about 45)

128 × 128, matching the existing 12.

| ID | Icons |
|---|---|
| `icon/ability_<id>` × 30 | One per recruit ability (3 each, ids from `campaign-content` section 1); add Bảo's 3 and Bà Mai's 1 only if they return. For example a slingshot, a message, a bandage, a grenade, shears, a rifle, a notebook, telephone wire, a violin, a walking stick. |
| `icon/status_*` | silence (a mouth crossed by a paper strip), poison (a teacup with green steam), root (a leash), marked (a red string knot), linked (wire), last stand (a standing silhouette), bide / AP pip (a small candle, lit or unlit). |
| `icon/item_*` | Revive items ×2 (a joss stick; a small bell), a permanent boost (a silver bracelet). |
| `icon/fragment_*` × 8 | Memory fragment types: diary page, letter, photograph, ticket, flower, bracelet, song sheet, name tag. |
| `icon/lamp` | A small stage lamp, lit and flickering variants. |

## 11. Key art (1)

`key/title`, 1344 × 768:
- Le Thanh in red on an empty stage, seen from behind.
- The stalls full of faint ghosts with the khăn tang over their eyes, red strings rising from them
  into the dark.
- One living figure in modern clothes in the aisle, eyes in shadow, a folded fan in hand (gender left
  ambiguous, seen from behind).
- Title space at the top.

## 12. What the engine draws (not art; code tasks)

| Form | Tint / glow | Opacity | Strings | Mark |
|---|---|---|---|---|
| Player | none | 1 | — | — |
| Ally (a joined recruit) | **light green** glow (the user) | 0.95 | — | — |
| Recruit specter | red tint | 0.9 | yes | **gold knot** on its strings |
| Recruit / boon giver café ghost | pale white glow | 0.85 | — | — |
| The moment of freeing, freed boon givers, the ending, heroes | warm gold rim | 1 | — | — |
| Ghost, friendly | soft green glow [call: green means "on your side"; allies are lighter and carry a name plate] | 0.85 | — | — |
| Ghost, neutral | pale blue-grey glow | 0.85 | — | — |
| Ghost, aggressive | red glow | 0.85 | yes | — |
| Zodiac | strong red glow | 1 | thick | — |
| Le Thanh | faint warm glow | 0.95 | phase 1: to every specter, cut one by one | — |

- **Strings:** glowing lines from `anchors.json` up out of view, swaying a little.
- **Size on the board:** a height scale per unit (ordinary ghosts about 0.8) and the tiles a boss
  covers.
- **The café:** a texture on the back wall, a low platform in front of its painted stage, and prop
  sprites for the opened areas.

---

**Zodiacs never appear in the café** (the user): no café art for them.

**Count:**

| Group | Images |
|---|---|
| Player | 6 |
| Le Thanh | 8 |
| Recruits | 30 |
| Zodiacs | 25 |
| Heroes 4 + cards 2 | 6 |
| Boon givers | 12 |
| Ghosts | 5 |
| Backdrops | 16 |
| Café | 13 |
| Cutscenes | 19 |
| Icons | ≈45 |
| Key art | 1 |
| **Total** | **≈ 186** (Bảo and Mai on hold: +10 if they return) |

Each figure's portrait also gets 7 expressions through ExpressionEditor; they cost no new
generation. If the player can change class later, add 8.

**Quick match (a later phase, the user):** the human side picks from 12 (the player, Le Thanh and
the ten allies), and the monster side from the 12 zodiacs. It needs no new art beyond this list.

**GPU time:** ≈ 186 × 2-3 tries × 85 s, plus the khăn tang and cut-out passes, about 12-16 hours.
The concept art takes about 1-1.5 hours, and the rest of the first slice about 1 more.
