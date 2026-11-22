# packghost.sh <id> <seed> : an ordinary ghost, the bound painting only (sprite <id>)
set -e
id=$1; bs=$2; U=/mnt/user-data/uploads/output/lastcurtain; A=/home/claude/umbra/plugins/last-curtain/apps/LastCurtain/assets/art
python3 - <<PY
import json
from PIL import Image
c=Image.open('${id}_cut.png'); b=Image.open('$U/bound/${id}_bound_${bs}_00001_.png').convert('RGB')
b.putalpha(c.getchannel('A')); b.save('${id}_bcut.png')
PY
python3 tools/art/pack_figure.py ${id}_bcut.png $id $A
python3 - <<PY
import json
from PIL import Image
box=json.load(open('${id}_box.json')); b=Image.open('${id}_bcut.png')
b.crop(tuple(box)).resize((512,627),Image.LANCZOS).save('$A/sprites/${id}/face_calm.png',optimize=True)
PY
