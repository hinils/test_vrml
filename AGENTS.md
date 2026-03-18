# VRML Viewer - Project Context

## Project Overview

A cross-platform VRML (Virtual Reality Modeling Language) viewer with OpenGL 3.3 rendering. This project parses and renders both VRML 1.0 and VRML 2.0 files, providing an interactive 3D viewing experience.

### Key Technologies
- **Language**: C++17
- **Build System**: CMake 3.16+
- **Graphics**: OpenGL 3.3 Core Profile
- **Window/Input**: GLFW
- **Math**: GLM
- **OpenGL Loader**: glad

### Architecture

```
vrml_viewer/
├── vrml_parser/           # Static library for VRML parsing
│   ├── include/
│   │   └── vrml_parser.h  # Public API: Scene, IndexedFaceSet, Material
│   └── src/
│       ├── lexer.h        # Tokenizer for VRML syntax
│       ├── vrml1_parser.h # VRML 1.0 parser (state-stack based)
│       ├── vrml2_parser.h # VRML 2.0 parser (scene-graph based)
│       ├── vrml_parser.cpp # Triangulation & entry points
│       └── vrml_parser_test.cpp # Unit tests
├── viewer/
│   └── src/
│       └── main.cpp       # OpenGL viewer application
├── samples/               # Sample VRML files for testing
├── test_models/           # Additional test models
└── deps/glad/             # OpenGL loader
```

## Building

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build (Release mode recommended)
cmake --build . --config Release
```

### Build Outputs
- `build/Release/vrml_viewer.exe` - Main viewer application
- `build/Release/vrml_parser_test.exe` - Parser unit tests

## Running

```bash
# View a VRML file
./vrml_viewer path/to/model.wrl

# Without arguments (shows built-in cube demo)
./vrml_viewer
```

### Viewer Controls
| Input | Action |
|-------|--------|
| Left-drag | Orbit camera |
| Right-drag | Pan camera |
| Scroll | Zoom in/out |
| R | Reset camera |
| W | Toggle wireframe |
| Q / Esc | Quit |

## Supported VRML Nodes

### Geometry
- `IndexedFaceSet` - Polygonal geometry (triangulated automatically)
- `Coordinate3` / `Coordinate` - Vertex positions
- `Normal` / `NormalBinding` - Vertex normals
- `Color` - Vertex/face colors

### Materials
- `Material` - Surface properties (diffuse, specular, emissive, shininess)
- `MaterialBinding` - VRML 1.0 material binding
- `Appearance` - VRML 2.0 appearance container

### Structure
- `Separator` (VRML 1.0) - State scoping node
- `Transform` / `Group` (VRML 2.0) - Hierarchy with translation/rotation/scale
- `Shape` (VRML 2.0) - Geometry + appearance container
- `DEF` / `USE` - Node naming and reuse

## Parser Implementation Details

### VRML 1.0 Parser (`vrml1_parser.h`)
- State-stack based traversal
- `Separator` nodes push/pop state (coordinates, materials, normals)
- Supports: `Coordinate3`, `Normal`, `Material`, `IndexedFaceSet`, `TextureCoordinate2`

### VRML 2.0 Parser (`vrml2_parser.h`)
- Scene-graph based traversal
- Transform hierarchy with matrix stack
- Supports: `Shape`, `Appearance`, `Material`, `IndexedFaceSet`, `Coordinate`, `Normal`, `Color`
- Handles `DEF`/`USE` for geometry and materials

### Triangulation (`vrml_parser.cpp`)
- Converts polygon faces to triangles (fan triangulation)
- Computes flat or smooth normals based on `creaseAngle`
- Handles per-vertex and per-face attributes (normals, colors, UVs)

## Testing

### Run Parser Unit Tests
```bash
./vrml_parser_test
```

### Batch Test Sample Files
```bash
# Windows
test_samples.bat
```

## Development Conventions

### Code Style
- C++17 features (structured bindings, `std::optional`, etc.)
- Namespace: `vrml` for public API, `vrml::detail` for internals
- Header-only internal parsers (`.h` files in `src/`)
- Inline GLSL shaders in `main.cpp` (avoid file path issues)

### Adding New VRML Nodes
1. Add parsing logic in `vrml1_parser.h` or `vrml2_parser.h`
2. Update `IndexedFaceSet` struct in `vrml_parser.h` if new data needed
3. Add triangulation support in `vrml_parser.cpp` if geometry-related
4. Add test case in `vrml_parser_test.cpp`

### Debugging Tips
- Parser errors print to stderr with line numbers
- Use `g_wireframe` toggle (W key) to check geometry
- Check `vertices.size() / 3` for triangle count
- Normal validation in `vrml_parser_test.cpp` checks for degenerate normals

## Dependencies

All dependencies are fetched automatically via CMake FetchContent:
- **GLFW 3.3.8** - Window and input handling
- **GLM 0.9.9.8** - Mathematics library
- **glad** - OpenGL loader (bundled in `deps/`)

## File Formats

### VRML 1.0
```
#VRML V1.0 ascii
Separator {
  Material { diffuseColor [ 0.5 0.5 0.5 ] }
  Coordinate3 { point [ 0 0 0, 1 0 0, 0 1 0 ] }
  IndexedFaceSet { coordIndex [ 0, 1, 2, -1 ] }
}
```

### VRML 2.0
```
#VRML V2.0 utf8
Shape {
  appearance Appearance {
    material Material { diffuseColor 0.5 0.5 0.5 }
  }
  geometry IndexedFaceSet {
    coord Coordinate { point [ 0 0 0, 1 0 0, 0 1 0 ] }
    coordIndex [ 0 1 2 -1 ]
  }
}
```

## Common Tasks

### Add a new sample file
1. Place `.wrl` or `.vrml` file in `samples/`
2. Test with: `./vrml_viewer samples/new_file.wrl`

### Modify shader
- Edit `kVertSrc` or `kFragSrc` in `viewer/src/main.cpp`
- Rebuild: `cmake --build build --config Release`

### Add parser feature
1. Modify lexer if new token types needed (`lexer.h`)
2. Add parsing in appropriate parser (`vrml1_parser.h` or `vrml2_parser.h`)
3. Update data structures in `vrml_parser.h`
4. Add triangulation logic in `vrml_parser.cpp`
5. Write tests in `vrml_parser_test.cpp`
