# PlagC

A hobby game engine written in modern C++ with OpenGL, built to explore real-world engine architecture, rendering, data-oriented design, and systems programming.

> **Status:** Early development

---

## Goals

PlagC is a long-term learning project focused on understanding how modern game engines are built from the ground up rather than competing with production engines.

Current areas of focus include:

- Modern C++ architecture
- OpenGL rendering
- Engine/editor separation
- Entity Component System (EnTT)
- Data-oriented design
- Rendering pipeline
- Asset management
- Multithreading
- Engine tooling

---

## Current Features

- GLFW window creation
- OpenGL rendering context
- Event system
- Layer system
- Logging system (spdlog)
- ImGui integration
- Premake5 build system
- Sandbox application

---

## Planned Features

### Rendering

- 3D renderer
- Camera system
- Mesh rendering
- Materials
- Lighting
- Shadow mapping
- PBR pipeline
- Post-processing

### Engine

- ECS (EnTT)
- Scene system
- Serialization
- Asset manager
- Resource hot reloading

### Editor

- Scene hierarchy
- Inspector
- Content browser
- Gizmos
- Docking interface

### Future

- Animation
- Physics integration
- Audio
- Lua scripting
- Multithreaded renderer

---

## Third-Party Libraries

- GLFW
- GLAD
- GLM
- ImGui
- spdlog
- Premake5
- stb_image
- EnTT (planned)

---

## Building

```bash
git clone https://github.com/Muhammad-Uzair-Kayani/PlagC.git
cd PlagC
```

Generate the Visual Studio solution:

```
Build/Build.bat
```

Open the generated solution and build the `Sandbox` project.

---

## Project Structure

```
PlagC/
├── PlagC/          # Engine source
├── Sandbox/        # Example application
├── Build/          # Build scripts
├── vendor/         # Third-party libraries
└── premake5.lua
```

---

## Roadmap

This repository is under active development. Features are added incrementally as I learn and implement engine architecture concepts.

Progress is documented through Git commits.

---

## License

This project is licensed under the MIT License.