# packrec.sh <id> [sprite] : packs <id> (freed, faces) and <id>_bound from ./<id>_cut.png, ./<id>_pic mask, bound/<id>_bound_2
set -e
id=$1; sp=${2:-$1}; bs=${3:-2}; U=/mnt/user-data/uploads/output/lastcurtain; A=/home/claude/umbra/plugins/last-curtain/apps/LastCurtain/assets/art
bash tools/art/faces.sh $id
python3 tools/art/pack_figure.py ${id}_cut.png $sp $A f_$id
python3 - <<PY
from PIL import Image
c=Image.open('${id}_cut.png'); b=Image.open('$U/bound/${id}_bound_${bs}_00001_.png').convert('RGB')
b.putalpha(c.getchannel('A')); b.save('${id}_bcut.png')
PY
python3 tools/art/pack_figure.py ${id}_bcut.png ${sp}_bound $A
# the bound face: the bound picture cut at the portrait box
python3 - <<PY
import json
from PIL import Image
box=json.load(open('${id}_box.json')); b=Image.open('${id}_bcut.png')
b.crop(tuple(box)).resize((512,627),Image.LANCZOS).save('$A/sprites/${sp}_bound/face_calm.png',optimize=True)
PY
