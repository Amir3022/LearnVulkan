# Chapter 10 — Optional Advanced Engine Systems

## Taking the Chapters 6–9 engine from a strong baseline toward a production-style architecture

> **Where this chapter starts**
>
> This chapter assumes you completed:
>
> - VKGuide Chapters 0–5;
> - custom Chapter 6 — GPU-Driven Rendering;
> - custom Chapter 7 — From Renderer to Game Engine;
> - custom Chapter 8 — Asset System, World, and Levels;
> - custom Chapter 9 — Skeletal Animation and Jolt Physics.
>
> At this point you already have enough architecture to make a real game:
>
> - Vulkan 1.3;
> - Dynamic Rendering;
> - Synchronization2;
> - Buffer Device Address;
> - GPU-driven opaque rendering;
> - bindless materials;
> - a retained render scene;
> - an engine/world/renderer split;
> - stable runtime handles;
> - an asset registry and asset manager;
> - level loading;
> - skeletal animation;
> - vertex-shader skinning;
> - Jolt rigid-body physics;
> - fixed-timestep simulation;
> - clear transform ownership between gameplay, physics, animation, and rendering.
>
> Nothing in this chapter is required before you start building a game.
>
> This chapter contains the systems we deliberately postponed because they are much easier to understand once the simpler versions are already working.

The main rule for this chapter is:

> **Do not replace a working simple system merely because a more sophisticated system exists.**

Every section should be implemented because you can name the problem it solves.

---

# Table of contents

## Part I — Deciding when an advanced system is justified

1. [What we postponed and why](#1-what-we-postponed-and-why)
2. [The optimization ladder](#2-the-optimization-ladder)
3. [Establishing measurable engine budgets](#3-establishing-measurable-engine-budgets)

## Part II — CVars, diagnostics, and profiling

4. [A production-friendly CVar system](#4-a-production-friendly-cvar-system)
5. [Avoiding the global-static CVar trap](#5-avoiding-the-global-static-cvar-trap)
6. [Building an engine developer console](#6-building-an-engine-developer-console)
7. [CPU profiling](#7-cpu-profiling)
8. [GPU timestamp profiling](#8-gpu-timestamp-profiling)
9. [Engine statistics and counters](#9-engine-statistics-and-counters)
10. [Checkpoint A — measurable frames](#10-checkpoint-a--measurable-frames)

## Part III — Replacing the small World with EnTT

11. [Why we did not start with an ECS library](#11-why-we-did-not-start-with-an-ecs-library)
12. [What EnTT changes and what it must not change](#12-what-entt-changes-and-what-it-must-not-change)
13. [Adding EnTT](#13-adding-entt)
14. [Keeping your engine Entity handle stable](#14-keeping-your-engine-entity-handle-stable)
15. [Migrating components](#15-migrating-components)
16. [Writing systems as queries](#16-writing-systems-as-queries)
17. [Structural changes during iteration](#17-structural-changes-during-iteration)
18. [Transform hierarchy with an ECS](#18-transform-hierarchy-with-an-ecs)
19. [Render and physics proxies remain separate](#19-render-and-physics-proxies-remain-separate)
20. [Checkpoint B — the same game world on EnTT](#20-checkpoint-b--the-same-game-world-on-entt)

## Part IV — Turning the importer into an asset cooker

21. [Why source import eventually stops being enough](#21-why-source-import-eventually-stops-being-enough)
22. [Source assets versus runtime assets](#22-source-assets-versus-runtime-assets)
23. [Designing a cooked file header](#23-designing-a-cooked-file-header)
24. [Versioning and deterministic rebuilds](#24-versioning-and-deterministic-rebuilds)
25. [Cooking meshes](#25-cooking-meshes)
26. [Cooking textures](#26-cooking-textures)
27. [Cooking materials](#27-cooking-materials)
28. [Cooking skeletons and animation](#28-cooking-skeletons-and-animation)
29. [Cooking collision](#29-cooking-collision)
30. [Dependencies, bundles, and manifests](#30-dependencies-bundles-and-manifests)
31. [The cooker executable](#31-the-cooker-executable)
32. [Checkpoint C — runtime no longer parses GLTF](#32-checkpoint-c--runtime-no-longer-parses-gltf)

## Part V — Render graph and automatic Synchronization2 barriers

33. [When manual barriers become the wrong abstraction](#33-when-manual-barriers-become-the-wrong-abstraction)
34. [What our first render graph will and will not do](#34-what-our-first-render-graph-will-and-will-not-do)
35. [Logical resources](#35-logical-resources)
36. [Declaring pass reads and writes](#36-declaring-pass-reads-and-writes)
37. [Mapping logical use to Vulkan state](#37-mapping-logical-use-to-vulkan-state)
38. [Generating VkDependencyInfo](#38-generating-vkdependencyinfo)
39. [Dynamic Rendering from the graph](#39-dynamic-rendering-from-the-graph)
40. [The complete frame expressed as passes](#40-the-complete-frame-expressed-as-passes)
41. [Transient-resource lifetime and aliasing](#41-transient-resource-lifetime-and-aliasing)
42. [Checkpoint D — no hand-written inter-pass barriers](#42-checkpoint-d--no-hand-written-inter-pass-barriers)

## Part VI — Shader hot reload and pipeline lifetime

43. [Why shader compilation should leave CMake](#43-why-shader-compilation-should-leave-cmake)
44. [Tracking shader dependencies](#44-tracking-shader-dependencies)
45. [Background compilation](#45-background-compilation)
46. [Creating replacement pipelines](#46-creating-replacement-pipelines)
47. [Swapping pipelines safely](#47-swapping-pipelines-safely)
48. [Pipeline compatibility rules](#48-pipeline-compatibility-rules)
49. [Checkpoint E — edit shader, save, see result](#49-checkpoint-e--edit-shader-save-see-result)

## Part VII — Upgrading the job system

50. [Why a mutex queue is a good first job system](#50-why-a-mutex-queue-is-a-good-first-job-system)
51. [Task handles and dependencies](#51-task-handles-and-dependencies)
52. [Parallel-for](#52-parallel-for)
53. [Per-frame task graphs](#53-per-frame-task-graphs)
54. [Work stealing](#54-work-stealing)
55. [Avoiding false parallelism](#55-avoiding-false-parallelism)
56. [Integrating Jolt with your scheduler](#56-integrating-jolt-with-your-scheduler)
57. [Vulkan work from multiple CPU threads](#57-vulkan-work-from-multiple-cpu-threads)
58. [Secondary command buffers with Dynamic Rendering](#58-secondary-command-buffers-with-dynamic-rendering)
59. [Checkpoint F — animation and physics scale across cores](#59-checkpoint-f--animation-and-physics-scale-across-cores)

## Part VIII — Async transfer and async compute

60. [Queue families are an optimization tool, not an architecture](#60-queue-families-are-an-optimization-tool-not-an-architecture)
61. [Dedicated transfer submissions](#61-dedicated-transfer-submissions)
62. [Timeline semaphore upload tickets](#62-timeline-semaphore-upload-tickets)
63. [Queue-family ownership](#63-queue-family-ownership)
64. [When async compute is useful](#64-when-async-compute-is-useful)
65. [Why overlap can make performance worse](#65-why-overlap-can-make-performance-worse)
66. [Checkpoint G — streaming without frame hitches](#66-checkpoint-g--streaming-without-frame-hitches)

## Part IX — Compute skinning

67. [Why Chapter 9 started with vertex-shader skinning](#67-why-chapter-9-started-with-vertex-shader-skinning)
68. [Skin once, consume many times](#68-skin-once-consume-many-times)
69. [The skinned-vertex arena](#69-the-skinned-vertex-arena)
70. [Compute skinning input and output](#70-compute-skinning-input-and-output)
71. [Dispatching skin jobs](#71-dispatching-skin-jobs)
72. [Compute-to-graphics synchronization](#72-compute-to-graphics-synchronization)
73. [Feeding the Chapter 6 GPU scene](#73-feeding-the-chapter-6-gpu-scene)
74. [Shadows no longer reskin vertices](#74-shadows-no-longer-reskin-vertices)
75. [Animated bounds and culling](#75-animated-bounds-and-culling)
76. [Checkpoint H — one skinning pass per character](#76-checkpoint-h--one-skinning-pass-per-character)

## Part X — Advanced animation and ragdolls

77. [Animation state machines](#77-animation-state-machines)
78. [Blend trees and 1D blend spaces](#78-blend-trees-and-1d-blend-spaces)
79. [Additive animation](#79-additive-animation)
80. [IK as a post-process on the pose](#80-ik-as-a-post-process-on-the-pose)
81. [Ragdoll assets](#81-ragdoll-assets)
82. [Animation to ragdoll handoff](#82-animation-to-ragdoll-handoff)
83. [Ragdoll to animation recovery](#83-ragdoll-to-animation-recovery)
84. [Checkpoint I — physics can own the skeleton](#84-checkpoint-i--physics-can-own-the-skeleton)

## Part XI — SIMD after data-oriented design

85. [Why SIMD comes after layout](#85-why-simd-comes-after-layout)
86. [Let the compiler vectorize first](#86-let-the-compiler-vectorize-first)
87. [A SIMD-friendly culling layout](#87-a-simd-friendly-culling-layout)
88. [Portable SIMD strategy](#88-portable-simd-strategy)
89. [What not to SIMD](#89-what-not-to-simd)
90. [Checkpoint J — optimize a measured hotspot](#90-checkpoint-j--optimize-a-measured-hotspot)

## Part XII — Descriptor heaps as an optional backend

91. [Why descriptor indexing is still a good baseline](#91-why-descriptor-indexing-is-still-a-good-baseline)
92. [What VK_EXT_descriptor_heap changes](#92-what-vk_ext_descriptor_heap-changes)
93. [Keep resource handles independent of Vulkan descriptor strategy](#93-keep-resource-handles-independent-of-vulkan-descriptor-strategy)
94. [A dual descriptor backend](#94-a-dual-descriptor-backend)
95. [When not to migrate](#95-when-not-to-migrate)

## Part XIII — Meshlets and mesh shaders

96. [Why mesh shaders were postponed](#96-why-mesh-shaders-were-postponed)
97. [Meshlets are useful even without mesh shaders](#97-meshlets-are-useful-even-without-mesh-shaders)
98. [Cooking meshlets](#98-cooking-meshlets)
99. [Meshlet bounds and cone culling](#99-meshlet-bounds-and-cone-culling)
100. [The mesh-shader pipeline](#100-the-mesh-shader-pipeline)
101. [Task shaders](#101-task-shaders)
102. [Keeping the indexed-indirect fallback](#102-keeping-the-indexed-indirect-fallback)
103. [Checkpoint K — two geometry backends, one RenderScene](#103-checkpoint-k--two-geometry-backends-one-renderscene)

## Part XIV — Final architecture and implementation order

104. [The upgraded engine](#104-the-upgraded-engine)
105. [Recommended implementation order](#105-recommended-implementation-order)
106. [Features you still should not add just because they exist](#106-features-you-still-should-not-add-just-because-they-exist)
107. [File-by-file expansion plan](#107-file-by-file-expansion-plan)
108. [References](#108-references)

---

# Part I — Deciding when an advanced system is justified

# 1. What we postponed and why

Chapters 7–9 deliberately preferred simple implementations.

Examples:

```text
small custom World
    instead of
full ECS library
```

```text
runtime GLTF import
    instead of
offline cooker + custom binary format
```

```text
simple worker queue
    instead of
dependency-aware task scheduler
```

```text
vertex-shader skinning
    instead of
compute skinning
```

```text
Jolt JobSystemThreadPool
    instead of
custom Jolt scheduler adapter
```

```text
explicit Sync2 barriers
    instead of
render graph
```

Those were not temporary hacks.

They were **reference implementations**.

A reference implementation gives you something extremely valuable:

> a correct version against which an optimized version can be compared.

If compute skinning breaks a character, you can switch back to vertex skinning.

If a render graph generates a bad barrier, you know which manually-written barrier used to work.

If the cooked asset differs from the GLTF importer, you can load both and compare them.

That ability is worth more than immediately having the most sophisticated architecture.

---

# 2. The optimization ladder

A useful engine-development rule is:

```text
Correct
  |
  v
Observable
  |
  v
Measured
  |
  v
Optimized
  |
  v
Generalized
```

Do not reverse that order.

A common failure mode is:

```text
Generalized
   |
   v
Abstracted
   |
   v
Complicated
   |
   v
still no game
```

The systems in this chapter should enter your engine only after the earlier systems have real workloads.

For example:

**Do not add compute skinning because compute skinning is advanced.**

Add it when:

- the same character is skinned in a shadow pass and a main pass;
- you have enough animated vertices for the duplicated work to matter;
- another GPU consumer needs deformed vertices;
- profiling shows vertex skinning cost is relevant.

Similarly:

**Do not add a render graph because AAA engines have render graphs.**

Add one when manual resource transitions have become difficult to reason about.

---

# 3. Establishing measurable engine budgets

Before optimizing, decide what a successful frame means.

Suppose your target is:

```text
60 FPS
```

The total frame budget is roughly:

```text
16.67 ms
```

Do not assign all of that to the GPU.

A practical budget may look conceptually like:

```text
CPU simulation       3.0 ms
animation            1.0 ms
physics              2.0 ms
render preparation   1.0 ms
misc gameplay        2.0 ms

GPU rendering       12.0 ms

headroom             required
```

These values are examples, not universal targets.

The point is to make statements like:

> Animation currently costs 3.7 ms for 400 characters.

That is actionable.

This is not:

> Animation feels expensive.

So our first optional upgrade is not SIMD, a task graph, or mesh shaders.

It is **instrumentation**.

---

# Part II — CVars, diagnostics, and profiling

# 4. A production-friendly CVar system

VKGuide's optional CVar chapter demonstrates a very useful idea:

```cpp
render.occlusion
render.scale
physics.debug
animation.debugSkeleton
```

Engine settings should be queryable and editable at runtime.

For our engine, begin with typed variables:

```cpp
enum class CVarType : uint8_t
{
    Int,
    Float,
    Bool,
    String
};

enum class CVarFlags : uint32_t
{
    None       = 0,
    ReadOnly   = 1 << 0,
    Cheat      = 1 << 1,
    Persistent = 1 << 2,
    Developer  = 1 << 3
};
```

A record can contain metadata:

```cpp
struct CVarRecord
{
    std::string name;
    std::string description;

    CVarType type;
    CVarFlags flags;

    std::variant<int32_t, float, bool, std::string> defaultValue;
    std::variant<int32_t, float, bool, std::string> value;
};
```

Then:

```cpp
class CVarRegistry
{
public:
    template<typename T>
    CVarHandle register_var(
        std::string_view name,
        T defaultValue,
        std::string_view description,
        CVarFlags flags = CVarFlags::None);

    template<typename T>
    T get(CVarHandle handle) const;

    template<typename T>
    void set(CVarHandle handle, const T& value);
};
```

You can expose:

```cpp
CVarHandle cvarOcclusion;
CVarHandle cvarPhysicsDebug;
CVarHandle cvarRenderScale;
```

and register them during subsystem initialization.

---

# 5. Avoiding the global-static CVar trap

The old convenient pattern is:

```cpp
AutoCVar_Int CVAR_TestInt(...);
```

as a global object.

That works, but your Chapters 7–9 architecture has an important design goal:

> ownership should be explicit.

Static initialization creates hidden ordering.

You can still offer an `AutoCVar` wrapper, but make the **registry itself engine-owned**.

For example:

```cpp
class Engine
{
public:
    CVarRegistry cvars;
};
```

Renderer initialization:

```cpp
void Renderer::init(CVarRegistry& cvars)
{
    occlusionCVar = cvars.register_var(
        "render.occlusion",
        true,
        "Enable Hi-Z occlusion culling",
        CVarFlags::Developer);
}
```

Physics:

```cpp
void PhysicsWorld::init(CVarRegistry& cvars)
{
    debugDrawCVar = cvars.register_var(
        "physics.debug_draw",
        false,
        "Draw Jolt collision shapes",
        CVarFlags::Developer);
}
```

Now the lifetime is obvious.

## Fast reads

Do not hash a string every frame in a hot loop.

Resolve a name once:

```text
"render.occlusion"
        |
        v
CVarHandle 42
```

Then hot code reads:

```cpp
if (cvars.get<bool>(occlusionCVar))
{
    dispatch_occlusion_culling(...);
}
```

Later you can make numeric values atomic when writes may happen from a developer UI thread.

---

# 6. Building an engine developer console

The ImGui UI from the VKGuide chapters can become the first engine console.

Useful commands:

```text
render.gpu_driven 1
render.frustum_culling 1
render.occlusion 1
render.freeze_culling 0

render.shadow 1
render.render_scale 1.0

animation.pause 0
animation.debug_skeleton 0

physics.debug_draw 0
physics.gravity -9.81

asset.hot_reload 1
```

Also add read-only counters:

```text
stat.frame.cpu_ms
stat.frame.gpu_ms
stat.render.visible_objects
stat.render.culled_frustum
stat.render.culled_hiz
stat.animation.characters
stat.physics.active_bodies
stat.asset.pending_uploads
```

A CVar system becomes much more useful when variables and statistics share the same searchable developer UI.

---

# 7. CPU profiling

Instrument **systems**, not individual random functions.

You want a frame visualization like:

```text
Frame
|
+-- World update
|
+-- Animation
|    +-- sample clips
|    +-- build global poses
|
+-- Physics
|
+-- RenderScene sync
|
+-- Render submission
```

The API can be extremely small:

```cpp
struct CpuProfileScope
{
    CpuProfileScope(const char* name);
    ~CpuProfileScope();
};
```

Then:

```cpp
#define PROFILE_CPU_SCOPE(name) \
    CpuProfileScope profileScope_##__LINE__(name)
```

Usage:

```cpp
void AnimationSystem::update(float dt)
{
    PROFILE_CPU_SCOPE("AnimationSystem::update");

    ...
}
```

Initially you can accumulate durations yourself.

For serious analysis, integrate a profiler such as Tracy rather than building a complete profiler UI from scratch.

The important thing is that your engine code owns a small profiling macro layer so the backend can change.

---

# 8. GPU timestamp profiling

CPU timing around:

```cpp
vkQueueSubmit2(...)
```

does not tell you how long GPU work took.

Use timestamp queries.

Create a query pool:

```cpp
VkQueryPoolCreateInfo info{
    .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    .queryType = VK_QUERY_TYPE_TIMESTAMP,
    .queryCount = MAX_GPU_TIMESTAMPS
};
```

At the beginning of a frame:

```cpp
vkCmdResetQueryPool(
    cmd,
    frame.timestampPool,
    0,
    MAX_GPU_TIMESTAMPS);
```

Write timestamps around major passes:

```cpp
vkCmdWriteTimestamp2(
    cmd,
    VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
    frame.timestampPool,
    queryIndex);
```

After a pass:

```cpp
vkCmdWriteTimestamp2(
    cmd,
    VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
    frame.timestampPool,
    queryIndex + 1);
```

Convert timestamp ticks using:

```cpp
VkPhysicalDeviceLimits::timestampPeriod
```

Do not immediately read a query written by an in-flight frame.

Read results from a frame slot only after that slot's fence/timeline completion proves the GPU has finished.

## Better pass-level markers

Store:

```cpp
struct GpuProfileRange
{
    std::string name;
    uint32_t beginQuery;
    uint32_t endQuery;
};
```

Then the developer UI can show:

```text
GPU frame           8.62 ms

GPU culling         0.18 ms
shadow              1.07 ms
opaque              3.21 ms
Hi-Z                0.12 ms
transparent         0.53 ms
post                0.84 ms
UI                  0.21 ms
```

Now the rest of Chapter 10 can be data-driven.

---

# 9. Engine statistics and counters

Not every performance question needs a timer.

Chapter 6 already naturally produces counters such as:

```text
candidate objects
visible objects
indirect draw count
```

Expand this into:

```cpp
struct RendererStats
{
    uint32_t objectCount = 0;
    uint32_t visibleObjectCount = 0;

    uint32_t frustumRejected = 0;
    uint32_t occlusionRejected = 0;

    uint32_t indirectDrawCount = 0;
    uint32_t skinnedCharacterCount = 0;

    uint64_t uploadedBytes = 0;
};
```

Physics:

```cpp
struct PhysicsStats
{
    uint32_t bodyCount = 0;
    uint32_t activeBodyCount = 0;
    uint32_t contactCount = 0;
};
```

Assets:

```cpp
struct AssetStats
{
    uint32_t loadedAssets = 0;
    uint32_t loadingAssets = 0;
    uint32_t failedAssets = 0;

    uint64_t residentTextureBytes = 0;
    uint64_t residentMeshBytes = 0;
};
```

This is how you answer:

> Did Hi-Z actually remove enough work to justify its cost?

Compare:

```text
occlusionRejected
```

against:

```text
Hi-Z build + occlusion compute GPU time
```

---

# 10. Checkpoint A — measurable frames

Do not continue until you can answer:

- how long CPU animation takes;
- how long Jolt simulation takes;
- how long render preparation takes;
- how long each major GPU phase takes;
- how many render objects exist;
- how many survive culling;
- how many bodies are awake;
- how many assets are loading.

This checkpoint makes every later optional system much safer.

---

# Part III — Replacing the small World with EnTT

# 11. Why we did not start with an ECS library

Chapter 7 deliberately avoided this:

```cpp
entt::registry registry;
```

because using an ECS before understanding ownership often teaches the wrong lesson.

You first needed to understand:

```text
World entity
    !=
RenderScene instance
    !=
Jolt body
    !=
asset
```

Now you do.

That means an ECS library can become what it should be:

> an efficient component-storage and query implementation.

It should **not** become the engine architecture.

---

# 12. What EnTT changes and what it must not change

Today your Chapter 8 world conceptually owns:

```text
Entity
TransformComponent
RenderComponent
RigidBodyComponent
AnimationComponent
```

EnTT can replace the underlying component storage.

It must not replace:

```text
AssetManager
Renderer
RenderScene
PhysicsWorld
AnimationSystem
```

The relationship remains:

```text
          World / ECS
             |
      +------+------+
      |             |
      v             v
 Render proxy    Physics proxy
      |             |
      v             v
 RenderScene      Jolt
```

An entity is still a gameplay identity.

---

# 13. Adding EnTT

EnTT is header-only and its current mainline requires a C++20-capable compiler.

Pin a known release rather than tracking `main`.

CMake conceptually:

```cmake
include(FetchContent)

FetchContent_Declare(
    EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG <pinned-release-tag>
)

FetchContent_MakeAvailable(EnTT)

target_link_libraries(MyEngine PRIVATE EnTT::EnTT)
```

Do not copy a random version from a tutorial forever.

Make the version explicit in your dependency configuration.

---

# 14. Keeping your engine Entity handle stable

Do not spread:

```cpp
entt::entity
```

through renderer and physics APIs.

Keep:

```cpp
struct Entity
{
    uint32_t value = 0;
};
```

or your existing generational handle abstraction.

One approach is to wrap the EnTT value internally:

```cpp
class World
{
public:
    Entity create_entity();
    void destroy_entity(Entity entity);

private:
    entt::registry registry;
};
```

A conversion helper remains inside `World`:

```cpp
entt::entity World::to_entt(Entity entity) const;
```

Why?

Because later you may want:

- serialized IDs;
- editor UUIDs;
- network IDs;
- world-instance IDs.

Those are not necessarily the same as an ECS storage identifier.

---

# 15. Migrating components

Components stay data-oriented.

```cpp
struct TransformComponent
{
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 world{1.0f};

    Entity parent{};
    bool dirty = true;
};
```

```cpp
struct RenderComponent
{
    AssetID model;
    RenderInstanceHandle instance;
};
```

```cpp
struct RigidBodyComponent
{
    PhysicsBodyHandle body;
};
```

```cpp
struct AnimationComponent
{
    AssetID skeleton;
    AssetID clip;

    float time = 0.0f;
    float speed = 1.0f;

    PoseHandle pose;
};
```

Migration should not change the renderer.

That is the test that your Chapters 7–9 subsystem boundaries worked.

---

# 16. Writing systems as queries

A transform-to-render synchronization system can become:

```cpp
auto view = registry.view<
    const TransformComponent,
    RenderComponent>();

for (auto entity : view)
{
    auto& transform =
        view.get<const TransformComponent>(entity);

    auto& render =
        view.get<RenderComponent>(entity);

    if (!transform.dirty)
        continue;

    renderer.update_instance_transform(
        render.instance,
        transform.world);
}
```

Physics sync:

```cpp
auto view = registry.view<
    TransformComponent,
    const RigidBodyComponent>();

for (auto entity : view)
{
    ...
}
```

This is where ECS-style storage begins to help:

> systems iterate only the component sets they need.

---

# 17. Structural changes during iteration

Be careful with:

```text
create entity
destroy entity
add component
remove component
```

while iterating a query.

Even when a library supports some mutations, designing systems that freely restructure the world during iteration creates hard-to-reason-about behavior.

Keep the command-queue concept from Chapter 7:

```cpp
struct WorldCommand
{
    enum Type
    {
        Create,
        Destroy,
        AddComponent,
        RemoveComponent
    };

    ...
};
```

Systems enqueue structural changes:

```cpp
worldCommands.destroy(entity);
```

At a synchronization point:

```cpp
world.apply_commands();
```

That also makes future parallel systems much easier.

---

# 18. Transform hierarchy with an ECS

Do not assume ECS storage order gives you hierarchy order.

The transform relationship is still a graph/tree:

```text
Car
 |
 +-- WheelFL
 +-- WheelFR
 +-- WheelRL
 +-- WheelRR
```

You have several options:

### Option A — recursive dirty propagation

Keep the Chapter 8 implementation.

Simple and fine for modest worlds.

### Option B — topological transform list

Maintain:

```cpp
std::vector<Entity> transformOrder;
```

such that parents appear before children.

Then:

```cpp
for (Entity entity : transformOrder)
{
    update_world_transform(entity);
}
```

This becomes attractive once transform counts are high.

An ECS does not remove the need for specialized data structures where the problem demands them.

---

# 19. Render and physics proxies remain separate

Do not make a render component contain Vulkan state:

```cpp
struct RenderComponent
{
    VkBuffer vertexBuffer; // bad
};
```

Do not make a rigid-body component expose Jolt internals everywhere:

```cpp
struct RigidBodyComponent
{
    JPH::Body* body; // avoid
};
```

Keep the handles:

```cpp
RenderInstanceHandle
PhysicsBodyHandle
```

The subsystem owns the backend object.

This also prevents ECS storage relocation from invalidating backend pointers.

---

# 20. Checkpoint B — the same game world on EnTT

After migration:

- level loading still works;
- asset references are unchanged;
- renderer API is unchanged;
- physics API is unchanged;
- animation still works;
- entity destruction removes render/physics proxies correctly;
- save/load-facing persistent IDs have not become `entt::entity`.

If the ECS rewrite forced major renderer changes, revisit the boundaries.

---

# Part IV — Turning the importer into an asset cooker

# 21. Why source import eventually stops being enough

Chapter 8 intentionally allowed runtime GLTF import.

That is excellent while developing.

Eventually you will care about:

```text
startup time
package size
format validation
precomputed LODs
meshlets
collision preprocessing
texture compression
animation compression
dependency manifests
patching
streaming
```

Source formats are authoring formats.

Runtime formats are execution formats.

Those goals are different.

---

# 22. Source assets versus runtime assets

The pipeline becomes:

```text
DCC / source files
|
+-- car.glb
+-- road.glb
+-- asphalt_basecolor.png
+-- driver.glb
|
v
Asset Cooker
|
+-- meshes
+-- materials
+-- textures
+-- skeletons
+-- animation clips
+-- collision
|
v
Cooked asset packages
|
v
Runtime AssetManager
```

The important architecture from Chapter 8 does not change:

```text
AssetID
```

is still the stable identity.

Only the loader changes.

Before:

```cpp
import_gltf(sourcePath);
```

After:

```cpp
load_cooked_asset(assetRecord.cookedPath);
```

---

# 23. Designing a cooked file header

Do not begin with one giant C++ struct dumped using:

```cpp
file.write(
    reinterpret_cast<char*>(&myStruct),
    sizeof(myStruct));
```

That becomes fragile because of:

- padding;
- compiler differences;
- pointer fields;
- platform endianness;
- future version changes.

Create an explicit header.

```cpp
struct CookedAssetHeader
{
    uint32_t magic;
    uint32_t formatVersion;

    uint32_t assetType;
    uint32_t flags;

    uint64_t assetID;

    uint64_t metadataOffset;
    uint64_t metadataSize;

    uint64_t payloadOffset;
    uint64_t payloadSize;
};
```

Then write fields deliberately.

For a learning engine, little-endian PC files are a reasonable first target, but make the assumption explicit.

---

# 24. Versioning and deterministic rebuilds

A cooked result depends on more than the source timestamp.

For example:

```text
car.glb
+
mesh importer version
+
coordinate conversion version
+
mesh optimization settings
+
meshlet settings
+
engine asset format version
```

Create a build key:

```text
hash(
    source content hash,
    importer version,
    cooker settings,
    engine format version)
```

Then:

```text
same key
    -> cache hit

different key
    -> recook
```

This is the beginning of a Derived Data Cache.

Do not start by making it distributed.

Start with:

```text
Cache/
    2F/
      2F908A....asset
```

---

# 25. Cooking meshes

Your runtime renderer from Chapter 6 wants mesh data shaped for GPU consumption.

The cooker can precompute:

```text
vertex format
index format
bounds
surface ranges
material slots
LOD ranges
optional meshlets
```

A cooked mesh metadata structure may conceptually contain:

```cpp
struct CookedMeshInfo
{
    uint32_t vertexCount;
    uint32_t indexCount;

    Bounds bounds;

    uint32_t lodCount;

    uint32_t vertexStride;
    uint32_t vertexFormat;
};
```

The payload can store tightly packed:

```text
[vertices]
[indices]
[LOD metadata]
[optional meshlets]
```

## Do expensive mesh work offline

Operations such as:

- tangent generation;
- vertex deduplication;
- index optimization;
- LOD generation;
- meshlet construction;

belong naturally in the cooker.

Your shipping runtime should not need to know how a GLTF accessor is encoded.

---

# 26. Cooking textures

Runtime texture work ideally becomes:

```text
read compressed GPU-ready payload
       |
       v
upload
```

rather than:

```text
read PNG/JPEG
decode
convert
generate mipmaps
compress
upload
```

A practical path is to investigate KTX2/Basis Universal for cooked texture storage.

The exact compression format can depend on target hardware.

Your asset abstraction should let the cooker produce target-specific builds.

For example:

```text
Windows build:
BC formats

mobile build:
ASTC/ETC path
```

The game still requests:

```cpp
AssetID asphaltTexture;
```

It should not care which physical encoding the cooker selected.

---

# 27. Cooking materials

A runtime material should be boring data.

For the Chapter 6 renderer:

```cpp
struct GPUMaterialData
{
    uint32_t baseColorTexture;
    uint32_t metalRoughTexture;
    uint32_t normalTexture;

    glm::vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;

    uint32_t flags;
};
```

The cooker can turn source material descriptions into an engine representation.

Do not serialize bindless indices.

Those are runtime allocation results.

Serialize:

```text
AssetID of base-color texture
AssetID of normal texture
AssetID of metallic-roughness texture
material factors
shader/material class
```

At runtime:

```text
AssetID
   |
   v
TextureHandle
   |
   v
bindless slot
```

---

# 28. Cooking skeletons and animation

Chapter 9 stores animation in a direct, understandable form.

The cooker can later optimize it.

Possible offline work:

```text
validate skeleton hierarchy
remove unused channels
resample curves
compress rotations
compress translations
precompute clip bounds
build event tracks
```

Do not add all compression at once.

A first cooked animation format can simply reproduce Chapter 9's runtime structures without parsing GLTF.

That alone is already valuable.

Then measure memory.

---

# 29. Cooking collision

Collision is a perfect cooker responsibility.

The source model may contain:

```text
render mesh
```

but physics may want:

```text
box
sphere
capsule
convex hull
static triangle mesh
compound shape
```

Your cooker can produce collision metadata.

For example:

```cpp
enum class CollisionShapeType
{
    Box,
    Sphere,
    Capsule,
    ConvexHull,
    TriangleMesh,
    Compound
};
```

A car might produce:

```text
visual mesh:
80,000 triangles

collision:
5 convex pieces
```

Do not use a high-detail dynamic triangle mesh simply because that is what the artist exported.

Runtime still creates Jolt shapes inside `PhysicsWorld`.

The persistent cooked asset does not need to expose Jolt pointers.

---

# 30. Dependencies, bundles, and manifests

Chapter 8 already introduced dependencies.

Cooking makes them explicit.

A model asset may depend on:

```text
mesh A
mesh B
material X
material Y
texture 1
texture 2
skeleton
```

Store:

```cpp
struct CookedDependency
{
    AssetID id;
    AssetType type;
};
```

Now a level manifest can say:

```text
level_race_01
|
+-- track
+-- barriers
+-- car_player
+-- car_ai_a
+-- car_ai_b
+-- sky
+-- audio later
```

This becomes the basis for preload groups and streaming groups.

---

# 31. The cooker executable

Make the cooker a separate executable.

Conceptually:

```cmake
add_executable(AssetCooker
    tools/cooker/main.cpp
    tools/cooker/gltf_importer.cpp
    tools/cooker/mesh_cooker.cpp
    tools/cooker/texture_cooker.cpp
    tools/cooker/animation_cooker.cpp
    tools/cooker/collision_cooker.cpp
)

target_link_libraries(AssetCooker
    PRIVATE
    EngineAssetCommon
)
```

Share **format definitions**, not the whole runtime.

A useful structure:

```text
src/
    engine/
    renderer/
    world/
    physics/
    animation/
    assets/

tools/
    cooker/

shared/
    asset_format/
```

The cooker can use heavyweight source libraries.

The shipping runtime can become smaller.

---

# 32. Checkpoint C — runtime no longer parses GLTF

At this checkpoint:

```text
GLTF
  |
  v
AssetCooker
  |
  v
cooked files
  |
  v
AssetManager
```

The runtime path should no longer require GLTF parsing for cooked game content.

Keep source import available in developer builds if it is useful for iteration.

---

# Part V — Render graph and automatic Synchronization2 barriers

# 33. When manual barriers become the wrong abstraction

Chapter 6 intentionally wrote barriers explicitly.

That teaches Vulkan correctly.

But by now your frame may contain:

```text
compute culling
compute skinning
shadow
depth
opaque
Hi-Z
occlusion data
transparent
post
UI
uploads
```

Every resource has:

```text
stage
access
layout
```

across multiple uses.

Manual synchronization stops scaling.

The render graph's first job is not automatic pass reordering.

Its first job is:

> derive resource transitions from declared usage.

---

# 34. What our first render graph will and will not do

Version 1 will:

- execute passes in declaration order;
- track images and buffers;
- track read/write intent;
- compute Sync2 barriers;
- begin/end Dynamic Rendering for graphics passes;
- expose physical resources to execution callbacks.

It will **not yet**:

- reorder passes;
- schedule multiple queues;
- automatically alias all memory;
- fuse passes;
- invent async compute.

That keeps it debuggable.

---

# 35. Logical resources

A graph should refer to handles:

```cpp
struct RGImage
{
    uint32_t index;
};

struct RGBuffer
{
    uint32_t index;
};
```

rather than directly passing `VkImage`.

Import an existing image:

```cpp
RGImage depth =
    graph.import_image(
        "MainDepth",
        renderer.depthImage);
```

Create a transient image:

```cpp
RGImage hdrColor =
    graph.create_image(
        "HDR Color",
        hdrDesc);
```

Physical Vulkan resources remain hidden behind the graph.

---

# 36. Declaring pass reads and writes

Create usage descriptions.

```cpp
enum class RGImageUsage
{
    ColorAttachment,
    DepthAttachment,
    Sampled,
    StorageRead,
    StorageWrite,
    TransferSrc,
    TransferDst,
    Present
};
```

```cpp
enum class RGBufferUsage
{
    UniformRead,
    StorageRead,
    StorageWrite,
    IndirectRead,
    VertexRead,
    IndexRead,
    TransferSrc,
    TransferDst
};
```

Then:

```cpp
graph.add_compute_pass(
    "GPU Culling",
    [&](RGPassBuilder& pass)
    {
        pass.read(objectBuffer, RGBufferUsage::StorageRead);
        pass.write(indirectBuffer, RGBufferUsage::StorageWrite);
        pass.write(drawCount, RGBufferUsage::StorageWrite);
    },
    [&](RGPassContext& ctx)
    {
        dispatch_gpu_culling(ctx.cmd);
    });
```

Opaque:

```cpp
graph.add_graphics_pass(
    "Opaque",
    [&](RGPassBuilder& pass)
    {
        pass.read(
            indirectBuffer,
            RGBufferUsage::IndirectRead);

        pass.read(
            drawCount,
            RGBufferUsage::IndirectRead);

        pass.color_attachment(
            hdrColor,
            VK_ATTACHMENT_LOAD_OP_CLEAR);

        pass.depth_attachment(
            depth,
            VK_ATTACHMENT_LOAD_OP_CLEAR);
    },
    [&](RGPassContext& ctx)
    {
        draw_gpu_opaque(ctx.cmd);
    });
```

That declaration contains enough information to derive the important barriers.

---

# 37. Mapping logical use to Vulkan state

Create a table.

For an image:

```cpp
struct VulkanImageState
{
    VkPipelineStageFlags2 stages;
    VkAccessFlags2 access;
    VkImageLayout layout;
};
```

Example mapping:

```cpp
VulkanImageState state_for(RGImageUsage usage)
{
    switch (usage)
    {
    case RGImageUsage::ColorAttachment:
        return {
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

    case RGImageUsage::Sampled:
        return {
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

    case RGImageUsage::StorageWrite:
        return {
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_GENERAL
        };

    default:
        ...
    }
}
```

Do not make one overly broad state for every shader operation if you know the actual consumer.

Later, allow the caller to specify shader stages more precisely.

---

# 38. Generating VkDependencyInfo

The graph tracks:

```text
previous state
        |
        v
new requested state
```

If synchronization/layout change is needed, create:

```cpp
VkImageMemoryBarrier2 barrier{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

    .srcStageMask = previous.stages,
    .srcAccessMask = previous.access,

    .dstStageMask = next.stages,
    .dstAccessMask = next.access,

    .oldLayout = previous.layout,
    .newLayout = next.layout,

    .image = image,
    .subresourceRange = range
};
```

Batch all barriers before a pass:

```cpp
VkDependencyInfo dependency{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount =
        static_cast<uint32_t>(imageBarriers.size()),
    .pImageMemoryBarriers = imageBarriers.data(),
    .bufferMemoryBarrierCount =
        static_cast<uint32_t>(bufferBarriers.size()),
    .pBufferMemoryBarriers = bufferBarriers.data()
};

vkCmdPipelineBarrier2(cmd, &dependency);
```

This is essentially what Project Ascendant's simplified framegraph does conceptually: passes declare resource usage and the graph automates inter-pass barriers.

---

# 39. Dynamic Rendering from the graph

For graphics passes, the graph already knows the attachments.

Generate:

```cpp
VkRenderingAttachmentInfo
```

for every color/depth attachment.

Then:

```cpp
VkRenderingInfo info{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = renderArea,
    .layerCount = 1,
    .colorAttachmentCount =
        static_cast<uint32_t>(colors.size()),
    .pColorAttachments = colors.data(),
    .pDepthAttachment =
        hasDepth ? &depthAttachment : nullptr
};

vkCmdBeginRendering(cmd, &info);

pass.execute(context);

vkCmdEndRendering(cmd);
```

The render graph has **not brought back `VkRenderPass`**.

It is an engine-level pass system built on Dynamic Rendering.

---

# 40. The complete frame expressed as passes

Your Chapters 6–9 frame can become:

```text
Upload dirty scene data
        |
        v
Animation sampling (CPU)
        |
        v
Compute skinning [optional Chapter 10]
        |
        v
GPU culling
        |
        v
Shadow
        |
        v
Main opaque
        |
        v
Transparent
        |
        v
Depth pyramid
        |
        v
Post
        |
        v
UI
        |
        v
Present
```

Graph setup:

```cpp
build_frame_graph(RenderGraph& graph)
{
    add_skinning_pass(graph);
    add_culling_pass(graph);
    add_shadow_pass(graph);
    add_opaque_pass(graph);
    add_transparent_pass(graph);
    add_hiz_pass(graph);
    add_post_pass(graph);
    add_ui_pass(graph);
}
```

One major advantage is feature toggling.

If:

```text
render.shadow = 0
```

the shadow pass can simply not be added.

Resource usage is recalculated for the graph that actually exists this frame.

---

# 41. Transient-resource lifetime and aliasing

Once the graph is stable, it can calculate:

```text
first use
last use
```

for transient images.

Example:

```text
SSAO temp:
pass 4 -> pass 5

Bloom temp:
pass 9 -> pass 11
```

If lifetimes do not overlap, memory aliasing may be possible.

Do not implement aliasing in version 1.

The graph is already useful because it owns synchronization.

Add memory aliasing only after transient memory is significant enough to matter.

---

# 42. Checkpoint D — no hand-written inter-pass barriers

Do not remove helper barriers used for specialized local operations immediately.

But all major frame-to-frame/pass-to-pass transitions should now come from resource declarations.

Add a debug mode that prints:

```text
Opaque:
    MainColor
        GENERAL
        ->
        COLOR_ATTACHMENT_OPTIMAL

    DrawCommands
        COMPUTE/SHADER_WRITE
        ->
        DRAW_INDIRECT/INDIRECT_READ
```

A render graph you cannot inspect is difficult to trust.

---

# Part VI — Shader hot reload and pipeline lifetime

# 43. Why shader compilation should leave CMake

The tutorial compiles shaders as part of the build.

That is fine while shaders change infrequently.

A game engine needs iteration like:

```text
edit shader
save
see result
```

without rebuilding the executable.

Project Ascendant moved shader compilation into a separate script/tool and rebuilds affected pipelines.

We will go one step further and define an engine service around that workflow.

---

# 44. Tracking shader dependencies

A shader may contain:

```glsl
#include "gpu_scene.glsl"
#include "pbr.glsl"
```

If `pbr.glsl` changes, every shader using it must rebuild.

Create a dependency graph:

```text
gpu_mesh.frag
   |
   +-- pbr.glsl
   +-- gpu_scene.glsl
```

Your compiler service stores:

```cpp
struct ShaderRecord
{
    std::filesystem::path source;
    std::filesystem::path output;

    std::vector<std::filesystem::path> dependencies;

    uint64_t buildHash;
};
```

A simple first implementation can parse local include directives.

It does not need a complete C preprocessor.

---

# 45. Background compilation

Do not compile shaders inside the render loop.

Use the Chapter 7/10 job system:

```text
file watcher
    |
    v
shader compile job
    |
    v
SPIR-V result
    |
    v
pipeline rebuild request
```

Compilation failure is not fatal.

Keep the old pipeline active.

Display:

```text
Shader compile failed:
gpu_mesh.frag

line ...
```

in the developer UI.

That makes shader iteration pleasant rather than dangerous.

---

# 46. Creating replacement pipelines

Pipeline creation can also happen off the main gameplay path.

The pipeline description should be data:

```cpp
struct GraphicsPipelineDesc
{
    ShaderID vertexShader;
    ShaderID fragmentShader;

    VkFormat colorFormats[MAX_COLOR_ATTACHMENTS];
    uint32_t colorCount;

    VkFormat depthFormat;

    RasterState raster;
    DepthState depth;
    BlendState blend;
};
```

This is another reason Dynamic Rendering is useful.

Compatibility is described with attachment formats instead of an engine-owned `VkRenderPass`.

---

# 47. Swapping pipelines safely

Never:

```cpp
vkDestroyPipeline(device, oldPipeline, nullptr);
currentPipeline = newPipeline;
```

while old frames may still reference `oldPipeline`.

Instead:

```text
current pipeline A
        |
new pipeline B built
        |
        v
atomic/logical swap to B
        |
        v
A enters deferred-retirement queue
        |
GPU completion proves no old frame uses A
        |
        v
destroy A
```

Use the same retirement system Chapter 8 uses for streamed resources.

Timeline values are ideal if you already introduced them.

---

# 48. Pipeline compatibility rules

A shader hot reload is easy when only shader code changes.

It becomes architectural when resource interfaces change.

For example:

```text
old:
set 0 binding 3 = sampled image array

new:
set 0 binding 3 = storage buffer
```

Your current pipeline layout may no longer be compatible.

For the first hot reload system:

> Reject descriptor/push-layout incompatible reloads and request a full renderer reload/restart.

Later you can add reflection and automatic layout rebuilding.

Do not solve every possible shader edit on day one.

---

# 49. Checkpoint E — edit shader, save, see result

Success means:

- executable remains running;
- only affected shaders compile;
- compile errors keep old rendering alive;
- replacement pipeline appears automatically;
- old pipeline is retired safely;
- no `vkDeviceWaitIdle()` is required for ordinary hot reload.

---

# Part VII — Upgrading the job system

# 50. Why a mutex queue is a good first job system

Chapter 7's queue proves the boundary:

```cpp
jobs.enqueue(...);
```

That is more important than the first implementation being lock-free.

Now profiling may show that you need:

- dependencies;
- parallel-for;
- better worker utilization;
- Jolt integration;
- animation task batches.

Upgrade the backend without changing the idea that callers submit work to a scheduler.

---

# 51. Task handles and dependencies

Represent tasks:

```cpp
using TaskHandle = Handle<TaskTag>;
```

A task contains:

```cpp
struct Task
{
    SmallFunction<void()> function;

    std::atomic<uint32_t> unfinishedDependencies;

    std::vector<TaskHandle> dependents;
};
```

Then:

```text
sample animations A
sample animations B
sample animations C
        |
        +------+
               v
        finalize poses
               |
               v
        upload palettes
```

The scheduler runs a task when:

```text
unfinishedDependencies == 0
```

---

# 52. Parallel-for

A high-value primitive is:

```cpp
jobs.parallel_for(
    count,
    grainSize,
    [&](uint32_t begin, uint32_t end)
    {
        for (uint32_t i = begin; i < end; ++i)
            update_character(i);
    });
```

Do not make one job per trivial element.

Job overhead matters.

Chunk work:

```text
400 characters

bad:
400 tiny jobs

better:
8–32 chunks depending on work and hardware
```

Grain size should eventually be measured.

---

# 53. Per-frame task graphs

Your CPU frame can become:

```text
                    Input
                      |
                      v
                  Gameplay
                 /        \
                v          v
          Animation      AI later
                \          /
                 v        v
                Physics prep
                      |
                      v
                   Physics
                      |
                      v
             RenderScene sync
                      |
                      v
                 Render frame
```

This is not necessarily a fully dynamic task graph.

A fixed dependency graph per frame is already useful.

The important design question is:

> Which data can each task read and write?

Parallel scheduling is a data-ownership problem first.

---

# 54. Work stealing

A simple central queue may become contended.

A common next step is:

```text
worker 0 -> local deque
worker 1 -> local deque
worker 2 -> local deque
worker 3 -> local deque
```

A worker:

1. pops work from its own queue;
2. if empty, steals from another worker.

This reduces central contention and balances uneven workloads.

Do not write a complicated work-stealing scheduler unless profiling shows the Chapter 7 implementation is limiting scalability.

A third-party task library is also a perfectly legitimate engine decision.

---

# 55. Avoiding false parallelism

This:

```text
Animation task
Physics task
Gameplay task
```

does not mean they are safe to run together.

If they all mutate:

```text
TransformComponent
```

you created races.

Prefer phase ownership:

```text
animation produces desired pose
physics produces simulated transforms
gameplay produces commands
```

Then a synchronization phase commits those results.

For example:

```cpp
struct PhysicsTransformResult
{
    Entity entity;
    Transform transform;
};
```

Physics workers write a result buffer.

World synchronization applies it after physics completes.

This is often cleaner than making every component atomic/locked.

---

# 56. Integrating Jolt with your scheduler

Chapter 9 used Jolt's `JobSystemThreadPool`.

That remains a good option.

If thread oversubscription becomes measurable, you may integrate Jolt into your engine scheduler.

Jolt's `JobSystem` API requires correct handling of jobs and barriers. The official documentation recommends looking at `JobSystemThreadPool`, and if you want to reuse Jolt's barrier implementation you can derive from `JobSystemWithBarrier`.

Conceptually:

```cpp
class EngineJoltJobSystem final
    : public JPH::JobSystemWithBarrier
{
public:
    EngineJoltJobSystem(JobSystem& jobs);

    int GetMaxConcurrency() const override;

protected:
    JobHandle CreateJob(
        const char* name,
        JPH::ColorArg color,
        const JobFunction& function,
        uint32_t dependencyCount) override;

    void QueueJob(Job* job) override;
    void QueueJobs(Job** jobs, uint32_t count) override;

private:
    JobSystem& engineJobs;
};
```

Do not guess at the synchronization semantics.

Jolt barriers can wait while executing relevant jobs, and incorrect waiting can create deadlocks.

Treat this adapter as its own tested subsystem.

## First test

Compare:

```text
Jolt JobSystemThreadPool
```

against:

```text
EngineJoltJobSystem
```

for exactly the same simulation.

Do not delete the known-good backend immediately.

---

# 57. Vulkan work from multiple CPU threads

Vulkan supports multithreaded command recording, but external synchronization rules still matter.

A key rule:

> a command pool must not be used concurrently by multiple threads.

So use:

```text
FrameData
 |
 +-- Thread 0 command pool
 +-- Thread 1 command pool
 +-- Thread 2 command pool
 +-- Thread 3 command pool
```

Each worker gets its own per-frame pool.

At frame reuse:

```cpp
vkResetCommandPool(...);
```

only after the frame's completion proves those buffers are no longer executing.

---

# 58. Secondary command buffers with Dynamic Rendering

If you record a graphics pass into secondary command buffers, tell Vulkan which Dynamic Rendering formats they inherit.

Create:

```cpp
VkCommandBufferInheritanceRenderingInfo renderingInheritance{
    .sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,

    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &colorFormat,

    .depthAttachmentFormat = depthFormat,

    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
};
```

Then:

```cpp
VkCommandBufferInheritanceInfo inheritance{
    .sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,

    .pNext = &renderingInheritance
};
```

Begin the secondary command buffer with the appropriate render-pass-continue usage where required by your recording model.

The primary command buffer begins Dynamic Rendering with:

```cpp
VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT
```

and executes:

```cpp
vkCmdExecuteCommands(
    primary,
    count,
    secondaryBuffers);
```

Do not parallelize Vulkan command recording just because you can.

In a GPU-driven renderer, there may be very little CPU draw-command recording left.

The bigger CPU parallel opportunities may be:

- animation;
- asset decode;
- world systems;
- physics;
- render-scene update generation.

---

# 59. Checkpoint F — animation and physics scale across cores

Build stress scenes.

Measure:

```text
100 characters
200 characters
400 characters
800 characters
```

Compare:

```text
1 worker
2 workers
4 workers
8 workers
```

If runtime does not scale, inspect:

- task grain size;
- synchronization;
- false sharing;
- memory bandwidth;
- a serial finalization phase;
- Jolt worker configuration.

Do not assume more threads automatically means more performance.

---

# Part VIII — Async transfer and async compute

# 60. Queue families are an optimization tool, not an architecture

Your engine architecture should remain valid if the GPU exposes:

```text
one universal queue
```

or:

```text
graphics
compute
transfer
```

separately.

That means:

```text
AssetManager requests upload
Renderer schedules upload
```

not:

```text
AssetManager assumes transfer queue family #2 exists
```

---

# 61. Dedicated transfer submissions

If a useful transfer queue exists, your upload path can become:

```text
IO thread
   |
   v
decode/cooked read
   |
   v
UploadRequest
   |
   v
transfer command buffer
   |
   v
transfer queue
```

Staging allocations should come from a reusable upload ring/arena rather than creating one staging buffer per small resource forever.

For example:

```cpp
struct UploadSlice
{
    VkBuffer buffer;
    VkDeviceSize offset;
    VkDeviceSize size;

    void* mapped;
};
```

---

# 62. Timeline semaphore upload tickets

Create a transfer timeline semaphore.

Each submission signals:

```text
101
102
103
...
```

An upload request receives:

```cpp
struct UploadTicket
{
    uint64_t timelineValue;
};
```

An asset can move:

```text
LoadingCPU
    |
    v
WaitingForGPU(value = 103)
    |
timeline >= 103
    |
    v
Ready
```

This fits the Chapter 8 asset state machine naturally.

---

# 63. Queue-family ownership

If transfer and graphics use different queue families and the resource is created with exclusive sharing, ownership may need to transfer.

Conceptually:

```text
transfer queue:
write buffer
release ownership

graphics queue:
acquire ownership
read buffer
```

This requires queue-family indices in the barriers.

Do not invent ownership transfers if both operations use the same queue family.

A simpler approach for some frequently shared resources is concurrent sharing mode, but that has tradeoffs and should be chosen intentionally.

---

# 64. When async compute is useful

Potential candidates:

```text
compute skinning
some post effects
SSAO
particle simulation
some culling work
```

But only if dependencies allow overlap.

Example:

```text
graphics: shadow pass
compute:  SSAO
```

might overlap if they do not contend too heavily and required inputs are ready.

Your render graph can eventually expose:

```cpp
PassQueue::Graphics
PassQueue::Compute
```

but do not make the first graph scheduler automatically move passes between queues.

---

# 65. Why overlap can make performance worse

Graphics and compute queues share GPU hardware.

Running:

```text
heavy graphics
+
heavy compute
```

simultaneously may:

- compete for memory bandwidth;
- compete for compute units;
- reduce cache locality;
- add semaphore overhead;
- introduce ownership transitions.

So:

```text
async compute
```

is not synonymous with:

```text
faster
```

Use GPU profiling.

Project Ascendant explicitly notes that a plausible SSAO/shadow overlap was not necessarily a performance win in that project.

That is the right mindset.

---

# 66. Checkpoint G — streaming without frame hitches

Load a large level while rendering.

Track:

```text
frame time
upload bytes/frame
pending upload bytes
timeline latency
```

The player should not experience a long main-thread stall merely because a large texture or mesh entered memory.

---

# Part IX — Compute skinning

# 67. Why Chapter 9 started with vertex-shader skinning

Vertex-shader skinning is extremely useful as a baseline.

The pipeline is:

```text
rest-pose vertices
        |
        v
vertex shader
        |
      skin
        |
        v
rasterizer
```

It requires no output buffer and almost no scheduling.

For a modest character count, it may be all you need.

Compute skinning becomes interesting because the result survives the skinning dispatch.

---

# 68. Skin once, consume many times

With multiple rendering phases:

```text
shadow:
skin vertex

main:
skin same vertex again

other geometry consumer:
skin/approximate again
```

Compute skinning changes this to:

```text
joint palettes
      |
      v
Compute Skinning
      |
      v
Skinned Vertex Buffer
      |
      +--> shadow
      |
      +--> main
      |
      +--> debug visualization
      |
      +--> other GPU consumers
```

The current Khronos Vulkan tutorial describes this as a "skin once, use everywhere" model.

It maps very naturally onto our Chapter 6 GPU-driven architecture.

---

# 69. The skinned-vertex arena

Do not allocate a Vulkan buffer every character every frame.

Allocate a per-frame arena:

```text
Frame 0 Skinned Arena
Frame 1 Skinned Arena
FRAME_OVERLAP...
```

Each animated render instance gets a slice:

```cpp
struct SkinnedVertexSlice
{
    VkDeviceAddress address;
    uint32_t firstVertex;
    uint32_t vertexCount;
};
```

A linear allocator works well because all allocations are frame-temporary.

At frame-slot reuse:

```text
wait completion
reset arena
```

---

# 70. Compute skinning input and output

Rest-pose vertex:

```cpp
struct SkinnedSourceVertex
{
    glm::vec3 position;
    float uv_x;

    glm::vec3 normal;
    float uv_y;

    glm::vec4 tangent;

    glm::uvec4 joints;
    glm::vec4 weights;
};
```

Output may be smaller if downstream stages do not need joints/weights:

```cpp
struct SkinnedOutputVertex
{
    glm::vec3 position;
    float uv_x;

    glm::vec3 normal;
    float uv_y;

    glm::vec4 tangent;
};
```

Compute push constants:

```cpp
struct SkinningPushConstants
{
    VkDeviceAddress sourceVertices;
    VkDeviceAddress outputVertices;
    VkDeviceAddress jointMatrices;

    uint32_t vertexCount;
};
```

GLSL-style pseudocode:

```glsl
void main()
{
    uint i = gl_GlobalInvocationID.x;

    if (i >= pc.vertexCount)
        return;

    SourceVertex src = sourceVertices[i];

    mat4 skin =
        src.weights.x * joints[src.joints.x] +
        src.weights.y * joints[src.joints.y] +
        src.weights.z * joints[src.joints.z] +
        src.weights.w * joints[src.joints.w];

    vec4 p = skin * vec4(src.position, 1.0);

    OutputVertex outV;
    outV.position = p.xyz;

    // Normals/tangents need correct directional transformation.

    outputVertices[i] = outV;
}
```

Use the same joint-matrix convention you already validated in Chapter 9.

Do not casually copy a skinning equation from another engine with different object/skeleton spaces.

---

# 71. Dispatching skin jobs

For one character:

```cpp
uint32_t groups =
    (vertexCount + 63) / 64;

vkCmdDispatch(cmd, groups, 1, 1);
```

For many characters, repeatedly rebinding/dispatching may become overhead.

Two strategies:

### Version 1

One dispatch per animated render instance.

Simple and easy to debug.

### Version 2

Build:

```cpp
struct GPUSkinJob
{
    VkDeviceAddress source;
    VkDeviceAddress destination;
    VkDeviceAddress palette;

    uint32_t vertexCount;
};
```

Then a larger compute dispatch maps work to multiple skin jobs.

Do Version 1 first.

---

# 72. Compute-to-graphics synchronization

Compute writes:

```text
skinned vertex arena
```

Graphics reads it through BDA in the vertex shader.

Barrier:

```cpp
VkMemoryBarrier2 barrier{
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,

    .srcStageMask =
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,

    .srcAccessMask =
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,

    .dstStageMask =
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,

    .dstAccessMask =
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT
};
```

If the buffer is also consumed through another access type, include that actual use.

With the render graph, declare:

```text
ComputeSkinning:
    WRITE skinnedVertices as StorageWrite

Opaque:
    READ skinnedVertices as ShaderRead

Shadow:
    READ skinnedVertices as ShaderRead
```

Then the graph produces the dependency.

---

# 73. Feeding the Chapter 6 GPU scene

Your `GPUObjectData` already identifies a mesh and object.

For a skinned object, the effective vertex address changes every frame.

Two good approaches exist.

### Approach A — per-object skinned vertex address

```cpp
struct GPUObjectData
{
    glm::mat4 transform;

    uint32_t meshIndex;
    uint32_t materialIndex;

    VkDeviceAddress vertexAddress;

    uint32_t flags;
};
```

Static:

```text
vertexAddress -> original mesh data
```

Skinned:

```text
vertexAddress -> this frame's skinned slice
```

### Approach B — render mesh table override

Store an instance-specific vertex source index.

Approach A is easy with the BDA architecture you already have.

---

# 74. Shadows no longer reskin vertices

Before:

```text
shadow vertex shader
    -> skin

main vertex shader
    -> skin
```

After:

```text
compute skinning
       |
       v
skinned buffer
  |           |
  v           v
shadow       main
```

This is one of the clearest reasons to adopt compute skinning.

---

# 75. Animated bounds and culling

Compute skinning does not automatically solve animated bounds.

Options:

### Conservative static bound

Scale the bind-pose bound.

Simple but weak.

### Per-clip precomputed bounds

The asset cooker samples an animation clip and stores:

```text
clip bounds
```

This is a very strong general solution.

### GPU reduction

Skin vertices and reduce min/max bounds on GPU.

More accurate, but adds work and dependencies.

Do not begin with GPU bounds reduction.

The asset cooker now exists specifically so per-clip bounds are cheap to add.

---

# 76. Checkpoint H — one skinning pass per character

Compare vertex and compute skinning.

Measure:

```text
character count
shadow enabled/disabled
GPU skinning time
shadow time
main geometry time
total GPU frame
memory bandwidth
skinned arena size
```

Compute skinning may use more memory bandwidth because it writes deformed vertices to memory.

Do not assume it wins at tiny character counts.

---

# Part X — Advanced animation and ragdolls

# 77. Animation state machines

Chapter 9 gives you clips and blending.

A game needs states:

```text
Idle
Walk
Run
Brake
Turn
Crash
```

Do not make a state machine directly modify bones.

The state machine chooses animation operations.

```cpp
struct AnimationState
{
    AnimationClipHandle clip;
    bool looping;
};
```

Transitions:

```cpp
struct AnimationTransition
{
    StateID from;
    StateID to;

    float blendDuration;

    Condition condition;
};
```

Output:

```text
state machine
     |
     v
clip/blend command
     |
     v
pose evaluation
```

---

# 78. Blend trees and 1D blend spaces

For speed:

```text
0.0 m/s -> idle
2.0 m/s -> walk
6.0 m/s -> run
```

Rather than hard switching:

```text
walk/run
```

sample between clips.

Conceptually:

```cpp
float t =
    clamp(
        (speed - walkSpeed) /
        (runSpeed - walkSpeed),
        0.0f,
        1.0f);

pose =
    blend(
        walkPose,
        runPose,
        t);
```

A racing game may use the same idea for:

```text
steering-wheel / driver animation
suspension visual states
driver leaning
```

---

# 79. Additive animation

Sometimes you want:

```text
base locomotion
+
small recoil
```

or:

```text
driving pose
+
look direction
```

Store an additive pose relative to a reference pose.

Then:

```text
final local transform =
base
+
weighted delta
```

Quaternion rotation addition is not ordinary component-wise addition; use a proper delta-rotation representation.

Implement additive animation only after ordinary blends are correct.

---

# 80. IK as a post-process on the pose

Inverse kinematics fits after normal pose evaluation:

```text
sample state machine
       |
       v
blend
       |
       v
local pose
       |
       v
IK adjustments
       |
       v
global pose
       |
       v
skin matrices
```

Examples:

```text
foot placement
hand placement
head look-at
```

For your first engine, a simple CCD or FABRIK implementation is enough to learn the architecture.

Do not bury IK logic inside rendering shaders.

IK modifies the animation pose.

---

# 81. Ragdoll assets

Chapter 9 postponed ragdolls because they change transform ownership.

A ragdoll asset maps joints to physics bodies:

```cpp
struct RagdollJointDesc
{
    uint32_t skeletonJoint;

    CollisionShapeDesc shape;

    int32_t parentRagdollJoint;

    JointConstraintDesc constraint;
};
```

The cooker can generate/store this from metadata.

At runtime:

```text
SkeletonAsset
+
RagdollAsset
        |
        v
Jolt bodies + constraints
```

A ragdoll should use simplified collision.

Not one rigid body per render vertex.

---

# 82. Animation to ragdoll handoff

Before ragdoll:

```text
animation owns bone pose
```

At the transition frame:

1. evaluate the current animation pose;
2. compute every ragdoll body's world transform from the bone pose;
3. set Jolt bodies to those transforms;
4. give them suitable linear/angular velocities;
5. enable dynamic simulation;
6. mark the skeleton as physics-driven.

Now:

```text
physics owns ragdoll transforms
```

The animation system reads body transforms and converts them back into skeleton-space pose transforms.

Do not let animation and Jolt both overwrite the same bone transform during the same phase.

---

# 83. Ragdoll to animation recovery

Recovery is harder.

You cannot instantly do:

```text
ragdoll pose
    ->
idle animation
```

without a pop.

A basic recovery pipeline:

```text
ragdoll
   |
   v
capture current physics pose
   |
   v
choose get-up animation
   |
   v
align character root
   |
   v
blend ragdoll pose -> get-up pose
   |
   v
switch authority to animation
```

This is another reason Chapter 9 emphasized transform authority.

The difficult problem is not calling the Jolt API.

It is deciding which system owns state at each point in the transition.

---

# 84. Checkpoint I — physics can own the skeleton

Debug draw:

- skeleton joints;
- ragdoll bodies;
- constraints;
- bone-to-body mapping.

Test:

```text
animation
  ->
ragdoll
  ->
simulation
  ->
get-up
  ->
animation
```

Do not hide mapping bugs by disabling visual debug tools.

---

# Part XI — SIMD after data-oriented design

# 85. Why SIMD comes after layout

SIMD likes contiguous data.

This:

```cpp
struct Object
{
    glm::vec3 position;
    std::string name;
    Material* material;
    Bounds bounds;
    bool visible;
};
```

is not a particularly nice source for wide culling.

This is:

```text
centerX[]
centerY[]
centerZ[]
radius[]
```

The CPU architecture and SIMD articles in VKGuide are valuable because they emphasize that instruction-level optimization depends strongly on memory layout.

---

# 86. Let the compiler vectorize first

Before intrinsics:

1. remove unnecessary pointer chasing;
2. make loops simple;
3. use contiguous arrays;
4. compile with optimization;
5. inspect the generated code/profiler.

For example:

```cpp
for (size_t i = 0; i < count; ++i)
{
    out[i] =
        a[i] * b[i] +
        c[i];
}
```

is much easier for a compiler to vectorize than a virtual-function-heavy object loop.

Do not replace readable loops with intrinsics unless measurements justify it.

---

# 87. A SIMD-friendly culling layout

Suppose CPU-side broad culling still exists for some systems.

SoA:

```cpp
struct SphereBoundsSoA
{
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> z;
    std::vector<float> radius;
};
```

Now an AVX2 implementation can conceptually load eight X coordinates:

```cpp
__m256 x =
    _mm256_loadu_ps(&bounds.x[i]);
```

and operate on eight spheres at once.

Do not directly copy a specific intrinsic implementation before your scalar formula is unit-tested.

A very good workflow is:

```text
scalar reference implementation
        |
        v
SIMD implementation
        |
        v
compare outputs in tests
```

---

# 88. Portable SIMD strategy

Possible approaches:

### Compiler auto-vectorization

Lowest maintenance.

Try this first.

### SIMD abstraction library

Examples include libraries that wrap SSE/AVX/NEON.

Good when portability matters.

### Platform-specialized intrinsics

```text
x86 -> SSE/AVX
ARM -> NEON
```

Use only in measured kernels.

### ISPC

Interesting for large data-parallel CPU kernels.

Adds another compiler/toolchain component.

For your engine, keep a platform layer if you write intrinsics:

```cpp
namespace simd
{
    ...
}
```

Do not let AVX-specific types leak through the whole engine.

---

# 89. What not to SIMD

Bad candidates:

```text
filesystem logic
asset registry hash maps
rare editor actions
Jolt calls
Vulkan object creation
branch-heavy one-off gameplay
```

Good candidates may include:

```text
animation pose math
software culling
particle updates
procedural generation
audio DSP later
```

and only if profiling confirms cost.

---

# 90. Checkpoint J — optimize a measured hotspot

Write down:

```text
before: 2.10 ms
after:  1.32 ms
```

for a real workload.

If your handwritten SIMD changes:

```text
0.09 ms -> 0.07 ms
```

but adds 300 lines of platform-specific code, it may not be worth keeping.

Performance engineering includes maintenance cost.

---

# Part XII — Descriptor heaps as an optional backend

# 91. Why descriptor indexing is still a good baseline

Chapter 6 uses descriptor indexing/bindless arrays.

That remains a strong Vulkan 1.3-compatible architecture.

It is understandable, broadly available, and already works with your renderer.

Do not remove it simply because a newer descriptor model exists.

---

# 92. What VK_EXT_descriptor_heap changes

`VK_EXT_descriptor_heap` is a newer Vulkan extension that exposes descriptor storage as explicitly managed sampler/resource heaps.

Conceptually:

```text
Descriptor sets
    disappear from the heap-native model

Shader resource handle
    |
    v
descriptor heap offset
```

The extension has:

```text
one sampler heap
one resource heap
```

and descriptors can be managed in memory by the application.

This can fit a renderer that already thinks in global resource handles.

But it is an **optional extension path**, not the foundation of our engine.

Feature-check it.

---

# 93. Keep resource handles independent of Vulkan descriptor strategy

This Chapter 8 API was the important abstraction:

```cpp
TextureHandle
BufferHandle
SamplerHandle
```

The shader-facing index may be:

```text
descriptor-array index
```

today.

It may become:

```text
descriptor-heap offset/index
```

later.

Gameplay and assets must not care.

Renderer interface:

```cpp
class ResourceBindingBackend
{
public:
    virtual GPUTextureHandle register_texture(
        const TextureResource&) = 0;

    virtual void unregister_texture(
        GPUTextureHandle) = 0;
};
```

You may implement this without virtual functions if you prefer compile-time/backend selection.

The important part is conceptual separation.

---

# 94. A dual descriptor backend

Keep:

```text
DescriptorIndexingBackend
```

as fallback.

Add:

```text
DescriptorHeapBackend
```

behind capability selection:

```cpp
if (features.descriptorHeap)
{
    bindingBackend =
        BindingBackendType::DescriptorHeap;
}
else
{
    bindingBackend =
        BindingBackendType::DescriptorIndexing;
}
```

Shaders may need different compilation paths/macros.

Do not make every shader full of descriptor-backend conditionals if you can isolate access in shared shader helpers.

For example:

```glsl
Texture2D load_engine_texture(TextureHandle handle);
```

The implementation differs by backend.

Material shaders use the helper.

---

# 95. When not to migrate

Do not migrate if:

- descriptor management is not a bottleneck;
- your target hardware lacks the extension;
- the current bindless table capacity is sufficient;
- the engine is still changing rapidly.

A new descriptor model is an implementation upgrade, not a game feature.

---

# Part XIII — Meshlets and mesh shaders

# 96. Why mesh shaders were postponed

Chapter 6 already supports:

```text
GPU culling
+
indirect indexed drawing
+
bindless resources
```

That architecture is powerful.

If we had started with mesh shaders, you would have had to learn:

- GPU scene design;
- GPU culling;
- indirect execution;
- meshlet construction;
- mesh/task shader semantics;

at the same time.

Now mesh shaders can be understood as:

> an optional replacement for the geometry ingestion/pre-rasterization path.

---

# 97. Meshlets are useful even without mesh shaders

A meshlet is a small cluster of triangles/vertices.

Conceptually:

```text
Mesh
 |
 +-- Meshlet 0
 +-- Meshlet 1
 +-- Meshlet 2
 +-- Meshlet 3
```

Each meshlet has local metadata:

```cpp
struct Meshlet
{
    uint32_t vertexOffset;
    uint32_t vertexCount;

    uint32_t triangleOffset;
    uint32_t triangleCount;

    Bounds bounds;

    glm::vec3 coneAxis;
    float coneCutoff;
};
```

Even a compute-driven indexed renderer can use meshlet-level visibility and then generate draw work.

So the **offline representation** is valuable independently of `VK_EXT_mesh_shader`.

---

# 98. Cooking meshlets

This belongs in the Chapter 10 asset cooker.

Pipeline:

```text
source mesh
    |
    v
optimize indices
    |
    v
partition into meshlets
    |
    +--> local vertex references
    +--> local triangle data
    +--> bounds
    +--> normal cone
```

Do not partition meshes at game startup if the source mesh is static content.

Store meshlet data in the cooked mesh payload.

---

# 99. Meshlet bounds and cone culling

Whole-object frustum culling asks:

> Is any part of this object potentially visible?

Meshlet culling asks:

> Which clusters inside the visible object are useful?

For a large object:

```text
building
terrain section
large track mesh
```

this can significantly reduce geometry work.

Per meshlet:

```text
frustum test
occlusion test
backface/cone test
LOD decision
```

Again, measure.

Small props do not magically need 20 layers of culling.

---

# 100. The mesh-shader pipeline

`VK_EXT_mesh_shader` adds mesh and optional task shader stages that replace the traditional pre-rasterization stages.

Pipeline becomes:

```text
Mesh Shader
    |
    v
Rasterization
    |
    v
Fragment Shader
```

instead of:

```text
index fetch
    |
    v
vertex shader
    |
    v
primitive assembly
```

A mesh shader workgroup:

- loads meshlet data;
- determines output vertex/primitive counts;
- writes per-vertex outputs;
- writes primitive topology/indices.

Pseudocode conceptually:

```glsl
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32) in;
layout(triangles, max_vertices = 64, max_primitives = 126) out;

void main()
{
    uint meshletID = ...;

    Meshlet m = meshlets[meshletID];

    // Cooperatively load/transform vertices.
    // Write gl_MeshVerticesEXT.
    // Write primitive indices.
    // Set actual output counts.
}
```

Do not copy maximum sizes from an example blindly.

Query:

```cpp
VkPhysicalDeviceMeshShaderPropertiesEXT
```

and design your meshlets around your target strategy.

---

# 101. Task shaders

Task shaders can perform a coarse stage before mesh shader work.

Conceptually:

```text
Task shader
    |
    +-- reject meshlets
    |
    +-- choose work
    |
    v
emit mesh shader workgroups
```

That can fit:

```text
meshlet culling
LOD selection
```

However, your Chapter 6 compute culling already does visibility work.

Do not automatically duplicate it in a task shader.

Possible architectures:

### Compute-driven mesh shader

```text
compute culling
     |
     v
visible meshlet list
     |
     v
mesh shader
```

### Task-driven

```text
task shader
     |
cull/select
     |
     v
mesh shader
```

Choose based on workload and platform.

---

# 102. Keeping the indexed-indirect fallback

Do not destroy your proven renderer.

Keep:

```cpp
enum class GeometryBackend
{
    IndexedIndirect,
    MeshShader
};
```

The same high-level scene still owns:

```text
object
material
transform
bounds
mesh asset
```

The mesh asset may contain both:

```text
traditional index/vertex data
meshlet data
```

Then unsupported devices use:

```text
vkCmdDrawIndexedIndirectCount
```

while supported devices may use the mesh-shader path.

This is a very good test of whether your renderer architecture is actually separated from the world.

---

# 103. Checkpoint K — two geometry backends, one RenderScene

Render the same scene using:

```text
IndexedIndirect
```

and:

```text
MeshShader
```

Compare:

- image correctness;
- visible object counts;
- GPU time;
- triangle/primitive statistics;
- culling effectiveness;
- memory cost.

A technology only earns its complexity if it helps your workload.

---

# Part XIV — Final architecture and implementation order

# 104. The upgraded engine

After selected Chapter 10 upgrades, the engine can conceptually look like:

```text
                              Engine
                                |
        +-----------------------+-----------------------+
        |                       |                       |
        v                       v                       v
     World/ECS             AssetManager             Developer
       EnTT                     |                    Services
        |                       |                       |
        |                       +--> cooked assets      +--> CVars
        |                       +--> streaming          +--> profiling
        |                       +--> upload tickets     +--> hot reload
        |
        +-------------------+
        |                   |
        v                   v
 AnimationSystem       PhysicsWorld
        |                   |
        |                   +--> Jolt
        |                        |
        |                        +--> engine job adapter
        |
        +--> CPU pose evaluation
        +--> state machines
        +--> blends / IK
        +--> ragdoll ownership
        |
        v
  Skinning jobs
        |
        v
 Compute skinning
        |
        v
 Skinned vertex arena

                RenderScene
                    |
                    v
                GPU Scene
                    |
                    v
               RenderGraph
        +-----------+-----------+
        |           |           |
        v           v           v
    GPU cull     Shadow       Opaque
        |                       |
        +-----------+-----------+
                    |
                    v
                   Hi-Z
                    |
                    v
               Transparent
                    |
                    v
                   Post
                    |
                    v
                 Present

Geometry backend:
    IndexedIndirect
        or
    MeshShader

Resource binding backend:
    DescriptorIndexing
        or
    DescriptorHeap
```

This is significantly more sophisticated than VKGuide's tutorial renderer.

But notice something important:

The architecture from Chapters 7–9 is still recognizable.

We upgraded implementations without changing the responsibilities.

---

# 105. Recommended implementation order

Do **not** implement Chapter 10 in table-of-contents order simply because that is how it is written.

I recommend this actual order.

## Tier 1 — useful almost immediately

```text
1. CVars
2. CPU/GPU profiling
3. engine statistics
4. shader hot reload
```

These improve development velocity and inform every later decision.

## Tier 2 — when your project content grows

```text
5. asset cooker
6. deterministic cache/versioning
7. async transfer
8. render graph
```

The cooker becomes valuable once the game has real content.

The graph becomes valuable once passes multiply.

## Tier 3 — when CPU simulation grows

```text
9. EnTT migration, if the small World is becoming limiting
10. stronger job scheduler
11. animation parallel-for
12. optional Jolt scheduler integration
13. SIMD only for measured kernels
```

You may never need EnTT if your own World remains pleasant.

That is okay.

## Tier 4 — when animated GPU work grows

```text
14. compute skinning
15. animation state machine
16. blend trees
17. IK
18. ragdoll
```

Compute skinning is especially attractive when characters participate in multiple rendering phases.

## Tier 5 — renderer specialization

```text
19. meshlets
20. mesh shaders
21. descriptor heap backend
22. async compute
```

These are strongly hardware/workload dependent.

Do them last.

---

# 106. Features you still should not add just because they exist

Even after Chapter 10, avoid the trap of endlessly expanding engine infrastructure.

Examples not required for your first real game:

```text
custom scripting language
distributed asset build farm
fully parallel graph scheduler
custom physics engine
custom shader language
custom allocator for every type
network replication framework
general-purpose editor like Unreal
automatic multi-queue optimizer
ray tracing
virtual geometry
virtual textures
```

Any of those may become useful.

None is automatically the next step.

After Chapters 6–10, the best next project is increasingly:

> **make the actual game and allow the game to tell you what the engine is missing.**

---

# 107. File-by-file expansion plan

A possible project layout after Chapter 10:

```text
src/
|
+-- engine/
|   +-- engine.h
|   +-- engine.cpp
|   +-- engine_config.h
|
+-- core/
|   +-- handles.h
|   +-- job_system.h
|   +-- job_system.cpp
|   +-- cvar.h
|   +-- cvar.cpp
|   +-- profiler.h
|   +-- profiler.cpp
|   +-- log.h
|
+-- world/
|   +-- world.h
|   +-- world.cpp
|   +-- components.h
|   +-- world_commands.h
|
+-- assets/
|   +-- asset_id.h
|   +-- asset_registry.h
|   +-- asset_manager.h
|   +-- asset_manager.cpp
|   +-- cooked_formats.h
|   +-- asset_manifest.h
|
+-- renderer/
|   +-- renderer.h
|   +-- renderer.cpp
|   +-- render_scene.h
|   +-- render_scene.cpp
|   +-- render_graph.h
|   +-- render_graph.cpp
|   +-- gpu_scene.h
|   +-- gpu_scene.cpp
|   +-- gpu_profiler.h
|   +-- gpu_profiler.cpp
|   +-- shader_manager.h
|   +-- shader_manager.cpp
|   +-- pipeline_manager.h
|   +-- pipeline_manager.cpp
|   |
|   +-- bindings/
|   |   +-- binding_backend.h
|   |   +-- descriptor_indexing_backend.*
|   |   +-- descriptor_heap_backend.*
|   |
|   +-- geometry/
|       +-- indexed_indirect_backend.*
|       +-- mesh_shader_backend.*
|
+-- animation/
|   +-- animation_system.h
|   +-- animation_system.cpp
|   +-- animation_state_machine.h
|   +-- skinning_system.h
|   +-- skinning_system.cpp
|   +-- ik.h
|
+-- physics/
|   +-- physics_world.h
|   +-- physics_world.cpp
|   +-- jolt_job_system.h
|   +-- jolt_job_system.cpp
|   +-- ragdoll.h
|
+-- platform/
    +-- file_watcher.*
    +-- threading.*
```

Tools:

```text
tools/
|
+-- cooker/
|   +-- main.cpp
|   +-- mesh_cooker.*
|   +-- texture_cooker.*
|   +-- animation_cooker.*
|   +-- collision_cooker.*
|
+-- shader_compiler/
    +-- shader_build.py / executable
```

Shaders:

```text
shaders/
|
+-- gpu_scene.glsl
+-- gpu_mesh.vert
+-- gpu_mesh.frag
+-- gpu_cull.comp
+-- skin_vertices.comp
+-- gpu_shadow.vert
|
+-- mesh/
    +-- meshlet.mesh
    +-- meshlet.task
```

Do not reorganize the whole directory tree at once.

Move files when ownership boundaries are already stable.

---

# 108. References

This chapter intentionally combines material that was postponed from the earlier custom chapters with concepts from VKGuide's optional and Project Ascendant articles, plus current Khronos/Jolt/EnTT documentation.

## VKGuide

Extra Chapter index:

- https://vkguide.dev/docs/extra_chapter

CVar system:

- https://vkguide.dev/docs/extra-chapter/cvar_system/

CPU architecture:

- https://vkguide.dev/docs/extra-chapter/hardware/

Multithreading:

- https://vkguide.dev/docs/extra-chapter/multithreading/

Practical SIMD:

- https://vkguide.dev/docs/extra-chapter/intro_to_simd/

Project Ascendant — from tutorial to engine:

- https://vkguide.dev/docs/ascendant/from_tutorial_to_engine/

Project Ascendant — framegraph and synchronization:

- https://vkguide.dev/docs/ascendant/ascendant_light/

## Vulkan / Khronos

Advanced skeletal and compute skinning:

- https://docs.vulkan.org/tutorial/latest/Advanced_glTF/Skeletal_Compute_Skinning/01_introduction.html
- https://docs.vulkan.org/tutorial/latest/Advanced_glTF/Skeletal_Compute_Skinning/03_compute_skinning.html
- https://docs.vulkan.org/tutorial/latest/Advanced_glTF/Skeletal_Compute_Skinning/04_shared_vertex_buffer.html

Descriptor heap:

- https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_descriptor_heap.html
- https://docs.vulkan.org/guide/latest/descriptor_heap.html

Mesh shaders:

- https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_mesh_shader.html
- https://docs.vulkan.org/features/latest/features/proposals/VK_EXT_mesh_shader.html

## EnTT

Repository and integration notes:

- https://github.com/skypjack/entt

ECS documentation/wiki:

- https://github.com/skypjack/entt/wiki/Entity-Component-System

## Jolt Physics

Job system API:

- https://jrouwe.github.io/JoltPhysics/class_job_system.html

---

# Final perspective

Chapters 0–5 taught you how to use Vulkan.

Chapter 6 changed the renderer from:

```text
CPU decides every draw
```

to:

```text
GPU scene + compute visibility + indirect execution
```

Chapter 7 separated the renderer from the engine.

Chapter 8 separated source content, runtime assets, and world instances.

Chapter 9 added two systems that force architecture to become real:

```text
animation
physics
```

Chapter 10 is different.

It is not another mandatory stack of features.

It is a toolbox for the point where the game starts pushing back.

When you see:

```text
manual barriers becoming hard to audit
```

add the render graph.

When you see:

```text
shader iteration slowing development
```

add hot reload.

When you see:

```text
runtime source import increasing load time
```

add the cooker.

When you see:

```text
CPU systems becoming difficult to query and iterate
```

consider EnTT.

When you see:

```text
animation repeating skinning work across passes
```

try compute skinning.

When you see:

```text
a measured CPU math kernel dominating a frame
```

consider SIMD.

When you see:

```text
whole-mesh rendering spending too much work on invisible geometry
```

consider meshlets.

That is the main lesson of the optional chapter:

> **A good engine architecture lets you add sophistication when the workload earns it, without rewriting the game every time.**
