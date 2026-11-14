# The Last Curtain: cast (draft 6, 2026-11-15: goals)

Status: **draft for the user to edit.** Characters 1 and 2 are the user's; their new fields and
characters 3-7 are proposals. Nothing here is built yet.

Decided by the user (2026-11-14): one night of exploration and betrayal replaces the living town
as the example game; the place is the **Saigon Opera House** under its real name and history, and
only the guests and their secrets are invented (`game-design.md` section 3); the player's own character can become the traitor; traitor powers are special powers used
on the board like an item; the built-in model is required; **only the traitor's secret is true on
a given night**, so the signs that night point at one person.

Decided on draft 1 (user, 2026-11-14):

- **Seven characters.** The player picks one; the other six are bots.
- **Saigon, the present day.** A patron has hired the opera house for a private midnight gala.
- **Who turns is decided by the omens** (`game-design.md` section 12): the first guest to fail a
  curse roll is marked. "Strange conduct" and clues were withdrawn (2026-11-14) and are no longer on the sheet.
- **Reading the signs can save the run:** a player who attacks and eliminates the right character
  before midnight ends the night with no traitor.
- **The traitor is no longer the person.** Each traitor form has its own model and style: taken
  by the curse, with only a distorted version of the human's personality. This holds for Nhi too:
  what is fought is the cursed form, not the child.
- **No traitor weakness** (cut with tools and status effects, to hold the cost down). The quirk
  stays as character colour and for the bots' choices.
- **A marked player is told.**

## The sheet

The user's fields, then the added ones (marked +).

| Field | What it is for |
|---|---|
| Age, height, weight, clothing | The model and the look |
| Background, career | Why they are here tonight; what others know of them |
| Talent | Backstory only: it adds no rule |
| Quirk | Backstory, and colour for the bot's choices |
| Advantage weapon | Physical (uses Might) or magical (uses Sanity) |
| Personality | How they talk and whom they befriend |
| + Pronoun, voice | For the lines the model words (already in the engine) |
| + Stats | HP, Might, Sanity, Speed at level 1 (first numbers, to be balanced by test runs) |
| + Play style | How they play when a bot: what they go for, when they fight or run |
| + Bond | One feeling toward another guest at 19:00, the seed for allying or betraying |
| + Goals | Two or three reasons they came; one is drawn secretly each night (`goals.csv`) |
| + Curse | The element that takes them; names their power and their signs |
| Traitor plot | The secret that is true only on the night they turn |
| + Traitor form | Its own look and a distorted echo of the personality: no longer the person |
| Traitor power | The special power of their traitor form, in board terms |

Stats: Might and Sanity run 1-8, Speed is tiles per turn, HP is plain points.

---

## 1. Truong Uyen Nhi (user)

- **Age / height / weight:** 10, 1.20 m, 35 kg
- **Clothing:** student uniform, ponytail
- **Background:** daughter of the wealthy agent who arranged tonight's gala for the patron; brought here by her father and now looking for him inside the house
- **Career:** pupil at the local elementary school
- **Talent:** origami prodigy
- **Quirk:** no fear of supernatural things, yet cannot stand insects
- **Advantage weapon:** sharp paper (physical)
- **Personality:** vigilant, confident, hostile to those who mock her
- **+ Pronoun, voice:** she; short, blunt sentences, corrects adults
- **+ Stats:** HP 16, Might 3, Sanity 6, Speed 5
- **+ Play style:** Explorer. Opens rooms fast, avoids fights she did not pick, never flees from a ghost
- **+ Bond:** trusts Mai (the caretaker gave her sweets); dislikes Khang (he laughed at her)
- **+ Curse:** Wood
- **Traitor plot:** she discovers that one of tonight's guests assassinated her father, and will hunt all of them
- **+ Traitor form:** a tall figure of folded paper in the shape of a girl, uniform and ponytail creased into it, no face. Her vigilance has become a hunt: it names each guest's "lie" in a flat child's voice
- **Traitor power:** her origami carries the Wood curse: it strikes one player in a straight line from her, damage at once, no counter-attack

## 2. Le Thanh (user)

- **Age / height / weight:** 25, 1.56 m, 50 kg
- **Clothing:** red bridal dress, hair ribbon, silver bracelets
- **Background:** famous singer of this house, invited by the patron to sing on its stage at midnight
- **Career:** opera singer
- **Talent:** powerful voice
- **Quirk:** runs from supernatural entities, yet likes spiritual things
- **Advantage weapon:** voice (magical)
- **Personality:** mild-mannered, respectful, slow to make friends
- **+ Pronoun, voice:** she; formal and soft, never raises her voice in talk
- **+ Stats:** HP 20, Might 2, Sanity 7, Speed 4
- **+ Play style:** Loner. Keeps two rooms away from others, leaves a room when an entity enters, collects charms
- **+ Bond:** uneasy about Lan (the appraiser stared at her bracelets); respects Doctor Bao
- **+ Curse:** Flame
- **Traitor plot:** she remembers an earlier life: a singer burnt alive on this stage by her patron on the night of the 1944 air raid, her death hidden in the raid's damage. She seeks to destroy the house forever (the raid is real; the singer and the patron are invented)
- **+ Traitor form:** the Opera Specter, the bride as she burned: the red dress is embers, the ribbon a line of smoke, the bracelets glow. Her courtesy remains as ceremony: she bows to each guest before she sings at them
- **Traitor power:** her voice carries the Flame curse: damage at once to everyone within 2 tiles of her

## 3. Dinh Quoc Bao (proposal)

- **Age / height / weight:** 58, 1.68 m, 64 kg
- **Clothing:** grey suit a size too large, round glasses, a worn leather bag
- **Background:** the house's company doctor for thirty years, now retired; came because the invitation said someone would need him
- **Career:** retired army doctor
- **Talent:** steady hands
- **Quirk:** calm at the sight of blood, yet cannot enter a room with standing water without stopping at the door
- **Advantage weapon:** surgical kit (physical)
- **Personality:** patient, dry humour, answers a question with a question
- **+ Pronoun, voice:** he; measured, clinical, calls everyone by full name
- **+ Stats:** HP 24, Might 4, Sanity 6, Speed 4
- **+ Play style:** Protector. Stays near the weakest ally, spends turns healing, fights only when cornered
- **+ Bond:** protective of Nhi; distrusts Mai (the caretaker knows too much about the house)
- **+ Curse:** Water
- **Traitor plot:** he signed false papers for the patron's family, calling each death under this roof an accident; the dead have come to ask him to finish the list
- **+ Traitor form:** a drowned physician, suit dripping, glasses fogged from inside, water running from the bag. His bedside manner remains: he tells each guest calmly what they will die of
- **Traitor power:** the Water curse floods his room and every room next to it for 2 turns: Speed 1 inside, and damage to anyone who ends a turn there

## 4. Vo Minh Khang (proposal)

- **Age / height / weight:** 34, 1.82 m, 88 kg
- **Clothing:** leather jacket, rolled sleeves, a gold chain, taped knuckles
- **Background:** collects debts for people who do not write things down; the patron owes his employer, and he came to be paid
- **Career:** debt collector, once a boxer
- **Talent:** heavy hands
- **Quirk:** afraid of nothing he can hit, yet will not touch anything old or holy
- **Advantage weapon:** brass knuckles (physical)
- **Personality:** loud, generous to those on his side, mocks what he does not understand
- **+ Pronoun, voice:** he; slang, jokes, calls people "kid" and "boss"
- **+ Stats:** HP 30, Might 7, Sanity 2, Speed 3
- **+ Play style:** Fighter. Walks toward monsters for the XP, allies with whoever fought beside him last
- **+ Bond:** likes Huy (the student amuses him); mocked Nhi, and knows she has not forgiven it
- **+ Curse:** Metal
- **Traitor plot:** he learns the patron settled the debt by signing Khang himself over to the house; the other guests' lives buy his back
- **+ Traitor form:** a body half cast in black iron, chains grown from the gold one, fists like weights. His generosity has become bookkeeping: he announces what each guest "owes"
- **Traitor power:** the Metal curse pulls one player within 3 tiles into his room and strikes them at once

## 5. Phan Thi Mai (proposal)

- **Age / height / weight:** 67, 1.50 m, 45 kg
- **Clothing:** brown áo bà ba, a ring of keys at her waist, hair in a tight bun
- **Background:** caretaker of the opera house for forty years; she laid out tonight's gala and knows every door
- **Career:** theatre caretaker
- **Talent:** knows the house
- **Quirk:** at ease with the house's noises, yet will not look into a mirror
- **Advantage weapon:** incense and prayer beads (magical)
- **Personality:** warm to children, sharp with the proud, never says the patron's name
- **+ Pronoun, voice:** she; proverbs, "child" and "dear", stops mid-sentence to listen
- **+ Stats:** HP 18, Might 2, Sanity 8, Speed 3
- **+ Play style:** Schemer. Moves little, opens paths for others, gives items away to buy friends
- **+ Bond:** fond of Nhi; cold to Lan (who came to put a price on the house's things)
- **+ Curse:** Earth
- **Traitor plot:** her son died under the stage long ago, and the house keeps him as long as she keeps it fed; tonight's guests are the meal
- **+ Traitor form:** the caretaker rooted into the floor, soil in the folds of her clothes, keys grown into her hand. Her warmth has become a host's: she invites each guest, kindly, to stay
- **Traitor power:** the Earth curse seals one doorway within 3 tiles for 2 turns; nobody passes it

## 6. Hoang Gia Huy (proposal)

- **Age / height / weight:** 19, 1.74 m, 60 kg
- **Clothing:** hooded jacket, headlamp, a camera on a chest strap, scuffed trainers
- **Background:** films old buildings at night for a small online following; found the invitation in his mailbox with no stamp
- **Career:** university student
- **Talent:** quick feet
- **Quirk:** delighted by anything haunted, yet panics when his camera is off
- **Advantage weapon:** camera flash (magical)
- **Personality:** eager, talks to an audience that is not there, brave until alone
- **+ Pronoun, voice:** he; fast, narrates what he does, online slang
- **+ Stats:** HP 20, Might 3, Sanity 4, Speed 5
- **+ Play style:** Explorer. Always takes the unopened path, picks up everything, flees fights when alone
- **+ Bond:** admires Thanh (he has her recordings); follows Khang around
- **+ Curse:** Shadow
- **Traitor plot:** his own footage shows him dying in this house a year ago, the night he climbed in to film it; what walked out was a stand-in, and the house wants it back with company
- **+ Traitor form:** a silhouette with a lit camera lens where the chest should be, the body a beat behind its own movements. His narration goes on, now describing the guests' last moments before they happen
- **Traitor power:** the Shadow curse swaps his place with any player in an opened room, then he may act

## 7. Tran Ngoc Lan (proposal)

- **Age / height / weight:** 41, 1.63 m, 54 kg
- **Clothing:** dark silk blouse, white gloves, a jeweller's loupe on a chain
- **Background:** appraises antiques for auction houses; the patron invited her to value his collection of stage costumes and masks, shown at the gala
- **Career:** antique appraiser
- **Talent:** a trained eye
- **Quirk:** unmoved by ghosts that belong to an object she can price, yet cannot bear to see a valuable thing broken
- **Advantage weapon:** antique dagger (physical)
- **Personality:** polished, curious about people's belongings, counts favours
- **+ Pronoun, voice:** she; precise, compliments that sound like valuations
- **+ Stats:** HP 22, Might 5, Sanity 5, Speed 4
- **+ Play style:** Schemer. Goes for item rooms, trades, allies with whoever is strongest at 23:00
- **+ Bond:** curious about Thanh's bracelets; looks down on Khang
- **+ Curse:** Mirror
- **Traitor plot:** she recognises a mirror she sold to the patron years ago, now hung in the house; her reflection stayed behind in it and made a bargain in her name
- **+ Traitor form:** her reflection stepped out: left and right reversed, skin like silvered glass with fine cracks. Her appraising eye remains: she states each guest's worth, and it is always less than they hoped
- **Traitor power:** the Mirror curse turns the next attack or power used on her back on its user

## Goals (2026-11-15, one drawn each night; counts tuned by the soak) [call]

| Guest | Goals |
|---|---|
| Truong Uyen Nhi | reach the Patron's box (her father's seat) · carry her father's pocket watch · have Phan Thi Mai as an ally |
| Le Thanh | carry the Phoenix crown · reach the Auditorium · pass 2 event rolls |
| Dinh Quoc Bao | have Truong Uyen Nhi as an ally · reach the Green room (the old infirmary) · destroy 1 monster |
| Vo Minh Khang | carry the patron's Ledger · destroy 2 monsters · reach the Shuttered office |
| Phan Thi Mai | reach the Chapel of the stage ancestors · have Le Thanh as an ally · carry the ring of keys she lost |
| Hoang Gia Huy | pass 2 event rolls · reach the Fly loft · destroy 1 monster |
| Tran Ngoc Lan | carry the Gilded mask · carry the Phoenix crown · have Vo Minh Khang as an ally |

Keepsakes lie where the soak put them (plan 2b): the Phoenix crown in the Interval bar, the Ledger
on the Service stairwell, the Gilded mask in the Orchestra room, the pocket watch in the Box seats,
the ring of keys in the Back corridor [call]. Khang distrusts Lan (as she distrusts him).

## How the cast fits together (proposal)

| # | Name | Curse | Weapon | HP | Might | Sanity | Speed | Play style |
|---|---|---|---|---|---|---|---|---|
| 1 | Truong Uyen Nhi | Wood | physical | 16 | 3 | 6 | 5 | Explorer |
| 2 | Le Thanh | Flame | magical | 20 | 2 | 7 | 4 | Loner |
| 3 | Dinh Quoc Bao | Water | physical | 24 | 4 | 6 | 3 | Protector |
| 4 | Vo Minh Khang | Metal | physical | 30 | 7 | 2 | 3 | Fighter |
| 5 | Phan Thi Mai | Earth | magical | 18 | 2 | 8 | 3 | Schemer |
| 6 | Hoang Gia Huy | Shadow | magical | 20 | 3 | 4 | 6 | Explorer |
| 7 | Tran Ngoc Lan | Mirror | physical | 22 | 5 | 5 | 4 | Schemer |

- The first five curses are the five elements (Wood, Flame, Water, Metal, Earth); Shadow and Mirror fill the cast to seven.
- Each traitor power changes the board in a different way: a line (Nhi), an area (Thanh), a flooded zone (Bao), a pull (Khang), a sealed door (Mai), a swap (Huy), a reflection (Lan). So each traitor asks for a different plan.

## Open points

1. **Stats** are first guesses; test runs will balance them once the board exists.
2. The rules of the night are in `game-design.md`.
