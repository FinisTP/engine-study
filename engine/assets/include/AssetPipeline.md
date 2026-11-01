# Asset pipeline: source → derived model

```text
assets/models/tower.gltf  (+ tower.bin)        source, human-editable, in git
        │  BuildModel()  (GltfImporter + ModelCache)
        ▼
<build>/asset-cache/models/tower.gltf.umodel   derived, binary, never in git
        │  ReadModelFile()
        ▼
assets::Model (CPU)  →  Renderer::CreateMesh / CreateTexture  →  GPU handles
```

The source stays in glTF. Tools and AI edits work on readable files. The game loads the compact derived file, which needs no JSON parsing or image decoding.

## What the importer keeps

| glTF | Umbra |
|---|---|
| Node hierarchy, TRS or matrix | `ModelNode` with parent index; matrices are split into T, R, S (`glm::decompose`). Parents are reordered to come before children. |
| Mesh primitive (triangles) | `ModelMesh`: positions (required), normals, `TEXCOORD_0`, indices (or generated 0..n-1), base color factor, base color texture |
| Image (8-bit, 1–4 channels) | `core::TextureData`, expanded to RGBA |

**Warnings** (skipped, the model still loads): animations, skins, morph targets, non-triangle primitives, missing normals, `texCoord` other than 0.

**Errors** (the build fails and the old cache file is kept): unreadable or malformed JSON, `extensionsRequired`, sparse accessors, wrong accessor types, any accessor that reaches past its buffer view, external paths that are absolute or contain `..`, node cycles or shared children, meshes that fail `core::ValidateMesh`.

## When does a model rebuild?

```text
hash = FNV1a( formatVersion, importerVersion,
              bytes(source),
              for each dependency d: name(d), bytes(d) )
```

- **Dependencies** are the external files the glTF references (`.bin` buffers, image files). `data:` URIs are inside the source, so they're covered by the source bytes.
- The cached file stores the hash and the dependency list. `BuildModel` re-hashes the source plus *those* dependencies. A match means up to date, so nothing is parsed.
- If the source gains a new dependency, the source bytes changed too, so the hash differs anyway.
- Bumping `modelFormatVersion` or `importerVersion` changes every hash, so every model rebuilds.
- Import settings don't exist yet. When they do, they go into the hash too.

FNV-1a is used because it's tiny and fast. It is **not** cryptographic. It detects changes; it doesn't defend against someone crafting a collision.

## The `.umodel` format (version 1)

All values are little-endian. Every supported target (x64, ARM64, WebAssembly) is little-endian.

```text
"UMDL"  u32 formatVersion  u32 importerVersion  u64 sourceHash
u32 dependencyCount   { u32 length, bytes }*
u32 nodeCount         { string name, i32 parent, vec3 position, quat rotation, vec3 scale, u32 n, u32 mesh[n] }*
u32 meshCount         { vec4 baseColor, i32 texture, u32 v, Vertex[v], u32 i, u32 index[i] }*
u32 textureCount      { u32 width, u32 height, u8 rgba[width*height*4] }*
u64 checksum          FNV1a of every byte before it
```

Reading is defensive. Every count is checked against the bytes actually left before anything is allocated, so a corrupt file can't trigger a huge allocation or an out-of-bounds read. The checksum catches partial writes and bit flips.

Writing is atomic: write `file.umodel.tmp`, then rename it over `file.umodel`. A crash leaves either the old file or the new one. The same bytes come out for the same input (no timestamps), so repeated builds are byte-identical.

## Asset paths

Models are referenced by paths relative to the asset root (`"models/tower.gltf"`). `IsSafeAssetPath` rejects absolute paths and `..`, so content can't reach files outside the asset folder.
