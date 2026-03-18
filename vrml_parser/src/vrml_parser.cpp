#include "../include/vrml_parser.h"
#include "vrml1_parser.h"
#include "vrml2_parser.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <iostream>

namespace vrml {

// ─────────────────────────────────────────────────────────────────
//  Triangulation & normal computation
// ─────────────────────────────────────────────────────────────────

// Split coordIndex into per-face index lists (dropping -1 separators)
static std::vector<std::vector<int>> splitFaces(const std::vector<int>& ci) {
    std::vector<std::vector<int>> faces;
    std::vector<int> cur;
    for (int idx : ci) {
        if (idx == -1) {
            if (!cur.empty()) { faces.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(idx);
        }
    }
    if (!cur.empty()) faces.push_back(cur);
    return faces;
}

void IndexedFaceSet::computeFlatNormals(const std::vector<std::vector<int>>& faces) {
    normals.clear();
    normalIndex.clear();
    normalPerVertex = false;

    for (auto& face : faces) {
        Vec3f n = {0,1,0};
        if (face.size() >= 3) {
            Vec3f a = coords[face[0]];
            Vec3f b = coords[face[1]];
            Vec3f c = coords[face[2]];
            n = (b - a).cross(c - a).normalized();
        }
        normals.push_back(n);
    }
    // normalIndex is empty → face i uses normals[i]
}

void IndexedFaceSet::computeSmoothNormals(const std::vector<std::vector<int>>& faces) {
    std::vector<Vec3f> smoothN(coords.size(), {0,0,0});

    for (auto& face : faces) {
        if (face.size() < 3) continue;
        Vec3f a = coords[face[0]];
        Vec3f b = coords[face[1]];
        Vec3f c = coords[face[2]];
        Vec3f fn = (b - a).cross(c - a);  // area-weighted
        for (int vi : face) smoothN[vi] += fn;
    }
    for (auto& n : smoothN) n = n.normalized();

    normals = smoothN;
    normalPerVertex = true;
    normalIndex.clear(); // same layout as coordIndex
}

void IndexedFaceSet::triangulate() {
    if (coords.empty() || coordIndex.empty()) return;

    auto faces = splitFaces(coordIndex);

    // ── Determine if we need to compute normals ──────────────────
    // Check if existing normals are degenerate (all identical)
    bool normalsDegenerate = false;
    if (!normals.empty() && normals.size() >= 2) {
        Vec3f first = normals[0];
        normalsDegenerate = true;
        for (size_t i = 1; i < normals.size(); ++i) {
            float dx = std::abs(normals[i].x - first.x);
            float dy = std::abs(normals[i].y - first.y);
            float dz = std::abs(normals[i].z - first.z);
            if (dx > 0.001f || dy > 0.001f || dz > 0.001f) {
                normalsDegenerate = false;
                break;
            }
        }
    }

    bool needNormals = normals.empty() || normalsDegenerate;
    if (needNormals) {
        if (creaseAngle > 0.01f)
            computeSmoothNormals(faces);
        else
            computeFlatNormals(faces);
    }

    // ── Build per-face normal index list ─────────────────────────
    // We'll reconstruct a per-face OR per-vertex normal lookup below.

    // Utility: get normal for a vertex within a face
    auto getNormal = [&](size_t faceIdx, int vertIdx, int coordIdx) -> Vec3f {
        if (normals.empty()) return {0,1,0};
        if (!normalPerVertex) {
            // per-face normal
            size_t ni = faceIdx;
            if (!normalIndex.empty() && faceIdx < normalIndex.size())
                ni = (size_t)normalIndex[faceIdx];
            return ni < normals.size() ? normals[ni] : Vec3f{0,1,0};
        } else {
            // per-vertex normal
            int ni = coordIdx;
            if (!normalIndex.empty() && vertIdx < (int)normalIndex.size())
                ni = normalIndex[vertIdx];
            return (ni >= 0 && ni < (int)normals.size()) ? normals[ni] : Vec3f{0,1,0};
        }
    };

    // Utility: get color for a vertex within a face
    auto getColor = [&](size_t faceIdx, int vertIdx, int coordIdx) -> Color3f {
        if (colors.empty()) return material.diffuseColor;
        if (!colorPerVertex) {
            size_t ci = faceIdx;
            if (!colorIndex.empty() && faceIdx < colorIndex.size())
                ci = (size_t)colorIndex[faceIdx];
            return ci < colors.size() ? colors[ci] : material.diffuseColor;
        } else {
            int ci = coordIdx;
            if (!colorIndex.empty() && vertIdx < (int)colorIndex.size())
                ci = colorIndex[vertIdx];
            return (ci >= 0 && ci < (int)colors.size()) ? colors[ci] : material.diffuseColor;
        }
    };

    // Utility: get UV for a vertex
    auto getUV = [&](int vertIdx, int coordIdx) -> Vec2f {
        if (texCoords.empty()) return {0,0};
        int ti = coordIdx;
        if (!texCoordIndex.empty() && vertIdx < (int)texCoordIndex.size())
            ti = texCoordIndex[vertIdx];
        return (ti >= 0 && ti < (int)texCoords.size()) ? texCoords[ti] : Vec2f{0,0};
    };

    // Build a flat index into the original coordIndex array
    // to map per-vertex normal/uv indices when normalIndex shares layout with coordIndex
    // We track "linearIndex" = position in coordIndex (ignoring -1 entries)
    vertices.clear();

    // Reconstruct linear index offsets per face
    // face[f] starts at linearOffset[f] in the original coordIndex (without -1s)
    std::vector<int> linearOffset;
    {
        int off = 0;
        for (auto& face : faces) {
            linearOffset.push_back(off);
            off += (int)face.size();
        }
    }

    for (size_t f = 0; f < faces.size(); ++f) {
        const auto& face = faces[f];
        if (face.size() < 3) continue;

        // fan triangulate
        for (size_t i = 1; i + 1 < face.size(); ++i) {
            // indices in face: 0, i, i+1
            int triV[3] = { 0, (int)i, (int)i+1 };

            if (!ccw) std::swap(triV[1], triV[2]);

            for (int k = 0; k < 3; ++k) {
                int fvi = triV[k];  // index within the face's vertex list
                int ci  = face[fvi]; // coord index
                int linIdx = linearOffset[f] + fvi; // position in flattened coordIndex

                Vertex v;
                v.pos    = (ci >= 0 && ci < (int)coords.size()) ? coords[ci] : Vec3f{};
                v.normal = getNormal(f, linIdx, ci);
                v.color  = getColor(f, linIdx, ci);
                v.uv     = getUV(linIdx, ci);
                vertices.push_back(v);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────
//  Entry points
// ─────────────────────────────────────────────────────────────────

static int detectVersion(const std::string& content) {
    // First non-empty line should be the header
    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        if (line.find("#VRML V1.0") != std::string::npos ||
            line.find("#VRML V1")   != std::string::npos)
            return 1;
        if (line.find("#VRML V2.0") != std::string::npos ||
            line.find("#VRML V2")   != std::string::npos)
            return 2;
        // default to v2
        return 2;
    }
    return 2;
}

std::shared_ptr<Scene> parseString(const std::string& content) {
    int ver = detectVersion(content);
    try {
        if (ver == 1) {
            detail::Vrml1Parser p(content);
            return p.parse();
        } else {
            detail::Vrml2Parser p(content);
            return p.parse();
        }
    } catch (const std::exception& e) {
        std::cerr << "[vrml_parser] Error: " << e.what() << "\n";
        return std::make_shared<Scene>(); // return empty scene on error
    }
}

std::shared_ptr<Scene> parseFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("Cannot open VRML file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return parseString(ss.str());
}

} // namespace vrml
