# Repaints one face of a sprite with ExpressionEditor (ComfyUI-AdvancedLivePortrait) on the user's ComfyUI.
#   python tools/art/face_edit.py <sprite> <from face> <to face> '<settings json>' [--tag a] [--host 127.0.0.1:8000]
# Reads art/sprites/<sprite>/face_<from>.png, writes Temp/art-faces/<sprite>_<to>_<tag>.png with the
# original's cut-out (its alpha) put back, since the pose barely moves. Needs Python 3 only.
import argparse, io, json, os, struct, sys, time, urllib.error, urllib.parse, urllib.request, uuid, zlib

SPRITES = os.path.join('plugins', 'last-curtain', 'apps', 'LastCurtain', 'assets', 'art', 'sprites')


def call(host, path, body=None, headers=None):
    request = urllib.request.Request('http://' + host + path, data=body, headers=headers or {})
    try:
        with urllib.request.urlopen(request, timeout=120) as r:
            return r.read()
    except urllib.error.HTTPError as e:
        sys.exit('ComfyUI said %d: %s' % (e.code, e.read().decode('utf-8', 'replace')[:2000]))


def upload(host, file):
    # 1. The picture into ComfyUI's input folder (multipart form, written by hand).
    boundary = uuid.uuid4().hex
    data = open(file, 'rb').read()
    body = ('--%s\r\nContent-Disposition: form-data; name="image"; filename="%s"\r\nContent-Type: image/png\r\n\r\n' % (boundary, os.path.basename(file))).encode()
    body += data + ('\r\n--%s\r\nContent-Disposition: form-data; name="overwrite"\r\n\r\ntrue\r\n--%s--\r\n' % (boundary, boundary)).encode()
    return json.loads(call(host, '/upload/image', body, {'Content-Type': 'multipart/form-data; boundary=' + boundary}))['name']


def graph(name, settings):
    # 2. LoadImage -> ExpressionEditor (unset values 0, the house's crop and ratios) -> SaveImage.
    values = {'rotate_pitch': 0, 'rotate_yaw': 0, 'rotate_roll': 0, 'blink': 0, 'eyebrow': 0, 'wink': 0, 'pupil_x': 0, 'pupil_y': 0,
              'aaa': 0, 'eee': 0, 'woo': 0, 'smile': 0, 'src_ratio': 1, 'sample_ratio': 1, 'sample_parts': 'OnlyExpression', 'crop_factor': 1.7}
    values.update(settings)
    values['src_image'] = ['1', 0]
    return {'1': {'class_type': 'LoadImage', 'inputs': {'image': name}},
            '2': {'class_type': 'ExpressionEditor', 'inputs': values},
            '3': {'class_type': 'SaveImage', 'inputs': {'images': ['2', 0], 'filename_prefix': 'lastcurtain/faces/edit'}}}


def run(host, prompt):
    prompt_id = json.loads(call(host, '/prompt', json.dumps({'prompt': prompt}).encode(), {'Content-Type': 'application/json'}))['prompt_id']
    while True:
        time.sleep(2)
        history = json.loads(call(host, '/history/' + prompt_id))
        if prompt_id in history:
            break
    # Only SaveImage's picture (node 3): ExpressionEditor also reports a preview of its own.
    for image in history[prompt_id]['outputs'].get('3', {}).get('images', []):
        query = urllib.parse.urlencode({'filename': image['filename'], 'subfolder': image.get('subfolder', ''), 'type': image.get('type', 'output')})
        return call(host, '/view?' + query)
    raise RuntimeError('no picture came back: ' + json.dumps(history[prompt_id].get('status', {})))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('sprite'), p.add_argument('source'), p.add_argument('face'), p.add_argument('settings')
    p.add_argument('--tag', default='a'), p.add_argument('--host', default='')
    a = p.parse_args()
    for host in ([a.host] if a.host else ['127.0.0.1:8000', '127.0.0.1:8188']):
        try:
            call(host, '/system_stats')
            a.host = host
            break
        except OSError:
            a.host = ''
    if not a.host:
        sys.exit('ComfyUI is not running (tried ports 8000 and 8188). Start ComfyUI and run this again.')
    source = os.path.join(SPRITES, a.sprite, 'face_' + a.source + '.png')
    out_dir = os.path.join('Temp', 'art-faces')
    os.makedirs(out_dir, exist_ok=True)
    target = os.path.join(out_dir, '%s_%s_%s.png' % (a.sprite, a.face, a.tag))
    if os.path.exists(target):
        print('kept', target)
        return
    png = run(a.host, graph(upload(a.host, source), json.loads(a.settings)))
    open(target, 'wb').write(png) # the cut-out goes back in the cloud (no Pillow needed here)
    print('wrote', target)


if __name__ == '__main__':
    main()
