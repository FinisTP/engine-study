// WebGPU side of the browser renderer (RendererWeb.cpp, roadmap P10). Linked with --js-library.
// Scene.wgsl below is the hand port of Scene.hlsl (ADR 0001): per-draw data comes from a uniform
// buffer slot with a dynamic offset instead of push constants, and there is no Y flip.
// Colors are linear: textures are sRGB and the canvas is viewed as sRGB, like the Vulkan swapchain.
var UmbraRender = {
  $UG: {
    device: null, context: null, format: null, depth: null, name: 'WebGPU',
    meshes: [null], textures: [null], groups: [null], surfaces: [null],
    shader: `
// Must match Frame in RendererWeb.cpp (= core::LightingBlock after the matrices). Math: Lighting.md.
struct Frame {
  viewProjection: mat4x4f, shadowMatrix: mat4x4f, inverseViewProjection: mat4x4f,
  cameraPosition: vec4f, sunDirection: vec4f, sunColor: vec4f, skyColor: vec4f,
  horizonColor: vec4f, groundColor: vec4f, fogColor: vec4f, counts: vec4f,
  lightPositions: array<vec4f, 16>, lightColors: array<vec4f, 16>
};
struct Draw { model: mat4x4f, color: vec4f, texturing: vec4f }; // texturing: x = meters per repeat, y = emissive
@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(1) var<uniform> draw: Draw;
@group(0) @binding(2) var shadowMap: texture_depth_2d;
@group(0) @binding(3) var shadowSampler: sampler_comparison;
@group(1) @binding(0) var baseColorSampler: sampler;
@group(1) @binding(1) var baseColorTexture: texture_2d<f32>;
// The same locations as Scene.hlsl's VertexOutput, so converted custom surface shaders fit this vertex shader.
struct Out { @builtin(position) position: vec4f, @location(0) normal: vec3f, @location(1) uv: vec2f, @location(2) color: vec4f,
             @location(3) world: vec3f, @location(4) emissive: f32 };
@vertex fn vs(@location(0) position: vec3f, @location(1) normal: vec3f, @location(2) uv: vec2f) -> Out {
  var o: Out;
  let world = draw.model * vec4f(position, 1.0);
  o.position = frame.viewProjection * world;
  o.normal = normalize((draw.model * vec4f(normal, 0.0)).xyz);
  o.uv = uv;
  o.color = draw.color;
  o.world = world.xyz;
  o.emissive = draw.texturing.y;
  if (draw.texturing.x > 0.0) { // world-space texture coordinates (WorldUv.md)
    let n = abs(o.normal);
    let p = world.xyz;
    var planar = vec2f(p.x, -p.y);
    if (n.x >= n.y && n.x >= n.z) { planar = vec2f(p.z, -p.y); } else if (n.y >= n.z) { planar = vec2f(p.x, p.z); }
    o.uv = planar / draw.texturing.x;
  }
  return o;
}
// How much sun reaches this point (1 = lit). WebGPU: clip y = +1 is the map's first row.
fn sunShadow(p: vec3f, n: vec3f) -> f32 {
  let s = frame.shadowMatrix * vec4f(p + n * 0.06, 1.0);
  let uv = vec2f(0.5 + 0.5 * s.x, 0.5 - 0.5 * s.y);
  if (any(uv < vec2f(0.0)) || any(uv > vec2f(1.0)) || s.z > 1.0) { return 1.0; }
  var lit = 0.0;
  for (var y = -1; y <= 1; y++) {
    for (var x = -1; x <= 1; x++) {
      lit += textureSampleCompareLevel(shadowMap, shadowSampler, uv + vec2f(f32(x), f32(y)) * frame.counts.y, s.z - 0.0015);
    }
  }
  return lit / 9.0;
}
@fragment fn fs(i: Out) -> @location(0) vec4f {
  let albedo = textureSample(baseColorTexture, baseColorSampler, i.uv) * i.color;
  let n = normalize(i.normal);
  // 1. Sky light from above, ground light from below.
  let sky = mix(frame.horizonColor.rgb, frame.skyColor.rgb, 0.5);
  var light = mix(frame.groundColor.rgb, sky, n.y * 0.5 + 0.5) * frame.skyColor.w;
  // 2. The sun (or moon), unless something stands in the way.
  let sunAmount = max(dot(n, frame.sunDirection.xyz), 0.0);
  if (sunAmount > 0.0) {
    var shadow = 1.0;
    if (frame.sunDirection.w > 0.0) { shadow = sunShadow(i.world, n); }
    light += frame.sunColor.rgb * sunAmount * shadow;
  }
  // 3. Point lights: softly to nothing at their range.
  for (var l = 0; l < i32(frame.counts.x); l++) {
    let toLight = frame.lightPositions[l].xyz - i.world;
    let d = length(toLight);
    let x = d / frame.lightPositions[l].w;
    if (x >= 1.0) { continue; }
    let window = saturate(1.0 - x * x * x * x);
    light += frame.lightColors[l].rgb * (window * window / (1.0 + d * d)) * max(dot(n, toLight / max(d, 0.0001)), 0.0);
  }
  // 4. Its own glow, then fog.
  let color = albedo.rgb * (light + i.emissive);
  let fog = 1.0 - exp(-frame.fogColor.w * length(i.world - frame.cameraPosition.xyz));
  return vec4f(mix(color, frame.fogColor.rgb, fog), albedo.a);
}
// Sun shadow map: depth only, seen from the sun.
@vertex fn shadowVs(@location(0) position: vec3f) -> @builtin(position) vec4f {
  return frame.shadowMatrix * draw.model * vec4f(position, 1.0);
}
// Sky: one triangle covering the screen, drawn first.
struct SkyOut { @builtin(position) position: vec4f, @location(0) ndc: vec2f };
@vertex fn skyVs(@builtin(vertex_index) id: u32) -> SkyOut {
  var o: SkyOut;
  let ndc = vec2f(f32((id << 1u) & 2u), f32(id & 2u)) * 2.0 - 1.0;
  o.position = vec4f(ndc, 1.0, 1.0);
  o.ndc = ndc;
  return o;
}
@fragment fn skyFs(i: SkyOut) -> @location(0) vec4f {
  let far = frame.inverseViewProjection * vec4f(i.ndc, 1.0, 1.0);
  let ray = normalize(far.xyz / far.w - frame.cameraPosition.xyz);
  var color = mix(frame.horizonColor.rgb, frame.groundColor.rgb, saturate(-ray.y * 4.0));
  if (ray.y >= 0.0) { color = mix(frame.horizonColor.rgb, frame.skyColor.rgb, sqrt(ray.y)); }
  let toward = max(dot(ray, frame.sunDirection.xyz), 0.0);
  color += frame.sunColor.rgb * (pow(toward, 800.0) * 6.0 + pow(toward, 12.0) * 0.25);
  return vec4f(mix(color, frame.fogColor.rgb, saturate(frame.fogColor.w * 25.0)), 1.0);
}
struct LineOut { @builtin(position) position: vec4f, @location(0) color: vec4f };
@vertex fn lineVs(@location(0) position: vec3f, @location(1) color: vec4f) -> LineOut {
  var o: LineOut;
  o.position = frame.viewProjection * vec4f(position, 1.0);
  o.color = color;
  return o;
}
@fragment fn lineFs(i: LineOut) -> @location(0) vec4f { return i.color; }`,

    // Post.wgsl: the hand port of Post.hlsl (keep the two in step). uv 0 = top row in both.
    postShader: `
struct Post { exposure: vec4f, sizes: vec4f };
@group(0) @binding(0) var<uniform> post: Post;
@group(0) @binding(1) var clampSampler: sampler;
@group(0) @binding(2) var source: texture_2d<f32>;
@group(0) @binding(3) var bloom: texture_2d<f32>;
@group(0) @binding(4) var lut: texture_3d<f32>;
struct Out { @builtin(position) position: vec4f, @location(0) uv: vec2f };
@vertex fn fullscreenVs(@builtin(vertex_index) id: u32) -> Out {
  var o: Out;
  let uv = vec2f(f32((id << 1u) & 2u), f32(id & 2u));
  o.position = vec4f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
  o.uv = uv;
  return o;
}
fn box(uv: vec2f) -> vec3f {
  let d = post.sizes.xy * 0.5;
  return (textureSample(source, clampSampler, uv + vec2f(-d.x, -d.y)).rgb + textureSample(source, clampSampler, uv + vec2f(d.x, -d.y)).rgb +
          textureSample(source, clampSampler, uv + vec2f(-d.x, d.y)).rgb + textureSample(source, clampSampler, uv + vec2f(d.x, d.y)).rgb) * 0.25;
}
@fragment fn brightFs(i: Out) -> @location(0) vec4f { return vec4f(max(box(i.uv) * post.exposure.x - post.exposure.z, vec3f(0.0)), 1.0); }
@fragment fn downFs(i: Out) -> @location(0) vec4f { return vec4f(box(i.uv), 1.0); }
@fragment fn upFs(i: Out) -> @location(0) vec4f {
  let d = post.sizes.xy;
  var c = textureSample(source, clampSampler, i.uv).rgb * 4.0;
  c += (textureSample(source, clampSampler, i.uv + vec2f(d.x, 0.0)).rgb + textureSample(source, clampSampler, i.uv - vec2f(d.x, 0.0)).rgb +
        textureSample(source, clampSampler, i.uv + vec2f(0.0, d.y)).rgb + textureSample(source, clampSampler, i.uv - vec2f(0.0, d.y)).rgb) * 2.0;
  c += textureSample(source, clampSampler, i.uv + d).rgb + textureSample(source, clampSampler, i.uv - d).rgb +
       textureSample(source, clampSampler, i.uv + vec2f(d.x, -d.y)).rgb + textureSample(source, clampSampler, i.uv + vec2f(-d.x, d.y)).rgb;
  return vec4f(c / 16.0, 1.0);
}
fn tonemap(h: vec3f) -> vec3f {
  let x = max(h, vec3f(0.0)) * 0.8;
  return saturate(x * (2.51 * x + 0.03) / (x * (2.43 * x + 0.59) + 0.14));
}
fn toSrgb(c: vec3f) -> vec3f { let s = saturate(c); return select(1.055 * pow(s, vec3f(1.0 / 2.4)) - 0.055, s * 12.92, s <= vec3f(0.0031308)); }
fn toLinear(c: vec3f) -> vec3f { return select(pow((c + 0.055) / 1.055, vec3f(2.4)), c / 12.92, c <= vec3f(0.04045)); }
@fragment fn compositeFs(i: Out) -> @location(0) vec4f {
  let hdr = textureSample(source, clampSampler, i.uv).rgb * post.exposure.x + textureSample(bloom, clampSampler, i.uv).rgb * post.exposure.y;
  let n = post.sizes.z;
  let cell = toSrgb(tonemap(hdr)) * ((n - 1.0) / n) + 0.5 / n;
  var color = toLinear(textureSampleLevel(lut, clampSampler, cell, 0.0).rgb);
  let d = length(i.uv - vec2f(0.5)) * 1.41421356;
  color *= 1.0 - post.exposure.w * d * d;
  return vec4f(color, 1.0);
}`,

    uiShader: `
struct Ui { size: vec2f };
@group(0) @binding(0) var<uniform> ui: Ui;
@group(1) @binding(0) var uiSampler: sampler;
@group(1) @binding(1) var uiTexture: texture_2d<f32>;
struct Out { @builtin(position) position: vec4f, @location(0) uv: vec2f, @location(1) color: vec4f };
@vertex fn vs(@location(0) p: vec2f, @location(1) uv: vec2f, @location(2) color: vec4f) -> Out {
  var o: Out;
  o.position = vec4f(p.x / ui.size.x * 2.0 - 1.0, 1.0 - p.y / ui.size.y * 2.0, 0.0, 1.0);
  o.uv = uv;
  o.color = color;
  return o;
}
@fragment fn fs(i: Out) -> @location(0) vec4f { return i.color * textureSample(uiTexture, uiSampler, i.uv); }`,

    // HDR picture and bloom levels for a canvas of w x h, with one bind group per post pass.
    // Pass slots in postBuffer (256 bytes each): 0..5 down, 6..10 up, 11 composite.
    makePostTargets: function (w, h) {
      var device = UG.device;
      (UG.postTextures || []).forEach(function (t) { t.destroy(); });
      var make = function (tw, th) { return device.createTexture({ size: [Math.max(1, tw), Math.max(1, th)], format: 'rgba16float',
        usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING }); };
      UG.hdr = make(w, h);
      UG.hdrView = UG.hdr.createView();
      UG.levels = [];
      for (var i = 0; i < 6; ++i) UG.levels.push(make(Math.floor(w / (2 << i)), Math.floor(h / (2 << i))));
      UG.postTextures = [UG.hdr].concat(UG.levels);
      var group = function (slot, source, bloom) {
        return device.createBindGroup({ layout: UG.postLayout, entries: [
          { binding: 0, resource: { buffer: UG.postBuffer, offset: slot * 256, size: 32 } }, { binding: 1, resource: UG.clampSampler },
          { binding: 2, resource: source.createView() }, { binding: 3, resource: bloom.createView() }, { binding: 4, resource: UG.lut.createView({ dimension: '3d' }) }] });
      };
      UG.downGroups = [], UG.upGroups = [];
      // (a pass may not sample the picture it draws into, so unused bloom slots repeat the source)
      for (var i = 0; i < 6; ++i) { var from = i === 0 ? UG.hdr : UG.levels[i - 1]; UG.downGroups.push(group(i, from, from)); }
      for (var i = 0; i < 5; ++i) UG.upGroups.push(group(6 + i, UG.levels[i + 1], UG.levels[i + 1]));
      UG.compositeGroup = group(11, UG.hdr, UG.levels[0]);
    },
    // Writes each pass's constants (exposure block + the source's texel size) and records bloom.
    postPasses: function (encoder, post) {
      var p = HEAPF32.subarray(post >> 2, (post >> 2) + 4), data = new Float32Array(12 * 64);
      var size = function (t) { return [1 / t.width, 1 / t.height]; };
      var put = function (slot, source) { data.set(p, slot * 64); data.set(size(source), slot * 64 + 4); data[slot * 64 + 6] = 16; };
      for (var i = 0; i < 6; ++i) put(i, i === 0 ? UG.hdr : UG.levels[i - 1]);
      for (var i = 0; i < 5; ++i) put(6 + i, UG.levels[i + 1]);
      put(11, UG.hdr);
      UG.device.queue.writeBuffer(UG.postBuffer, 0, data);
      var run = function (pipeline, group, target, load) {
        var pass = encoder.beginRenderPass({ colorAttachments: [{ view: target.createView(), loadOp: load ? 'load' : 'clear',
          clearValue: { r: 0, g: 0, b: 0, a: 1 }, storeOp: 'store' }] });
        pass.setPipeline(pipeline); pass.setBindGroup(0, group); pass.draw(3); pass.end();
      };
      for (var i = 0; i < 6; ++i) run(i === 0 ? UG.brightPipeline : UG.downPipeline, UG.downGroups[i], UG.levels[i], false);
      for (var i = 4; i >= 0; --i) run(UG.upPipeline, UG.upGroups[i], UG.levels[i], true);
    },

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
      var canvasFormat = navigator.gpu.getPreferredCanvasFormat();
      UG.format = canvasFormat + '-srgb'; // we draw through an sRGB view of the canvas
      UG.context = Module.canvas.getContext('webgpu');
      UG.context.configure({ device: device, format: canvasFormat, viewFormats: [UG.format], alphaMode: 'opaque' });
      var module = device.createShaderModule({ code: UG.shader });
      var vertexLayout = { arrayStride: 32, attributes: [
        { shaderLocation: 0, offset: 0, format: 'float32x3' }, { shaderLocation: 1, offset: 12, format: 'float32x3' },
        { shaderLocation: 2, offset: 24, format: 'float32x2' }] };
      // Group 0: frame, per-draw slot, shadow map. The shadow pass gets a copy without the map (it draws into it).
      var frameEntries = [
        { binding: 0, visibility: GPUShaderStage.VERTEX | GPUShaderStage.FRAGMENT, buffer: { type: 'uniform' } },
        { binding: 1, visibility: GPUShaderStage.VERTEX | GPUShaderStage.FRAGMENT, buffer: { type: 'uniform', hasDynamicOffset: true } }];
      var shadowLayout = device.createBindGroupLayout({ entries: frameEntries });
      var frameLayout = device.createBindGroupLayout({ entries: frameEntries.concat([
        { binding: 2, visibility: GPUShaderStage.FRAGMENT, texture: { sampleType: 'depth' } },
        { binding: 3, visibility: GPUShaderStage.FRAGMENT, sampler: { type: 'comparison' } }]) });
      var textureLayout = device.createBindGroupLayout({ entries: [
        { binding: 0, visibility: GPUShaderStage.FRAGMENT, sampler: {} },
        { binding: 1, visibility: GPUShaderStage.FRAGMENT, texture: {} }] });
      // The lit scene; a custom surface shader (umbra_gpu_surface) swaps in its own fragment module.
      UG.sceneDesc = function (fragmentModule, entry) {
        return {
          layout: device.createPipelineLayout({ bindGroupLayouts: [frameLayout, textureLayout] }),
          vertex: { module: module, entryPoint: 'vs', buffers: [vertexLayout] },
          fragment: { module: fragmentModule, entryPoint: entry, targets: [{ format: 'rgba16float', blend: {
            color: { srcFactor: 'src-alpha', dstFactor: 'one-minus-src-alpha' }, alpha: { srcFactor: 'one', dstFactor: 'one-minus-src-alpha' } } }] },
          primitive: { topology: 'triangle-list', cullMode: 'none' },
          depthStencil: { format: 'depth24plus', depthWriteEnabled: true, depthCompare: 'less' } };
      };
      UG.pipeline = device.createRenderPipeline(UG.sceneDesc(module, 'fs'));
      UG.skyPipeline = device.createRenderPipeline({
        layout: device.createPipelineLayout({ bindGroupLayouts: [frameLayout] }),
        vertex: { module: module, entryPoint: 'skyVs' },
        fragment: { module: module, entryPoint: 'skyFs', targets: [{ format: 'rgba16float' }] },
        primitive: { topology: 'triangle-list' },
        depthStencil: { format: 'depth24plus', depthWriteEnabled: false, depthCompare: 'always' } });
      UG.shadowPipeline = device.createRenderPipeline({
        layout: device.createPipelineLayout({ bindGroupLayouts: [shadowLayout] }),
        vertex: { module: module, entryPoint: 'shadowVs', buffers: [vertexLayout] },
        primitive: { topology: 'triangle-list', cullMode: 'none' },
        depthStencil: { format: 'depth32float', depthWriteEnabled: true, depthCompare: 'less', depthBias: 2, depthBiasSlopeScale: 1.75 } });
      // The sun's shadow map (RendererWeb.cpp's shadowMapSize) and its hardware comparison.
      UG.shadowMap = device.createTexture({ size: [2048, 2048], format: 'depth32float',
        usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING });
      UG.shadowView = UG.shadowMap.createView();
      UG.shadowSampler = device.createSampler({ compare: 'less-equal', magFilter: 'linear', minFilter: 'linear' });
      UG.shadowLayout = shadowLayout;
      UG.linePipeline = device.createRenderPipeline({
        layout: device.createPipelineLayout({ bindGroupLayouts: [frameLayout] }),
        vertex: { module: module, entryPoint: 'lineVs', buffers: [{ arrayStride: 28, attributes: [
          { shaderLocation: 0, offset: 0, format: 'float32x3' }, { shaderLocation: 1, offset: 12, format: 'float32x4' }] }] },
        fragment: { module: module, entryPoint: 'lineFs', targets: [{ format: UG.format }] },
        primitive: { topology: 'line-list' } }); // drawn on the finished picture, over everything
      // The game UI: 2D triangles in pixels, textured and tinted, no depth (core::UiDrawList).
      var uiModule = device.createShaderModule({ code: UG.uiShader });
      UG.uiLayout = device.createBindGroupLayout({ entries: [{ binding: 0, visibility: GPUShaderStage.VERTEX, buffer: { type: 'uniform' } }] });
      UG.uiPipeline = device.createRenderPipeline({
        layout: device.createPipelineLayout({ bindGroupLayouts: [UG.uiLayout, textureLayout] }),
        vertex: { module: uiModule, entryPoint: 'vs', buffers: [{ arrayStride: 20, attributes: [
          { shaderLocation: 0, offset: 0, format: 'float32x2' }, { shaderLocation: 1, offset: 8, format: 'float32x2' },
          { shaderLocation: 2, offset: 16, format: 'unorm8x4' }] }] },
        fragment: { module: uiModule, entryPoint: 'fs', targets: [{ format: UG.format, blend: {
          color: { srcFactor: 'src-alpha', dstFactor: 'one-minus-src-alpha' }, alpha: { srcFactor: 'one', dstFactor: 'one-minus-src-alpha' } } }] },
        primitive: { topology: 'triangle-list', cullMode: 'none' } });
      // Post-processing (Post.md): bloom chain and the composite onto the canvas.
      var postModule = device.createShaderModule({ code: UG.postShader });
      UG.postLayout = device.createBindGroupLayout({ entries: [
        { binding: 0, visibility: GPUShaderStage.FRAGMENT, buffer: { type: 'uniform' } },
        { binding: 1, visibility: GPUShaderStage.FRAGMENT, sampler: {} },
        { binding: 2, visibility: GPUShaderStage.FRAGMENT, texture: {} },
        { binding: 3, visibility: GPUShaderStage.FRAGMENT, texture: {} },
        { binding: 4, visibility: GPUShaderStage.FRAGMENT, texture: { viewDimension: '3d' } }] });
      var postPipeline = function (entry, format, add) {
        var target = { format: format };
        if (add) target.blend = { color: { srcFactor: 'one', dstFactor: 'one' }, alpha: { srcFactor: 'one', dstFactor: 'one' } };
        return device.createRenderPipeline({ layout: device.createPipelineLayout({ bindGroupLayouts: [UG.postLayout] }),
          vertex: { module: postModule, entryPoint: 'fullscreenVs' }, fragment: { module: postModule, entryPoint: entry, targets: [target] },
          primitive: { topology: 'triangle-list' } });
      };
      UG.brightPipeline = postPipeline('brightFs', 'rgba16float', false);
      UG.downPipeline = postPipeline('downFs', 'rgba16float', false);
      UG.upPipeline = postPipeline('upFs', 'rgba16float', true);
      UG.compositePipeline = postPipeline('compositeFs', UG.format, false);
      UG.clampSampler = device.createSampler({ magFilter: 'linear', minFilter: 'linear' });
      UG.lut = device.createTexture({ size: [16, 16, 16], dimension: '3d', format: 'rgba8unorm', usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST });
      UG.postBuffer = device.createBuffer({ size: 16 * 256, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST });
      UG.uiBuffer = device.createBuffer({ size: 16, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST });
      UG.uiGroup = device.createBindGroup({ layout: UG.uiLayout, entries: [{ binding: 0, resource: { buffer: UG.uiBuffer } }] });
      UG.frameLayout = frameLayout;
      UG.frameBuffer = device.createBuffer({ size: 208 * 4, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST });
      UG.drawCapacity = 0;
      UG.sampler = device.createSampler({ magFilter: 'linear', minFilter: 'linear', mipmapFilter: 'linear', addressModeU: 'repeat', addressModeV: 'repeat' });
      // Fallback texture: one white pixel (like the Vulkan renderer's).
      var white = device.createTexture({ size: [1, 1], format: 'rgba8unorm-srgb', usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST });
      device.queue.writeTexture({ texture: white }, new Uint8Array([255, 255, 255, 255]), { bytesPerRow: 4 }, [1, 1]);
      UG.whiteGroup = UG.textureGroup(white);
      return 1;
    });
  },

  // A custom surface shader: WGSL converted at build time from Scene.hlsl + the material's surface file.
  umbra_gpu_surface__deps: ['$UG'],
  umbra_gpu_surface: function (code) {
    var module = UG.device.createShaderModule({ code: UTF8ToString(code) });
    return UG.put(UG.surfaces, UG.device.createRenderPipeline(UG.sceneDesc(module, 'main')));
  },

  // The post color table from its 256x16 strip (x = b * 16 + r, y = g) into the 16^3 texture.
  umbra_gpu_lut__deps: ['$UG'],
  umbra_gpu_lut: function (strip) {
    var src = HEAPU8.subarray(strip, strip + 256 * 16 * 4), out = new Uint8Array(16 * 16 * 16 * 4);
    for (var b = 0; b < 16; ++b) for (var g = 0; g < 16; ++g) out.set(src.subarray((g * 256 + b * 16) * 4, (g * 256 + b * 16 + 16) * 4), ((b * 16 + g) * 16) * 4);
    UG.device.queue.writeTexture({ texture: UG.lut }, out, { bytesPerRow: 64, rowsPerImage: 16 }, [16, 16, 16]);
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
    var t = UG.device.createTexture({ size: [width, height], mipLevelCount: levels, format: 'rgba8unorm-srgb',
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
  umbra_gpu_frame: function (frame, post, draws, drawCount, lines, lineCount, hud) {
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
      UG.makePostTargets(w, h);
    }
    // 2. Per-frame and per-draw uniforms: one 256-byte slot per draw (dynamic offset).
    device.queue.writeBuffer(UG.frameBuffer, 0, HEAPF32.buffer, frame, 208 * 4);
    if (!UG.drawBuffer || drawCount > UG.drawCapacity) {
      if (UG.drawBuffer) UG.drawBuffer.destroy();
      UG.drawCapacity = Math.max(64, drawCount * 2);
      UG.drawBuffer = device.createBuffer({ size: UG.drawCapacity * 256, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST });
      var entries = [{ binding: 0, resource: { buffer: UG.frameBuffer } }, { binding: 1, resource: { buffer: UG.drawBuffer, size: 96 } }];
      UG.shadowGroup = device.createBindGroup({ layout: UG.shadowLayout, entries: entries });
      UG.frameGroup = device.createBindGroup({ layout: UG.frameLayout, entries: entries.concat([
        { binding: 2, resource: UG.shadowView }, { binding: 3, resource: UG.shadowSampler }]) });
    }
    var src = HEAPF32.subarray(draws >> 2, (draws >> 2) + drawCount * 28);
    var slots = new Float32Array(Math.max(drawCount, 1) * 64);
    for (var d = 0; d < drawCount; ++d) slots.set(src.subarray(d * 28, d * 28 + 24), d * 64);
    if (drawCount > 0) device.queue.writeBuffer(UG.drawBuffer, 0, slots);
    // 3. Shadow pass (only when the sun casts shadows: frame float 48 + 7 = sunDirection.w).
    var encoder = device.createCommandEncoder();
    var castsShadow = HEAPF32[(frame >> 2) + 55] > 0;
    var shadowPass = encoder.beginRenderPass({ colorAttachments: [],
      depthStencilAttachment: { view: UG.shadowView, depthClearValue: 1, depthLoadOp: 'clear', depthStoreOp: 'store' } });
    if (castsShadow && drawCount > 0) {
      shadowPass.setPipeline(UG.shadowPipeline);
      for (var d = 0; d < drawCount; ++d) {
        var mesh = UG.meshes[src[d * 28 + 24]];
        if (!mesh) continue;
        shadowPass.setBindGroup(0, UG.shadowGroup, [d * 256]);
        shadowPass.setVertexBuffer(0, mesh.vertices);
        shadowPass.setIndexBuffer(mesh.indices, 'uint32');
        shadowPass.drawIndexed(mesh.count);
      }
    }
    shadowPass.end();
    // 4. Scene pass into the HDR picture: sky, then objects.
    var pass = encoder.beginRenderPass({
      colorAttachments: [{ view: UG.hdrView, clearValue: { r: 0, g: 0, b: 0, a: 1 }, loadOp: 'clear', storeOp: 'store' }],
      depthStencilAttachment: { view: UG.depth.createView(), depthClearValue: 1, depthLoadOp: 'clear', depthStoreOp: 'store' } });
    pass.setPipeline(UG.skyPipeline);
    pass.setBindGroup(0, UG.frameGroup, [0]);
    pass.draw(3);
    if (drawCount > 0) {
      var bound = null;
      for (var d = 0; d < drawCount; ++d) {
        var mesh = UG.meshes[src[d * 28 + 24]];
        if (!mesh) continue;
        var pipeline = UG.surfaces[src[d * 28 + 26]] || UG.pipeline; // custom surface shader, else the plain one
        if (pipeline !== bound) { pass.setPipeline(pipeline); bound = pipeline; }
        var texture = src[d * 28 + 25];
        pass.setBindGroup(0, UG.frameGroup, [d * 256]);
        pass.setBindGroup(1, (texture > 0 && UG.groups[texture]) || UG.whiteGroup);
        pass.setVertexBuffer(0, mesh.vertices);
        pass.setIndexBuffer(mesh.indices, 'uint32');
        pass.drawIndexed(mesh.count);
      }
    }
    pass.end();
    // 5. Bloom down then up, then the composite onto the canvas (Post.md); debug lines on top.
    UG.postPasses(encoder, post);
    pass = encoder.beginRenderPass({ colorAttachments: [{ view: UG.context.getCurrentTexture().createView({ format: UG.format }), loadOp: 'clear',
      clearValue: { r: 0, g: 0, b: 0, a: 1 }, storeOp: 'store' }] });
    pass.setPipeline(UG.compositePipeline);
    pass.setBindGroup(0, UG.compositeGroup);
    pass.draw(3);
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

  umbra_gpu_ui__deps: ['$UG'],
  umbra_gpu_ui: function (vertices, vertexCount, indices, indexCount, batches, batchCount) {
    // A second pass over the frame just drawn (loadOp 'load'), one draw per batch with its clip and texture.
    var device = UG.device, canvas = Module.canvas;
    device.queue.writeBuffer(UG.uiBuffer, 0, new Float32Array([canvas.width, canvas.height, 0, 0]));
    var vb = UG.buffer(GPUBufferUsage.VERTEX, HEAPU8.subarray(vertices, vertices + vertexCount * 20));
    var ib = UG.buffer(GPUBufferUsage.INDEX, HEAPU16.subarray(indices >> 1, (indices >> 1) + indexCount + (indexCount & 1)));
    var b = HEAPF32.subarray(batches >> 2, (batches >> 2) + batchCount * 8);
    var encoder = device.createCommandEncoder();
    var pass = encoder.beginRenderPass({ colorAttachments: [{ view: UG.context.getCurrentTexture().createView({ format: UG.format }), loadOp: 'load', storeOp: 'store' }] });
    pass.setPipeline(UG.uiPipeline);
    pass.setBindGroup(0, UG.uiGroup);
    pass.setVertexBuffer(0, vb);
    pass.setIndexBuffer(ib, 'uint16');
    for (var i = 0; i < batchCount; ++i) {
      var o = i * 8, texture = b[o];
      var x0 = Math.max(0, Math.floor(b[o + 1])), y0 = Math.max(0, Math.floor(b[o + 2]));
      var x1 = Math.min(canvas.width, Math.ceil(b[o + 3])), y1 = Math.min(canvas.height, Math.ceil(b[o + 4]));
      if (x1 <= x0 || y1 <= y0 || b[o + 7] <= 0) continue;
      pass.setScissorRect(x0, y0, x1 - x0, y1 - y0);
      pass.setBindGroup(1, (texture > 0 && UG.groups[texture]) || UG.whiteGroup);
      pass.drawIndexed(b[o + 7], 1, b[o + 6], b[o + 5]);
    }
    pass.end();
    device.queue.submit([encoder.finish()]);
    vb.destroy(); ib.destroy(); // freed once the submitted work is done
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
