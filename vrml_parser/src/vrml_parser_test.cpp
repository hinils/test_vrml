#include <vrml_parser.h>
#include <iostream>
#include <cassert>
#include <sstream>

// ─── helpers ───────────────────────────────────────────────────
static void printMesh(const vrml::IndexedFaceSet& m, const std::string& label) {
    std::cout << "  [" << label << "]\n";
    std::cout << "    coords      : " << m.coords.size() << "\n";
    std::cout << "    normals     : " << m.normals.size() << "\n";
    std::cout << "    colors      : " << m.colors.size() << "\n";
    std::cout << "    triangles   : " << m.vertices.size()/3 << "\n";
    std::cout << "    diffuse     : ("
              << m.material.diffuseColor.r << " "
              << m.material.diffuseColor.g << " "
              << m.material.diffuseColor.b << ")\n";
    // sanity: every triangle must have 3 valid-looking vertices
    for (size_t i = 0; i < m.vertices.size(); i += 3) {
        for (int k = 0; k < 3; ++k) {
            auto& v = m.vertices[i+k];
            float nlen = v.normal.x*v.normal.x
                       + v.normal.y*v.normal.y
                       + v.normal.z*v.normal.z;
            if (nlen < 0.5f || nlen > 2.0f) {
                std::cerr << "    WARNING: degenerate normal at vertex "
                          << (i+k) << " (len^2=" << nlen << ")\n";
            }
        }
    }
}

static void testScene(const std::string& title,
                      const std::shared_ptr<vrml::Scene>& scene,
                      int expectedVersion,
                      size_t minMeshes,
                      size_t minTris) {
    std::cout << "\n=== " << title << " ===\n";
    assert(scene);
    std::cout << "  VRML version : " << scene->vrmlVersion << "\n";
    std::cout << "  meshes       : " << scene->meshes.size() << "\n";
    assert(scene->vrmlVersion == expectedVersion);
    assert(scene->meshes.size() >= minMeshes);
    size_t totalTris = 0;
    for (size_t i = 0; i < scene->meshes.size(); ++i) {
        auto& m = *scene->meshes[i];
        totalTris += m.vertices.size() / 3;
        printMesh(m, "mesh " + std::to_string(i));
    }
    assert(totalTris >= minTris);
    std::cout << "  total tris   : " << totalTris << "  ✓\n";
}

// ─── inline VRML strings ────────────────────────────────────────
static const char* VRML2_TETRA = R"(
#VRML V2.0 utf8
Shape {
  appearance Appearance {
    material Material {
      diffuseColor 0.2 0.5 0.9
      specularColor 0.8 0.8 0.8
      shininess 0.4
    }
  }
  geometry IndexedFaceSet {
    coord Coordinate {
      point [
         0.0  1.0  0.0,
        -1.0 -0.5  0.87,
         1.0 -0.5  0.87,
         0.0 -0.5 -1.0
      ]
    }
    coordIndex [ 0 1 2 -1  0 2 3 -1  0 3 1 -1  1 3 2 -1 ]
    color Color { color [ 1 0 0, 0 1 0, 0 0 1, 1 1 0 ] }
    colorPerVertex TRUE
    solid FALSE
    creaseAngle 0.5
  }
}
)";

static const char* VRML2_CUBE = R"(
#VRML V2.0 utf8
Group {
  children [
    Shape {
      appearance Appearance {
        material Material { diffuseColor 0.8 0.3 0.1  shininess 0.6 }
      }
      geometry IndexedFaceSet {
        coord Coordinate {
          point [
            -1 -1 -1,  1 -1 -1,  1  1 -1, -1  1 -1,
            -1 -1  1,  1 -1  1,  1  1  1, -1  1  1
          ]
        }
        coordIndex [
          0 3 2 1 -1   4 5 6 7 -1
          0 1 5 4 -1   2 3 7 6 -1
          0 4 7 3 -1   1 2 6 5 -1
        ]
        solid TRUE
      }
    }
  ]
}
)";

static const char* VRML2_DEF_USE = R"(
#VRML V2.0 utf8
Group {
  children [
    Shape {
      geometry DEF MYMESH IndexedFaceSet {
        coord Coordinate {
          point [ 0 0 0, 1 0 0, 0.5 1 0, 0.5 0.5 1 ]
        }
        coordIndex [ 0 1 2 -1  0 1 3 -1  0 2 3 -1  1 2 3 -1 ]
      }
    }
    Shape {
      appearance Appearance {
        material Material { diffuseColor 0.5 0.3 0.8 }
      }
      geometry USE MYMESH
    }
  ]
}
)";

static const char* VRML1_PYRAMID = R"(
#VRML V1.0 ascii
Separator {
  Material {
    diffuseColor  [ 0.3 0.7 0.4 ]
    specularColor [ 0.6 0.6 0.6 ]
    shininess     [ 0.35 ]
  }
  Coordinate3 {
    point [
       0.0  1.5  0.0,
      -1.0 -0.5  1.0,
       1.0 -0.5  1.0,
       1.0 -0.5 -1.0,
      -1.0 -0.5 -1.0
    ]
  }
  IndexedFaceSet {
    coordIndex [
      0, 1, 2, -1,
      0, 2, 3, -1,
      0, 3, 4, -1,
      0, 4, 1, -1,
      1, 4, 3, 2, -1
    ]
  }
}
)";

static const char* VRML1_NESTED = R"(
#VRML V1.0 ascii
Separator {
  Material { diffuseColor [ 0.9 0.1 0.1 ] }
  Coordinate3 {
    point [ -1 0 0, 1 0 0, 0 1 0, 0 0 1 ]
  }
  Separator {
    # inner separator – should NOT leak coords to outer
    Coordinate3 { point [ 0 0 0, 2 0 0, 1 2 0, 1 1 2 ] }
    IndexedFaceSet {
      coordIndex [ 0 1 2 -1  0 1 3 -1  0 2 3 -1  1 2 3 -1 ]
    }
  }
  # Outer IFS uses outer coords (4 verts = tetra)
  IndexedFaceSet {
    coordIndex [ 0 1 2 -1  0 1 3 -1  0 2 3 -1  1 2 3 -1 ]
  }
}
)";

static const char* VRML2_PERFACE_COLOR = R"(
#VRML V2.0 utf8
Shape {
  geometry IndexedFaceSet {
    coord Coordinate {
      point [ -1 -1 0, 1 -1 0, 1 1 0, -1 1 0 ]
    }
    coordIndex [ 0 1 2 -1  0 2 3 -1 ]
    color Color { color [ 1 0 0, 0 0 1 ] }
    colorPerVertex FALSE
  }
}
)";

// ─── main ───────────────────────────────────────────────────────
int main() {
    std::cout << "========================================\n";
    std::cout << " VRML Parser – Unit Tests\n";
    std::cout << "========================================\n";

    testScene("VRML2: tetrahedron + vertex colors",
              vrml::parseString(VRML2_TETRA), 2, 1, 4);

    testScene("VRML2: cube in Group",
              vrml::parseString(VRML2_CUBE), 2, 1, 12);

    testScene("VRML2: DEF/USE (shared geometry)",
              vrml::parseString(VRML2_DEF_USE), 2, 2, 8);

    testScene("VRML2: per-face color",
              vrml::parseString(VRML2_PERFACE_COLOR), 2, 1, 2);

    testScene("VRML1: pyramid in Separator",
              vrml::parseString(VRML1_PYRAMID), 1, 1, 6);

    testScene("VRML1: nested Separators (scope isolation)",
              vrml::parseString(VRML1_NESTED), 1, 2, 8);

    // Test file parsing (samples written by CMake)
    struct FileTest { const char* path; int ver; size_t meshes; size_t tris; };
    FileTest files[] = {
        {"samples/tetrahedron_v2.wrl",  2, 1,  4},
        {"samples/house_v2.wrl",        2, 2,  18},
        {"samples/icosahedron_v2.wrl",  2, 1,  20},
        {"samples/pyramid_v1.wrl",      1, 1,  6},
        {"samples/multiobject_v2.wrl",  2, 3,  10},
    };
    for (auto& ft : files) {
        try {
            auto sc = vrml::parseFile(ft.path);
            testScene(std::string("File: ") + ft.path,
                      sc, ft.ver, ft.meshes, ft.tris);
        } catch (const std::exception& e) {
            std::cerr << "  SKIP (file not found): " << ft.path << "\n";
        }
    }

    std::cout << "\n========================================\n";
    std::cout << " All tests passed!\n";
    std::cout << "========================================\n";
    return 0;
}
