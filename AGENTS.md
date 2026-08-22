# AGENTS.md

C++20 Vulkan renderer, a personal learning project extended from the vkguide tutorial (vk_engine/vk_types/vk_initializers layout). Extended with GLTF loading (fastgltf), a scene graph, and PBR-style materials. Code style favors clear, commented, teaching-oriented code.

## Working rules
- Never commit or push. Read git history / blame freely.
- Do not create any files or folders without asking first.
- Do not invoke any agent or subagent unless explicitly instructed by the user.
- Only touch `CMakeLists.txt` when asked; do not run the build (cmake/msbuild) unless explicitly requested.

## Build (CMake)
- Configured build tree: `build/`, generator Visual Studio 17 2022, multi-config (Debug/Release). Binaries and copied DLLs land in `bin/`.
- Executable target: `engine` (defined in `src/CMakeLists.txt`, added source files must be listed there).
- Shaders: `shaders/*.vert|.frag|.comp` are compiled to sibling `.spv` files by the custom target `Shaders` via `glslangValidator` (located through `$VULKAN_SDK`/Bin). `.spv` files are gitignored, so after a fresh clone or any shader edit you must build the `Shaders` target before `engine`. The `Shaders` target is NOT part of ALL_BUILD.
- Requires a Vulkan SDK install (`find_package(Vulkan)` and glslangValidator).

## Architecture
- Entry: `main.cpp` -> `VulkanEngine::init()` -> `run()` -> `cleanup()`. `VulkanEngine` (src/vk_engine.h) owns instance/device/swapchain, per-frame `FrameData`, deletion queues, descriptors, pipelines.
- Shader/C++ interface sync: `shaders/input_structures.glsl` mirrors C++ structs. Keep `GPUSceneData` (vk_types.h) and `MaterialConstants` (in `GLTF_MetallicRoughMaterial`, vk_engine.h) in lockstep with it. `MaterialConstants` is padded to a 256-byte multiple for buffer alignment.
- Assets: `.glb` files in `assets/`, loaded by `vkutil::loadGLTF` / `loadMeshFromFile` (src/vk_loader.cpp). Register loaded scenes in `VulkanEngine::init_Loaded_Scenes()`. Paths come from `ASSET_PATH` / `SHADER_PATH` compile definitions.
- Draw-list renderer: scene nodes (`Node` / `MeshNode` / `LoadedGLTF`, src/vk_renderTypes.h) push `RenderObject`s into a `DrawContext` with separate `opaqueMeshObjects` and `transparentMeshObjects`; `draw_Geometry` draws opaque first, then transparent.
- Resource lifetime: VMA-backed buffers/images; frame-scoped Vulkan objects go through `deletionQueue` (`_frameDeletionQueue` flushed each frame, `_mainDeletionQueue` at shutdown). Don't free Vulkan objects ad hoc; use the queues.
- Error handling: use the `VK_CHECK(x)` macro (vk_types.h).

## Style
- Naming: `init_*` / `create_*` / `destroy_*` / `draw_*` for methods, members prefixed `_`, functions `camelCase_With_Underscore`, files/types `snake_case` with `vk_` prefix.
- Logging with fmt (`fmt::println`), `std::span` for mesh data, C++20 with precompiled headers (`target_precompile_headers`).

## Vendored third_party
- All deps vendored under `third_party/` (SDL2, imgui, fastgltf, fmt, glm, VMA, vkbootstrap, stb_image). Don't modify vendored code. New libraries are wired via `third_party/CMakeLists.txt` (target_sources / include dirs) — note several are INTERFACE/STATIC libs defined there.
