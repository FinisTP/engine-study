# The Last Curtain: campaign content — recruits, zodiac spirits, stages (2026-11-22, updated 2026-11-23)

**What this is.** The content plan for the campaign in `2026-11-21-campaign-design.md`: the 10
recruitable spirits, the 12 zodiac spirits in fight order, and every stage. It shapes character
design, abilities, dialogue and AI text, bosses and difficulty, and level design. Everything here is
**[call]** unless marked (the user), and is a first pass for the user to edit.

**Settled by the user on 2026-11-22.**
- The setting is the real Saigon Opera House (option a), so Le Thanh died before 1955.
- The Commander is an unnamed composite.
- The model gets a Vietnamese testing phase.

**Settled by the user on 2026-11-22 and 2026-11-23.**
- **The ten recruits are the official allies.** Bảo and Mai are café characters who join you
  temporarily in the third tutorial.
- The zodiac spirits are **animal-shaped** ghosts.
- The dead wear a **khăn tang** over the eyes.
- **Allies glow blue.** A recruit (or boon giver) specter carries a **gold knot**.
- If the **living player falls, it is game over**; a retry starts the stage over.
- Le Thanh is the **hostess**.
- Heroes are known by **titles only**.

---

## 0. Le Thanh's timeline (from `cast.md`, option a)

- **Le Thanh, 25, the house's famous singer.** She married on the night of the **1944 air raid**, in
  a red bridal dress. That night she was **burnt alive in her dressing room**, and her death was
  hidden in the raid's damage. The raid is real; the singer, the fire and everyone in her life are
  invented.
- **Her twelve tormentors** are people from her life before 1944. When they died, she bound them to
  the house as her **Saigon circus band**: ghosts in the **shape of their zodiac animals** (the user),
  each still carrying one thing from the person it was, on red marionette strings, playing for her
  every night.
- **From 1955 to 1975 the house fell silent as a theatre.** Le Thanh's grief turned into the rite of
  grief, and the war's dead, lost and unclaimed, drifted into the house that still had a singer.
  This is the only use of the building's real history, and it stays offstage: no parliament, no
  politics, just "the stage went dark for twenty years".
- **The visual language of the dead:** each zodiac controls its spirits with **red strings**. Freeing
  a recruit means **cutting the strings**, never killing the person.

---

## 1. The ten recruits (the allies)

Two per class. **Stats** start from the class stat line in `characters.csv`, as HP/Atk/Def/Spd/Move,
with the tweak listed for each recruit:

| Class | HP | Atk | Def | Spd | Move |
|---|---|---|---|---|---|
| Attacker | 20 | 6 | 2 | 4 | 6 |
| Defender | 30 | 3 | 5 | 2 | 6 |
| Supporter | 20 | 2 | 3 | 3 | 6 |
| Speedster | 18 | 4 | 2 | 6 | 10 |
| All-Rounder | 24 | 4 | 3 | 4 | 8 |

**Abilities** use the existing classes (strike, shield, heal, hit). The few new effect flags needed
are listed in section 4. Costs and powers are first numbers for the soak to tune. Names are adjusted;
inspirations are in `candidates-2026-11-22.md`.

**Shared look.** The dead appear as they were in their last hour. Their eyes are covered by a white
**khăn tang**, the mourning cloth (the rite of grief, the user).
- **As a red specter**, a recruit carries a **gold knot** on its strings (the user), so the player
  knows to free it, not destroy it.
- **In the café**, before joining, it glows pale.
- **Joined**, the cloth is gone and it glows **blue**: an ally (the user).

The engine draws the tints, the strings and the knot; the art is one painting with the cloth and one
without (`art-requests` section 3).

**Voice sheet fields** (for the dialogue writer and the model): dialect, register, verbal habits,
topics they return to, words they never say, and memory fragments (restored one per café visit; the
last one completes the recruit).

### 1. Bé Tí — the Little Messenger (Speedster) · freed in Stage 1

- **Inspiration:** a composite of child liaisons in Quảng Ngãi.
- **Look:** barefoot boy of 12, oversized faded shirt, conical hat hanging on his back, a folded
  paper message tucked behind his ear, a ticket stub pinned to his shirt.
- **Story:** he ran messages between hamlets and counted patrols from the dunes. He promised his
  mother he'd be home for Tết. He fell on the white sand with one message still behind his ear.
- **Stats:** Speedster line, HP −2, Move +2 (the fastest unit in the game).
- **Abilities:**
  - *Ná cao su* (slingshot): battle strike, 0 HP, power 3.
  - *Đưa tin* (carry the word): board, 2 HP, range 6 walk; gives an ally 1 of Tí's stored action points.
  - *Chân sáo* (skipping feet): passive, +3 move on a turn after he bides.
- **Speciality:** the action-point engine. He bides, then passes the extra action to whoever needs it.
- **Voice:** Quảng Ngãi dialect ("tui", "răng", "rứa"), short sentences, calls the player "anh/chị";
  curious and cheeky. Topics: his mother's cooking, Tết, the sea. Never: boasting about killing,
  swearing.
- **Memory fragments:** 1) his mother's name; 2) the Tết promise; 3) the message itself ("they come
  at dawn"); 4) the white sand where he fell.

### 2. Nhân — the Student of January (All-Rounder) · freed in Stage 2

- **Inspiration:** Trần Văn Ơn, the 9 Jan 1950 student march.
- **Look:** 19, white short-sleeved school shirt, dark trousers, canvas satchel, ink-stained fingers,
  a torn strip of banner tied as a scarf.
- **Story:** a top pupil from a Mekong coconut village, studying in Saigon. On the day of the march he
  stepped in front of the younger pupils.
- **Stats:** All-Rounder line, Def +1.
- **Abilities:**
  - *Che chở* (shield them): battle shield, 1 HP, power 4; covers an ally in his group instead of
    himself.
  - *Hô hào* (rally): board, 3 HP, range 3; allies there gain +1 Attack until their next turn.
  - *Đầu đoàn* (front of the line): passive; in a group battle, his partner's strikes gain +1 power.
- **Speciality:** protector and pair-fighter. He makes two-on-a-tile groups stronger.
- **Voice:** Southern Mekong youth, polite ("dạ", "thưa"), earnest, repeats what his teachers said.
  Topics: exams, his mother's coconut garden, fairness. Never: cynicism, insults.
- **Memory fragments:** 1) his school and teacher; 2) his mother's coconut grove; 3) the boy he pushed
  aside; 4) the exam paper he never handed in.

### 3. Diệp — the Doctor (Supporter) · freed in Stage 3

- **Inspiration:** Đặng Thùy Trâm.
- **Look:** 27, faded green field shirt, red-cross armband, hair in a low bun, round glasses, a worn
  notebook held to her chest with loose pages escaping, a canvas medical bag. As a specter, the red
  tint colours the pages.
- **Story:** a Hanoi-trained surgeon who ran a forest clinic in the Centre under bombing and wrote in
  her diary every night. The diary was lost in the war and, decades later, found its way home.
- **Stats:** Supporter line, HP +2.
- **Abilities:**
  - *Mổ dã chiến* (field surgery): board, 6 HP, adjacent; **revives a fallen ally at 30% HP, once per
    stage**. It's the game's first revive.
  - *Băng bó* (bandage): battle heal, 2 HP, power 5; can heal a partner in her group.
  - *Bình tĩnh* (steady hands): passive; allies next to her at the start of their turn heal 1.
- **Speciality:** the healer and reviver. She makes the "falls stay down" rule survivable.
- **Voice:** Hanoi Northern, educated, gentle; calls the wounded "em"; starts café talks like diary
  entries ("Ngày… tháng…"). Topics: her mother, her student nurses, peace. Never: hatred, gore.
- **Memory fragments** (diary pages): 1) a letter from her mother; 2) the clinic in the forest; 3)
  the student nurses she taught; 4) the last page, two days before.

### 4. Quang — the Citadel Soldier (Attacker) · freed in Stage 4

- **Inspiration:** a composite of the Quảng Trị citadel student-soldiers, 1972.
- **Look:** 20, mud to the knees, faded uniform, a leaf-camouflaged helmet, a slide rule in his
  breast pocket.
- **Story:** an engineering student from Thái Bình who left his drawing board for the citadel. He
  crossed the Thạch Hãn river at night. In the 81 days he forgot the date but never his mother's handwriting.
- **Stats:** Attacker line, Def +1, Move −1.
- **Abilities:**
  - *Bám trụ* (hold the ground): passive, +1 Defence each turn he doesn't move (max +3).
  - *Lựu đạn* (grenade): board hit, 3 HP, power 4, range 5 straight; hits the target tile and the
    tiles next to it.
  - *Xung phong* (charge): battle strike, 3 HP, power 7.
- **Speciality:** the anchor that holds a line, and the first area attack.
- **Voice:** Northern rural student; jokes to hide fear; explains things with numbers and bridges.
  Topics: his mother, his unbuilt bridge, the river. Never: glory talk.
- **Memory fragments:** 1) his mother's letter; 2) the river crossing; 3) his bridge drawing; 4) a friend
  whose name he can't remember, and then does.

### 5. Ba Tài — the Tailor (Speedster) · freed in Stage 5

- **Inspiration:** Trần Phú Cương, Saigon commando, Tết 1968.
- **Look:** 34, crisp white shirt, a measuring tape round his neck, tailor's chalk behind one ear,
  sandals, shears.
- **Story:** his small tailor shop in the city hid a commando cell. He went out on the first night of
  Tết and didn't come back. His wife couldn't claim him without giving the others away.
- **Stats:** Speedster line, Atk +1.
- **Abilities:**
  - *Đường may* (the seam): board, 2 HP; **steps through one wall tile** next to him.
  - *Kéo cắt* (shears): battle strike, 2 HP, power 6.
  - *Khách quen* (regular customer): passive; the first attack on him in each stage misses.
- **Speciality:** mobility and flanking; reaches zodiacs and fallen allies others can't.
- **Voice:** Saigon Southern, warm and joking, "anh Hai, chị Hai", talks about cloth and customers.
  Topics: his wife's hands at the sewing machine, the shop, Tết food. Never: bitterness toward his
  wife.
- **Memory fragments:** 1) the shop's sign; 2) his wife sewing; 3) the last New Year's eve; 4) the
  street where she walked past him without stopping.

### 6. Chị Tư Lành — the Mother With a Rifle (Attacker) · freed in Stage 6

- **Inspiration:** Út Tịch.
- **Look:** 37, black bà ba, a checked khăn rằn scarf, a rifle across her back, a baby's tiny silver
  bracelet on her wrist.
- **Story:** a servant girl turned guerrilla in the Mekong, mother of six. She went back to the fight
  two weeks after her youngest was born, and never held her again.
- **Stats:** Attacker line, HP +4, Spd −1.
- **Abilities:**
  - *Phục kích* (ambush): battle strike, 3 HP, power 8, **+3 power if she didn't move this turn**.
  - *Bắn tỉa* (sniper): board hit, 3 HP, power 5, range 6 straight.
  - *Lòng mẹ* (a mother's heart): passive; +2 Attack for the rest of the stage when an ally falls.
- **Speciality:** the heaviest single hitter. She pairs with biding: sit, then strike hard.
- **Voice:** Mekong Southern, plain, funny, mothering ("mấy đứa"); counts her children by name.
  Topics: her six children, the river, rice. Never: self-pity.
- **Memory fragments:** 1–6) one child each (six short fragments, two per visit, so three visits);
  last: the bracelet.

### 7. Thầy Phong — the Poet-Teacher (Defender) · freed in Stage 7

- **Inspiration:** Lê Anh Xuân.
- **Look:** 28, thin, glasses, worn khaki, a poetry notebook in his pocket; stands very straight.
- **Story:** a history lecturer from Bến Tre who turned down study abroad to go home to the South. He
  wrote his last poem about a fighter who stood to the end, two months before he fell himself.
- **Stats:** Defender line, Atk +1, Def −1.
- **Abilities:**
  - *Dáng đứng* (the standing pose): passive, **once per stage, when he falls, he stays standing at 1
    HP until his next turn**.
  - *Vần thơ* (verse): battle shield, 2 HP, power 6, given to his **whole group**.
  - *Bình minh* (dawn): board, 3 HP, range 3; allies there gain +1 Defence until their next turn.
- **Speciality:** the last wall; buys one more turn for the side.
- **Voice:** soft Southern intellectual, speaks in images, connects everything to history. Topics:
  his father's lectures, the girl he wrote birthday letters to. Never: slogans or quoting real poems.
- **Memory fragments:** 1) his father's classroom; 2) the scholarship he refused; 3) the birthday
  letters; 4) the poem's last line (written fresh, never the real poem).

### 8. Khôi — the Signalman (All-Rounder) · freed in Stage 8

- **Inspiration:** Nguyễn Văn Thạc.
- **Look:** 19, thin, big ears, a coil of field-telephone wire over one shoulder, a pencil and a small
  diary.
- **Story:** a literature prize winner who left university for the front, writing letters to his brother
  from the road south. He never turned twenty.
- **Stats:** All-Rounder line, Spd +1.
- **Abilities:**
  - *Nối dây* (patch the line): board, 2 HP, range 5; links Khôi to an ally. **If that ally is attacked,
    Khôi joins the battle** as if he were on their tile.
  - *Lưỡi lê* (bayonet): battle strike, 2 HP, power 6.
  - *Thư về nhà* (letter home): passive; his board abilities cost 1 less on any turn he bided.
- **Speciality:** remote group-battle support; turns a 1v2 into a 2v2.
- **Voice:** Hanoi Northern, bookish, idealistic, a little shy; writes as if to his brother ("anh ạ").
  Topics: books, his brother, being twenty. Never: quoting the real diary.
- **Memory fragments:** 1) his brother's letters; 2) the literature prize; 3) the junction where he
  wrote his last page; 4) the number twenty.

### 9. Hiền — the Composer (Supporter) · freed in Stage 9

- **Inspiration:** Hoàng Việt.
- **Look:** 39, white shirt with rolled sleeves, a small violin, sheet music charred at the edges.
- **Story:** born in Chợ Lớn; after the country was divided he wrote a love song to a woman left in
  the South and broadcast it hoping she'd hear. He died by a canal in the delta with a symphony
  unfinished.
- **Stats:** Supporter line, Spd +1.
- **Abilities:**
  - *Tình ca* (love song): board, 4 HP, range 3; heals every ally there by 3. It's the first group heal.
  - *Hợp xướng* (chorus): battle shield, 1 HP, power 3, to his whole group.
  - *Bản dang dở* (the unfinished piece): passive; every third ability he uses in a stage costs 0 HP.
- **Speciality:** group sustain; pairs with Diệp.
- **Voice:** Saigon-Chợ Lớn Southern, musical, wistful, hums. Topics: the woman across the river, his
  symphony, the radio. Never: the real song's lyrics or melody.
- **Memory fragments:** 1) her name (invented); 2) the radio broadcast; 3) the song that wasn't allowed;
  4) the last bars of the symphony.

### 10. Thủ trưởng Bảy — the Commander (Defender) · freed in Stage 10

- **Inspiration:** an unnamed composite regimental commander of peasant roots (the user).
- **Look:** about 50, grey cropped hair, a plain uniform with no rank shown, a pith helmet in his
  hand, a bamboo walking stick, a rice ball wrapped in a leaf.
- **Story:** a farmer's son who became the man young soldiers called "anh Bảy". He buried more of them
  than he can count, and kept their names in a cloth bag. He never saw the end.
- **Stats:** Defender line, Atk +1, Spd +1.
- **Abilities:**
  - *Mệnh lệnh* (the order): board, 4 HP, range 4; **an ally takes an extra move and action right
    now**; once per round.
  - *Giữ chốt* (hold the post): battle shield, 3 HP, power 8.
  - *Gần dân* (close to the people): passive; allies within 2 tiles gain +1 Defence.
- **Speciality:** the tactician; gives turns to others and steadies the line.
- **Voice:** Central plain speech, proverbs, few words, calls the young "tụi nhỏ". Topics: rice, the
  names in his bag, the young ones. Never: boasting, rank.
- **Memory fragments:** 1) his father's field; 2) the cloth bag of names; 3) the youngest soldier; 4)
  his own name, last.

**Class balance:**
- Attacker: Quang, Tư Lành
- Defender: Phong, Bảy
- Supporter: Diệp, Hiền
- Speedster: Tí, Tài
- All-Rounder: Nhân, Khôi

**AI text design notes:**
- Every recruit's lines are authored facts plus a voice sheet. The model words them in Vietnamese,
  in the right dialect, and the line guard checks each line against the "never says" list.
- Fight memory (who freed them, how) enters their first café line: "Cái người cắt dây cho tui là
  anh…" ("the one who cut my strings was you…").
- Battle barks: 6–10 authored per recruit, worded live from match events.

---

## 2. The twelve zodiac spirits, weakest to strongest

The Vietnamese zodiac (with the **Cat** in place of the Rabbit). Each was a person who harmed Le Thanh
before 1944. In death each is a performer in her circus band, **in the shape of its zodiac animal**
(the user):
- Imposing and gaunt, ghost-pale; never gory.
- It carries **one thing from the person it was**. The props are a guide, not a rule (the user).
- Red strings run up into the dark.

Each uses one class line, scaled up, plus a boss kit. **Boss HP** is a first number to tune: it
starts near 40 and climbs to about 150.

| # | Zodiac | Who they were to Le Thanh | Class | Room | Boss HP |
|---|---|---|---|---|---|
| 1 | **Tý — the Rat** | The box-office clerk who skimmed her pay and stole her mother's jade bracelet | Speedster | Box office | 40 |
| 2 | **Dậu — the Rooster** | The critic who wrote that her voice was a fraud | Supporter | Balcony boxes | 50 |
| 3 | **Mão — the Cat** | The rival actress who was jealous of her and ruined her costume before a premiere | Attacker | Dressing rooms | 60 |
| 4 | **Sửu — the Buffalo** | The stage foreman who beat her when she was a girl in the troupe | Defender | Scene workshop | 75 |
| 5 | **Tuất — the Dog** | The patron's bodyguard who dragged her back the night she tried to run away | Speedster | Stage door and back corridors | 80 |
| 6 | **Mùi — the Goat** | Her stepmother, who sold her to the troupe at nine | Supporter | Costume store | 90 |
| 7 | **Hợi — the Pig** | Her older brother, who gambled away her contract and her earnings | All-Rounder | Wine cellar (a gambling den) | 100 |
| 8 | **Thân — the Monkey** | The troupe's ringmaster, who worked her like a circus animal | Speedster | Fly tower | 110 |
| 9 | **Ngọ — the Horse** | The lover who promised to run away with her, then left her alone at the station | Attacker | Grand staircase | 120 |
| 10 | **Tỵ — the Snake** | The patron's wife, who put poison in her tea and stole her voice for a season | Supporter | Orchestra pit | 125 |
| 11 | **Dần — the Tiger** | Her husband, married to her for her fame, cruel from the first day, who led her to the dressing room that night | Defender | The stalls | 140 |
| 12 | **Thìn — the Dragon** | **The patron**, who owned her contract, set the fire and hid her death in the air raid | All-Rounder, two phases | The main stage | 150 + phase 2 |

The order runs from small hurts to the largest. Each stage also reveals a piece of Le Thanh's story,
so the player pieces it together in order: money, words, jealousy, violence, being hunted, being sold,
betrayal by family, exploitation, abandonment, poison, marriage, murder. By Stage 12, the player knows
exactly why she wants an audience forever, and why it still isn't right.

### The kits

1. **The Rat.**
   - *Look:* a hunched, gaunt rat as big as a man, long clawed hands; brass spectacles and a green
     eyeshade, tickets and jewellery spilling from a pouch.
   - *Kit:* **Móc túi** (pickpocket): when next to a unit, he steals an item from its bag. **Chạy làng**
     (bolt): after a battle he darts 3 tiles away.
   - *Gimmick:* ticket windows open and close each round, letting out rat specters. Stolen items sit
     on the windows until he is hit.
   - *Teaches:* items, the first revive (Diệp's isn't available yet, so a bandage-revive item is placed
     on the map).
2. **The Rooster.**
   - *Look:* a tall rooster with ragged feathers like burnt paper and long spurs; a straw boater, a
     fountain pen bound to one spur.
   - *Kit:* **Gáy sáng** (dawn crow): every 3 rounds all specters gain +1 Attack (stacking) unless the
     Rooster was hit that round. **Bút sắc** (sharp pen): a board hit, range 7 straight, from the boxes.
   - *Gimmick:* height. Fight across the balcony boxes, where rails block walking but not straight
     reach.
   - *Teaches:* reach (walk vs straight), pressure to act fast.
3. **The Cat.**
   - *Look:* a tall thin cat sitting upright, its neck too long; a pearl necklace, long gloves, a hand
     mirror at its paws.
   - *Kit:* **Gương soi** (the mirror): makes 2 copies of herself; copies have 1 HP and vanish when hit.
     **Cào** (claw): a heavy strike.
   - *Gimmick:* bulb-ringed mirrors. Units seen in a mirror can be struck through it once per stage.
   - *Teaches:* choosing targets, group battles (two on a tile).
4. **The Buffalo.**
   - *Look:* a huge emaciated water buffalo with chipped horns; a great hammer chained to its horns.
   - *Kit:* **Húc** (charge): rams in a straight line, pushing units 2 tiles. **Da trâu** (hide): a big
     shield.
   - *Gimmick:* scenery flats fall on marked tiles (warned a round ahead). **Offerings:** carry his own
     dropped tools to the workbench to break his hide (the offerings gimmick).
   - *Teaches:* **biding** against a tank, offerings, warned hazards.
   - Boon carrier here: **the Girl With the Hoe**.
5. **The Dog.**
   - *Look:* a starved hound with one torn ear; a leash wound round its foreleg, a fedora at its feet.
   - *Kit:* **Đánh hơi** (the scent): hunts the unit that moved most last round. **Cắn giữ** (hold
     fast): his target can't move next turn.
   - *Gimmick:* fog in narrow corridors (only tiles near your units show); the stage door locks at round 8.
   - *Teaches:* falls that stay down; moving with care.
6. **The Goat.**
   - *Look:* a goat standing on its hind legs like a stern woman, a thin beard; a dark áo dài over its
     shoulders, an abacus on a cord.
   - *Kit:* **Đổi áo** (the costume change): each round, her specters swap classes from the racks.
     **Vỗ về** (soothe): heals all specters.
   - *Gimmick:* costume racks that block lines of sight; a lost child's pair of shoes to carry to the
     mirror breaks her heal (offerings).
   - *Teaches:* reading changing enemies; burst damage to beat a healer.
   - Hero after this stage: **the Girl of Đất Đỏ** visits the café.
7. **The Pig.**
   - *Look:* a bloated boar; dice in its trotter, coins scattered at its feet, a cigarette at its lip.
   - *Kit:* **Lắc bầu cua** (shake the dice): each round a die sets a rule for everyone, such as
     double damage, no healing, swap places, or a free action. **Ăn gian** (cheat): rerolls once if the
     roll hurts him.
   - *Gimmick:* the gambling tables are cover.
   - *Teaches:* adapting to rules; using a power card at the right moment.
8. **The Monkey.**
   - *Look:* a crouching monkey with long thin arms, as if to leap; a red ringmaster's tailcoat and top
     hat, a whip and a tin cup.
   - *Kit:* **Nhào lộn** (somersault): jumps between catwalk levels. **Roi** (the whip): a strike that
     pulls its target 2 tiles.
   - *Gimmick:* the fly tower has three catwalk levels joined by ropes, and sandbags drop on marked tiles.
     **Perform** returns as a gimmick: hold the small stage below for a full turn to drop the
     counterweight and bring him down.
   - *Teaches:* vertical maps, holding a point.
9. **The Horse.**
   - *Look:* a gaunt pale horse with a mane of drifting smoke; a train ticket tucked in its bridle.
   - *Kit:* **Phi nước đại** (gallop): charges straight down the staircase, striking everyone in the
     line. **Chuyến tàu cuối** (the last train): he grows stronger every round; at round 15 he is at full
     power.
   - *Gimmick:* the great staircase and chandelier. Bring the chandelier down on him (a timed objective).
   - *Teaches:* racing the clock.
   - Boon carrier here: **the Wife of Four Days**.
10. **The Snake.**
    - *Look:* a great cobra rearing, hood spread, jade eyes; coiled round a porcelain teacup.
    - *Kit:* **Trà độc** (the poisoned tea): silence, so the target can't use abilities for 2 turns.
      **Nọc** (venom): poison, 2 HP a turn.
    - *Gimmick:* the orchestra pit floods. Water rises one row every 3 rounds and costs 2 move to wade.
    - *Teaches:* status effects, cleansing (Diệp, Hiền).
    - Boon carrier here: **the Ferry Mother**.
11. **The Tiger.**
    - *Look:* a huge lean tiger, stripes like smeared ink; a groom's khăn đóng on its head, a red
      wedding candle in its jaws.
    - *Kit:* **Gầm** (roar): every enemy within 3 loses its next move. **Vồ** (pounce): leaps 4 tiles
      to a strike.
    - *Gimmick:* the stalls' rows of velvet seats block walking. He is warded by **three marriage papers**:
      carry each to a candle and burn it to break a ward (offerings).
    - *Teaches:* everything together.
    - Hero after this stage: **the Young Electrician of Công Lý** visits the café.
12. **The Dragon (the patron).**
    - *Look:* a wingless serpentine Vietnamese dragon with gold-tinged scales; a cigar glowing like a
      coal in its mouth, a gold ring on one claw. In phase 2 the strings snap and it becomes a dragon of
      smoke and embers above the stage.
    - *Kit:* **Lửa** (fire): fire spreads one tile a round from where it lands. **Diễn** (the
      performance): he Performs on the stage to heal; hit him before his next turn to break it (the
      Perform rule, reversed). Phase 2: **Hỏa hoạn 1944** (the 1944 fire), where the whole stage burns
      in a ring that closes.
    - *Teaches:* nothing new; the exam.

**After Stage 12: Le Thanh, the final boss** (see the stages below).

---

## 3. The stages

The hub is **Le Thanh's café**: the walkable 3D room, with a painted back wall showing a 1940s
Saigon café inside the theatre's foyer (the user). The wall has phin filters on the shelves, a
gramophone, ceiling fans, and a small stage with a red curtain where Le Thanh dances in her red
dress. Each zodiac win opens a theatre area, and a prop from it joins the room: a ticket rack from the
box office, a balcony seat, the costume rack. Restored spirits sit there as regulars.

**Party and level targets [call].**
- **Who fights:** the living player is always in the party, and **if the player falls, it is game
  over**: retry (the stage starts over from its beginning), go back to the café, or quit (the user).
- **Before recruits join:** Bảo and Bà Mai, café characters, fight beside you temporarily in T3 (the
  user). Stage 1's party is open; my proposal is that they stay one more stage.
- **Level cap:** it must rise from 5 to **15** for the campaign, with new XP steps. The recommended
  levels are what each stage is tuned for.

| # | Stage (Vietnamese / English) | Room and look | Enemy | Party | Lvl | New | Freed / met | Le Thanh unlocks |
|---|---|---|---|---|---|---|---|---|
| T1 | **Khách lạ** / The Stranger | The real foyer today: a night tour, phone light, dust sheets. The lights die and the old theatre appears behind them. | Rat specters | 1 (you) | 1 | Moving, attacking | Le Thanh's rescue | — |
| T2 | **Quán cà phê** / The Café | The hub, first visit | — | — | — | Café talk | Bảo and Bà Mai | Café |
| T3 | **Hành lang chân dung** / Portrait Corridor | A corridor of portraits whose eyes follow you, red carpet, gaslight | Specters, a mini-boss | 3 (you + Bảo and Mai, temporarily) | 2 | Biding, falls, reviving, game over if you fall | — | — |
| 1 | **Phòng vé** / The Box Office | Brass grilles, ceiling fans, drifts of old tickets, a safe | Rat | 3 | 3 | Items, stealing, the gold knot | Bé Tí | Memory restoring |
| 2 | **Lô ban công** / The Balcony Boxes | Gilded boxes, velvet curtains, the stage far below | Rooster | 3 | 4 | Reach, height | Nhân | — |
| 3 | **Phòng hóa trang** / The Dressing Rooms | Bulb-ringed mirrors, powder dust, wigs on stands | Cat | 3 | 5 | Group battles, decoys | Diệp | **Boons usable** |
| 4 | **Xưởng cảnh trí** / The Scene Workshop | Half-painted flats, sawdust, pulleys, a workbench | Buffalo | 4 | 6 | Offerings, hazards | Quang; Girl With the Hoe | — |
| 5 | **Cửa hậu** / The Stage Door | Narrow back corridors, a porter's lodge, a rain-wet alley door | Dog | 4 | 7 | Fog, locks | Ba Tài | **Power cards usable** |
| 6 | **Kho phục trang** / The Costume Store | Racks of costumes, masks on hooks, moth-light | Goat | 4 | 8 | Changing enemies | Tư Lành | Hero: the Girl of Đất Đỏ (after) |
| 7 | **Hầm rượu** / The Wine Cellar | Barrels, a card table, lamps swinging, dice everywhere | Pig | 5 | 9 | Rule dice | Thầy Phong | — |
| 8 | **Giàn treo** / The Fly Tower | Catwalks in three levels, ropes, sandbags, darkness below | Monkey | 5 | 10 | Vertical map, Perform | Khôi | **Permanent boosts** |
| 9 | **Cầu thang lớn** / The Grand Staircase | Marble stairs, a crystal chandelier, moonlight through tall windows | Horse | 5 | 11 | Timed objective | Hiền; Wife of Four Days | — |
| 10 | **Hố nhạc** / The Orchestra Pit | Music stands half under water, a sunken piano, rising flood | Snake | 5 | 12 | Status effects, flooding | Thủ trưởng Bảy; Ferry Mother | Auto-battle presets |
| 11 | **Khán phòng** / The Stalls | Rows of red velvet seats, the full house of ghostly audience, the great ceiling | Tiger | 5 | 13 | Everything | — | Hero: the Young Electrician of Công Lý (after) |
| 12 | **Sân khấu** / The Stage | The main stage, painted 1944 backdrop, footlights, then fire | Dragon | 5 | 14 | Two phases | — | Le Thanh's power whole |
| F | **Màn cuối** / The Last Curtain | The café's illusion breaks into what it hid: the burnt dressing room and the stage beyond, the whole audience watching | **Le Thanh** | 5 + restored spirits | 15 | The last rite | — | The ending |

**The final battle [call].**
- **Phase 1, the audience:** every lingering spirit whose memory you didn't restore fights for Le
  Thanh as red specters. Every one you restored fights beside you, as allies outside your 5.
- **Phase 2, the rite:** Le Thanh sings. **Khúc ca lãng quên** (the song of forgetting) silences a
  unit each round. Her red dress becomes embers. The only way to break the song is through each of
  your restored recruits' memories: they speak a line from their story, worded by the model from
  their own fragments, which breaks the silence on someone.
- **The ending.** Win, and she is pacified. You can stay to hear her own story, and the spirits are
  free to move on. Lose, and you become her audience (the bad ending). How many spirits you restored
  shapes the epilogue.

---

## 4. What this needs from the rules

**New effect flags** (each a column in `abilities.csv`, added only for these):
- `revive` (Diệp, items)
- `give-ap` (Tí)
- `grant-action` (Bảy)
- `last-stand` (Phong)
- `group-shield` (Phong, Hiền)
- `guard-ally` (Nhân)
- `link-join` (Khôi)
- `phase` / step through a wall (Tài)
- `area` (Quang's grenade, Hiền's song)
- `status`: silence, poison, root (Dog, Snake)
- `push` / `pull` (Buffalo, Monkey)
- `steal` (Rat)
- `decoy` (Cat)

**Stage gimmick switches:** windows/spawns, height, mirrors, hazards with a warning, offerings, fog
and locks, class swaps, rule dice, levels and ropes, Perform, timers, flooding, seats as walls, fire
spread, phases. Most reuse existing code (spawns, Perform, hunters). New ones: flooding, fire spread,
rule dice, levels.

**Also needed:** game over when the player falls (retry from the stage's start, café, quit);
temporary party members (Bảo and Mai in T3); the engine forms (tints, strings, the gold knot).

**Level cap:** 15, with XP steps tuned in the soak.

**Quick match (a later phase, the user):** the human side picks from 12 (the player, Le Thanh and the
ten allies), and the monster side from the 12 zodiacs, so every zodiac kit must also work under a
player's control.

---

## 5. Open for the user

1. The 12 tormentors are first drafts. Swap any role, animal or order.
2. Le Thanh's wedding night as the night of the fire: keep or change?
3. Each recruit's adjusted name, look and fragments: edit freely.
4. Bảo and Mai's café roles.
5. Stage 1's party: Bảo and Mai once more (my proposal), or another way.
