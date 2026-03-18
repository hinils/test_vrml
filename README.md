# VRML Viewer

A cross-platform VRML (Virtual Reality Modeling Language) viewer with OpenGL 3.3 rendering.

## Features

- **VRML 1.0 & 2.0 Support** - Parse and render both VRML versions
- **OpenGL 3.3 Core** - Modern rendering pipeline with shaders
- **Interactive Camera** - Arcball-style orbit, pan, and zoom controls
- **Material Support** - Diffuse, specular, emissive colors with lighting
- **Wireframe Mode** - Toggle between solid and wireframe rendering

## Requirements

- CMake 3.16+
- C++17 compiler
- OpenGL 3.3+

## Build

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Usage

```bash
# View a VRML file
./vrml_viewer path/to/model.wrl

# Without arguments, shows a built-in cube demo
./vrml_viewer
```

### Controls

| Input | Action |
|-------|--------|
| Left-drag | Orbit camera |
| Right-drag | Pan camera |
| Scroll | Zoom in/out |
| R | Reset camera |
| W | Toggle wireframe |
| Q / Esc | Quit |

## Project Structure

```
vrml_viewer/
├── vrml_parser/          # VRML parser library
│   ├── include/          # Public headers
│   └── src/              # Parser implementation
├── viewer/               # OpenGL viewer application
│   └── src/              # Main application
├── samples/              # Sample VRML files
├── test_models/          # Test models for development
└── deps/                 # Dependencies (glad)
```

## Supported VRML Nodes

- `IndexedFaceSet` - Polygonal geometry
- `Coordinate3` / `Coordinate` - Vertex positions
- `Normal` / `NormalBinding` - Vertex normals
- `Material` / `MaterialBinding` - Surface materials
- `Shape` / `Appearance` - VRML 2.0 appearance nodes

## Dependencies

- [GLFW](https://www.glfw.org/) - Window and input handling
- [GLM](https://github.com/g-truc/glm) - Mathematics library
- [glad](https://glad.dav1d.de/) - OpenGL loader

Dependencies are automatically fetched via CMake FetchContent.

## License

MIT License
