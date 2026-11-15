# Builds tools/art/lastcurtain_prompts.json from the night's data: one prompt per room, event,
# guest portrait, item, monster and minion, plus the UI pieces (plan 4a). Run from the repo root.
import csv, json

N = 'plugins/last-curtain/assets/night/'
STYLE = ('1960s Saigon cinema poster illustration, hand-painted gouache, faded print colours, lacquer red, '
         'mustard yellow, jade teal, cream paper, ink black, subtle halftone grain, visible brush strokes')
NEG = ('photo, photograph, 3d render, text, letters, words, calligraphy, chinese characters, writing, watermark, '
       'logo, signature, blurry, neon, modern plastic, deformed, perspective, people')

ROOMS = {
    'foyer': 'a grand marble foyer, chequered floor, twin staircases, gilt columns, red carpet runner',
    'understage': 'a cramped under-stage cellar, stone floor, wooden trap mechanisms, ropes and pulleys, candles',
    'auditorium': 'rows of red velvet seats facing a stage, gilt balcony fronts',
    'pit': 'an orchestra pit, music stands, chairs, a grand piano, sheet music on the floor',
    'corridor': 'a narrow back corridor, worn wooden floor, pipes along the walls',
    'stairwell': 'a service stairwell, iron steps spiralling, a bare bulb',
    'boxes': 'a row of private box seats, small gilt chairs, velvet curtains',
    'balcony': 'an upper balcony, curved rows of seats, a brass rail',
    'costumes': 'a costume store, racks of colourful costumes, hat boxes, mannequins',
    'props': 'a prop room, painted swords, crowns, a cardboard throne, crates',
    'dressing': 'a dressing room, a dressing table with a round bulb mirror, a folding screen, a clothes rail',
    'lighting': 'a lighting box, a console of brass switches, a spotlight on a stand',
    'green': 'a green room, green walls, a worn sofa, a tea table, armchairs',
    'archive': 'a programme archive, shelves of old programmes, paper stacks, a desk lamp',
    'mirrors': 'a hall of mirrors, many tall mirrors on the walls, a polished floor',
    'fly': 'a fly loft, wooden walkways, coils of rope, hanging sandbags',
    'boards': 'rotten stage boards, broken planks, holes in the floor',
    'rigging': 'a rigging pit, iron counterweights, chains and ropes in the dark',
    'patronbox': "the patron's box, a gilt armchair, a small table with a decanter, red velvet",
    'prompter': "a prompter's box, a tiny desk, a script, a lamp, a bell",
    'maskcabinet': 'a mask cabinet, glass cases of theatre masks, a velvet stool',
    'office': 'a shuttered office, a desk with ledgers, a safe, closed shutters',
    'vault': 'a dusty vault, old trunks, cobwebs, a heavy door',
    'trapdoor': 'the stage floor with a square trapdoor in the middle, chalk marks',
    'catwalk': 'a narrow iron catwalk over darkness, a rail, cables',
    'rehearsal': 'a rehearsal room, a piano, mirrors, a wooden floor, chairs in a circle',
    'chapel': 'a small chapel of the stage ancestors, an altar with incense, red candles, offerings of fruit',
    'gallery': 'a rigging gallery, wooden rails, ropes tied to cleats',
    'hallway': 'a hallway, patterned tiles, a long rug, framed posters',
    'passage': 'a stone passageway, damp floor, a lantern',
    'sidecorridor': 'a side corridor, encaustic tiles, a bench',
    'landing': 'a stair landing, a window, a potted palm',
    'emptyroom': 'an empty room, bare boards, one chair, dust',
    'cloakroom': 'a cloakroom, a counter, rows of coat hooks, hats',
    'storeroom': 'a storeroom, boxes, paint tins, a ladder',
    'servantstair': "a servants' stair, narrow wooden steps, a handrail",
    'ticketlobby': 'a ticket lobby, a ticket window, a queue rope, posters',
    'bar': 'an interval bar, a long counter, bottles, small round tables',
    'orchestraroom': 'an orchestra room, instrument cases, chairs, music stands',
    'wigroom': 'a wig room, wigs on stands, a dressing table, combs',
    'armoury': 'a stage armoury, racks of prop spears, shields, a breastplate',
    'sewing': 'a sewing room, a sewing machine, bolts of cloth, a dress form',
    'paintshop': 'a paint shop, a huge canvas backdrop on the floor, paint pots, brushes',
    'lostfound': 'a lost property room, shelves of umbrellas, gloves, bags',
    'pantry': 'an old pantry, shelves of jars, a sack of rice, a clay stove',
    'flytower': 'a fly tower, a dizzying shaft of ropes and battens',
    'flyloft2': 'a rope room, walls lined with ropes and pins, a winch',
    'recording': 'a recording booth, a reel-to-reel tape machine, a microphone, padded walls',
    'boxoffice': 'a box office, a cash register, ticket rolls, a ledger',
    'dimmer': 'a dimmer room, big lever switches, cables, a warning lamp',
    'projection': 'a projection room, a film projector, film reels, a small window',
    'balletroom': 'a ballet room, a barre along the wall, a polished floor, a piano',
    'shrine': 'a little shrine, a small altar, incense sticks, a photograph, flowers',
    'royalbox': 'the royal box, a pair of gilt thrones, a balustrade, a canopy',
    'waitingroom': "an artists' waiting room, a bench, a clock, a coat stand",
    'loosestairs': 'loose wooden stairs, missing steps, a broken rail',
    'pitcover': 'a broken pit cover, cracked boards over a dark hole',
    'boilerroom': 'a boiler room, a big iron boiler, pipes, gauges, steam',
    'roofterrace': 'a roof terrace at night, tiles, a parapet, potted plants',
    'composer': "a composer's room, a piano covered in scores, a desk, a candle",
    'weaponrack': 'a fencing hall, a rack of foils, a fencing strip on the floor',
    'infirmary': 'an old infirmary, an iron bed, a medicine cabinet, a screen',
}
FEATURED = {'foyer', 'understage'}  # plus goal rooms, below
PORTRAITS = {
    'nhi': 'a young Vietnamese elementary schoolgirl in a white school uniform, ponytail, serious eyes',
    'thanh': 'a young Vietnamese opera singer in a red bridal ao dai, hair ribbon, silver bracelets, proud',
    'bao': 'an old Vietnamese retired army doctor, grey suit a size too large, round glasses, kind tired face',
    'khang': 'a Vietnamese debt collector, former boxer, leather jacket, gold chain, broken nose',
    'mai': 'an older Vietnamese theatre caretaker woman, brown ao ba ba, tight bun, a ring of keys',
    'huy': 'a young Vietnamese university student, hooded jacket, headlamp on his forehead, camera strap',
    'lan': 'an elegant Vietnamese antique appraiser woman, dark silk blouse, white gloves, a loupe on a chain',
}

def rows(name):
    return list(csv.DictReader(open(N + name, encoding='utf-8')))

goals = {g['target'] for g in rows('goals.csv') if g['kind'] == 'room'}
out = {'style': STYLE, 'negative': NEG, 'rooms': [], 'events': [], 'portraits': [], 'items': [], 'cards': [], 'ui': []}
for i, r in enumerate(rows('rooms.csv')):
    featured = r['id'] in FEATURED or r['id'] in goals
    out['rooms'].append({'id': r['id'], 'kind': r['kind'], 'featured': featured, 'seed': 4000 + i,
                         'prompt': 'top-down view from directly above, flat floor plan of ' + ROOMS[r['id']] +
                                   ', inside the old Saigon Opera House' + (', rich gold ornament, warm glow' if featured else '') + ', game map tile'})
for i, e in enumerate(rows('events.csv')):
    first = e['text'].split('. ')[0].rstrip('.')
    out['events'].append({'id': e['id'], 'seed': 5000 + i, 'prompt': 'eerie scene in an old opera house at night: ' + first + ', haunting, dramatic shadows, one accent of red'})
for i, (g, d) in enumerate(PORTRAITS.items()):
    out['portraits'].append({'id': g, 'seed': 6000 + i, 'prompt': 'head and shoulders portrait of ' + d + ', plain cream background, 1960s film poster portrait'})
for i, it in enumerate(rows('items.csv')):
    out['items'].append({'id': it['id'], 'seed': 7000 + i, 'prompt': 'a single ' + it['name'].lower() + ', one object centred on a plain cream background, isolated item, game icon'})
for i, m in enumerate(rows('monsters.csv') + rows('minions.csv')):
    out['cards'].append({'id': m['id'], 'seed': 8000 + i, 'prompt': 'full body ' + m['name'].lower() + ', a monster of a haunted opera house, front view, standing, on a plain cream background, painted theatre cut-out'})
out['ui'] = [
    {'id': 'panel', 'seed': 9000, 'prompt': 'an ornate empty rectangular frame border, art deco 1960s cinema bill border, gold and lacquer red, plain dark centre, flat, symmetric'},
    {'id': 'paper', 'seed': 9001, 'prompt': 'plain aged cream paper texture, faint halftone grain, subtle stains, flat, seamless'},
    {'id': 'title', 'seed': 9002, 'prompt': 'the Saigon Opera House facade at night under a full moon, cinema poster key art, dramatic'},
]
json.dump(out, open('tools/art/lastcurtain_prompts.json', 'w'), indent=1)
print(len(out['rooms']), 'rooms', sum(r['featured'] for r in out['rooms']), 'featured,', len(out['events']), 'events,', len(out['items']), 'items,', len(out['cards']), 'cards')
