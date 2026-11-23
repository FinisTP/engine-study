# Paints the campaign UI style board's pictures with the user's ComfyUI (ui-art-requests section 0).
#   python tools/art/ui_style_board.py [--host 127.0.0.1:8000] [--out Temp/art-ui/raw] [--only name]
# Each job x seed -> <out>/<name>_<seed>.png; pictures already there are skipped, so it can be
# stopped and started again. Needs only Python 3 (no packages); ComfyUI must be running.
import argparse, json, os, sys, time, urllib.parse, urllib.request


def workflow(job, seed, style, negative):
    # 1. SDXL base, the house recipe: 28 steps, cfg 6.5, dpmpp_2m karras.
    text = job['prompt'] + ('' if job.get('plain') else ', ' + style)
    return {
        '1': {'class_type': 'CheckpointLoaderSimple', 'inputs': {'ckpt_name': 'sd_xl_base_1.0.safetensors'}},
        '2': {'class_type': 'CLIPTextEncode', 'inputs': {'text': text, 'clip': ['1', 1]}},
        '3': {'class_type': 'CLIPTextEncode', 'inputs': {'text': negative, 'clip': ['1', 1]}},
        '4': {'class_type': 'EmptyLatentImage', 'inputs': {'width': job['w'], 'height': job['h'], 'batch_size': 1}},
        '5': {'class_type': 'KSampler', 'inputs': {'model': ['1', 0], 'positive': ['2', 0], 'negative': ['3', 0], 'latent_image': ['4', 0],
                                                   'seed': seed, 'steps': 28, 'cfg': 6.5, 'sampler_name': 'dpmpp_2m', 'scheduler': 'karras', 'denoise': 1.0}},
        '6': {'class_type': 'VAEDecode', 'inputs': {'samples': ['5', 0], 'vae': ['1', 2]}},
        '7': {'class_type': 'SaveImage', 'inputs': {'images': ['6', 0], 'filename_prefix': 'lastcurtain/ui/' + job['name']}},
    }


def call(host, path, body=None):
    request = urllib.request.Request('http://' + host + path, data=json.dumps(body).encode() if body else None,
                                     headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(request, timeout=60) as r:
        return r.read()


def paint(host, graph, target):
    # 2. Queue it, wait for the history entry, fetch the picture.
    prompt_id = json.loads(call(host, '/prompt', {'prompt': graph}))['prompt_id']
    while True:
        time.sleep(2)
        history = json.loads(call(host, '/history/' + prompt_id))
        if prompt_id in history:
            break
    for output in history[prompt_id]['outputs'].values():
        for image in output.get('images', []):
            query = urllib.parse.urlencode({'filename': image['filename'], 'subfolder': image['subfolder'], 'type': image['type']})
            with open(target, 'wb') as f:
                f.write(call(host, '/view?' + query))
            return


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--host', default='')
    parser.add_argument('--out', default=os.path.join('Temp', 'art-ui', 'raw'))
    parser.add_argument('--only', default='')
    args = parser.parse_args()
    spec = json.load(open(os.path.join(os.path.dirname(__file__), 'ui_style_board.json'), encoding='utf-8'))
    os.makedirs(args.out, exist_ok=True)
    # 1. ComfyUI Desktop listens on 8000, the portable one on 8188: take the first that answers.
    for host in ([args.host] if args.host else ['127.0.0.1:8000', '127.0.0.1:8188']):
        try:
            call(host, '/system_stats')
            args.host = host
            break
        except Exception:
            continue
    else:
        sys.exit('ComfyUI did not answer on %s. Is it running?' % (args.host or '127.0.0.1:8000 or 8188'))
    print('ComfyUI at ' + args.host, flush=True)
    todo = [(j, s) for j in spec['jobs'] for s in j['seeds'] if not args.only or j['name'] == args.only]
    for n, (job, seed) in enumerate(todo, 1):
        target = os.path.join(args.out, '%s_%d.png' % (job['name'], seed))
        if os.path.exists(target):
            continue
        started = time.time()
        try:
            paint(args.host, workflow(job, seed, spec['style'], spec['negative']), target)
        except Exception as e:
            sys.exit('ComfyUI at %s did not answer (%s). Is it running?' % (args.host, e))
        print('%2d/%d %s (%.0f s)' % (n, len(todo), os.path.basename(target), time.time() - started), flush=True)
    print('Done: ' + os.path.abspath(args.out))


if __name__ == '__main__':
    main()
