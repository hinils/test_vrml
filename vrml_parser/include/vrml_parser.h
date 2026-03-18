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
//  Transform matrix (4x4 for VRML transforms)
// ─────────────────────────────────────────────────────────────────
struct Mat4 {
    // Column-major 4x4 matrix (OpenGL style)
    float m[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    static Mat4 identity() { return Mat4(); }

    static Mat4 translation(float tx, float ty, float tz) {
        Mat4 r;
        r.m[12] = tx; r.m[13] = ty; r.m[14] = tz;
        return r;
    }

    static Mat4 rotation(float ax, float ay, float az, float angle) {
        // Axis-angle to rotation matrix
        float c = std::cos(angle), s = std::sin(angle);
        float t = 1 - c;
        Mat4 r;
        r.m[0] = t*ax*ax + c;    r.m[4] = t*ax*ay - s*az; r.m[8]  = t*ax*az + s*ay;
        r.m[1] = t*ax*ay + s*az; r.m[5] = t*ay*ay + c;    r.m[9]  = t*ay*az - s*ax;
        r.m[2] = t*ax*az - s*ay; r.m[6] = t*ay*az + s*ax; r.m[10] = t*az*az + c;
        return r;
    }

    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 r;
        r.m[0] = sx; r.m[5] = sy; r.m[10] = sz;
        return r;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                r.m[col*4 + row] =
                    m[0*4+row]*o.m[col*4+0] +
                    m[1*4+row]*o.m[col*4+1] +
                    m[2*4+row]*o.m[col*4+2] +
                    m[3*4+row]*o.m[col*4+3];
            }
        }
        return r;
    }

    Vec3f transformPoint(const Vec3f& p) const {
        return {
            m[0]*p.x + m[4]*p.y + m[8]*p.z  + m[12],
            m[1]*p.x + m[5]*p.y + m[9]*p.z  + m[13],
            m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14]
        };
    }

    Vec3f transformNormal(const Vec3f& n) const {
        // For normals, use inverse transpose (simplified: just rotation part)
        Vec3f result = {
            m[0]*n.x + m[4]*n.y + m[8]*n.z,
            m[1]*n.x + m[5]*n.y + m[9]*n.z,
            m[2]*n.x + m[6]*n.y + m[10]*n.z
        };
        return result.normalized();
    }
};

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
