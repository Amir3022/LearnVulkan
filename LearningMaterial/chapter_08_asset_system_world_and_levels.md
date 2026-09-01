# Chapter 8 — Asset System, World, and Level Loading
## Turning renderer resources into reusable engine content

> **Where this chapter starts**
>
> This chapter assumes you completed **Chapter 7 — From Vulkan Renderer to Game Engine Architecture**.
>
> You now have an `Engine` that orchestrates systems, a `Renderer` that owns Vulkan, a retained `RenderScene`, stable generational renderer handles, and a small job system. The Chapter 6 GPU-driven renderer still sits underneath `RenderScene`.
>
> We are now going to solve the content side of the engine: persistent asset identity, asset caching, loading, world entities, reusable levels, safe unloading, and a path toward asynchronous streaming.

At the end of VKGuide Chapter 5, loading a GLTF scene directly into a `LoadedGLTF` object is perfectly reasonable.

For a game engine, however, the phrase:

```cpp
loadGltf(this, "assets/structure.glb");
```

contains too many responsibilities at once.

It may mean:

```text
find file
read file
parse GLTF
decode images
create textures
upload meshes
create materials
create scene nodes
spawn render objects
own everything until shutdown
```

That architecture becomes painful as soon as you want:

- the same mesh used by many entities;
- two levels sharing textures;
- a level that can be unloaded;
- background asset loading;
- placeholder assets;
- hot reload;
- skeletal meshes later;
- physics collision assets;
- an editor;
- cooked runtime formats.

The key idea of this chapter is:

> **A source file is not the same thing as an engine asset, and an engine asset is not the same thing as a world entity.**

By the end of the chapter our content flow will look like this:

```text
Source files
  car.glb
  asphalt.png
  track.glb
      |
      v
Asset Registry
  persistent AssetID values
      |
      v
AssetManager
  MeshAsset / TextureAsset / MaterialAsset / LevelAsset
      |
      v
World
  Entity + components
      |
      v
RenderComponent
      |
      v
RenderScene
      |
      v
Chapter 6 GPU scene
```

---

# Table of contents

1. [Asset identity, runtime handles, and entities are different things](#1-asset-identity-runtime-handles-and-entities-are-different-things)
2. [The target architecture](#2-the-target-architecture)
3. [AssetID versus AssetHandle](#3-assetid-versus-assethandle)
4. [Persistent metadata](#4-persistent-metadata)
5. [The Asset Registry](#5-the-asset-registry)
6. [Do not key your whole engine by file paths](#6-do-not-key-your-whole-engine-by-file-paths)
7. [Defining runtime asset types](#7-defining-runtime-asset-types)
8. [Separating CPU assets from GPU resources](#8-separating-cpu-assets-from-gpu-resources)
9. [The AssetManager](#9-the-assetmanager)
10. [Checkpoint A — synchronous cached loading](#10-checkpoint-a--synchronous-cached-loading)
11. [Importing the Chapter 5 GLTF loader behind the asset system](#11-importing-the-chapter-5-gltf-loader-behind-the-asset-system)
12. [Meshes, materials, and textures as independent assets](#12-meshes-materials-and-textures-as-independent-assets)
13. [Subresources inside GLTF files](#13-subresources-inside-gltf-files)
14. [Asset lifetime and residency](#14-asset-lifetime-and-residency)
15. [GPU destruction with frames in flight](#15-gpu-destruction-with-frames-in-flight)
16. [Placeholder resources](#16-placeholder-resources)
17. [Checkpoint B — load the same asset twice without duplicating it](#17-checkpoint-b--load-the-same-asset-twice-without-duplicating-it)
18. [Asynchronous loading architecture](#18-asynchronous-loading-architecture)
19. [CPU jobs versus Vulkan upload work](#19-cpu-jobs-versus-vulkan-upload-work)
20. [Upload requests and completion](#20-upload-requests-and-completion)
21. [Timeline semaphore upgrade](#21-timeline-semaphore-upgrade)
22. [Bindless descriptor stability while streaming](#22-bindless-descriptor-stability-while-streaming)
23. [Do we need a custom cooked format now?](#23-do-we-need-a-custom-cooked-format-now)
24. [Creating the World](#24-creating-the-world)
25. [Entity handles](#25-entity-handles)
26. [Components without committing to a third-party ECS](#26-components-without-committing-to-a-third-party-ecs)
27. [TransformComponent and hierarchy](#27-transformcomponent-and-hierarchy)
28. [RenderComponent](#28-rendercomponent)
29. [Synchronizing World to RenderScene](#29-synchronizing-world-to-renderscene)
30. [LevelAsset: data, not live objects](#30-levelasset-data-not-live-objects)
31. [A simple level file format](#31-a-simple-level-file-format)
32. [Loading a level](#32-loading-a-level)
33. [Unloading a level](#33-unloading-a-level)
34. [Level ownership and persistent entities](#34-level-ownership-and-persistent-entities)
35. [Spawning reusable object descriptions](#35-spawning-reusable-object-descriptions)
36. [Save games are not level assets](#36-save-games-are-not-level-assets)
37. [Checkpoint C — switch between two levels](#37-checkpoint-c--switch-between-two-levels)
38. [Hot reload and file watching](#38-hot-reload-and-file-watching)
39. [File-by-file implementation plan](#39-file-by-file-implementation-plan)
40. [Final architecture](#40-final-architecture)
41. [What Chapter 9 will add](#41-what-chapter-9-will-add)
42. [References](#42-references)

---

# 1. Asset identity, runtime handles, and entities are different things

We will use three kinds of identity.

## Persistent content identity

```cpp
AssetID
```

This answers:

> Which asset is this across editor sessions, renames, level files, and engine restarts?

## Runtime resource identity

```cpp
AssetHandle<MeshAsset>
```

This answers:

> Which currently loaded runtime object contains this asset?

## World identity

```cpp
Entity
```

This answers:

> Which live game object is this instance?

Imagine ten trees all using one mesh:

```text
AssetID(TreeMesh)
       |
       v
one MeshAsset loaded once
       |
       +----------+----------+----------+
       |          |          |          |
       v          v          v          v
   Entity 10  Entity 11  Entity 12 ... Entity 19
```

The mesh asset should not be copied ten times merely because ten entities use it.

---

# 2. The target architecture

A useful end-state is:

```text
                    AssetRegistry
                         |
                         | AssetID -> metadata
                         v
                    AssetManager
                         |
        +----------------+----------------+
        |                |                |
        v                v                v
    MeshAsset       TextureAsset     MaterialAsset
        |                |                |
        +-------- GPU handles ------------+
                         |
                         v
                      Renderer


                    LevelAsset
                         |
                         v
                       World
                         |
                         +--> TransformComponent
                         +--> RenderComponent
                         +--> future AnimatorComponent
                         +--> future RigidBodyComponent
                         |
                         v
                    RenderScene
```

There are two particularly important one-way relationships:

```text
AssetManager -> Renderer
```

because assets may create GPU resources;

and:

```text
World -> RenderScene
```

because game objects publish render state.

The renderer should not load levels, and the asset system should not own live entities.

---

# 3. AssetID versus AssetHandle

Create a persistent ID type:

```cpp
struct AssetID {
    uint64_t value = 0;

    bool is_valid() const { return value != 0; }
    auto operator<=>(const AssetID&) const = default;
};
```

For a learning engine, a random 64-bit value is enough.

Generate it once when the asset enters the registry and save it to metadata.

Do not regenerate it every run.

Runtime handles can reuse the generational handle pattern from Chapter 7:

```cpp
template<typename AssetT>
using AssetHandle = Handle<AssetT>;
```

A registry lookup therefore becomes:

```text
AssetID 0x91A...
      |
      v
AssetRecord
      |
      v
runtime AssetHandle<MeshAsset>
```

The persistent ID can exist even when the asset is not loaded.

The runtime handle cannot.

---

# 4. Persistent metadata

A tiny sidecar file is enough to begin.

For:

```text
assets/vehicles/car.glb
```

store:

```text
assets/vehicles/car.glb.meta
```

Example:

```json
{
    "id": 1044719301775038921,
    "type": "gltf",
    "version": 1
}
```

Why use metadata rather than hashing the file path?

Because this:

```text
vehicles/car.glb
```

may later become:

```text
vehicles/player/sports_car.glb
```

The asset is still conceptually the same asset.

If identity is `hash(path)`, every level that references it breaks when it moves.

With metadata, the registry updates the path while the `AssetID` stays stable.

---

# 5. The Asset Registry

Create metadata that describes content without necessarily loading it.

```cpp
enum class AssetType : uint8_t {
    Unknown,
    Texture,
    Mesh,
    Material,
    Model,
    Level,
    Skeleton,
    Animation
};

struct AssetRecord {
    AssetID id;
    AssetType type = AssetType::Unknown;
    std::filesystem::path sourcePath;
};
```

Registry:

```cpp
class AssetRegistry {
public:
    bool scan(const std::filesystem::path& root);

    const AssetRecord* find(AssetID id) const;
    std::optional<AssetID> find_by_path(const std::filesystem::path& path) const;

private:
    std::unordered_map<uint64_t, AssetRecord> byID;
    std::unordered_map<std::string, AssetID> byPath;
};
```

Scanning can initially be simple:

```text
walk assets/
    |
    +--> if .meta exists: read ID
    |
    +--> if no .meta: create ID + write metadata
```

For a shipping game you normally do not mutate source metadata at runtime. This scanner is a development-time convenience.

---

# 6. Do not key your whole engine by file paths

Paths are useful at the edges:

```cpp
registry.find_by_path("assets/vehicles/car.glb");
```

But level data should prefer:

```json
"mesh": 1044719301775038921
```

over:

```json
"mesh": "../../assets/vehicles/car.glb"
```

Persistent IDs make dependencies explicit and renames survivable.

You can still display paths in tools because the registry maps the ID back to a source path.

---

# 7. Defining runtime asset types

Do not keep `LoadedGLTF` as the universal content type.

We want runtime data with engine meaning.

```cpp
struct MeshAsset {
    AssetID id;
    MeshHandle gpuMesh;
    Bounds bounds;
};

struct TextureAsset {
    AssetID id;
    TextureHandle gpuTexture;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct MaterialAsset {
    AssetID id;
    MaterialHandle gpuMaterial;

    AssetID baseColorTexture;
    AssetID metallicRoughnessTexture;

    glm::vec4 baseColorFactor{1.0f};
};
```

A GLTF **model** can then describe a reusable hierarchy of mesh/material references:

```cpp
struct ModelPrimitive {
    AssetID mesh;
    AssetID material;
};

struct ModelNode {
    std::string name;
    glm::mat4 localTransform{1.0f};
    std::vector<uint32_t> children;
    std::vector<ModelPrimitive> primitives;
};

struct ModelAsset {
    AssetID id;
    std::vector<ModelNode> nodes;
    std::vector<uint32_t> roots;
};
```

This is much more useful to the engine than exposing the fastgltf parse tree everywhere.

---

# 8. Separating CPU assets from GPU resources

A subtle but important distinction:

```text
MeshAsset != VkBuffer
```

`MeshAsset` is the runtime engine representation.

It may own or reference a renderer resource:

```cpp
MeshHandle gpuMesh;
```

This gives you freedom later.

For example, a mesh asset could contain:

```text
CPU collision geometry
LOD descriptions
meshlet metadata
streaming metadata
GPU mesh handle
```

without any gameplay system touching a `VkBuffer`.

Likewise `TextureAsset` can eventually know:

```text
source dimensions
format
mip count
streaming state
GPU TextureHandle
```

while Vulkan details remain inside `Renderer`.

---

# 9. The AssetManager

A first manager can be type-specific internally even if the public API is templated later.

```cpp
enum class AssetState : uint8_t {
    Unloaded,
    Loading,
    WaitingForGPU,
    Ready,
    Failed
};

template<typename T>
struct AssetSlot {
    AssetID id;
    AssetState state = AssetState::Unloaded;
    std::unique_ptr<T> asset;
};
```

Manager:

```cpp
class AssetManager {
public:
    bool init(AssetRegistry& registry, Renderer& renderer, JobSystem& jobs);
    void shutdown();

    AssetHandle<MeshAsset> load_mesh(AssetID id);
    AssetHandle<TextureAsset> load_texture(AssetID id);
    AssetHandle<ModelAsset> load_model(AssetID id);

    MeshAsset* get(AssetHandle<MeshAsset> handle);
    TextureAsset* get(AssetHandle<TextureAsset> handle);

    void update();

private:
    AssetRegistry* registry = nullptr;
    Renderer* renderer = nullptr;
    JobSystem* jobs = nullptr;
};
```

Do not force every asset category through one `void*` container just to make the API look generic.

Strong types are helpful here.

---

# 10. Checkpoint A — synchronous cached loading

Before adding threads, prove caching and ownership.

Pseudo-code:

```cpp
AssetHandle<TextureAsset> AssetManager::load_texture(AssetID id)
{
    if (auto existing = loadedTexturesByID.find(id.value);
        existing != loadedTexturesByID.end()) {
        return existing->second;
    }

    const AssetRecord* record = registry->find(id);
    if (!record)
        return {};

    DecodedImage image = decode_image(record->sourcePath);

    TextureHandle gpu = renderer->create_texture({
        .pixels = image.pixels,
        .width = image.width,
        .height = image.height,
        .format = image.format
    });

    TextureAsset asset{
        .id = id,
        .gpuTexture = gpu,
        .width = image.width,
        .height = image.height
    };

    auto handle = textureSlots.insert(std::move(asset));
    loadedTexturesByID[id.value] = handle;

    return handle;
}
```

Call it twice:

```cpp
auto a = assets.load_texture(textureID);
auto b = assets.load_texture(textureID);
```

Assert:

```cpp
assert(a == b);
```

The same content should not be uploaded twice.

This is the first real asset-system victory.

---

# 11. Importing the Chapter 5 GLTF loader behind the asset system

Do not delete your working fastgltf loader.

Move it behind an importer boundary.

```cpp
struct ImportedModel {
    std::vector<ImportedMesh> meshes;
    std::vector<ImportedTexture> textures;
    std::vector<ImportedMaterial> materials;
    std::vector<ImportedNode> nodes;
};

class GltfImporter {
public:
    ImportedModel import(const std::filesystem::path& path);
};
```

The important change is that `GltfImporter` returns **plain CPU data**.

It should not need to call:

```cpp
VulkanEngine::uploadMesh()
```

or:

```cpp
engine->create_image(...)
```

Instead:

```text
fastgltf
   |
   v
ImportedModel (CPU)
   |
   v
AssetManager
   |
   +--> Renderer creates GPU meshes/textures
   |
   v
ModelAsset
```

This makes async import possible later because parsing can run without touching Vulkan state.

---

# 12. Meshes, materials, and textures as independent assets

One GLTF file may contain:

```text
3 meshes
7 primitives
5 materials
12 images
```

If every load creates all of them as an indivisible `LoadedGLTF`, reuse is difficult.

The asset system should conceptually decompose it:

```text
car.glb
   |
   +--> ModelAsset: car
   +--> MeshAsset: body
   +--> MeshAsset: wheel
   +--> MaterialAsset: paint
   +--> MaterialAsset: tire
   +--> TextureAsset: paint_basecolor
   +--> TextureAsset: paint_mr
```

You do not need separate physical files for every subresource yet.

They can be logical assets generated by the importer.

---

# 13. Subresources inside GLTF files

A practical subresource key is:

```cpp
struct AssetSubresourceKey {
    AssetID container;
    AssetType type;
    uint32_t index;
};
```

For example:

```text
car.glb / mesh 0
car.glb / mesh 1
car.glb / material 0
```

During development, the registry can derive stable child IDs from:

```text
parent AssetID + type + importer-local stable key
```

If you want strong rename/reorder stability inside source files later, store imported subresource metadata explicitly.

For now the important rule is simply:

> World code should not depend on fastgltf indices.

Convert them to engine IDs during import.

---

## Asset dependencies

Assets rarely stand alone.

A material may depend on textures:

```text
MaterialAsset CarPaint
    |
    +--> BaseColor TextureAsset
    +--> MetallicRoughness TextureAsset
```

A model depends on meshes and materials:

```text
ModelAsset Car
    |
    +--> MeshAsset Body
    +--> MeshAsset Wheel
    +--> MaterialAsset Paint
    +--> MaterialAsset Rubber
```

A level depends on models, and Chapter 9 will add collision, skeleton, and animation dependencies.

Track those relationships explicitly in the asset layer rather than discovering them by walking live renderer objects.

A development-time registry record can grow into:

```cpp
struct AssetRecord {
    AssetID id;
    AssetType type;
    std::filesystem::path sourcePath;

    std::vector<AssetID> dependencies;
};
```

For direct source import, dependencies may be discovered when the importer runs. For cooked assets later, store them in the cooked metadata so level loading can request dependencies before instantiation.

This gives you useful future operations:

```text
What assets does this level require?
What depends on this texture?
Can this asset be unloaded?
Which assets need recooking after this source changes?
```

Do not build a complicated graph solver yet. A dependency list plus clear ownership is enough.

## Import versioning and invalidation

Once you cache imported/cooked data, source modification is not the only reason an asset becomes stale.

Your importer itself changes.

Suppose version 1 of your mesh importer stored:

```text
position + normal + UV
```

and version 2 adds:

```text
tangent + mesh LOD metadata
```

The source `.glb` may be unchanged, but the old runtime/cooked asset is no longer valid.

Give importers a version:

```cpp
static constexpr uint32_t GltfImporterVersion = 3;
```

A future cooked record can store:

```text
source fingerprint
importer version
engine asset format version
```

Then cache validity becomes a deterministic question instead of "delete the cache folder and try again."

For Chapter 8 you do not need a disk cache yet, but designing `AssetRecord` and importer output with versioning in mind prevents painful migration later.

## Asset state is a state machine

Asynchronous loading becomes much easier to debug when legal transitions are explicit.

Use something like:

```text
Unloaded
   |
   v
LoadingCPU
   |
   v
WaitingForGPU
   |
   v
Ready
```

Failures may occur from either loading stage:

```text
LoadingCPU ----> Failed
WaitingForGPU -> Failed
```

Unloading later can add:

```text
Ready -> PendingUnload -> Unloaded
```

Avoid booleans such as:

```cpp
bool loaded;
bool uploading;
bool failed;
```

because impossible combinations eventually appear:

```text
loaded=true, uploading=true, failed=true
```

One enum makes invariants much clearer.

---

# 14. Asset lifetime and residency

There are several possible policies.

The simplest engine policy is:

```text
Everything loaded stays resident until level unload or engine shutdown.
```

That is a good starting point.

Do not immediately build automatic LRU eviction, streaming budgets, and reference-count graphs.

Track explicit usage instead.

For example:

```cpp
struct AssetRuntimeInfo {
    uint32_t strongReferences = 0;
    bool pinned = false;
};
```

A level may acquire references when it loads and release them when it unloads.

If reference count reaches zero, you may either:

1. keep the asset cached;
2. mark it unloadable;
3. actually unload it immediately.

For a small engine, option 1 is often best at first.

Correctness before memory-budget cleverness.

---

# 15. GPU destruction with frames in flight

Suppose level A is unloaded immediately after frame N submitted.

The GPU may still be reading its mesh during frame N.

This is invalid:

```text
submit frame using mesh
CPU unload level
vkDestroyBuffer(mesh)
GPU still executing previous frame
```

Use deferred renderer destruction.

```cpp
renderer.destroy_mesh(meshHandle);
```

should logically mean:

```text
remove from future scene usage now
destroy underlying Vulkan memory after GPU-safe retirement
```

A simple approach is to enqueue destruction into the current frame's deletion queue after the resource has been removed from future submissions.

With `FRAME_OVERLAP`, the fence for that frame slot gives you the safe retirement point.

Later a timeline semaphore can provide more precise retirement values.

The asset system should **never call `vkDestroyBuffer()` directly**.

---

# 16. Placeholder resources

Async loading is much easier if every unresolved asset has a valid fallback.

Create at startup:

```text
white texture
checkerboard error texture
flat normal texture
default material
unit cube / error mesh
```

Then an asset can exist in state:

```cpp
AssetState::Loading
```

while the world still has something safe to render.

This avoids special-case code throughout shaders and gameplay.

For textures especially, give each runtime texture a stable bindless slot.

Initially the slot points to the error texture.

When upload completes, update the slot at a descriptor-safe point.

---

# 17. Checkpoint B — load the same asset twice without duplicating it

Build a small test level:

```text
100 entities
all use the same rock mesh
all use the same material
```

Check:

```text
100 RenderHandles
1 MeshAsset
1 MaterialAsset
1 GPU mesh allocation
```

Then destroy fifty entities.

The shared mesh must remain valid for the other fifty.

Then unload the whole level.

No stale render object should remain in the Chapter 6 GPU object table.

---

# 18. Asynchronous loading architecture

Now we can use the Chapter 7 job system.

Do **not** make the whole load function execute on a worker and mutate the renderer.

Split loading into stages:

```text
Request asset
     |
     v
AssetState::Loading
     |
     v
Worker job
  file I/O
  parse
  decode
  CPU conversion
     |
     v
UploadRequest queue
     |
     v
Renderer safe point
  staging
  vkCmdCopy...
     |
     v
GPU completion
     |
     v
AssetState::Ready
```

Each stage has clear ownership.

---

# 19. CPU jobs versus Vulkan upload work

A worker thread can safely do things such as:

```text
read file
parse JSON/GLTF
decode PNG/KTX
repack vertices
compute bounds
build animation arrays
```

The worker should produce immutable upload data:

```cpp
struct PendingMeshUpload {
    AssetID asset;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    Bounds bounds;
};
```

Then push it to a concurrent queue.

The renderer consumes the request.

Why not upload from the worker immediately?

Vulkan can absolutely be used from multiple threads under its external-synchronization rules, but your engine would then need thread-safe ownership for:

- command pools;
- staging allocators;
- descriptor allocation;
- global mesh arenas;
- bindless slots;
- deletion queues;
- queue submission.

That is unnecessary complexity at this stage.

Keep one GPU-resource ownership path first.

---

# 20. Upload requests and completion

Define a renderer-facing request:

```cpp
struct MeshUploadRequest {
    uint64_t ticket = 0;
    std::shared_ptr<const MeshUploadData> data;
};
```

Renderer returns completion information:

```cpp
struct MeshUploadComplete {
    uint64_t ticket = 0;
    MeshHandle mesh;
};
```

Asset manager maps ticket to asset slot:

```text
ticket 42
  -> MeshAsset slot 7
```

Frame flow:

```cpp
assets.update();                 // collect CPU job results
renderer.process_uploads();      // create/copy GPU resources
renderer.render(view);
assets.process_gpu_completions();
```

Initially `process_uploads()` may use the same immediate-submit strategy you already understand.

Once correct, upgrade it.

---

# 21. Timeline semaphore upgrade

Vulkan 1.2 made timeline semaphores core, so they are available on your Vulkan 1.3 baseline.

A more scalable upload flow is:

```text
transfer/upload submission
        |
        | signals timeline value 81
        v
asset records requiredValue = 81

later graphics submission
        |
        | waits as needed
        v
resource becomes safely usable
```

You can keep a monotonically increasing upload value:

```cpp
uint64_t nextUploadValue = 1;
```

Each batch signals a new value.

A pending asset stores:

```cpp
struct PendingGPUAsset {
    uint64_t readyValue;
};
```

When the semaphore counter reaches it, transition the asset to `Ready`.

Do not add a dedicated transfer queue merely because one exists on the GPU.

First implement the same queue with timeline-based completion. Separate queue families introduce ownership transfer rules that should be justified by profiling.

---

# 22. Bindless descriptor stability while streaming

Chapter 6 introduced a bindless texture table.

Streaming adds an important lifetime rule.

Suppose material 17 contains:

```cpp
baseColorTexture = 52;
```

Texture slot 52 should remain **stable** for that texture's runtime lifetime.

Before the real image is ready:

```text
slot 52 -> error texture
```

After upload completion:

```text
slot 52 -> actual car paint texture
```

Do descriptor mutation only at a safe point unless you deliberately configured update-after-bind behavior and understand the lifetime rules.

The simple pattern is:

```text
wait frame fence
    |
    v
current frame resources are safe to mutate
    |
    v
apply pending descriptor updates
```

This keeps streaming deterministic and validation-friendly.

---

# 23. Do we need a custom cooked format now?

VKGuide's old Extra Chapter asset system advocates converting source files into engine-specific runtime files.

The concept is valid, but doing it **now** would combine two large problems:

```text
engine runtime architecture
+
content cooker/toolchain
```

Keep source import for now:

```text
GLTF / PNG / KTX
      |
      v
Importer
      |
      v
engine runtime assets
```

Once your asset interfaces stabilize, add:

```text
Source assets
      |
      v
Cooker executable
      |
      v
.meshbin / .texbin / .levelbin
      |
      v
Runtime loader
```

The important thing Chapter 8 does is make that future transition possible **without changing World or Renderer APIs**.

The importer/backend changes. Asset IDs and runtime asset meaning stay the same.

---

# 24. Creating the World

An asset describes reusable content.

A world describes live instances.

Create:

```cpp
class World {
public:
    Entity create_entity(std::string name = {});
    void destroy_entity(Entity entity);

    void update(float dt);

private:
    // Entity/component storage
};
```

Do not make `World` inherit from the renderer or store Vulkan resources.

The world should be able to exist in a headless server eventually.

That is a useful architectural test even if you never build the server.

---

# 25. Entity handles

Use generational handles again.

```cpp
struct EntityTag {};
using Entity = Handle<EntityTag>;
```

An entity is primarily identity.

Do not immediately put every possible field inside:

```cpp
struct EntityObject {
    Transform transform;
    Mesh mesh;
    PhysicsBody physics;
    Animation animation;
    ...
};
```

Components let different entity types carry only the data they need.

---

# 26. Components without committing to a third-party ECS

We can use a small component store.

```cpp
template<typename T>
class ComponentPool {
public:
    T& add(Entity entity, T component = {});
    void remove(Entity entity);
    T* get(Entity entity);

    template<typename Fn>
    void each(Fn&& fn);
};
```

A simple implementation may keep:

```cpp
std::vector<T> denseComponents;
std::vector<Entity> denseEntities;
std::unordered_map<uint32_t, uint32_t> entityToDense;
```

This already gives decent iteration locality:

```text
Transform 0
Transform 1
Transform 2
Transform 3
...
```

instead of chasing a forest of heap pointers.

Later, if you adopt EnTT or write a more advanced sparse set, the higher-level component definitions can stay largely similar.

---

# 27. TransformComponent and hierarchy

Create a transform representation that stores TRS separately.

```cpp
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    Entity parent{};

    glm::mat4 worldMatrix{1.0f};

    bool localDirty = true;
    bool worldDirty = true;
};
```

Why not store only a matrix?

Because gameplay, animation, interpolation, editor manipulation, and physics naturally work with translation/rotation/scale.

Build local matrix:

```cpp
glm::mat4 local =
    glm::translate(glm::mat4(1.0f), position) *
    glm::mat4_cast(rotation) *
    glm::scale(glm::mat4(1.0f), scale);
```

Then:

```cpp
world = parentWorld * local;
```

When a parent becomes dirty, descendants must also update.

For the first implementation, a recursive hierarchy update is fine.

Later, flattening hierarchy order can make transform evaluation more cache-friendly.

---

# 28. RenderComponent

The world should refer to assets and renderer instances, not Vulkan resources.

```cpp
struct RenderComponent {
    AssetID modelAsset;

    std::vector<RenderHandle> renderInstances;

    bool visible = true;
};
```

Why a vector?

One model asset may instantiate multiple mesh primitives/materials.

When a model becomes ready:

```text
Entity RenderComponent
      |
      v
ModelAsset
      |
      +--> primitive 0 -> RenderHandle
      +--> primitive 1 -> RenderHandle
      +--> primitive 2 -> RenderHandle
```

Every render instance receives the entity's world matrix multiplied by the model-node transform.

---

# 29. Synchronizing World to RenderScene

Do not let `Renderer::render()` iterate all world components.

Instead make synchronization explicit.

```cpp
void World::sync_render_state(Renderer& renderer)
{
    for (Entity e : dirtyRenderTransforms) {
        TransformComponent* transform = transforms.get(e);
        RenderComponent* render = renderComponents.get(e);

        if (!transform || !render)
            continue;

        for (RenderHandle handle : render->renderInstances) {
            renderer.set_transform(handle, transform->worldMatrix);
        }
    }

    dirtyRenderTransforms.clear();
}
```

The real implementation may need per-model-node local transforms, but the principle is the same.

This is where Chapter 9 physics will connect:

```text
Jolt updates TransformComponent
        |
        v
world marks transform dirty
        |
        v
sync_render_state()
        |
        v
RenderScene dirty update
```

Physics never needs a renderer pointer.

---

# 30. LevelAsset: data, not live objects

A `LevelAsset` should describe what to create.

It should not contain live `Entity` handles because entity handles are valid only inside a particular runtime `World`.

```cpp
struct LevelEntityDesc {
    uint64_t localID = 0;
    std::string name;

    glm::vec3 position{0.0f};
    glm::quat rotation{1,0,0,0};
    glm::vec3 scale{1.0f};

    AssetID model;
};

struct LevelAsset {
    AssetID id;
    std::vector<LevelEntityDesc> entities;
};
```

Later the descriptor can add:

```text
rigid body settings
lights
scripts
audio emitters
spawn points
AI data
```

without changing renderer code.

---

# 31. A simple level file format

Use JSON while developing.

It is readable, debuggable, and easy to diff.

```json
{
    "version": 1,
    "entities": [
        {
            "id": 1,
            "name": "Track",
            "transform": {
                "position": [0, 0, 0],
                "rotation": [0, 0, 0, 1],
                "scale": [1, 1, 1]
            },
            "model": 52319528701231
        },
        {
            "id": 2,
            "name": "PlayerCar",
            "transform": {
                "position": [0, 0, 5],
                "rotation": [0, 0, 0, 1],
                "scale": [1, 1, 1]
            },
            "model": 91234077814490
        }
    ]
}
```

Do not store renderer handles in this file.

They are runtime implementation details.

---

# 32. Loading a level

The level-load sequence should be explicit.

```text
Load LevelAsset
      |
      v
Resolve asset dependencies
      |
      v
Create world entities
      |
      v
Add TransformComponent
      |
      v
Add RenderComponent
      |
      v
Request ModelAsset
      |
      +--> if ready: instantiate render objects
      |
      +--> if loading: keep placeholder / pending component
```

Example:

```cpp
LevelInstance World::instantiate_level(const LevelAsset& level)
{
    LevelInstance instance;
    instance.asset = level.id;

    for (const LevelEntityDesc& desc : level.entities) {
        Entity e = create_entity(desc.name);

        auto& t = transforms.add(e);
        t.position = desc.position;
        t.rotation = desc.rotation;
        t.scale = desc.scale;

        auto& r = renderComponents.add(e);
        r.modelAsset = desc.model;

        instance.entities.push_back(e);
    }

    return instance;
}
```

Asset readiness can be resolved in the world's update stage.

---

# 33. Unloading a level

Keep a runtime instance:

```cpp
struct LevelInstance {
    AssetID asset;
    std::vector<Entity> entities;
};
```

Unload:

```cpp
void World::unload_level(LevelInstance& level)
{
    for (Entity e : level.entities)
        destroy_entity(e);

    level.entities.clear();
}
```

Entity destruction must cascade to components.

For a render component:

```cpp
for (RenderHandle handle : render.renderInstances)
    renderer.destroy_render_object(handle);
```

The renderer then safely retires GPU scene entries according to frame lifetime rules.

Asset references owned by the level are released afterward.

---

# 34. Level ownership and persistent entities

Soon you will have objects that should survive a level transition:

```text
player profile
engine camera
UI state
network session
possibly player entity
```

Do not put everything into the level's entity list.

A useful split is:

```text
World
   |
   +--> persistent entities
   |
   +--> LevelInstance A entities
```

Then loading a new level can destroy only level-owned entities.

For your future racing game this could let the player/game session survive while changing tracks.

---

# 35. Spawning reusable object descriptions

Levels are not the only thing that creates entities.

You will want reusable object templates.

Call them prefabs, prototypes, archetypes, or entity templates.

A minimal form:

```cpp
struct EntityPrototype {
    AssetID model;
    // later physics, animation, gameplay defaults...
};
```

Then:

```cpp
Entity spawn(const EntityPrototype& prototype, const Transform& transform);
```

Do not build a complicated inheritance system yet.

Composition is usually easier:

```text
CarPrototype
  Model = car model
  Physics = vehicle settings
  Controller = player/AI
```

Chapter 9 will add the physics and animation pieces.

---

# 36. Save games are not level assets

A level file describes authored starting content.

A save game describes runtime state.

Do not overwrite your level asset every time the player saves.

Level:

```text
Enemy A starts at X
Door starts closed
```

Save game:

```text
Enemy A dead
Door open
Player health 72
```

Keep those concepts separate early and future serialization becomes much easier.

---

# 37. Checkpoint C — switch between two levels

Create:

```text
level_a.json
level_b.json
```

They should share at least one mesh/texture.

Test:

1. load A;
2. render;
3. switch to B;
4. render;
5. switch back to A.

Verify:

```text
no stale entities
no stale RenderHandles
no invalid GPU object indices
shared assets are reused
unique level objects disappear
validation remains clean
```

This is the point where your project has a real **level system**, even if it has no editor yet.

---

# 38. Hot reload and file watching

Do not make hot reload a requirement for completing Chapter 8.

But the new architecture makes it possible.

File watcher detects:

```text
asphalt.png changed
```

Registry resolves:

```text
AssetID 81
```

AssetManager reloads it.

Renderer uploads replacement image and updates the existing texture slot at a safe point.

World entities need no change because they refer to the same asset identity.

That is exactly why persistent IDs and stable runtime handles are useful.

Shader hot reload is similar conceptually but belongs more naturally to renderer tooling.

---

# 39. File-by-file implementation plan

## `assets/asset_id.h`

Add:

```text
AssetID
hash support
serialization helpers
```

## `assets/asset_registry.h/.cpp`

Add:

```text
AssetRecord
AssetType
scan()
find()
find_by_path()
.meta read/write
```

## `assets/asset_manager.h/.cpp`

Add synchronous loading first:

```text
load_texture
load_mesh
load_model
get
cache by AssetID
```

Then add:

```text
AssetState
worker jobs
upload requests
completion processing
```

## `assets/importers/gltf_importer.*`

Move fastgltf parsing out of renderer ownership.

Return plain CPU `ImportedModel` data.

## `assets/importers/image_importer.*`

Decode image files into CPU `DecodedImage`.

## `renderer/renderer.h`

Public resource APIs:

```text
create_mesh
create_texture
create_material
destroy_mesh
destroy_texture
destroy_material
```

No fastgltf types in this header.

## `world/world.h/.cpp`

Add:

```text
Entity lifecycle
component storage
level instances
render synchronization
```

## `world/components/transform.h`

TRS, hierarchy, dirty state.

## `world/components/render_component.h`

Asset references + `RenderHandle` instances.

## `assets/level_asset.h`

Serializable level description.

## `renderer/render_scene.*`

Keep stable GPU-object ownership from Chapter 7.

Add any helper needed to instantiate model primitives without exposing Vulkan state.

---

# 40. Final architecture

After Chapter 8:

```text
                       assets/ directory
                              |
                              v
                        AssetRegistry
                              |
                              | persistent IDs
                              v
                         AssetManager
                   +----------+----------+
                   |          |          |
                   v          v          v
               MeshAsset  TextureAsset MaterialAsset
                   |          |          |
                   +----------+----------+
                              |
                       renderer handles
                              |
                              v
                           Renderer
                              |
                              v
                          RenderScene
                              ^
                              |
                              | runtime instances
                              |
                            World
                              ^
                              |
                              v
                         LevelAsset
```

And asynchronous loading can flow beside it:

```text
Asset request
    |
    v
JobSystem: IO/parse/decode
    |
    v
Upload queue
    |
    v
Renderer GPU upload
    |
    v
Asset Ready
    |
    v
World resolves pending RenderComponent
```

This is a much stronger base than tying `LoadedGLTF` ownership directly to `VulkanEngine`.

---

# 41. What Chapter 9 will add

Now we can add two systems without contaminating the renderer architecture:

```text
Skeletal Animation
        |
        v
AnimatorComponent -> bone palette -> skinned render phase

Jolt Physics
        |
        v
RigidBodyComponent -> TransformComponent -> RenderScene dirty transform
```

Neither system needs to own Vulkan resources.

The asset system will gain:

```text
SkeletonAsset
AnimationClipAsset
SkinnedMeshAsset
CollisionAsset
```

The world will gain:

```text
AnimatorComponent
RigidBodyComponent
```

And the engine loop will gain a fixed physics update.

The important part is that all of those now have somewhere clean to live.

---

# 42. References

VKGuide's older asset-system article contains the useful core idea of separating source-format conversion from a simpler runtime asset representation. This chapter deliberately applies that idea incrementally instead of requiring a custom format immediately:

- https://vkguide.dev/docs/extra-chapter/asset_system/

Project Ascendant is also useful context because it demonstrates that a real project built from VKGuide moved to retained renderer objects rather than walking the tutorial scene graph for every draw:

- https://vkguide.dev/docs/ascendant/from_tutorial_to_engine/

For Vulkan upload/synchronization patterns, including timeline semaphores, Synchronization2, frames-in-flight safe points, and descriptor updates:

- https://docs.vulkan.org/guide/latest/versions.html
- https://docs.vulkan.org/guide/latest/synchronization.html
- https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Advanced_Topics/Synchronization_and_Streaming.html
- https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Advanced_Topics/Descriptor_Indexing_UpdateAfterBind.html

Chapter 9 will extend the GLTF importer with skins and animation according to the Khronos glTF 2.0 specification.
