// W0 browser sample: the real turn rules (WebAssembly), the settlement drawn with WebGPU,
// and a storage round trip. Each part reports its own result so one failure doesn't hide the others.
import { loadSimulation } from './umbra_sim.js';

const log = (id, text, ok) => {
    const el = document.getElementById(id);
    el.textContent = text;
    el.className = ok === undefined ? '' : ok ? 'ok' : 'fail';
};

// ---------- 1. Simulation ----------
async function runSimulation(wasmBytes, contentText) {
    try {
        const sim = await loadSimulation(wasmBytes);
        sim.load(contentText);
        sim.reset();
        const lines = [];
        let guard = 0;
        do {
            const s = sim.state();
            lines.push(`round ${s.round}: ` + s.fighters.map((f) => `${f.id} ${f.health}`).join(', '));
        } while (sim.stepAi() && ++guard < 500);
        const s = sim.state();
        const hero = s.fighters.find((f) => f.id === 'hero');
        const match = s.steps === 26 && s.round === 12 && s.winner === 0 && hero.health === 32;
        document.getElementById('sim-log').textContent = lines.join('\n');
        log('sim', `${s.steps} actions, round ${s.round}, winner team ${s.winner}, hero ${hero.health} HP. ` +
            (match ? 'Matches the native build exactly.' : 'DOES NOT match native (26 / 12 / 0 / 32).'), match);
    } catch (error) {
        log('sim', `failed: ${error.message}`, false);
    }
}

// ---------- 2. Storage ----------
function testStorage() {
    try {
        const key = 'umbra.w0.visits';
        const visits = Number(localStorage.getItem(key) || '0') + 1;
        localStorage.setItem(key, String(visits));
        const back = Number(localStorage.getItem(key));
        log('storage', back === visits ? `localStorage write/read OK (visit ${visits}; reload to see it persist)` : 'read back a different value', back === visits);
    } catch (error) {
        log('storage', `localStorage unavailable: ${error.name} (private mode or blocked storage)`, false);
    }
}

// ---------- 3. Rendering ----------
const mat4 = {
    multiply(a, b) {
        const out = new Float32Array(16);
        for (let c = 0; c < 4; ++c)
            for (let r = 0; r < 4; ++r)
                for (let k = 0; k < 4; ++k) out[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
        return out;
    },
    // Right-handed, depth 0..1 (same as glm::perspectiveRH_ZO). No Y flip: WebGPU's NDC has +Y up.
    perspective(fovY, aspect, near, far) {
        const f = 1 / Math.tan(fovY / 2);
        return new Float32Array([f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, far / (near - far), -1, 0, 0, (near * far) / (near - far), 0]);
    },
    lookAt(eye, target, up) {
        const sub = (a, b) => a.map((v, i) => v - b[i]);
        const norm = (v) => { const l = Math.hypot(...v); return v.map((x) => x / l); };
        const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
        const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
        const f = norm(sub(target, eye)), s = norm(cross(f, up)), u = cross(s, f);
        return new Float32Array([s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0, -dot(s, eye), -dot(u, eye), dot(f, eye), 1]);
    },
    // Translation * rotation (quaternion x, y, z, w) * scale, like world::Transform.
    trs(p, q, s) {
        const [x, y, z, w] = q;
        return new Float32Array([
            (1 - 2 * (y * y + z * z)) * s[0], 2 * (x * y + z * w) * s[0], 2 * (x * z - y * w) * s[0], 0,
            2 * (x * y - z * w) * s[1], (1 - 2 * (x * x + z * z)) * s[1], 2 * (y * z + x * w) * s[1], 0,
            2 * (x * z + y * w) * s[2], 2 * (y * z - x * w) * s[2], (1 - 2 * (x * x + y * y)) * s[2], 0,
            p[0], p[1], p[2], 1]);
    },
};

// The -1..1 cube of scenes/phase2.gltf: 6 faces x 4 vertices (position, normal).
function cubeMesh() {
    const v = [], idx = [];
    const faces = [[0, 1], [0, -1], [1, 1], [1, -1], [2, 1], [2, -1]];
    for (const [axis, sign] of faces) {
        const n = [0, 0, 0]; n[axis] = sign;
        const a = (axis + 1) % 3, b = (axis + 2) % 3, base = v.length / 6;
        for (const [da, db] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) {
            const p = [0, 0, 0]; p[axis] = sign; p[a] = da; p[b] = db * sign;
            v.push(...p, ...n);
        }
        idx.push(base, base + 1, base + 2, base, base + 2, base + 3);
    }
    return { vertices: new Float32Array(v), indices: new Uint16Array(idx) };
}

async function render(sceneJson, shaderCode) {
    if (!navigator.gpu) return log('gpu', 'WebGPU is not available in this browser', false);
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) return log('gpu', 'no WebGPU adapter (GPU blocked or unsupported)', false);
    const device = await adapter.requestDevice();
    const canvas = document.getElementById('view');
    const context = canvas.getContext('webgpu');
    const format = navigator.gpu.getPreferredCanvasFormat();
    context.configure({ device, format, alphaMode: 'opaque' });

    // 1. Shader: compile and report errors instead of rendering nothing.
    const module = device.createShaderModule({ code: shaderCode });
    const info = await module.getCompilationInfo();
    const errors = info.messages.filter((m) => m.type === 'error');
    if (errors.length) return log('gpu', 'WGSL errors: ' + errors.map((m) => `${m.lineNum}: ${m.message}`).join('; '), false);

    // 2. Draws: every cube entity of the scene (the tower instance is a separate model, skipped).
    const draws = sceneJson.entities.filter((e) => e.model === 'scenes/phase2.gltf' && e.mesh === 0);
    const stride = 256; // minUniformBufferOffsetAlignment
    const drawData = new ArrayBuffer(stride * draws.length);
    draws.forEach((e, i) => {
        const model = mat4.trs(e.position || [0, 0, 0], e.rotation || [0, 0, 0, 1], e.scale || [1, 1, 1]);
        new Float32Array(drawData, i * stride, 20).set([...model, ...(e.color || [1, 1, 1, 1])]);
    });

    const mesh = cubeMesh();
    const buffer = (data, usage) => {
        const b = device.createBuffer({ size: (data.byteLength + 3) & ~3, usage: usage | GPUBufferUsage.COPY_DST });
        device.queue.writeBuffer(b, 0, data);
        return b;
    };
    const vertexBuffer = buffer(mesh.vertices, GPUBufferUsage.VERTEX);
    const indexBuffer = buffer(new Uint16Array([...mesh.indices, 0]), GPUBufferUsage.INDEX); // padded to 4 bytes
    const drawBuffer = buffer(drawData, GPUBufferUsage.UNIFORM);
    const sceneBuffer = device.createBuffer({ size: 128, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST });

    const sceneLayout = device.createBindGroupLayout({ entries: [{ binding: 0, visibility: GPUShaderStage.VERTEX, buffer: {} }] });
    const drawLayout = device.createBindGroupLayout({
        entries: [{ binding: 0, visibility: GPUShaderStage.VERTEX, buffer: { hasDynamicOffset: true, minBindingSize: 80 } }],
    });
    const pipeline = device.createRenderPipeline({
        layout: device.createPipelineLayout({ bindGroupLayouts: [sceneLayout, drawLayout] }),
        vertex: {
            module, entryPoint: 'VSMain',
            buffers: [{ arrayStride: 24, attributes: [{ shaderLocation: 0, offset: 0, format: 'float32x3' }, { shaderLocation: 1, offset: 12, format: 'float32x3' }] }],
        },
        fragment: { module, entryPoint: 'PSMain', targets: [{ format }] },
        primitive: { topology: 'triangle-list', cullMode: 'back', frontFace: 'ccw' },
        depthStencil: { format: 'depth32float', depthWriteEnabled: true, depthCompare: 'less' },
    });
    const sceneGroup = device.createBindGroup({ layout: sceneLayout, entries: [{ binding: 0, resource: { buffer: sceneBuffer } }] });
    const drawGroup = device.createBindGroup({ layout: drawLayout, entries: [{ binding: 0, resource: { buffer: drawBuffer, size: 80 } }] });
    let depth = null;

    // 3. Frame loop: an orbit camera around the settlement.
    const frame = (time) => {
        if (!depth || depth.width !== canvas.width) {
            depth?.destroy();
            depth = device.createTexture({ size: [canvas.width, canvas.height], format: 'depth32float', usage: GPUTextureUsage.RENDER_ATTACHMENT });
        }
        const angle = time * 0.0002;
        const view = mat4.lookAt([Math.sin(angle) * 22, 12, Math.cos(angle) * 22], [0, 0, -2], [0, 1, 0]);
        const projection = mat4.perspective(Math.PI / 3, canvas.width / canvas.height, 0.1, 200);
        device.queue.writeBuffer(sceneBuffer, 0, new Float32Array([...view, ...projection]));

        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
            colorAttachments: [{ view: context.getCurrentTexture().createView(), clearValue: { r: 0.05, g: 0.07, b: 0.1, a: 1 }, loadOp: 'clear', storeOp: 'store' }],
            depthStencilAttachment: { view: depth.createView(), depthClearValue: 1, depthLoadOp: 'clear', depthStoreOp: 'store' },
        });
        pass.setPipeline(pipeline);
        pass.setBindGroup(0, sceneGroup);
        pass.setVertexBuffer(0, vertexBuffer);
        pass.setIndexBuffer(indexBuffer, 'uint16');
        for (let i = 0; i < draws.length; ++i) {
            pass.setBindGroup(1, drawGroup, [i * stride]);
            pass.drawIndexed(36);
        }
        pass.end();
        device.queue.submit([encoder.finish()]);
        requestAnimationFrame(frame);
    };
    device.lost.then((reason) => log('gpu', `GPU device lost: ${reason.message}`, false));
    requestAnimationFrame(frame);
    log('gpu', `WebGPU OK: ${draws.length} cubes from settlement.scene.json, ${adapter.info?.vendor || 'unknown vendor'} ${adapter.info?.architecture || ''}`, true);
}

// ---------- start ----------
export async function start(assets) {
    testStorage();
    await runSimulation(assets.wasm, assets.content);
    await render(assets.scene, assets.shader).catch((error) => log('gpu', `failed: ${error.message}`, false));
}
