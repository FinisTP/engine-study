// WebGPU side of the browser renderer (RendererWeb.cpp, roadmap P10). Linked with --js-library.
// Scene.wgsl below is the hand port of Scene.hlsl (ADR 0001): per-draw data comes from a uniform
// buffer slot with a dynamic offset instead of push constants, and there is no Y flip.
var UmbraRender = {
  $UG: {
    device: null, context: null, format: null, depth: null, name: 'WebGPU',
    meshes: [null], textures: [null], groups: [null],
    shader: `
struct Frame { viewProjection: mat4x4f };
struct Draw { model: mat4x4f, color: vec4f, texturing: vec4f };
@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(1) var<uniform> draw: Draw;
@group(1) @binding(0) var baseColorSampler: sampler;
@group(1) @binding(1) var baseColorTexture: texture_2d<f32>;
struct Out { @builtin(position) position: vec4f, @location(0) normal: vec3f, @location(1) uv: vec2f };
@vertex fn vs(@location(0) position: vec3f, @location(1) normal: vec3f, @location(2) uv: vec2f) -> Out {
  var o: Out;
  let world = draw.model * vec4f(position, 1.0);
  o.position = frame.viewProjection * world;
  o.normal = normalize((draw.model * vec4f(normal, 0.0)).xyz);
  o.uv = uv;
  if (draw.texturing.x > 0.0) { // world-space texture coordinates (WorldUv.md)
    let n = abs(o.normal);
    let p = world.xyz;
    var planar = vec2f(p.x, -p.y);
    if (n.x >= n.y && n.x >= n.z) { planar = vec2f(p.z, -p.y); } else if (n.y >= n.z) { planar = vec2f(p.x, p.z); }
    o.uv = planar / draw.texturing.x;
  }
  return o;
}
@fragment fn fs(i: Out) -> @location(0) vec4f {
  let light = normalize(vec3f(0.4, 0.8, 0.5));
  let lighting = 0.25 + 0.75 * max(dot(normalize(i.normal), light), 0.0);
  let albedo = textureSample(baseColorTexture, baseColorSampler, i.uv) * draw.color;
  return vec4f(albedo.rgb * lighting, albedo.a);
}
struct LineOut { @builtin(position) position: vec4f, @location(0) color: vec4f };
@vertex fn lineVs(@location(0) position: vec3f, @location(1) color: vec4f) -> LineOut {
  var o: LineOut;
  o.position = frame.viewProjection * vec4f(position, 1.0);
  o.color = color;
  return o;
}
@fragment fn lineFs(i: LineOut) -> @location(0) vec4f { return i.color; }`,

    // A free slot in a list (index 0 is never used: it means "none").
    put: function (list, value) {
      for (var i = 1; i < list.length; ++i) if (list[i] === null) { list[i] = value; return i; }
      list.push(value); return list.length - 1;
    },
    buffer: function (usage, data) {
      var size = Math.max(16, Math.ceil(data.byteLength / 4) * 4); // writeBuffer needs multiples of 4
      var b = UG.device.createBuffer({ size: size, usage: usage | GPUBufferUsage.COPY_DST });
      UG.device.queue.writeBuffer(b, 0, data.buffer, data.byteOffset, data.byteLength);
      return b;
    },
    meshFrom: function (vertices, vertexCount, indices, indexCount) {
      return {
        vertices: UG.buffer(GPUBufferUsage.VERTEX, HEAPF32.subarray(vertices >> 2, (vertices >> 2) + vertexCount * 8)),
        indices: UG.buffer(GPUBufferUsage.INDEX, HEAPU32.subarray(indices >> 2, (indices >> 2) + indexCount)),
        count: indexCount
      };
    },
    textureGroup: function (texture) {
      return UG.device.createBindGroup({ layout: UG.pipeline.getBindGroupLayout(1), entries: [
        { binding: 0, resource: UG.sampler }, { binding: 1, resource: texture.createView() }] });
    },
  },

  umbra_gpu_init__deps: ['$UG'],
  umbra_gpu_init__async: true,
  umbra_gpu_init: function () {
    return Asyncify.handleAsync(async function () {
      if (!navigator.gpu) return 0;
      var adapter = await navigator.gpu.requestAdapter({ powerPreference: 'high-performance' });
      if (!adapter) return 0;
      UG.adapter = adapter; // keep it referenced for the page's lifetime
      var info = adapter.info || {};
      UG.name = info.description || [info.vendor, info.architecture].filter(Boolean).join(' ') || 'WebGPU';
      var device = UG.device = await adapter.requestDevice();
      device.lost.then(function (lost) { if (Module.onGpuLost) Module.onGpuLost(lost.message); });
      device.onuncapturederror = function (e) { if (!UG.errors) UG.errors = 0; if (UG.errors++ < 10) console.warn('WebGPU: ' + e.error.message); };
      UG.format = navigator.gpu.getPreferredCanvasFormat();
      UG.context = Module.canvas.getContext('webgpu');
      UG.context.configure({ device: device, format: UG.format, alphaMode: 'opaque' });
      var module = device.createShaderModule({ code: UG.shader });
      var vertexLayout = { arrayStride: 32, attributes: [
        { shaderLocation: 0, offset: 0, format: 'float32x3' }, { shaderLocation: 1, offset: 12, format: 'float32x3' },
        { shaderLocation: 2, offset: 24, format: 'float32x2' }] };
      var frameLayout = device.createBindGroupLayout({ entries: [
        { binding: 0, visibility: GPUShaderStage.VERTEX, buffer: { type: 'uniform' } },
        { binding: 1, visibility: GPUShaderStage.VERTEX | GPUShaderStage.FRAGMENT, buffer: { type: 'uniform', hasDynamicOffset: true } }] });
      var textureLayout = device.createBindGroupLayout({ entries: [
        { binding: 0, visibility: GPUShaderStage.FRAGMENT, sampler: {} },
        { binding: 1, visibility: GPUShaderStage.FRAGMENT, texture: {} }] });
      UG.pipeline = device.createRenderPipeline({
        layout: device.createPipelineLayout({ bindGroupLayouts: [frameLayout, textureLayout] }),
        vertex: { module: module, entryPoint: 'vs', buffers: [vertexLayout] },
        fragment: { module: module, entryPoint: 'fs', targets: [{ format: UG.format, blend: {
          color: { srcFactor: 'src-alpha', dstFactor: 'one-minus-src-alpha' }, alpha: { srcFactor: 'one', dstFactor: 'one-minus-src-alpha' } } }] },
        primitive: { topology: 'triangle-list', cullMode: 'none' },
        depthStencil: { format: 'depth24plus', depthWriteEnabled: true, depthCompare: 'less' } });
      UG.linePipeline = device.createRenderPipeline({
        layout: device.createPipelineLayout({ bindGroupLayouts: [frameLayout] }),
        vertex: { module: module, entryPoint: 'lineVs', buffers: [{ arrayStride: 28, attributes: [
          { shaderLocation: 0, offset: 0, format: 'float32x3' }, { shaderLocation: 1, offset: 12, format: 'float32x4' }] }] },
        fragment: { module: module, entryPoint: 'lineFs', targets: [{ format: UG.format }] },
        primitive: { topology: 'line-list' },
        depthStencil: { format: 'depth24plus', depthWriteEnabled: false, depthCompare: 'always' } });
      UG.frameLayout = frameLayout;
      UG.frameBuffer = device.createBuffer({ size: 64, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST });
      UG.drawCapacity = 0;
      UG.sampler = device.createSampler({ magFilter: 'linear', minFilter: 'linear', mipmapFilter: 'linear', addressModeU: 'repeat', addressModeV: 'repeat' });
      // Fallback texture: one white pixel (like the Vulkan renderer's).
      var white = device.createTexture({ size: [1, 1], format: 'rgba8unorm', usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST });
      device.queue.writeTexture({ texture: white }, new Uint8Array([255, 255, 255, 255]), { bytesPerRow: 4 }, [1, 1]);
      UG.whiteGroup = UG.textureGroup(white);
      return 1;
    });
  },

  umbra_gpu_mesh__deps: ['$UG'],
  umbra_gpu_mesh: function (vertices, vertexCount, indices, indexCount) {
    return UG.put(UG.meshes, UG.meshFrom(vertices, vertexCount, indices, indexCount));
  },
  umbra_gpu_replace_mesh__deps: ['$UG'],
  umbra_gpu_replace_mesh: function (id, vertices, vertexCount, indices, indexCount) {
    var old = UG.meshes[id];
    if (old) { old.vertices.destroy(); old.indices.destroy(); }
    UG.meshes[id] = UG.meshFrom(vertices, vertexCount, indices, indexCount);
  },
  umbra_gpu_destroy_mesh__deps: ['$UG'],
  umbra_gpu_destroy_mesh: function (id) {
    var m = UG.meshes[id];
    if (m) { m.vertices.destroy(); m.indices.destroy(); UG.meshes[id] = null; }
  },
  umbra_gpu_texture__deps: ['$UG'],
  umbra_gpu_texture: function (width, height, levels) {
    var t = UG.device.createTexture({ size: [width, height], mipLevelCount: levels, format: 'rgba8unorm',
      usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST });
    var id = UG.put(UG.textures, t);
    UG.groups[id] = UG.textureGroup(t);
    return id;
  },
  umbra_gpu_texture_level__deps: ['$UG'],
  umbra_gpu_texture_level: function (id, level, width, height, rgba) {
    UG.device.queue.writeTexture({ texture: UG.textures[id], mipLevel: level }, HEAPU8.subarray(rgba, rgba + width * height * 4),
      { bytesPerRow: width * 4, rowsPerImage: height }, [width, height]);
  },
  umbra_gpu_destroy_texture__deps: ['$UG'],
  umbra_gpu_destroy_texture: function (id) {
    if (UG.textures[id]) { UG.textures[id].destroy(); UG.textures[id] = null; UG.groups[id] = null; }
  },

  umbra_gpu_frame__deps: ['$UG'],
  umbra_gpu_frame: function (viewProjection, draws, drawCount, lines, lineCount, hud) {
    Module.frameCount = (Module.frameCount || 0) + 1; // for the page and tests
    if (Module.onHud) Module.onHud(UTF8ToString(hud));
    var device = UG.device, canvas = Module.canvas;
    // 1. Canvas and depth buffer follow the element's size on screen.
    var scale = window.devicePixelRatio || 1;
    var w = Math.max(1, Math.floor(canvas.clientWidth * scale)), h = Math.max(1, Math.floor(canvas.clientHeight * scale));
    if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
    if (!UG.depth || UG.depth.width !== w || UG.depth.height !== h) {
      if (UG.depth) UG.depth.destroy();
      UG.depth = device.createTexture({ size: [w, h], format: 'depth24plus', usage: GPUTextureUsage.RENDER_ATTACHMENT });
    }
    // 2. Per-frame and per-draw uniforms: one 256-byte slot per draw (dynamic offset).
    device.queue.writeBuffer(UG.frameBuffer, 0, HEAPF32.buffer, viewProjection, 64);
    if (!UG.drawBuffer || drawCount > UG.drawCapacity) {
      if (UG.drawBuffer) UG.drawBuffer.destroy();
      UG.drawCapacity = Math.max(64, drawCount * 2);
      UG.drawBuffer = device.createBuffer({ size: UG.drawCapacity * 256, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST });
      UG.frameGroup = device.createBindGroup({ layout: UG.frameLayout, entries: [
        { binding: 0, resource: { buffer: UG.frameBuffer } }, { binding: 1, resource: { buffer: UG.drawBuffer, size: 96 } }] });
    }
    var src = HEAPF32.subarray(draws >> 2, (draws >> 2) + drawCount * 28);
    var slots = new Float32Array(Math.max(drawCount, 1) * 64);
    for (var d = 0; d < drawCount; ++d) slots.set(src.subarray(d * 28, d * 28 + 24), d * 64);
    if (drawCount > 0) device.queue.writeBuffer(UG.drawBuffer, 0, slots);
    // 3. One pass: objects, then debug lines on top.
    var encoder = device.createCommandEncoder();
    var pass = encoder.beginRenderPass({
      colorAttachments: [{ view: UG.context.getCurrentTexture().createView(), clearValue: { r: 0.015, g: 0.02, b: 0.04, a: 1 }, loadOp: 'clear', storeOp: 'store' }],
      depthStencilAttachment: { view: UG.depth.createView(), depthClearValue: 1, depthLoadOp: 'clear', depthStoreOp: 'store' } });
    if (drawCount > 0) {
      pass.setPipeline(UG.pipeline);
      for (var d = 0; d < drawCount; ++d) {
        var mesh = UG.meshes[src[d * 28 + 24]];
        if (!mesh) continue;
        var texture = src[d * 28 + 25];
        pass.setBindGroup(0, UG.frameGroup, [d * 256]);
        pass.setBindGroup(1, (texture > 0 && UG.groups[texture]) || UG.whiteGroup);
        pass.setVertexBuffer(0, mesh.vertices);
        pass.setIndexBuffer(mesh.indices, 'uint32');
        pass.drawIndexed(mesh.count);
      }
    }
    var vb = null;
    if (lineCount > 0) {
      var raw = HEAPF32.subarray(lines >> 2, (lines >> 2) + lineCount * 10), verts = new Float32Array(lineCount * 14);
      for (var l = 0; l < lineCount; ++l) {
        verts.set(raw.subarray(l * 10, l * 10 + 3), l * 14); verts.set(raw.subarray(l * 10 + 6, l * 10 + 10), l * 14 + 3);
        verts.set(raw.subarray(l * 10 + 3, l * 10 + 6), l * 14 + 7); verts.set(raw.subarray(l * 10 + 6, l * 10 + 10), l * 14 + 10);
      }
      vb = UG.buffer(GPUBufferUsage.VERTEX, verts);
      pass.setPipeline(UG.linePipeline);
      pass.setBindGroup(0, UG.frameGroup, [0]);
      pass.setVertexBuffer(0, vb);
      pass.draw(lineCount * 2);
    }
    pass.end();
    device.queue.submit([encoder.finish()]);
    if (vb) vb.destroy(); // freed once the submitted work is done
  },

  umbra_wait_frame__async: true,
  umbra_wait_frame: function () {
    return Asyncify.handleAsync(function () { return new Promise(function (resolve) { requestAnimationFrame(function () { resolve(); }); }); });
  },
  umbra_gpu_width: function () { return Module.canvas.width; },
  umbra_gpu_height: function () { return Module.canvas.height; },
  umbra_gpu_name__deps: ['$UG', '$stringToNewUTF8'],
  umbra_gpu_name: function () { if (!UG.nameUtf8) UG.nameUtf8 = stringToNewUTF8(UG.name); return UG.nameUtf8; },
};
autoAddDeps(UmbraRender, '$UG');
mergeInto(LibraryManager.library, UmbraRender);
