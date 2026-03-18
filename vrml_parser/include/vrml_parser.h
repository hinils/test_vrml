#pragma once
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <cmath>

namespace vrml {

// ─────────────────────────────────────────────────────────────────
//  Math helpers
// ─────────────────────────────────────────────────────────────────
struct Vec2f { float x = 0, y = 0; };

struct Vec3f {
    float x = 0, y = 0, z = 0;
    Vec3f operator+(const Vec3f& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3f operator-(const Vec3f& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3f operator*(float s)        const { return {x*s,   y*s,   z*s};   }
    Vec3f& operator+=(const Vec3f& o){ x+=o.x; y+=o.y; z+=o.z; return *this; }
    float  dot  (const Vec3f& o) const { return x*o.x+y*o.y+z*o.z; }
    Vec3f  cross(const Vec3f& o) const {
        return {y*o.z-z*o.y, z*o.x-x*o.z, x*o.y-y*o.x};
    }
    float  length()     const { return std::sqrt(x*x+y*y+z*z); }
    Vec3f  normalized() const {
        float l = length();
        return l > 1e-9f ? Vec3f{x/l, y/l, z/l} : Vec3f{0,1,0};
    }
};

struct Color3f { float r = 0.8f, g = 0.8f, b = 0.8f; };

// ─────────────────────────────────────────────────────────────────
//  Material
// ─────────────────────────────────────────────────────────────────
struct Material {
    Color3f diffuseColor     = {0.8f, 0.8f, 0.8f};
    Color3f specularColor    = {0.0f, 0.0f, 0.0f};
    Color3f emissiveColor    = {0.0f, 0.0f, 0.0f};
    Color3f ambientColor     = {0.2f, 0.2f, 0.2f}; // VRML1
    float   shininess        = 0.2f;
    float   transparency     = 0.0f;
    float   ambientIntensity = 0.2f;                // VRML2
};

// ─────────────────────────────────────────────────────────────────
//  IndexedFaceSet  (supports VRML 1 & 2)
// ─────────────────────────────────────────────────────────────────
struct IndexedFaceSet {
    // ── raw parsed data ──────────────────────────────────────────
    std::vector<Vec3f>   coords;
    std::vector<int>     coordIndex;     // polygons separated by -1

    std::vector<Vec3f>   normals;
    std::vector<int>     normalIndex;
    bool                 normalPerVertex = true;

    std::vector<Color3f> colors;
    std::vector<int>     colorIndex;
    bool                 colorPerVertex  = true;

    std::vector<Vec2f>   texCoords;
    std::vector<int>     texCoordIndex;

    bool  solid       = true;
    bool  ccw         = true;
    float creaseAngle = 0.0f;

    Material material;

    // ── GPU-ready output (filled by triangulate()) ───────────────
    struct Vertex {
        Vec3f   pos;
        Vec3f   normal;
        Color3f color;
        Vec2f   uv;
    };
    // vertices: flat list, every 3 form a triangle
    std::vector<Vertex> vertices;

    // Call after parsing to populate `vertices`
    void triangulate();

private:
    void computeFlatNormals(const std::vector<std::vector<int>>& faces);
    void computeSmoothNormals(const std::vector<std::vector<int>>& faces);
};

// ─────────────────────────────────────────────────────────────────
//  Scene
// ─────────────────────────────────────────────────────────────────
struct Scene {
    std::vector<std::shared_ptr<IndexedFaceSet>> meshes;
    int vrmlVersion = 2;   // 1 or 2
};

// ─────────────────────────────────────────────────────────────────
//  Entry points
// ─────────────────────────────────────────────────────────────────
std::shared_ptr<Scene> parseFile  (const std::string& path);
std::shared_ptr<Scene> parseString(const std::string& content);

} // namespace vrml
