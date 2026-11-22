# faces.sh <id> : copies ComfyUI's expr/<id>/face_<f>_00001_.png to ./f_<id>/face_<f>.png
mkdir -p f_$1; for f in /mnt/user-data/uploads/output/lastcurtain/expr/$1/face_*_00001_.png; do cp $f f_$1/$(basename $f _00001_.png).png; done
