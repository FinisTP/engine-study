# Fonts of The Last Curtain

Every font here draws every Vietnamese letter (checked by `ArtTests.cpp`, "Every font in the Last
Curtain skin draws every Vietnamese letter"). All are SIL Open Font License 1.1; each licence is the
`*-OFL.txt` beside it.

| File | Skin roles | Source | SHA-256 |
|---|---|---|---|
| `BeVietnamPro-Regular.ttf` | body, small | google/fonts `ofl/bevietnampro/`, commit `06bfe0d62a98375cb09c942fa18e68bd32cff057` | `cd1ef6e9d7db28ad5cdb88a65ccbe693870e60d340b791f349d248342b4fe4c3` |
| `BeVietnamPro-Medium.ttf` | strong, tag | same | `b60832bfa0fcd015158112c64d7e3fdad3b0c6287d1823f85a3103636e845268` |
| `PlayfairDisplay.ttf` | title, essence | google/fonts `ofl/playfairdisplay/PlayfairDisplay[wght].ttf`, same commit, renamed only | `c40f2293766a503bc70cce9e512ef844a4ccb7cbcde792fe2ea31d191917d8d6` |
| `Lora.ttf` | heading | the rpg-ui plugin's copy (Google Fonts) | `822a6621ccbe8d97d20ac88c1c41f5615c9c2c202eaa75f272cd452aac6475a7` |

**Playfair Display is a variable font**; the engine (stb_truetype) draws its default instance,
Regular (weight 400). The upstream static Bold (`clauseggers/Playfair-Display` tag v1.004) has no
Vietnamese letters, and the licence reserves the name "Playfair Display", so a bold instance made
here would need a new name. Titles are therefore Regular.

Replaced on 2026-11-23 (campaign plan 1): Poppins and Abril Fatface, which lack ư, ả, ắ, ợ, ậ...
