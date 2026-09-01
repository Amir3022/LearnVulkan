# Chapter 7 — From Vulkan Renderer to Game Engine Architecture
## Refactoring the Chapter 6 codebase without throwing away the renderer you just built

> **Where this chapter starts**
>
> This chapter assumes you completed **VKGuide Chapters 0–5** and the custom **Chapter 6 — GPU-Driven Rendering**.
>
> At this point you already have a Vulkan 1.3 renderer using Dynamic Rendering, Synchronization2, Buffer Device Address, bindless material data, a retained GPU-scene direction, compute-generated indirect draws, GPU frustum culling, and optionally Hi-Z occlusion and GPU-driven shadows.
>
> We are **not** going to add a second renderer. We are going to turn the renderer you already have into one subsystem of a game engine.

The most dangerous moment in a rendering tutorial is the moment where the tutorial renderer starts becoming a game engine.

Up to now, putting many things in `VulkanEngine` has been convenient. It made the tutorial easy to follow because one class could own the device, swapchain, scene, camera, GLTF files, materials, frame data, and the main loop.

That stops scaling once we add:

- reusable assets;
- multiple levels;
- skeletal animation;
- physics;
- gameplay objects;
- streaming;
- background work;
- an editor or developer UI;
- save/load;
- networking later.

The goal of this chapter is therefore not to make the code look "enterprise". The goal is to create **ownership boundaries** so every system can evolve without knowing the implementation details of every other system.

By the end of the chapter the engine will conceptually look like this:

```text
Application
    |
    v
Engine
    |
    +------------------+------------------+------------------+
    |                  |                  |                  |
    v                  v                  v                  v
World              Renderer          JobSystem        DeveloperServices
    |                  |
    |                  v
    |              RenderScene
    |                  |
    |                  v
    |              Vulkan backend
    |
    +--> gameplay / transforms / future physics / animation
```

The critical rule is:

> **The world owns game state. The renderer owns render state. Vulkan objects stay inside the renderer.**

---

# Table of contents

1. [What is wrong with keeping everything in VulkanEngine?](#1-what-is-wrong-with-keeping-everything-in-vulkanengine)
2. [The dependency rule](#2-the-dependency-rule)
3. [The target folder structure](#3-the-target-folder-structure)
4. [Checkpoint A — move Vulkan into Renderer without changing behavior](#4-checkpoint-a--move-vulkan-into-renderer-without-changing-behavior)
5. [Designing the Renderer API](#5-designing-the-renderer-api)
6. [Do not leak Vulkan handles into game code](#6-do-not-leak-vulkan-handles-into-game-code)
7. [Typed handles](#7-typed-handles)
8. [Generational handle pools](#8-generational-handle-pools)
9. [Turning the Chapter 6 GPU scene into RenderScene](#9-turning-the-chapter-6-gpu-scene-into-renderscene)
10. [Stable render instances](#10-stable-render-instances)
11. [Dirty updates instead of rebuilding](#11-dirty-updates-instead-of-rebuilding)
12. [Separating Engine from Renderer](#12-separating-engine-from-renderer)
13. [The game loop](#13-the-game-loop)
14. [Variable update versus fixed simulation](#14-variable-update-versus-fixed-simulation)
15. [Frame ownership and frames in flight](#15-frame-ownership-and-frames-in-flight)
16. [Initialization and shutdown order](#16-initialization-and-shutdown-order)
17. [Introducing a small JobSystem](#17-introducing-a-small-jobsystem)
18. [What should actually become a job?](#18-what-should-actually-become-a-job)
19. [Avoiding shared mutable data](#19-avoiding-shared-mutable-data)
20. [Command queues between systems](#20-command-queues-between-systems)
21. [Developer services: logging, CVars, and timing](#21-developer-services-logging-cvars-and-timing)
22. [Checkpoint B — the renderer no longer owns the scene](#22-checkpoint-b--the-renderer-no-longer-owns-the-scene)
23. [Checkpoint C — stable retained render objects](#23-checkpoint-c--stable-retained-render-objects)
24. [What not to abstract yet](#24-what-not-to-abstract-yet)
25. [File-by-file implementation plan](#25-file-by-file-implementation-plan)
26. [Final architecture](#26-final-architecture)
27. [What Chapter 8 will build on top](#27-what-chapter-8-will-build-on-top)
28. [References](#28-references)

---

# 1. What is wrong with keeping everything in VulkanEngine?

Nothing is wrong with it **while Vulkan itself is the project**.

The problem appears when unrelated systems begin depending on it.

A tutorial engine often evolves into something like:

```cpp
class VulkanEngine {
public:
    VkInstance instance;
    VkDevice device;
    VkQueue graphicsQueue;

    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

    Camera mainCamera;

    // GPU scene
    GPUDrivenRenderer gpuDriven;

    // Later...
    PhysicsSystem physics;
    AnimationSystem animations;
    AssetManager assets;
    World world;
    AudioSystem audio;
};
```

At first this seems convenient. Then every system starts calling every other system:

```text
Physics -> VulkanEngine -> Renderer
Animation -> VulkanEngine -> LoadedGLTF
World -> VulkanEngine -> GPU buffers
Renderer -> VulkanEngine -> Camera
Asset loader -> VulkanEngine -> immediate_submit()
```

Now ownership is unclear.

Who destroys a mesh?

Who decides whether an entity exists?

Can a physics body exist without a render object?

Can a level unload a texture while the GPU still references it?

Can a worker thread call `immediate_submit()`?

These are not Vulkan questions. They are engine architecture questions.

We want to answer them now, before Jolt and skeletal animation make them much harder.

---

# 2. The dependency rule

We will use one simple dependency rule throughout the next chapters:

```text
High-level systems may depend on low-level interfaces.
Low-level systems must not depend on high-level game state.
```

For this engine:

```text
Game / World
     |
     v
Renderer public API
     |
     v
RenderScene
     |
     v
Vulkan implementation
```

The Vulkan implementation must not ask:

```cpp
if (player->isDead()) { ... }
```

It should only receive render facts:

```cpp
renderer.set_transform(renderHandle, matrix);
renderer.set_visible(renderHandle, false);
```

Likewise physics should not do:

```cpp
renderer.set_transform(...);
```

Physics updates the world transform. The world/render synchronization stage informs the renderer.

This gives us a very useful flow:

```text
Gameplay
   |
   v
World state
   |
   +--> Physics reads/writes simulation state
   |
   +--> Animation reads/writes pose state
   |
   v
Render synchronization
   |
   v
RenderScene dirty updates
   |
   v
GPU scene
```

That separation is going to matter enormously in Chapter 9.

---

# 3. The target folder structure

Do not spend a week reorganizing folders.

Create only enough structure to make ownership visible.

A reasonable layout is:

```text
src/
    app/
        main.cpp

    engine/
        engine.h
        engine.cpp
        time.h

    core/
        handle.h
        slot_map.h
        job_system.h
        job_system.cpp
        log.h
        cvar.h

    renderer/
        renderer.h
        renderer.cpp
        render_scene.h
        render_scene.cpp

        vulkan/
            vk_context.h
            vk_context.cpp
            vk_descriptors.*
            vk_images.*
            vk_pipelines.*
            vk_loader.*
            vk_types.h

    world/
        world.h
        world.cpp
        entity.h
        transform.h
```

You do **not** need to move every VKGuide file immediately.

The safe migration is:

```text
old VulkanEngine
      |
      v
Renderer
      |
      v
split helper files later
```

The first objective is ownership, not beautiful directories.

---

# 4. Checkpoint A — move Vulkan into Renderer without changing behavior

Start by renaming the rendering responsibility instead of redesigning it.

Create:

```cpp
class Renderer {
public:
    bool init(SDL_Window* window);
    void shutdown();

    void begin_frame();
    void render(const CameraData& camera);
    void end_frame();

private:
    // Move the Vulkan state from VulkanEngine here.
};
```

Then make `Engine` extremely small:

```cpp
class Engine {
public:
    bool init();
    void run();
    void shutdown();

private:
    SDL_Window* window = nullptr;
    Renderer renderer;
};
```

For the first checkpoint, do not change your rendering path.

If Chapter 6 currently does:

```cpp
engine.draw();
```

make the new `Renderer::render()` execute the same code.

The scene can even remain hardcoded temporarily.

The checkpoint is complete when:

- the same GLTF scene renders;
- GPU culling still works;
- Hi-Z still works;
- resizing works;
- ImGui works;
- validation remains clean.

This sounds trivial, but it gives you an important new invariant:

> `Engine` can exist without knowing what a `VkPipeline`, `VkImage`, or `VkDescriptorSet` is.

---

# 5. Designing the Renderer API

A common beginner mistake is to "separate" the renderer like this:

```cpp
class Renderer {
public:
    VkDevice get_device();
    VkQueue get_graphics_queue();
    VkCommandBuffer get_command_buffer();
    VkDescriptorSet get_bindless_set();
};
```

That is not separation.

It just moved the Vulkan variables behind getters.

Instead ask what the rest of the engine **needs to request**.

Eventually the public renderer API should look closer to:

```cpp
class Renderer {
public:
    MeshHandle create_mesh(const MeshUpload& upload);
    TextureHandle create_texture(const TextureUpload& upload);
    MaterialHandle create_material(const MaterialDesc& desc);

    RenderHandle create_render_object(const RenderObjectDesc& desc);
    void destroy_render_object(RenderHandle handle);

    void set_transform(RenderHandle handle, const glm::mat4& transform);
    void set_material(RenderHandle handle, MaterialHandle material);
    void set_visible(RenderHandle handle, bool visible);

    void render(const RenderView& view);
};
```

Notice what is absent:

```text
VkBuffer
VkImage
VkDescriptorSet
VkPipeline
VkDeviceAddress
```

Those belong to the renderer backend.

Later the asset system can say:

```cpp
MeshHandle gpuMesh = renderer.create_mesh(upload);
```

without learning how your global index arena works.

---

# 6. Do not leak Vulkan handles into game code

Suppose an entity stores:

```cpp
struct RenderComponent {
    VkDeviceAddress vertexAddress;
    VkBuffer indexBuffer;
    VkDescriptorSet materialSet;
};
```

Now the world is tied directly to your current Vulkan strategy.

If you later change:

- descriptor indexing;
- the index arena;
- memory allocation;
- meshlets;
- compute skinning;
- descriptor heaps;

then the world must change.

Instead store engine handles:

```cpp
struct RenderComponent {
    RenderHandle renderObject;
};
```

And let `RenderScene` contain the GPU-facing IDs.

```cpp
struct RenderInstance {
    GPUMeshID mesh;
    GPUMaterialID material;
    glm::mat4 transform;
    Bounds bounds;
    uint32_t flags;
};
```

This is a crucial distinction:

```text
World identity != renderer identity != Vulkan object identity
```

They may map to one another, but they should not be the same type.

---

# 7. Typed handles

Avoid passing raw integers everywhere.

This compiles:

```cpp
renderer.destroy_texture(meshID);
```

if both are `uint32_t`.

Typed handles make accidental cross-system misuse harder.

```cpp
template<typename Tag>
struct Handle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;

    bool is_valid() const {
        return index != UINT32_MAX;
    }

    auto operator<=>(const Handle&) const = default;
};

struct MeshTag {};
struct TextureTag {};
struct MaterialTag {};
struct RenderTag {};

using MeshHandle = Handle<MeshTag>;
using TextureHandle = Handle<TextureTag>;
using MaterialHandle = Handle<MaterialTag>;
using RenderHandle = Handle<RenderTag>;
```

Now this is a compile error:

```cpp
TextureHandle texture;
renderer.destroy_mesh(texture);
```

Small type-safety improvements pay off quickly in engine code.

---

# 8. Generational handle pools

Why does the handle contain a generation?

Consider:

```text
slot 17 = Rock
Rock destroyed
slot 17 reused for Car
```

Some gameplay object may still hold the old handle to Rock.

If a handle is only:

```cpp
uint32_t index;
```

then the stale Rock handle suddenly refers to Car.

Generations solve this.

```text
Rock handle = { index 17, generation 3 }
Rock destroyed
slot 17 generation becomes 4
Car handle  = { index 17, generation 4 }
```

The old handle fails validation.

A simple reusable pool:

```cpp
template<typename T, typename Tag>
class SlotMap {
public:
    using HandleT = Handle<Tag>;

    HandleT insert(T value) {
        uint32_t index;

        if (!freeList.empty()) {
            index = freeList.back();
            freeList.pop_back();
            slots[index].value = std::move(value);
            slots[index].alive = true;
        } else {
            index = static_cast<uint32_t>(slots.size());
            slots.push_back(Slot{
                .value = std::move(value),
                .generation = 1,
                .alive = true
            });
        }

        return { index, slots[index].generation };
    }

    T* get(HandleT handle) {
        if (handle.index >= slots.size())
            return nullptr;

        Slot& slot = slots[handle.index];

        if (!slot.alive || slot.generation != handle.generation)
            return nullptr;

        return &slot.value;
    }

    void erase(HandleT handle) {
        if (T* value = get(handle)) {
            Slot& slot = slots[handle.index];
            slot.alive = false;
            ++slot.generation;
            freeList.push_back(handle.index);
        }
    }

private:
    struct Slot {
        T value{};
        uint32_t generation = 1;
        bool alive = false;
    };

    std::vector<Slot> slots;
    std::vector<uint32_t> freeList;
};
```

This implementation is intentionally simple.

Later you may want denser iteration, sparse sets, free-list compaction, or separate metadata/value arrays. Do not optimize that yet.

---

# 9. Turning the Chapter 6 GPU scene into RenderScene

Chapter 6 introduced the idea that opaque objects should become stable GPU-scene records rather than temporary CPU draw commands.

Now formalize that as its own class.

```cpp
class RenderScene {
public:
    RenderHandle create(const RenderObjectDesc& desc);
    void destroy(RenderHandle handle);

    void set_transform(RenderHandle handle, const glm::mat4& transform);
    void set_material(RenderHandle handle, MaterialHandle material);
    void set_visible(RenderHandle handle, bool visible);

    void flush_updates(Renderer& renderer, FrameData& frame);

private:
    SlotMap<RenderInstance, RenderTag> instances;

    std::vector<uint32_t> dirtyTransforms;
    std::vector<uint32_t> dirtyMaterials;
    std::vector<uint32_t> destroyedInstances;
};
```

`RenderInstance` is CPU-side retained render state:

```cpp
struct RenderInstance {
    MeshHandle mesh;
    MaterialHandle material;

    glm::mat4 transform{1.0f};
    Bounds localBounds{};

    uint32_t gpuObjectIndex = UINT32_MAX;
    uint32_t flags = 0;
};
```

Do not make it contain a gameplay `Entity*`.

The renderer should not need to dereference world objects during rendering.

---

# 10. Stable render instances

At the end of Chapter 5, a render object was essentially rebuilt by walking the GLTF hierarchy.

Chapter 6 moved toward retained GPU scene objects.

Now creation should happen once:

```cpp
RenderHandle carRender = renderer.create_render_object({
    .mesh = carMesh,
    .material = carPaint,
    .transform = initialTransform
});
```

Then movement is an update:

```cpp
renderer.set_transform(carRender, newTransform);
```

Not:

```cpp
scene.Draw(glm::mat4(...), drawContext);
```

every frame.

Your frame preparation becomes proportional to **what changed**, not necessarily to how many objects exist.

For a static level with 100,000 objects and a moving player car, this distinction is enormous.

---

# 11. Dirty updates instead of rebuilding

Do not immediately upload a GPU buffer every time `set_transform()` is called.

That creates hard-to-control synchronization and may upload the same object many times per frame.

Mark it dirty:

```cpp
void RenderScene::set_transform(RenderHandle handle, const glm::mat4& transform)
{
    RenderInstance* instance = instances.get(handle);
    if (!instance)
        return;

    instance->transform = transform;

    if ((instance->flags & RenderInstance_TransformDirty) == 0) {
        instance->flags |= RenderInstance_TransformDirty;
        dirtyTransforms.push_back(handle.index);
    }
}
```

Then once per frame:

```cpp
renderScene.flush_updates(renderer, frame);
```

Conceptually:

```text
Gameplay / Physics / Animation
          |
          | set_transform() many times
          v
      CPU RenderScene
          |
          | dirty indices
          v
   frame synchronization point
          |
          v
      GPUObjectData[]
```

This is the bridge between game state and the Chapter 6 GPU scene.

---

# 12. Separating Engine from Renderer

Now `Engine` becomes the orchestrator.

```cpp
class Engine {
public:
    bool init();
    void run();
    void shutdown();

private:
    void poll_events();
    void update(float dt);
    void fixed_update(float dt);
    void sync_render_state();

    SDL_Window* window = nullptr;

    Renderer renderer;
    World world;
    JobSystem jobs;

    bool running = true;
};
```

Notice that `Engine` coordinates systems, but does not implement them.

A useful rule is:

> If a function contains Vulkan commands, it probably belongs in `Renderer` or `renderer/vulkan`.

Likewise:

> If a function decides game-object existence or gameplay behavior, it probably does not belong in `Renderer`.

---

# 13. The game loop

A first version:

```cpp
void Engine::run()
{
    using clock = std::chrono::steady_clock;

    auto previous = clock::now();

    while (running) {
        const auto now = clock::now();
        const float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;

        poll_events();
        update(dt);
        sync_render_state();

        RenderView view = build_main_view();
        renderer.render(view);
    }
}
```

This is fine before physics.

However Chapter 9 will require a fixed simulation step, so we will prepare for that now.

---

# 14. Variable update versus fixed simulation

Rendering and input are naturally variable-rate.

Physics generally wants a stable timestep.

Split them:

```cpp
void Engine::run()
{
    constexpr double fixedDt = 1.0 / 60.0;
    double accumulator = 0.0;

    auto previous = Clock::now();

    while (running) {
        auto now = Clock::now();
        double frameDt = seconds(now - previous);
        previous = now;

        frameDt = std::min(frameDt, 0.25);
        accumulator += frameDt;

        poll_events();
        update(static_cast<float>(frameDt));

        while (accumulator >= fixedDt) {
            fixed_update(static_cast<float>(fixedDt));
            accumulator -= fixedDt;
        }

        const float alpha = static_cast<float>(accumulator / fixedDt);

        sync_render_state();
        renderer.render(build_main_view(alpha));
    }
}
```

We are not using physics yet, but this loop establishes the correct place for it.

Why cap `frameDt`?

If you break in the debugger for five seconds, you do not want the engine to try to simulate 300 physics steps to "catch up".

This is sometimes called avoiding the **spiral of death**.

---

# 15. Frame ownership and frames in flight

Your Vulkan renderer already has frame resources such as:

```cpp
FrameData frames[FRAME_OVERLAP];
```

Keep them renderer-owned.

Game code should not know which frame slot the GPU currently uses.

A good split is:

```text
Engine frame
    |
    +--> update game state
    +--> fixed simulation
    +--> publish render changes
    |
    v
Renderer::render()
    |
    +--> wait current frame fence
    +--> retire deferred resources
    +--> apply safe descriptor updates
    +--> upload dirty GPU scene records
    +--> dispatch culling
    +--> dynamic rendering
    +--> submit / present
```

That means asset streaming later can queue an upload request to the renderer, but the renderer decides the safe Vulkan moment to perform it.

---

# 16. Initialization and shutdown order

Engine systems form a dependency graph.

If `RenderScene` uses GPU resources, the renderer must exist before those resources are created.

A simple initialization order is:

```text
1. Platform / SDL
2. JobSystem
3. Renderer
4. AssetManager        (Chapter 8)
5. World
6. Animation           (Chapter 9)
7. Physics             (Chapter 9)
8. Game
```

Shutdown in reverse:

```text
Game
Physics
Animation
World
AssetManager
Renderer
JobSystem
SDL
```

Do not rely on arbitrary C++ global destructor order.

Make ownership explicit.

A useful RAII improvement later is to give every subsystem deterministic destruction, but during this refactor explicit `init()` / `shutdown()` calls are easier to debug.

---

# 17. Introducing a small JobSystem

VKGuide's multithreading article correctly emphasizes that modern engines benefit more from jobs than from creating one permanent thread for every subsystem.

We are not going to build a production scheduler yet.

We only need an interface that lets Chapter 8 perform CPU asset work away from the main thread.

```cpp
class JobSystem {
public:
    using Job = std::function<void()>;

    explicit JobSystem(uint32_t workerCount);
    ~JobSystem();

    void enqueue(Job job);
    void wait_idle();

private:
    void worker_main();

    std::vector<std::thread> workers;

    std::mutex mutex;
    std::condition_variable cv;
    std::condition_variable idleCv;

    std::deque<Job> queue;

    bool stopping = false;
    uint32_t activeJobs = 0;
};
```

Worker loop:

```cpp
void JobSystem::worker_main()
{
    for (;;) {
        Job job;

        {
            std::unique_lock lock(mutex);

            cv.wait(lock, [&] {
                return stopping || !queue.empty();
            });

            if (stopping && queue.empty())
                return;

            job = std::move(queue.front());
            queue.pop_front();
            ++activeJobs;
        }

        job();

        {
            std::lock_guard lock(mutex);
            --activeJobs;

            if (queue.empty() && activeJobs == 0)
                idleCv.notify_all();
        }
    }
}
```

This is not a lock-free scheduler. That is okay.

It is enough to establish the architectural boundary.

Later, if profiling proves this queue is a bottleneck, replace the implementation without changing every caller.

---

# 18. What should actually become a job?

Do not parallelize functions simply because they are expensive.

Good jobs tend to have:

- clear input data;
- clear output data;
- little shared mutation;
- enough work to amortize scheduling overhead.

Good future examples:

```text
Read file bytes
Decode PNG/KTX image
Parse GLTF
Build mesh collision data
Sample many animation instances
Generate navigation data
Cook assets
```

Bad first examples:

```text
Every TransformComponent update
Every single draw object
Tiny 20-instruction helpers
Arbitrary Vulkan calls from worker threads
```

Vulkan itself supports multithreaded command recording, but that does not mean every Vulkan operation should immediately become threaded.

Keep command submission and renderer resource ownership controlled until the engine architecture is stable.

---

# 19. Avoiding shared mutable data

The worst job architecture is:

```cpp
jobs.enqueue([&] {
    world.entities.push_back(...);
    renderer.upload_mesh(...);
    assetMap[path] = ...;
});
```

Now three systems may be mutated from an arbitrary worker thread.

Instead jobs should preferably produce a result:

```text
worker thread
    |
    v
ParsedMeshData
    |
    v
thread-safe completion queue
    |
    v
main/render safe point
    |
    v
create GPU resource
```

This is **message passing**, and it is much easier to reason about.

We will use this heavily in Chapter 8.

---

## CPU architecture interlude — why data layout now matters

Once draw submission moves to the GPU, the next CPU bottleneck is often not a dramatic algorithm. It is the way ordinary engine data is walked.

Modern CPUs do not fetch one C++ field from DRAM exactly when you access it. Memory arrives through cache lines and several cache levels. That means nearby data that you consume together is cheap, while chains of unrelated heap pointers can make the processor wait repeatedly.

Consider:

```cpp
struct GameObject {
    std::string name;
    Transform transform;
    RenderComponent render;
    PhysicsComponent physics;
    AIComponent ai;
    Inventory inventory;
};

std::vector<std::unique_ptr<GameObject>> objects;
```

A transform update that wants only position/rotation may bounce through allocations while dragging unrelated inventory/name/AI data into cache.

A component-oriented layout lets the transform system iterate something closer to:

```cpp
std::vector<TransformComponent> transforms;
```

The important point is not that "ECS is always faster." The point is that **systems should be able to iterate the data they actually use without chasing unrelated ownership graphs**.

### Array of Structures versus Structure of Arrays

An array of structures:

```cpp
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    float lifetime;
};

std::vector<Particle> particles;
```

is already contiguous and can be excellent when an update uses all three fields.

A structure of arrays:

```cpp
struct Particles {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> velocities;
    std::vector<float> lifetimes;
};
```

can be better when passes consume only selected fields or when SIMD/vectorization becomes important.

Do not mechanically convert every type to SoA. Ask:

```text
Which fields does this loop read?
Which fields does it write?
How many objects does it process?
Are those fields stored close together?
```

That question is more useful than a blanket rule.

### Hot and cold data

A render instance may eventually contain:

```text
transform                hot: read/update frequently
mesh/material IDs        hot: culling/render preparation
bounds                   hot
editor display name      cold
source path              cold
debug notes              cold
```

Keep frequently traversed data compact. Cold metadata can live elsewhere.

This becomes especially useful for the retained render scene:

```cpp
struct RenderInstanceHot {
    glm::mat4 transform;
    Bounds bounds;
    uint32_t mesh;
    uint32_t material;
    uint32_t flags;
};

struct RenderInstanceMetadata {
    std::string debugName;
    // Editor/source information.
};
```

You do not have to implement that split now. Just avoid designing APIs that make it impossible later.

### False sharing

Multithreading introduces another cache issue.

Two worker threads can modify *different variables* that happen to sit on the same cache line. The CPU then repeatedly moves ownership of that line between cores.

For example, avoid a design where many workers constantly increment adjacent shared counters:

```cpp
workerCounters[threadIndex]++;
```

if profiling shows cache-line contention.

Prefer worker-local accumulation followed by a merge:

```text
worker 0 local results ---+
worker 1 local results ----+--> merge
worker 2 local results ---+
```

This principle also supports our command-queue design: workers produce mostly private results, and a controlled stage merges them.

### Allocation strategy

Do not build a custom allocator merely because engines use custom allocators.

But distinguish lifetimes:

```text
persistent engine lifetime
asset lifetime
level lifetime
frame lifetime
temporary job lifetime
```

Frame-temporary data is a great candidate for arena/linear allocation later because it can be discarded all at once after a frame-safe point.

Persistent asset data is not.

The architecture matters first. Specialized allocation comes after you can identify a lifetime and measure allocation pressure.

---

# 20. Command queues between systems

A useful lightweight pattern is a multi-producer queue of requests.

For example:

```cpp
struct MeshUploadRequest {
    AssetID asset;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};
```

Worker threads push completed requests.

Renderer consumes them at a known point:

```cpp
void Renderer::process_upload_requests()
{
    MeshUploadRequest request;

    while (uploadQueue.try_pop(request)) {
        create_gpu_mesh(request);
    }
}
```

The renderer remains the sole owner of the Vulkan operation.

This greatly reduces the number of locks you need around Vulkan-side structures.

---

# 21. Developer services: logging, CVars, and timing

Now is a good point to create a tiny `core/` layer for development utilities.

You do not need a giant framework.

## Logging

At minimum:

```cpp
enum class LogLevel {
    Info,
    Warning,
    Error
};

void log(LogLevel level, std::string_view category, std::string_view message);
```

Categories become useful immediately:

```text
Renderer
Asset
World
Animation
Physics
```

## CVars

A small CVar system becomes valuable once Chapter 6 gives you toggles such as:

```text
r.gpuCulling
r.occlusion
r.freezeCullingCamera
r.shadowCulling
r.drawBounds
```

Do not make CVars the source of gameplay state.

They are best used for:

- renderer/debug settings;
- profiling toggles;
- experimental values;
- developer cheats.

## CPU timing

Add scoped timers around major systems:

```text
Frame
  Input
  Game Update
  Physics
  Animation
  Render Sync
  Render CPU
```

You are about to add complexity. Timing it from the beginning helps stop guesses from becoming "optimizations".

---

# 22. Checkpoint B — the renderer no longer owns the scene

At this checkpoint, your hardcoded GLTF ownership should be moving out of the renderer.

The renderer may still temporarily receive already-created GPU resources, but it should not decide **what level exists**.

Target:

```text
Engine
  |
  +--> World decides objects
  |
  +--> RenderScene mirrors renderable state
  |
  v
Renderer draws RenderScene
```

A useful test is to create two objects manually:

```cpp
RenderHandle a = renderer.create_render_object(...);
RenderHandle b = renderer.create_render_object(...);

renderer.set_transform(a, transformA);
renderer.set_transform(b, transformB);
```

Then destroy only one:

```cpp
renderer.destroy_render_object(a);
```

Verify:

- the correct GPU object is retired;
- stale handles are rejected;
- the other object remains valid;
- GPU culling does not reference the removed object;
- frame-overlap destruction is safe.

---

# 23. Checkpoint C — stable retained render objects

The final Chapter 7 frame should no longer rebuild opaque render objects by walking every GLTF node every frame.

Instead:

```text
Level load / object spawn
        |
        v
create RenderHandle once
        |
        v
RenderScene slot
        |
        v
GPUObjectData slot
```

During normal frames:

```text
Object moved?
    no  -> no scene upload
    yes -> dirty transform -> partial GPU update
```

Then Chapter 6 continues doing what it is good at:

```text
GPUObjectData[]
     |
     v
compute visibility
     |
     v
indirect draw list
```

The architecture now reduces both:

1. CPU draw submission overhead;
2. CPU scene-rebuild overhead.

---

# 24. What not to abstract yet

It is tempting to turn this refactor into a universal engine framework.

Do not.

## Do not create an RHI yet

You currently use Vulkan.

An abstraction such as:

```cpp
IRenderDevice
IGraphicsPipeline
ICommandList
```

is useful if you genuinely plan to support D3D12 or Metal soon.

Otherwise it makes learning Vulkan harder because every Vulkan concept is hidden behind an abstraction you must invent before you fully understand what needs abstracting.

## Do not replace STL just because Project Ascendant used EASTL

The important lesson from Project Ascendant is allocator control and data-oriented architecture, not that `std::vector` is inherently wrong.

Use STL until profiling or platform needs provide a concrete reason to change.

## Do not build a full ECS framework yet

Chapter 8 will introduce a small world/component model.

You can later replace it with EnTT, Flecs, or your own sparse-set ECS after you understand the access patterns your game actually has.

## Do not make every system a singleton

Global access feels convenient but silently recreates the same coupling we just removed from `VulkanEngine`.

Prefer explicit ownership and references passed during initialization.

---

# 25. File-by-file implementation plan

## New: `engine/engine.h` / `.cpp`

Own:

```text
main loop
system initialization
system shutdown
frame timing
fixed-update accumulator
```

## New: `renderer/renderer.h` / `.cpp`

Move the public rendering responsibility out of `VulkanEngine`.

Initially this can contain most of the old implementation.

## New: `renderer/render_scene.h` / `.cpp`

Own:

```text
retained RenderInstance objects
stable RenderHandle values
dirty transform/material lists
GPU scene synchronization
```

## New: `core/handle.h`

Add typed generational handles.

## New: `core/slot_map.h`

Add the simple reusable handle storage.

## New: `core/job_system.h` / `.cpp`

Add the basic worker pool.

Do not use it for Vulkan resource mutation yet.

## Existing `vk_engine.*`

Migrate gradually.

A useful transitional step is:

```cpp
using VulkanEngine = Renderer;
```

or simply rename the class first, then split helpers later.

Do not combine architectural refactoring with a complete Vulkan backend rewrite.

## Existing Chapter 6 GPU scene

Move the GPU-object storage/update functions behind `RenderScene` / `Renderer` rather than deleting them.

Preserve:

```text
GPUMeshData
GPUMaterialData
GPUObjectData
indirect buffers
culling pipeline
Hi-Z resources
```

They are already the correct rendering foundation.

---

# 26. Final architecture

After Chapter 7:

```text
                            Application
                                |
                                v
                              Engine
                                |
              +-----------------+------------------+
              |                 |                  |
              v                 v                  v
            World           JobSystem          Renderer
              |                                    |
              |                               RenderScene
              |                                    |
              |                            CPU retained objects
              |                                    |
              +---- transform changes ------------>|
                                                   |
                                                   v
                                              GPUObjectData[]
                                                   |
                                      +------------+------------+
                                      |                         |
                                      v                         v
                                 GPU culling                  Hi-Z
                                      |
                                      v
                               indirect commands
                                      |
                                      v
                               Dynamic Rendering
```

The renderer no longer owns the meaning of an object.

It only owns how objects are represented and rendered.

That is exactly the separation we need before adding persistent assets, levels, animation, and Jolt.

---

# 27. What Chapter 8 will build on top

Chapter 7 gives us **runtime ownership**.

Chapter 8 will add **persistent content identity**.

We will solve:

```text
How do I refer to a mesh across levels?
How do I stop loading the same texture twice?
Who owns a loaded GLTF?
How does a level refer to assets?
How do assets load asynchronously?
How do I unload a level safely?
How do I represent game entities independently of renderer objects?
```

The key transition will be:

```text
"load this GLTF and draw it"
```

becoming:

```text
Asset Registry
      |
      v
AssetManager
      |
      +--> MeshAsset
      +--> TextureAsset
      +--> MaterialAsset
      +--> LevelAsset
      |
      v
World entities
      |
      v
RenderScene handles
```

That is where the project starts functioning like a reusable game engine rather than a renderer demo.

---

# 28. References

The architectural direction in this chapter is intentionally compatible with the ideas shown in VKGuide's **Project Ascendant — From tutorial to Engine**, particularly the move from the Chapter 5 all-in-one `VulkanEngine` toward a retained object renderer and clearer engine subsystems:

- https://vkguide.dev/docs/ascendant/from_tutorial_to_engine/

VKGuide's multithreading article is useful background for the task-based direction discussed here:

- https://vkguide.dev/docs/extra-chapter/multithreading/

For the Vulkan-side resource/synchronization rules that remain inside `Renderer`, keep using the current Khronos Vulkan documentation:

- https://docs.vulkan.org/guide/latest/synchronization.html
- https://docs.vulkan.org/guide/latest/buffer_device_address.html
- https://docs.vulkan.org/guide/latest/versions.html

The next chapter deliberately keeps source-asset parsing behind the asset layer rather than allowing format libraries to spread throughout the runtime.
