# Chapter 9 — Skeletal Animation and Jolt Physics
## Adding animated characters and physical simulation without breaking the GPU-driven renderer

> **Where this chapter starts**
>
> This chapter assumes you completed Chapters 7 and 8 after the custom GPU-driven Chapter 6.
>
> You now have a modular `Engine`, a Vulkan-owning `Renderer`, a retained `RenderScene`, persistent `AssetID` values, runtime asset handles, a `World` with entities/components, level loading, and a job system.
>
> This chapter adds two major engine systems: **skeletal animation** and **Jolt Physics**. The important goal is not merely to make each system work. It is to integrate both without turning the renderer, world, asset system, and physics library into one coupled object graph.

This chapter is intentionally larger than a normal tutorial step because skeletal animation and physics are two places where early engine architecture is tested hard.

If we had kept the Chapter 5 architecture, it would be tempting to do this:

```text
GLTF node
  owns mesh
  owns skin
  owns animation
  owns transform
  owns render data
  owns physics body
```

That works for a demo, but it creates conflicts immediately.

Who controls the transform?

```text
animation?
physics?
gameplay?
network replication?
editor?
```

Our Chapters 7 and 8 already gave us the answer:

```text
World is authoritative for game state.
Renderer receives render state.
Physics is a simulation service.
Animation evaluates poses.
Assets hold immutable reusable data.
```

The final frame architecture will look approximately like this:

```text
Input / gameplay
      |
      v
World variable update
      |
      +----------------------+
      |                      |
      v                      v
AnimatorSystem          Kinematic commands
      |                      |
      v                      v
Local poses              Jolt bodies
      |                      |
      v                      |
Joint matrices               |
      |                      |
      |                 fixed physics step
      |                      |
      |                      v
      |                dynamic transforms
      |                      |
      +----------+-----------+
                 |
                 v
          TransformComponents
                 |
                 v
        World -> RenderScene sync
                 |
        +--------+---------+
        |                  |
        v                  v
 GPUObjectData       skin palette buffer
        |                  |
        +--------+---------+
                 v
          GPU culling / draw
```

---

# Table of contents

## Part I — Skeletal animation

1. [Static meshes versus skinned meshes](#1-static-meshes-versus-skinned-meshes)
2. [What GLTF actually stores for skinning](#2-what-gltf-actually-stores-for-skinning)
3. [Do not turn GLTF nodes into your runtime animation system](#3-do-not-turn-gltf-nodes-into-your-runtime-animation-system)
4. [Extending the importer](#4-extending-the-importer)
5. [Skinned vertex data](#5-skinned-vertex-data)
6. [SkeletonAsset](#6-skeletonasset)
7. [SkinAsset and inverse bind matrices](#7-skinasset-and-inverse-bind-matrices)
8. [AnimationClipAsset](#8-animationclipasset)
9. [Animation samplers and channels](#9-animation-samplers-and-channels)
10. [Checkpoint A — load and inspect a skeleton without rendering it](#10-checkpoint-a--load-and-inspect-a-skeleton-without-rendering-it)
11. [Runtime AnimatorComponent](#11-runtime-animatorcomponent)
12. [Sampling translation and scale](#12-sampling-translation-and-scale)
13. [Sampling quaternion rotation](#13-sampling-quaternion-rotation)
14. [STEP, LINEAR, and CUBICSPLINE](#14-step-linear-and-cubicspline)
15. [Building a local pose](#15-building-a-local-pose)
16. [Local pose to global joint transforms](#16-local-pose-to-global-joint-transforms)
17. [The skinning matrix and coordinate spaces](#17-the-skinning-matrix-and-coordinate-spaces)
18. [CPU animation, GPU skinning](#18-cpu-animation-gpu-skinning)
19. [Skin palette allocation](#19-skin-palette-allocation)
20. [Skinned vertex shader](#20-skinned-vertex-shader)
21. [Integrating skinned meshes with the Chapter 6 GPU scene](#21-integrating-skinned-meshes-with-the-chapter-6-gpu-scene)
22. [Culling animated meshes](#22-culling-animated-meshes)
23. [Checkpoint B — one animated GLTF character](#23-checkpoint-b--one-animated-gltf-character)
24. [Animation blending](#24-animation-blending)
25. [Root motion](#25-root-motion)
26. [When to move to compute skinning](#26-when-to-move-to-compute-skinning)

## Part II — Jolt Physics

27. [Why physics must remain separate from rendering](#27-why-physics-must-remain-separate-from-rendering)
28. [Adding Jolt to the CMake project](#28-adding-jolt-to-the-cmake-project)
29. [Jolt initialization](#29-jolt-initialization)
30. [Collision layers](#30-collision-layers)
31. [Creating PhysicsSystem](#31-creating-physicssystem)
32. [PhysicsSystem wrapper](#32-physicssystem-wrapper)
33. [RigidBodyComponent](#33-rigidbodycomponent)
34. [Mapping Entity to BodyID](#34-mapping-entity-to-bodyid)
35. [Shape assets](#35-shape-assets)
36. [Static meshes, convex hulls, and dynamic bodies](#36-static-meshes-convex-hulls-and-dynamic-bodies)
37. [Checkpoint C — falling box without renderer coupling](#37-checkpoint-c--falling-box-without-renderer-coupling)
38. [Fixed timestep integration](#38-fixed-timestep-integration)
39. [World → physics synchronization](#39-world--physics-synchronization)
40. [Physics → world synchronization](#40-physics--world-synchronization)
41. [Rendering interpolation](#41-rendering-interpolation)
42. [Sleeping bodies and dirty transforms](#42-sleeping-bodies-and-dirty-transforms)
43. [Contact events](#43-contact-events)
44. [Do not mutate the World directly from physics callbacks](#44-do-not-mutate-the-world-directly-from-physics-callbacks)
45. [Jolt's JobSystem and your JobSystem](#45-jolts-jobsystem-and-your-jobsystem)
46. [Character controllers](#46-character-controllers)
47. [Debug drawing](#47-debug-drawing)

## Part III — Putting both systems together

48. [Transform authority](#48-transform-authority)
49. [Animated characters with simple collision](#49-animated-characters-with-simple-collision)
50. [Ragdolls later](#50-ragdolls-later)
51. [The final engine frame](#51-the-final-engine-frame)
52. [Multithreading opportunities](#52-multithreading-opportunities)
53. [Common bugs and how to diagnose them](#53-common-bugs-and-how-to-diagnose-them)
54. [File-by-file implementation plan](#54-file-by-file-implementation-plan)
55. [Final architecture](#55-final-architecture)
56. [What to build after Chapter 9](#56-what-to-build-after-chapter-9)
57. [References](#57-references)

---

# Part I — Skeletal animation

# 1. Static meshes versus skinned meshes

A static vertex has a fixed model-space position.

Chapter 6 can conceptually do:

```glsl
worldPosition = object.transform * vertex.position;
```

A skinned vertex first moves according to the skeleton pose:

```text
bind-pose vertex
      |
      v
weighted joint transforms
      |
      v
posed model-space vertex
      |
      v
object/world transform
      |
      v
world-space vertex
```

This does **not** mean skinned meshes need a totally separate renderer.

The object still has:

```text
object ID
mesh/material IDs
world transform
bounds
visibility
```

The difference is that its vertex position is generated using a skin palette.

That is why Chapter 6 deliberately kept the GPU scene generic.

---

# 2. What GLTF actually stores for skinning

GLTF 2.0 skinning is based on four pieces of information.

## A joint hierarchy

The `skin.joints` array references GLTF nodes.

Those nodes form the skeleton hierarchy.

## Inverse bind matrices

Each joint may have an inverse bind matrix.

Conceptually it transforms a vertex from the bind-pose mesh space into the coordinate system where the joint transform can be applied.

## Vertex joint indices

The vertex attribute:

```text
JOINTS_0
```

normally stores four joint indices.

## Vertex weights

The attribute:

```text
WEIGHTS_0
```

stores the contribution of those joints.

A vertex might contain:

```text
joint indices = [3, 7, 9, 0]
weights       = [0.55, 0.30, 0.15, 0.0]
```

Its final skin matrix is approximately:

```text
0.55 * jointMatrix[3]
+ 0.30 * jointMatrix[7]
+ 0.15 * jointMatrix[9]
```

GLTF supports additional `JOINTS_n` / `WEIGHTS_n` sets, but start with `JOINTS_0` and `WEIGHTS_0`.

Four influences per vertex are common and sufficient for this engine milestone.

---

# 3. Do not turn GLTF nodes into your runtime animation system

The importer sees GLTF concepts.

The runtime should see engine concepts.

Bad long-term architecture:

```cpp
AnimatorComponent {
    fastgltf::Asset* asset;
    size_t animationIndex;
    size_t skinIndex;
}
```

Now gameplay and animation depend permanently on your import library.

Instead convert during import:

```text
GLTF skin
   -> SkeletonAsset / SkinAsset

GLTF animation
   -> AnimationClipAsset

GLTF mesh primitive
   -> SkinnedMeshAsset
```

The importer is allowed to understand accessor layouts, normalized integer weights, GLTF node indices, and interpolation modes.

The runtime should not have to.

---

# 4. Extending the importer

Your Chapter 8 `GltfImporter` now needs to extract:

```text
JOINTS_0
WEIGHTS_0
skins[]
inverseBindMatrices
animations[]
animation channels
animation samplers
joint-node local TRS
```

Extend the CPU import result:

```cpp
struct ImportedSkin {
    std::vector<uint32_t> jointNodeIndices;
    std::vector<glm::mat4> inverseBindMatrices;
    int32_t skeletonRootNode = -1;
};

struct ImportedAnimationSampler {
    enum class Interpolation {
        Step,
        Linear,
        CubicSpline
    };

    std::vector<float> times;
    std::vector<glm::vec4> values;
    Interpolation interpolation = Interpolation::Linear;
};
```

For actual implementation, translation/scale and rotation channels should use appropriately typed arrays rather than forcing all values into `vec4`. We will define better runtime types shortly.

---

# 5. Skinned vertex data

Do not enlarge every static vertex if most meshes are static.

Keep two vertex layouts.

For example:

```cpp
struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};
```

and:

```cpp
struct SkinnedVertex {
    glm::vec3 position;
    float uv_x;

    glm::vec3 normal;
    float uv_y;

    glm::vec4 color;

    glm::u16vec4 joints;
    glm::vec4 weights;
};
```

Exact packing is your choice.

Important considerations:

- GLTF joint indices may arrive as unsigned bytes or unsigned shorts;
- weights may arrive as floats or normalized integer formats;
- convert them into one known runtime representation;
- renormalize weights defensively if source conversion introduces small error.

For example:

```cpp
float sum = w.x + w.y + w.z + w.w;
if (sum > 0.0f)
    w /= sum;
else
    w = {1, 0, 0, 0};
```

---

# 6. SkeletonAsset

We need a reusable skeleton definition.

```cpp
struct SkeletonJoint {
    std::string name;

    int32_t parent = -1;

    glm::vec3 bindTranslation{0.0f};
    glm::quat bindRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 bindScale{1.0f};
};

struct SkeletonAsset {
    AssetID id;
    std::vector<SkeletonJoint> joints;
};
```

Notice the runtime hierarchy uses **joint indices**, not GLTF node indices.

During import build a mapping:

```text
gltf node index -> skeleton joint index
```

This lets the animation runtime iterate compact arrays:

```text
joint 0
joint 1
joint 2
...
```

without carrying the entire original GLTF scene hierarchy.

---

# 7. SkinAsset and inverse bind matrices

A skeleton describes hierarchy and bind TRS.

A skin describes how a particular skinned mesh is attached to those joints.

For your first engine, they may map one-to-one, but keeping the meaning separate is useful.

```cpp
struct SkinAsset {
    AssetID id;
    AssetID skeleton;
    std::vector<glm::mat4> inverseBindMatrices;
};
```

The array order must correspond to your runtime joint order.

During import, reorder GLTF inverse bind matrices if necessary when translating from GLTF node indices to compact joint indices.

If GLTF omits inverse bind matrices, treat them as identity as specified by GLTF.

---

# 8. AnimationClipAsset

An animation clip should be immutable reusable data.

Do not store playback time inside the asset.

```cpp
enum class AnimationPath : uint8_t {
    Translation,
    Rotation,
    Scale
};

enum class AnimationInterpolation : uint8_t {
    Step,
    Linear,
    CubicSpline
};
```

Typed channel data:

```cpp
struct Vec3Channel {
    uint32_t joint = 0;
    AnimationPath path;
    AnimationInterpolation interpolation;

    std::vector<float> times;
    std::vector<glm::vec3> values;
};

struct QuatChannel {
    uint32_t joint = 0;
    AnimationInterpolation interpolation;

    std::vector<float> times;
    std::vector<glm::quat> values;
};
```

Clip:

```cpp
struct AnimationClipAsset {
    AssetID id;
    AssetID skeleton;

    float duration = 0.0f;

    std::vector<Vec3Channel> translationChannels;
    std::vector<QuatChannel> rotationChannels;
    std::vector<Vec3Channel> scaleChannels;
};
```

For `CUBICSPLINE`, you will need tangent data as well. We will postpone the full representation until the basic path works.

---

# 9. Animation samplers and channels

GLTF separates:

```text
sampler = time/value/interpolation data
channel = sampler + target node/property
```

Your importer can flatten those into runtime channels.

Example:

```text
GLTF channel
  sampler 4
  target node 17
  target path rotation
       |
       v
node 17 -> joint 6
       |
       v
QuatChannel { joint = 6, times, rotations }
```

This is another example of doing format translation **once at import time** instead of every animation frame.

Determine clip duration from the maximum timestamp among its channels.

---

# 10. Checkpoint A — load and inspect a skeleton without rendering it

Before skinning a single vertex, load an animated GLTF and print:

```text
Skeleton: Soldier
Joint count: 67

0 Root parent=-1
1 Hips parent=0
2 Spine parent=1
...

Animation: Idle duration=2.03
Animation: Walk duration=1.10
```

Then validate:

- every channel target maps to a valid joint;
- inverse bind count matches the skin joint count;
- every parent index is earlier or otherwise topologically resolvable;
- animation timestamps are ordered;
- no NaNs appear in imported transforms.

Debug import problems before adding shaders.

---

# 11. Runtime AnimatorComponent

Assets are immutable.

Playback state belongs to the entity.

```cpp
struct JointTRS {
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct AnimatorComponent {
    AssetID skeleton;
    AssetID skin;
    AssetID clip;

    float time = 0.0f;
    float playbackRate = 1.0f;
    bool looping = true;
    bool playing = true;

    std::vector<JointTRS> localPose;
    std::vector<glm::mat4> globalPose;
    std::vector<glm::mat4> skinMatrices;

    uint32_t gpuPaletteOffset = UINT32_MAX;
};
```

For many characters, allocating vectors inside every component is not ideal.

It is fine for the first implementation.

Later `AnimationSystem` can own contiguous pose arenas.

---

# 12. Sampling translation and scale

Given sorted timestamps:

```text
0.0  0.4  1.0  1.6
```

and time `0.7`, find:

```text
key A = 0.4
key B = 1.0
```

Interpolation factor:

```cpp
float alpha = (time - t0) / (t1 - t0);
```

Then:

```cpp
glm::vec3 value = glm::mix(v0, v1, alpha);
```

A simple search:

```cpp
size_t find_key(const std::vector<float>& times, float t)
{
    auto it = std::upper_bound(times.begin(), times.end(), t);

    if (it == times.begin())
        return 0;

    if (it == times.end())
        return times.size() - 1;

    return static_cast<size_t>((it - times.begin()) - 1);
}
```

Later, because animation time usually moves forward, cache the previous key index to avoid binary searching every channel every frame.

Do not optimize that before the animation is correct.

---

# 13. Sampling quaternion rotation

Do not linearly interpolate quaternion components and forget normalization.

Use spherical interpolation:

```cpp
glm::quat rotation = glm::slerp(q0, q1, alpha);
```

Also remember that `q` and `-q` represent the same orientation.

Most `slerp` implementations handle shortest-path behavior, but verify your math library behavior.

Normalize imported/runtime quaternions defensively:

```cpp
rotation = glm::normalize(rotation);
```

Animation bugs caused by bad quaternion interpolation often look like a bone spinning almost 360 degrees between two apparently nearby keys.

---

# 14. STEP, LINEAR, and CUBICSPLINE

GLTF animation samplers support:

```text
STEP
LINEAR
CUBICSPLINE
```

Implement in this order.

## STEP

```cpp
return values[keyA];
```

## LINEAR

For vectors:

```cpp
return glm::mix(a, b, alpha);
```

For rotations:

```cpp
return glm::slerp(a, b, alpha);
```

## CUBICSPLINE

Do not silently treat cubic spline data as ordinary values.

GLTF stores, for each key:

```text
in tangent
value
out tangent
```

The Hermite interpolation also scales tangents by the time interval between the two keys.

For the first checkpoint, detect cubic clips and either:

1. implement the GLTF Hermite equation correctly; or
2. clearly report them unsupported rather than reading the data with the wrong stride.

After basic LINEAR animation works, add proper cubic support.

---

# 15. Building a local pose

Start every frame from the skeleton's bind pose.

```cpp
for (uint32_t j = 0; j < skeleton.joints.size(); ++j) {
    const SkeletonJoint& src = skeleton.joints[j];

    pose[j].translation = src.bindTranslation;
    pose[j].rotation = src.bindRotation;
    pose[j].scale = src.bindScale;
}
```

Then animation channels overwrite only the targeted property.

This is important because an animation clip may animate:

```text
rotation of joint 5
translation of joint 0
```

while leaving every other property at the bind value.

Do not initialize unanimated properties to zero every frame.

---

# 16. Local pose to global joint transforms

Build each local matrix:

```cpp
glm::mat4 local =
    glm::translate(glm::mat4(1.0f), trs.translation) *
    glm::mat4_cast(trs.rotation) *
    glm::scale(glm::mat4(1.0f), trs.scale);
```

Then hierarchy:

```cpp
if (joint.parent < 0)
    global[j] = local;
else
    global[j] = global[joint.parent] * local;
```

For this direct loop to work, parent joints should appear before children.

During import, either:

- construct joints in topological hierarchy order; or
- store an evaluation order array.

Do not assume arbitrary GLTF node index order gives you that guarantee.

---

# 17. The skinning matrix and coordinate spaces

This is the part most likely to cause an animated mesh to explode, rotate around the wrong origin, or appear in the wrong location.

You must define what coordinate space your runtime joint globals use.

We will use this convention:

```text
Animator globalPose[] is in the model/skeleton-root space used by the skinned asset.
Entity TransformComponent supplies the world transform separately.
```

Then the runtime skin matrix is:

```cpp
skinMatrix[j] = globalPose[j] * inverseBindMatrix[j];
```

Vertex shader later does:

```text
modelVertex
   -> skinMatrix blend
   -> skinned model-space position
   -> entity world transform
```

If you preserve GLTF joint global transforms in the original scene coordinate system instead of converting them to an asset-local skeleton space, you may need an additional inverse reference transform:

```text
inverse(skinningRootGlobal) * jointGlobal * inverseBind
```

Do **not** mix those conventions.

The importer should normalize the data into one runtime convention and document it.

A very useful debug test is bind pose:

```text
animation disabled
skin matrices generated from bind pose
```

The mesh should look identical to its authored bind pose.

If it does not, fix the coordinate spaces before sampling animation.

---

# 18. CPU animation, GPU skinning

For the first implementation:

```text
CPU
 sample clip
 build local pose
 build global pose
 build skin matrices
      |
      v
GPU storage buffer
 skinMatrices[]
      |
      v
vertex shader skinning
```

This is a good architecture because animation evaluation and skinning are different problems.

Animation asks:

> Where are the joints now?

Skinning asks:

> Given those joints, where are the vertices now?

Keep those stages separable so compute skinning can replace vertex skinning later without rewriting the animation system.

---

# 19. Skin palette allocation

Do not create one descriptor set per animated character.

Create a per-frame storage buffer containing palettes for all visible/active animated instances.

```text
SkinMatrixBuffer

character 0: matrices 0..66
character 1: matrices 67..133
character 2: matrices 134..200
```

Each `GPUObjectData` needs a palette offset or an invalid sentinel.

```cpp
struct GPUObjectData {
    glm::mat4 transform;
    glm::vec4 boundsSphere;

    uint32_t meshIndex;
    uint32_t materialIndex;

    uint32_t skinPaletteOffset;
    uint32_t flags;
};
```

Static object:

```cpp
skinPaletteOffset = UINT32_MAX;
```

Skinned object:

```cpp
skinPaletteOffset = first matrix for this animator;
```

Because frames overlap, the palette buffer should be per-frame or suballocated into frame-safe regions.

---

# 20. Skinned vertex shader

With Buffer Device Address, your skinned mesh data can remain consistent with the Chapter 6 vertex-pulling architecture.

Conceptual GLSL:

```glsl
#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

struct SkinnedVertex {
    vec3 position;
    float uv_x;

    vec3 normal;
    float uv_y;

    vec4 color;
    uvec4 joints;
    vec4 weights;
};

layout(buffer_reference, scalar) readonly buffer SkinnedVertexBuffer {
    SkinnedVertex vertices[];
};
```

Build skin transform:

```glsl
mat4 skin =
      v.weights.x * skinMatrices[palette + v.joints.x]
    + v.weights.y * skinMatrices[palette + v.joints.y]
    + v.weights.z * skinMatrices[palette + v.joints.z]
    + v.weights.w * skinMatrices[palette + v.joints.w];
```

Position:

```glsl
vec4 skinnedPosition = skin * vec4(v.position, 1.0);
vec4 worldPosition = object.transform * skinnedPosition;
```

Normals require the rotational/normal transformation as well.

For non-uniform scale you must be careful with normal matrices. For the initial character path, avoid pathological non-uniform scaling on skeleton joints and entity transforms until the basic pipeline is correct.

---

# 21. Integrating skinned meshes with the Chapter 6 GPU scene

There are two reasonable first implementations.

## Option A — separate static and skinned pipelines

```text
opaque static indirect buffer
opaque skinned indirect buffer
```

Culling classifies objects by mesh flags and writes to the appropriate list.

Advantages:

- static shader stays simple;
- static vertex layout stays unchanged;
- easier debugging.

This is what I recommend first.

## Option B — one shader with branches

The shader checks object flags and loads either static or skinned vertex data.

This reduces pipeline changes but makes data access more complex.

You can explore it later.

The renderer still remains GPU-driven either way.

You are not returning to one CPU draw per character.

---

# 22. Culling animated meshes

Chapter 6 culling assumes bounds represent the current object.

Bind-pose bounds may become invalid when arms, weapons, or limbs move outside them.

The simplest correct solution is **conservative animated bounds**.

At asset import/cook time, determine bounds that contain the animation range.

For a first version, manually inflate the bind-pose bounds:

```cpp
animatedBounds.extents *= 1.25f;
```

That is inefficient but safe.

Better later:

```text
sample clips offline
compute per-clip bounds
```

or compute bounds from bone influence volumes.

Never make culling aggressively incorrect just to save a few pixels.

A falsely visible object costs performance.

A falsely invisible object is a rendering bug.

---

# 23. Checkpoint B — one animated GLTF character

Before adding blending or physics, prove this sequence:

```text
load skinned GLTF
    |
    v
create SkeletonAsset / SkinAsset / AnimationClipAsset
    |
    v
spawn entity
    |
    +--> TransformComponent
    +--> RenderComponent
    +--> AnimatorComponent
    |
    v
sample animation on CPU
    |
    v
upload skin matrices
    |
    v
GPU-driven skinned indirect draw
```

Debug toggles should include:

```text
anim.pause
anim.speed
anim.drawSkeleton
anim.forceBindPose
r.drawAnimatedBounds
```

Bind pose and animation should both render through the same retained object system.

---

# 24. Animation blending

Do not start by building an animation state machine.

First implement a crossfade between two clips.

Sample both into two local poses:

```text
Pose A
Pose B
```

Blend per joint:

```cpp
out.translation = glm::mix(a.translation, b.translation, weight);
out.scale = glm::mix(a.scale, b.scale, weight);
out.rotation = glm::slerp(a.rotation, b.rotation, weight);
```

Then build the global pose once from the blended local pose.

Correct order:

```text
sample clips
   |
   v
blend LOCAL transforms
   |
   v
build hierarchy globals
```

Do not normally blend already-composed global matrices.

Later you can add:

```text
state machine
blend spaces
additive animation
layer masks
IK
```

but crossfade teaches the essential architecture.

---

# 25. Root motion

Some animations translate/rotate the root joint.

You have two choices.

## In-place animation

Ignore/extract root displacement and let gameplay/physics move the entity.

This is simplest for early engine integration.

## Root-motion-driven movement

Extract root delta each animation step:

```text
root position at previous time
root position at current time
      |
      v
delta transform
```

Feed that desired motion into gameplay/character physics.

Do **not** directly modify a dynamic Jolt body from the vertex animation result after physics has simulated.

Transform authority needs one clear direction, which we will define later in the chapter.

---

# 26. When to move to compute skinning

Vertex shader skinning repeats skinning work for every rendering phase:

```text
depth/shadow pass -> skin vertices
main pass         -> skin vertices again
other pass        -> skin vertices again
```

Compute skinning can do:

```text
bone palette
    |
    v
compute shader once
    |
    v
skinned vertex buffer
    |
    +--> shadow
    +--> depth
    +--> main
```

It also fits well with the GPU-driven architecture.

But compute skinning introduces:

- output-buffer allocation;
- compute-to-graphics barriers;
- per-frame skinned vertex memory;
- scheduling decisions;
- visibility questions.

Start with vertex skinning.

Move to compute only when you have enough animated geometry/passes for it to matter.

---

# Part II — Jolt Physics

# 27. Why physics must remain separate from rendering

A renderer transforms triangles.

A physics engine simulates collision shapes and constraints.

A car entity might use:

```text
render mesh: detailed car body + wheels
physics: several primitive/convex shapes
```

They are intentionally different representations.

Do not derive physics from whatever GPU buffer happens to exist at runtime.

Likewise do not store `JPH::BodyID` inside `RenderInstance`.

The world entity connects them:

```text
Entity
   +--> TransformComponent
   +--> RenderComponent
   +--> RigidBodyComponent
```

This becomes the fundamental integration pattern.

---

# 28. Adding Jolt to the CMake project

As of the current Jolt documentation, Jolt 5.6.x is available. Pin the exact release or commit you test rather than following `master` silently.

One CMake approach is `FetchContent`:

```cmake
include(FetchContent)

# These names follow Jolt's Build/CMakeLists.txt options.
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.6.0
    SOURCE_SUBDIR Build
)

FetchContent_MakeAvailable(JoltPhysics)

target_link_libraries(${PROJECT_NAME} PRIVATE Jolt)
```

You can also use vcpkg or a git submodule.

Whichever method you choose, keep the dependency version pinned.

Jolt build defines must match between your application and the library. If you enable options such as double precision, custom allocators, debug rendering, or determinism, treat those as part of your build configuration rather than random source-file defines.

---

# 29. Jolt initialization

Jolt's official HelloWorld demonstrates several required initialization pieces.

Wrap them inside your physics subsystem rather than putting them in `main.cpp` permanently.

Conceptually:

```cpp
bool PhysicsWorld::init(JobSystem& engineJobs)
{
    JPH::RegisterDefaultAllocator();

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // allocator / Jolt job system / PhysicsSystem setup follows

    return true;
}
```

Shutdown must reverse registration:

```cpp
void PhysicsWorld::shutdown()
{
    // Destroy all bodies and physics-owned state first.

    JPH::UnregisterTypes();

    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}
```

Do not hide lifetime order inside unrelated globals.

---

# 30. Collision layers

Jolt separates object layers and broad-phase layers so it can cheaply eliminate impossible collision pairs.

Start small.

```cpp
namespace PhysicsLayers {
    static constexpr JPH::ObjectLayer Static = 0;
    static constexpr JPH::ObjectLayer Dynamic = 1;
    static constexpr JPH::ObjectLayer Character = 2;
    static constexpr JPH::ObjectLayer Sensor = 3;

    static constexpr uint32_t Count = 4;
}
```

Broad phase can be even simpler:

```text
NonMoving
Moving
```

Typical rules:

```text
Static vs Static     -> no
Static vs Dynamic    -> yes
Dynamic vs Dynamic   -> yes
Character vs Static  -> yes
Character vs Dynamic -> yes
Sensor                -> based on intended query/contact behavior
```

Do not copy layer rules blindly from a sample. They are part of your game's collision design.

---

# 31. Creating PhysicsSystem

Jolt needs limits and filter interfaces during initialization.

A learning-engine configuration might start around:

```cpp
static constexpr uint32_t MaxBodies = 65536;
static constexpr uint32_t NumBodyMutexes = 0; // Jolt chooses default
static constexpr uint32_t MaxBodyPairs = 65536;
static constexpr uint32_t MaxContactConstraints = 10240;
```

Then:

```cpp
physicsSystem.Init(
    MaxBodies,
    NumBodyMutexes,
    MaxBodyPairs,
    MaxContactConstraints,
    broadPhaseLayerInterface,
    objectVsBroadPhaseFilter,
    objectLayerPairFilter
);
```

These are capacities, not performance targets.

Choose values large enough for your game and monitor failures/stats later.

---

# 32. PhysicsSystem wrapper

Avoid naming your wrapper exactly `PhysicsSystem`, because Jolt already has that class.

For example:

```cpp
class PhysicsWorld {
public:
    bool init();
    void shutdown();

    void step(float fixedDt);

    PhysicsBodyHandle create_body(const PhysicsBodyDesc& desc);
    void destroy_body(PhysicsBodyHandle handle);

    void set_kinematic_transform(PhysicsBodyHandle handle,
                                 const glm::vec3& position,
                                 const glm::quat& rotation,
                                 float dt);

    PhysicsTransform get_transform(PhysicsBodyHandle handle) const;

private:
    JPH::PhysicsSystem system;
    std::unique_ptr<JPH::TempAllocator> tempAllocator;
    std::unique_ptr<JPH::JobSystem> joltJobs;
};
```

The world should not need broad-phase details every time it moves a body.

---

# 33. RigidBodyComponent

A world component:

```cpp
enum class BodyMotion : uint8_t {
    Static,
    Kinematic,
    Dynamic
};

struct RigidBodyComponent {
    PhysicsBodyHandle body;
    AssetID collisionAsset;
    BodyMotion motion = BodyMotion::Static;

    bool interpolate = true;

    PhysicsTransform previous;
    PhysicsTransform current;
};
```

Again, we use an engine handle rather than exposing `JPH::BodyID` everywhere.

Inside `PhysicsWorld`, the handle maps to Jolt data.

This means you can validate stale handles and centralize Jolt access rules.

---

# 34. Mapping Entity to BodyID

You also need to identify which entity was involved in a contact.

Jolt body user data is useful for this.

Pack an entity identifier carefully.

For example:

```cpp
uint64_t pack_entity(Entity e)
{
    return (uint64_t(e.generation) << 32) | uint64_t(e.index);
}
```

Store it in body creation settings user data.

Then contact events can recover an `Entity` value.

You must still validate the entity against the `World`, because the body/event may be stale by the time the queued event is consumed.

---

# 35. Shape assets

Do not regenerate expensive collision geometry every time an entity spawns.

The Chapter 8 asset system can hold collision data.

```cpp
enum class CollisionShapeType : uint8_t {
    Box,
    Sphere,
    Capsule,
    ConvexHull,
    TriangleMesh
};

struct CollisionAsset {
    AssetID id;
    CollisionShapeType type;

    // Engine-side cooked description / Jolt shared shape later.
};
```

At runtime it may cache:

```cpp
JPH::ShapeRefC shape;
```

inside the physics subsystem/asset integration layer.

The world still refers to it by `AssetID`.

For a custom cooker later, collision cooking becomes another asset conversion step.

---

# 36. Static meshes, convex hulls, and dynamic bodies

Do not automatically use the render triangle mesh as the shape for every body.

A good starting policy:

```text
Static environment
    -> triangle mesh where appropriate

Dynamic props
    -> primitives / convex hull / compound convex shapes

Character
    -> capsule or similar controller shape

Vehicle
    -> purpose-built chassis/wheel collision representation
```

Detailed concave triangle geometry is often inappropriate for dynamic rigid bodies.

Collision representation should be designed for stable simulation, not visual fidelity.

For your future racing game, the road/track may use static triangle collision, while cars should use a simplified chassis shape rather than their render triangles.

---

# 37. Checkpoint C — falling box without renderer coupling

Create an entity:

```text
Entity FallingBox
  TransformComponent
  RenderComponent -> cube mesh
  RigidBodyComponent -> dynamic box shape
```

Spawn Jolt body from the transform.

Then run physics.

After each fixed step:

```text
Jolt position/rotation
      |
      v
TransformComponent
      |
      v
World render-sync
      |
      v
RenderScene.set_transform()
```

If you find yourself calling:

```cpp
renderer.set_transform(...)
```

from `PhysicsWorld::step()`, stop and move that responsibility back to world synchronization.

This checkpoint exists to prove the architecture, not just gravity.

---

# 38. Fixed timestep integration

Chapter 7 already prepared the accumulator.

Now use it.

```cpp
while (accumulator >= fixedDt) {
    world.pre_physics(fixedDt);

    physics.step(fixedDt);

    world.post_physics(fixedDt);

    accumulator -= fixedDt;
}
```

Inside physics:

```cpp
void PhysicsWorld::step(float dt)
{
    constexpr int collisionSteps = 1;

    system.Update(
        dt,
        collisionSteps,
        tempAllocator.get(),
        joltJobs.get()
    );
}
```

If tunneling or stability requires more work, investigate CCD, substeps/collision steps, or body configuration based on the actual problem rather than globally increasing every update cost.

---

# 39. World → physics synchronization

Not every body receives transforms from physics.

## Static

Created once. Normally no per-frame sync.

## Kinematic

Gameplay/world chooses the target transform.

Before the physics step:

```text
TransformComponent
      |
      v
Jolt kinematic target
```

Use Jolt's kinematic movement APIs rather than teleporting whenever you expect physically meaningful interaction.

## Dynamic

Do **not** overwrite the body from the world every frame.

Physics is authoritative after creation unless gameplay performs an explicit teleport/reset.

This distinction avoids the classic bug where a dynamic body falls one frame and is teleported back to its game transform on the next.

---

# 40. Physics → world synchronization

After the Jolt step, update dynamic entity transforms.

```cpp
for (Entity e : dynamicPhysicsEntities) {
    auto& rb = *rigidBodies.get(e);
    auto& tr = *transforms.get(e);

    rb.previous = rb.current;
    rb.current = physics.get_transform(rb.body);

    tr.position = rb.current.position;
    tr.rotation = rb.current.rotation;
    tr.worldDirty = true;
}
```

For parented dynamic rigid bodies, be careful.

Physics usually works in world space.

A simple first policy is:

> Dynamic rigid bodies are roots in the world transform hierarchy.

Support parented physics objects later only when you have a clear transform-space policy.

---

# 41. Rendering interpolation

Physics may run at 60 Hz while rendering runs at 144 Hz.

If you render only the last physics transform, motion can appear stepped.

Chapter 7 gives us:

```cpp
alpha = accumulator / fixedDt;
```

Interpolate between previous/current physics states for rendering.

```cpp
glm::vec3 p = glm::mix(previous.position, current.position, alpha);
glm::quat q = glm::slerp(previous.rotation, current.rotation, alpha);
```

Important:

This interpolated transform is a **render presentation transform**.

Do not feed it back into the physics simulation as authoritative state.

You can keep simulation transform and render transform separately if needed.

---

# 42. Sleeping bodies and dirty transforms

Jolt can put inactive bodies to sleep.

Do not iterate and upload tens of thousands of unchanged sleeping transforms if you can avoid it.

A simple first version may iterate all dynamic bodies.

Then optimize using activation state/events:

```text
active dynamic bodies
    -> sync transforms

sleeping bodies
    -> no update
```

This connects beautifully with Chapter 7's dirty render-scene updates.

A sleeping body can remain completely absent from CPU-to-GPU transform traffic.

---

# 43. Contact events

Games need events such as:

```text
car touched wall
projectile hit enemy
player entered trigger
```

Jolt supports contact listeners.

Translate callbacks into engine events:

```cpp
struct PhysicsContactEvent {
    Entity a;
    Entity b;
    enum class Type { Added, Persisted, Removed } type;
};
```

Queue them for consumption by the world/gameplay layer.

Do not expose `JPH::ContactManifold` throughout gameplay unless the gameplay system genuinely needs those low-level details.

Translate the useful information at the physics boundary.

---

# 44. Do not mutate the World directly from physics callbacks

Physics callbacks can happen during simulation and may execute in a multithreaded context depending on the callback and configuration.

This is dangerous:

```cpp
OnContactAdded(...) {
    world.destroy_entity(enemy);
}
```

Instead:

```text
Jolt callback
   |
   v
thread-safe PhysicsEvent queue
   |
   v
after physics step
   |
   v
World consumes events
```

Now entity destruction happens at a controlled engine point.

This same command/event pattern was introduced for asset jobs in Chapter 8.

You are starting to see a recurring engine pattern:

> Do work in specialized systems; publish results; mutate shared world state at controlled boundaries.

---

# 45. Jolt's JobSystem and your JobSystem

Jolt's simulation step uses a `JPH::JobSystem`.

The official example provides `JobSystemThreadPool`, which is ideal for getting started.

Use it first.

```cpp
joltJobs = std::make_unique<JPH::JobSystemThreadPool>(
    JPH::cMaxPhysicsJobs,
    JPH::cMaxPhysicsBarriers,
    workerCount
);
```

Later, when your own Chapter 7 job system becomes more capable, you can implement an adapter deriving from `JPH::JobSystem` so Jolt submits tasks into the same global scheduler.

Do not do that immediately.

A correct scheduler adapter must honor Jolt's dependency/barrier semantics, job lifetime, and waiting behavior.

Use the provided thread pool until you have a reason to unify them.

---

# 46. Character controllers

Jolt offers both `Character` and `CharacterVirtual` approaches.

For a player-style character, `CharacterVirtual` is often attractive because movement can be evaluated at a controlled point in the game frame and includes functionality for wall sliding, slopes, moving platforms, and related character behavior.

It is not just a normal rigid body.

That distinction matters.

For your engine, do not use a dynamic capsule plus huge forces just because it seems easy.

When you later create a third-person or first-person character, study `CharacterVirtual` and its samples.

For your planned racing game, vehicle physics will be more relevant than a character controller, so treat this as optional exploration after the base rigid-body integration.

---

# 47. Debug drawing

Physics integration without visualization is unnecessarily painful.

You want to see:

```text
collision boxes
capsules
convex hulls
contact points
broadphase bounds if useful
```

Do not make Jolt's debug renderer directly own Vulkan pipelines.

Create an engine debug-draw interface:

```cpp
class DebugDraw {
public:
    void line(glm::vec3 a, glm::vec3 b, glm::vec4 color);
    void box(...);
    void sphere(...);
};
```

Jolt adapter writes debug primitives into it.

Renderer draws the resulting line/triangle buffers.

Then animation can use the same service to draw skeleton bones.

One debug-draw system becomes useful across the whole engine.

---

# Part III — Putting both systems together

# 48. Transform authority

Once animation and physics coexist, establish explicit rules.

For each entity, ask:

> Who owns the entity root transform this frame?

A useful policy:

| Object type | Root transform authority | Bone pose authority |
|---|---|---|
| Static prop | World | none |
| Dynamic rigid body | Physics | none |
| Kinematic platform | Gameplay/World -> Physics | none |
| Animated NPC | Gameplay/character controller | Animation |
| Animated dynamic ragdoll | Physics | Physics/ragdoll |
| Decorative animated object | World | Animation |

Do not let two systems continuously overwrite the same transform.

This table is more important than any individual API call.

---

# 49. Animated characters with simple collision

Do not begin skeletal animation + physics integration by making every bone a rigid body.

Start with:

```text
Entity root
   |
   +--> Character/kinematic collision shape
   |
   +--> AnimatorComponent
   |
   +--> skinned RenderComponent
```

Physics/gameplay moves the root.

Animation poses the skeleton relative to that root.

Renderer does:

```text
world root transform
    *
skinned model-space vertex
```

This covers the majority of ordinary animated game characters.

---

# 50. Ragdolls later

Ragdolls introduce a different authority model.

Instead of:

```text
animation -> bones
```

it becomes:

```text
physics bodies -> bone transforms
```

Often games blend between them:

```text
animation pose
     |
     +--> motor/constraint targets
     |
physics ragdoll pose
     |
     v
rendered skeleton
```

Do not implement this until:

- ordinary animation works;
- ordinary rigid bodies work;
- transform authority is clear;
- skeleton debug drawing works;
- physics debug drawing works.

Jolt has ragdoll support, but it should be treated as a later engine feature, not the first physics milestone.

---

# 51. The final engine frame

At the end of Chapter 9 your main loop can conceptually look like:

```cpp
while (running) {
    begin_frame_time();

    poll_events();

    // Variable-rate gameplay.
    world.update(frameDt);

    // Evaluate input / desired animation states.
    animation.update_playback_state(world, frameDt);

    accumulator += frameDt;

    while (accumulator >= fixedDt) {
        // Gameplay-controlled/kinematic transforms are pushed to physics.
        world.sync_kinematics_to_physics(physics, fixedDt);

        physics.step(fixedDt);

        // Dynamic bodies publish authoritative transforms back to World.
        world.sync_dynamics_from_physics(physics);

        accumulator -= fixedDt;
    }

    float alpha = float(accumulator / fixedDt);

    // Sample/evaluate skeletal poses for the frame we will present.
    animation.evaluate(world, alpha);

    // Publish dirty root transforms and animation palette data.
    world.sync_render_state(renderer, alpha);
    animation.upload_skin_palettes(renderer);

    renderer.render(build_main_view(alpha));
}
```

Exact ordering may change with your game.

The essential data directions should remain clear.

---

# 52. Multithreading opportunities

Do not parallelize everything immediately.

Once profiling shows animation CPU cost, pose evaluation is naturally parallel across independent characters:

```text
Animator 0 ---- job
Animator 1 ---- job
Animator 2 ---- job
Animator 3 ---- job
```

Then join before skin-palette upload.

Jolt already runs internal physics jobs through its `JobSystem`.

Asset loading already uses Chapter 7/8 background jobs.

A future frame may therefore use CPU cores like:

```text
Main thread
   gameplay + orchestration

Worker pool
   animation jobs
   asset decode/import
   AI/pathfinding later

Jolt job workers
   physics simulation
```

Later you can unify schedulers if thread oversubscription becomes a measured problem.

---

# 53. Common bugs and how to diagnose them

## Mesh explodes when animation starts

Check:

```text
joint index remapping
inverse bind matrix order
matrix multiplication order
mesh/skeleton root coordinate space
JOINTS integer conversion
weight normalization
```

First force bind pose.

If bind pose is already wrong, animation sampling is not the problem.

## Character appears at origin while entity is elsewhere

You probably mixed world-space joint matrices with a separate object transform.

Revisit the convention from section 17.

## Bones rotate the long way around

Check quaternion interpolation and normalization.

## Static mesh works but skinned mesh indices are corrupted

Check BDA struct packing and `scalar`/alignment agreement between C++ and GLSL.

## Animated object disappears while moving arms

Bounds are too tight.

Disable culling or inflate animated bounds to confirm.

## Dynamic body keeps snapping back

You are likely syncing World -> Physics every frame for a dynamic body.

Dynamic bodies should normally synchronize Physics -> World.

## Visible physics jitter at high refresh rate

Add previous/current physics transforms and rendering interpolation.

## Crash when destroying entities during contacts

Queue physics events and destroy entities after the physics update instead of inside callbacks.

## Jolt link/runtime mismatch

Check build configuration defines between your target and Jolt library.

## CPU usage increases after adding Jolt even with an empty scene

Check how many worker threads your own job system plus Jolt thread pool created. Avoid extreme oversubscription.

---

# 54. File-by-file implementation plan

## Asset layer

### `assets/skeleton_asset.h`

```text
SkeletonJoint
SkeletonAsset
SkinAsset
```

### `assets/animation_asset.h`

```text
AnimationClipAsset
translation channels
rotation channels
scale channels
interpolation enum
```

### `assets/collision_asset.h`

```text
CollisionShapeType
CollisionAsset
```

### `assets/importers/gltf_importer.*`

Add parsing/conversion for:

```text
JOINTS_0
WEIGHTS_0
skin.joints
inverseBindMatrices
animations
samplers
channels
```

Keep fastgltf details inside importer code.

---

## World layer

### `world/components/animator_component.h`

Playback/runtime pose state.

### `world/components/rigid_body_component.h`

Engine physics handle, motion type, previous/current physics transforms.

### `world/world.cpp`

Add controlled stages:

```text
sync_kinematics_to_physics
sync_dynamics_from_physics
consume_physics_events
sync_render_state
```

---

## Animation system

### `animation/animation_system.h/.cpp`

Responsibilities:

```text
resolve animation assets
advance playback
sample channels
blend local poses
build global pose
build skin matrices
allocate/upload frame palettes
```

Do not make it create Vulkan buffers directly. Use renderer-facing palette upload API.

---

## Renderer

### `renderer/render_scene.*`

Extend render instances/GPU object flags with skinned-object metadata.

### `renderer/renderer.h`

Add an API such as:

```cpp
SkinPaletteAllocation allocate_skin_palette(uint32_t matrixCount);
void upload_skin_palette(SkinPaletteAllocation dst,
                         std::span<const glm::mat4> matrices);
```

### GPU culling shader

Classify or output skinned draw commands separately from static commands.

### New shaders

```text
gpu_skinned.vert
```

Optionally later:

```text
skin_vertices.comp
```

---

## Physics

### `physics/physics_world.h/.cpp`

Own all Jolt global/system state.

### `physics/physics_layers.h/.cpp`

Implement object-layer and broad-phase filters.

### `physics/physics_handle.h`

Typed engine `PhysicsBodyHandle`.

### `physics/jolt_conversion.h`

Centralize conversions:

```cpp
glm::vec3 <-> JPH::Vec3
glm::quat <-> JPH::Quat
```

Do not sprinkle conversion code throughout gameplay.

### `physics/physics_events.h`

Engine-facing contact event types/queue.

---

## CMake

Pin Jolt release/commit and link `Jolt`.

Keep Jolt build options centralized in your root CMake configuration.

---

# 55. Final architecture

After Chapter 9:

```text
                                  AssetRegistry
                                      |
                                      v
                                  AssetManager
                                      |
              +-----------------------+-----------------------+
              |                       |                       |
              v                       v                       v
         Mesh/Material          Skeleton/Animation      CollisionAsset
              |                       |                       |
              |                       v                       v
              |                 AnimationSystem          PhysicsWorld
              |                       |                       |
              |                       | bone poses            | simulation
              |                       v                       v
              |                AnimatorComponent      RigidBodyComponent
              |                       |                       |
              +-----------------------+-----------+-----------+
                                                  |
                                                  v
                                                World
                                                  |
                                      TransformComponents
                                                  |
                                                  v
                                           RenderScene
                                                  |
                        +-------------------------+---------------------+
                        |                                               |
                        v                                               v
                 GPUObjectData[]                               SkinMatrixBuffer
                        |                                               |
                        +----------------------+------------------------+
                                               |
                                               v
                                         GPU culling
                                               |
                               +---------------+---------------+
                               |                               |
                               v                               v
                         static indirect                 skinned indirect
                               |                               |
                               +---------------+---------------+
                                               |
                                               v
                                      Dynamic Rendering
```

The important achievement is not merely that a character animates or a box falls.

The achievement is that both features joined the engine through stable boundaries.

---

# 56. What to build after Chapter 9

At this point you have enough engine architecture to begin building actual game systems.

A strong continuation would be:

```text
Chapter 10 — Lighting, shadows, and a small render graph
Chapter 11 — Editor/debug tooling and serialization improvements
Chapter 12 — Audio + input abstraction
Chapter 13 — Gameplay framework / scripting boundary
Chapter 14 — Vehicle physics and racing-game foundations
Chapter 15 — AI racing line, navigation, and opponent controllers
```

Before chasing mesh shaders or a sophisticated asset cooker, build a tiny playable test:

```text
load level
spawn controllable object
collide with environment
play an animation
switch level
reload assets
```

That will stress the engine boundaries far more usefully than another isolated rendering feature.

---

# 57. References

## GLTF skinning and animation

Khronos glTF 2.0 specification — skins, joints, `JOINTS_n` / `WEIGHTS_n`, animations, channels, samplers, and interpolation:

- https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html

Khronos glTF skinning tutorial, including inverse bind matrices and the weighted joint-matrix calculation:

- https://github.khronos.org/glTF-Tutorials/gltfTutorial/gltfTutorial_020_Skins.html

Khronos Vulkan tutorial material on skeletal compute skinning is useful later when moving beyond vertex-shader skinning:

- https://github.khronos.org/Vulkan-Site/tutorial/latest/Advanced_glTF/Skeletal_Compute_Skinning/02_skinning_math.html

## Jolt Physics

Official Jolt architecture/API documentation:

- https://jrouwe.github.io/JoltPhysics/

Official Jolt HelloWorld example showing allocator registration, factory/type registration, temp allocator, `JobSystemThreadPool`, `PhysicsSystem`, body creation, update, and shutdown:

- https://github.com/jrouwe/JoltPhysics/blob/master/HelloWorld/HelloWorld.cpp

Versioned Jolt documentation archive:

- https://jrouwe.github.io/JoltPhysicsDocs/

Official Jolt repository/build instructions:

- https://github.com/jrouwe/JoltPhysics
- https://github.com/jrouwe/JoltPhysics/blob/master/Build/README.md

## VKGuide architecture background

Project Ascendant discusses growing the tutorial renderer into a real engine and includes Jolt, retained render objects, animation, and wider engine-system refactoring:

- https://vkguide.dev/docs/ascendant/from_tutorial_to_engine/

VKGuide multithreading article:

- https://vkguide.dev/docs/extra-chapter/multithreading/
