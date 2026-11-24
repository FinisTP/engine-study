# Inpaints part of a picture with SDXL base on the user's ComfyUI (the eyes of a portrait, say).
#   python tools/art/inpaint.py <src.png> <mask.png> "<prompt>" --out <dir> [--denoise 0.55 0.7] [--seeds 1 2 3 4]
# The mask is white where to repaint. Writes <out>/<name>_<denoise>_<seed>.png; existing ones are skipped.
# Needs Python 3 only.
import argparse, json, os, sys, time, urllib.error, urllib.parse, urllib.request, uuid

NEG = 'closed eyes, squinting, cartoon, anime, 3d render, photo, text, watermark, extra eyes, deformed'


def call(host, path, body=None, headers=None):
    request = urllib.request.Request('http://' + host + path, data=body, headers=headers or {})
    try:
        with urllib.request.urlopen(request, timeout=120) as r:
            return r.read()
    except urllib.error.HTTPError as e:
        sys.exit('ComfyUI said %d: %s' % (e.code, e.read().decode('utf-8', 'replace')[:2000]))


def upload(host, file):
    boundary = uuid.uuid4().hex
    body = ('--%s\r\nContent-Disposition: form-data; name="image"; filename="%s"\r\nContent-Type: image/png\r\n\r\n' % (boundary, os.path.basename(file))).encode()
    body += open(file, 'rb').read() + ('\r\n--%s\r\nContent-Disposition: form-data; name="overwrite"\r\n\r\ntrue\r\n--%s--\r\n' % (boundary, boundary)).encode()
    return json.loads(call(host, '/upload/image', body, {'Content-Type': 'multipart/form-data; boundary=' + boundary}))['name']


def graph(src, mask, prompt, denoise, seed):
    # 1. Encode the picture, keep its latent outside the mask, sample inside it (house recipe: 30 steps, cfg 7).
    return {
        '1': {'class_type': 'CheckpointLoaderSimple', 'inputs': {'ckpt_name': 'sd_xl_base_1.0.safetensors'}},
        '2': {'class_type': 'CLIPTextEncode', 'inputs': {'text': prompt, 'clip': ['1', 1]}},
        '3': {'class_type': 'CLIPTextEncode', 'inputs': {'text': NEG, 'clip': ['1', 1]}},
        '4': {'class_type': 'LoadImage', 'inputs': {'image': src}},
        '5': {'class_type': 'LoadImageMask', 'inputs': {'image': mask, 'channel': 'red'}},
        '6': {'class_type': 'VAEEncode', 'inputs': {'pixels': ['4', 0], 'vae': ['1', 2]}},
        '7': {'class_type': 'SetLatentNoiseMask', 'inputs': {'samples': ['6', 0], 'mask': ['5', 0]}},
        '8': {'class_type': 'KSampler', 'inputs': {'model': ['1', 0], 'positive': ['2', 0], 'negative': ['3', 0], 'latent_image': ['7', 0],
                                                   'seed': seed, 'steps': 30, 'cfg': 7, 'sampler_name': 'dpmpp_2m', 'scheduler': 'karras', 'denoise': denoise}},
        '9': {'class_type': 'VAEDecode', 'inputs': {'samples': ['8', 0], 'vae': ['1', 2]}},
        '10': {'class_type': 'SaveImage', 'inputs': {'images': ['9', 0], 'filename_prefix': 'lastcurtain/inpaint/edit'}},
    }


def run(host, prompt):
    # 2. Queue, wait, fetch SaveImage's picture (node 10).
    prompt_id = json.loads(call(host, '/prompt', json.dumps({'prompt': prompt}).encode(), {'Content-Type': 'application/json'}))['prompt_id']
    while True:
        time.sleep(2)
        history = json.loads(call(host, '/history/' + prompt_id))
        if prompt_id in history:
            break
    for image in history[prompt_id]['outputs'].get('10', {}).get('images', []):
        query = urllib.parse.urlencode({'filename': image['filename'], 'subfolder': image.get('subfolder', ''), 'type': image.get('type', 'output')})
        return call(host, '/view?' + query)
    raise RuntimeError('no picture came back: ' + json.dumps(history[prompt_id].get('status', {})))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('src'), p.add_argument('mask'), p.add_argument('prompt')
    p.add_argument('--out', required=True), p.add_argument('--name', default='inpaint'), p.add_argument('--host', default='')
    p.add_argument('--denoise', type=float, nargs='+', default=[0.55, 0.7]), p.add_argument('--seeds', type=int, nargs='+', default=[1, 2, 3, 4])
    a = p.parse_args()
    for host in ([a.host] if a.host else ['127.0.0.1:8000', '127.0.0.1:8188']):
        try:
            call(host, '/system_stats')
            a.host = host
            break
        except Exception:
            continue
    else:
        sys.exit('ComfyUI is not running (tried 8000 and 8188)')
    os.makedirs(a.out, exist_ok=True)
    src, mask = upload(a.host, a.src), upload(a.host, a.mask)
    for d in a.denoise:
        for s in a.seeds:
            target = os.path.join(a.out, '%s_%02d_%d.png' % (a.name, round(d * 100), s))
            if os.path.exists(target):
                continue
            open(target, 'wb').write(run(a.host, graph(src, mask, a.prompt, d, s)))
            print('wrote', target, flush=True)


if __name__ == '__main__':
    main()
