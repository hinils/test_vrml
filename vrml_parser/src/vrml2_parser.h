#pragma once
#include "lexer.h"
#include "../include/vrml_parser.h"
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <cstdlib>

namespace vrml {
namespace detail {

using namespace vrml;

class Vrml2Parser {
public:
    explicit Vrml2Parser(const std::string& src) : lex_(src) {}

    std::shared_ptr<Scene> parse() {
        scene_ = std::make_shared<Scene>();
        scene_->vrmlVersion = 2;
        while (!lex_.atEnd()) parseStatement();
        return scene_;
    }

private:
    Lexer  lex_;
    std::shared_ptr<Scene> scene_;
    std::unordered_map<std::string, std::shared_ptr<IndexedFaceSet>> defMeshMap_;
    std::unordered_map<std::string, Material> defMaterialMap_;

    // ── Type converters ──────────────────────────────────────────
    static float toFloat(const std::string& s) { return std::stof(s); }
    static int   toInt  (const std::string& s) { return std::stoi(s); }
    static bool  toBool (const std::string& s) {
        return s == "TRUE" || s == "true" || s == "True" || s == "1";
    }

    // Read a single SFBool: next token is TRUE/FALSE/true/false/1/0
    bool readSFBool() {
        Token t = lex_.next();
        return toBool(t.value);
    }

    // Read a single SFFloat
    float readSFFloat() {
        Token t = lex_.next();
        return toFloat(t.value);
    }

    // ── Skip helpers ─────────────────────────────────────────────
    void skipNodeBody() {
        // consume everything inside matching braces
        int depth = 1;
        while (!lex_.atEnd() && depth > 0) {
            Token t = lex_.next();
            if      (t.type == TokType::LBrace) ++depth;
            else if (t.type == TokType::RBrace) --depth;
        }
    }

    void skipFieldValue() {
        Token t = lex_.peek();
        if (t.type == TokType::LBracket) {
            lex_.next();
            int d = 1;
            while (!lex_.atEnd() && d > 0) {
                Token tt = lex_.next();
                if      (tt.type == TokType::LBracket) ++d;
                else if (tt.type == TokType::RBracket) --d;
            }
        } else if (t.type == TokType::LBrace) {
            lex_.next();
            skipNodeBody();
        } else {
            // Skip single or multiple values until we hit a keyword, brace, or bracket
            // SF types can be: number, string, or keyword (TRUE/FALSE)
            // We need to skip until we see something that looks like a field name or structure
            while (!lex_.atEnd()) {
                Token p = lex_.peek();
                // Stop at structural tokens
                if (p.type == TokType::RBrace || p.type == TokType::RBracket) break;
                // Stop at keywords that look like field names (start with lowercase) or node types
                if (p.type == TokType::Word) {
                    // Check if this looks like a field name (lowercase start) or node type (uppercase start)
                    if (!p.value.empty() && std::islower((unsigned char)p.value[0])) break;
                    // Node types like Transform, Shape, etc. - also stop
                    if (p.value == "Transform" || p.value == "Shape" || p.value == "Group" ||
                        p.value == "DEF" || p.value == "USE" || p.value == "Appearance" ||
                        p.value == "Material" || p.value == "IndexedFaceSet" || p.value == "Coordinate" ||
                        p.value == "Normal" || p.value == "Color" || p.value == "TextureCoordinate") break;
                }
                lex_.next();
            }
        }
    }

    // ── Multi-value field readers ─────────────────────────────────

    // Reads [ float , float , ... ] or a single float
    std::vector<float> readMFFloat() {
        std::vector<float> v;
        if (lex_.peek().type == TokType::LBracket) {
            lex_.next();
            while (true) {
                Token p = lex_.peek();
                if (p.type == TokType::RBracket || p.type == TokType::Eof) { lex_.next(); break; }
                if (p.type == TokType::Comma) { lex_.next(); continue; }
                if (p.type == TokType::Number) { lex_.next(); v.push_back(toFloat(p.value)); }
                else { lex_.next(); } // skip non-number tokens inside bracket
            }
        } else {
            Token p = lex_.peek();
            if (p.type == TokType::Number) { lex_.next(); v.push_back(toFloat(p.value)); }
        }
        return v;
    }

    std::vector<int> readMFInt32() {
        std::vector<int> v;
        if (lex_.peek().type == TokType::LBracket) {
            lex_.next();
            while (true) {
                Token p = lex_.peek();
                if (p.type == TokType::RBracket || p.type == TokType::Eof) { lex_.next(); break; }
                if (p.type == TokType::Comma) { lex_.next(); continue; }
                if (p.type == TokType::Number) { lex_.next(); v.push_back(toInt(p.value)); }
                else { lex_.next(); }
            }
        } else {
            Token p = lex_.peek();
            if (p.type == TokType::Number) { lex_.next(); v.push_back(toInt(p.value)); }
        }
        return v;
    }

    Vec3f readSFVec3f() {
        float x = toFloat(lex_.next().value);
        float y = toFloat(lex_.next().value);
        float z = toFloat(lex_.next().value);
        return {x, y, z};
    }

    Color3f readSFColor() {
        float r = toFloat(lex_.next().value);
        float g = toFloat(lex_.next().value);
        float b = toFloat(lex_.next().value);
        return {r, g, b};
    }

    std::vector<Vec3f> readMFVec3f() {
        std::vector<Vec3f> v;
        if (lex_.peek().type == TokType::LBracket) {
            lex_.next();
            while (true) {
                Token p = lex_.peek();
                if (p.type == TokType::RBracket || p.type == TokType::Eof) { lex_.next(); break; }
                if (p.type == TokType::Comma) { lex_.next(); continue; }
                if (p.type == TokType::Number) v.push_back(readSFVec3f());
                else { lex_.next(); }
            }
        } else if (lex_.peek().type == TokType::Number) {
            v.push_back(readSFVec3f());
        }
        return v;
    }

    std::vector<Vec2f> readMFVec2f() {
        std::vector<Vec2f> v;
        if (lex_.peek().type == TokType::LBracket) {
            lex_.next();
            while (true) {
                Token p = lex_.peek();
                if (p.type == TokType::RBracket || p.type == TokType::Eof) { lex_.next(); break; }
                if (p.type == TokType::Comma) { lex_.next(); continue; }
                if (p.type == TokType::Number) {
                    float x = toFloat(lex_.next().value);
                    float y = toFloat(lex_.next().value);
                    v.push_back({x, y});
                } else { lex_.next(); }
            }
        }
        return v;
    }

    std::vector<Color3f> readMFColor() {
        std::vector<Color3f> v;
        if (lex_.peek().type == TokType::LBracket) {
            lex_.next();
            while (true) {
                Token p = lex_.peek();
                if (p.type == TokType::RBracket || p.type == TokType::Eof) { lex_.next(); break; }
                if (p.type == TokType::Comma) { lex_.next(); continue; }
                if (p.type == TokType::Number) v.push_back(readSFColor());
                else { lex_.next(); }
            }
        }
        return v;
    }

    // Helper: read a sub-node that carries a named array field
    // e.g.  Coordinate { point [ ... ] }  returns the Vec3f array
    std::vector<Vec3f> readCoordinateNode() {
        std::vector<Vec3f> pts;
        if (lex_.peek().type != TokType::LBrace) return pts;
        lex_.next();
        while (true) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace || t.type == TokType::Eof) { lex_.next(); break; }
            lex_.next();
            if (t.value == "point") pts = readMFVec3f();
            else skipFieldValue();
        }
        return pts;
    }

    std::vector<Vec3f> readNormalNode() {
        std::vector<Vec3f> v;
        if (lex_.peek().type != TokType::LBrace) return v;
        lex_.next();
        while (true) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace || t.type == TokType::Eof) { lex_.next(); break; }
            lex_.next();
            if (t.value == "vector") v = readMFVec3f();
            else skipFieldValue();
        }
        return v;
    }

    std::vector<Vec2f> readTexCoordNode() {
        std::vector<Vec2f> v;
        if (lex_.peek().type != TokType::LBrace) return v;
        lex_.next();
        while (true) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace || t.type == TokType::Eof) { lex_.next(); break; }
            lex_.next();
            if (t.value == "point") v = readMFVec2f();
            else skipFieldValue();
        }
        return v;
    }

    std::vector<Color3f> readColorNode() {
        std::vector<Color3f> v;
        if (lex_.peek().type != TokType::LBrace) return v;
        lex_.next();
        while (true) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace || t.type == TokType::Eof) { lex_.next(); break; }
            lex_.next();
            if (t.value == "color") v = readMFColor();
            else skipFieldValue();
        }
        return v;
    }

    // ── Top-level statement ──────────────────────────────────────
    void parseStatement() {
        Token t = lex_.peek();
        if (t.type == TokType::Eof) return;

        if (t.value == "DEF") {
            lex_.next();
            Token name = lex_.next();
            parseNode(name.value);
        } else if (t.value == "USE") {
            lex_.next(); lex_.next();
        } else if (t.value == "PROTO" || t.value == "EXTERNPROTO") {
            lex_.next(); lex_.next();
            if (lex_.peek().type == TokType::LBracket) {
                lex_.next(); int d=1;
                while (!lex_.atEnd()&&d>0){ Token tt=lex_.next();
                    if(tt.type==TokType::LBracket)++d;
                    else if(tt.type==TokType::RBracket)--d; }
            }
            if (lex_.peek().type == TokType::LBrace) { lex_.next(); skipNodeBody(); }
        } else if (t.value == "ROUTE") {
            for (int i=0;i<4&&!lex_.atEnd();++i) lex_.next();
        } else {
            parseNode("");
        }
    }

    void parseNode(const std::string& defName) {
        Token typeToken = lex_.next();
        std::string nt = typeToken.value;

        if (nt == "Shape") {
            parseShape(defName);
        } else if (nt=="Transform"||nt=="Group"||nt=="Collision"||
                   nt=="Switch"  ||nt=="Billboard"||nt=="Anchor"||
                   nt=="LOD"     ||nt=="Inline") {
            parseGroupNode();
        } else {
            if (lex_.peek().type == TokType::LBrace) { lex_.next(); skipNodeBody(); }
        }
    }

    void parseGroupNode() {
        if (lex_.peek().type != TokType::LBrace) return;
        lex_.next();
        while (true) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace || t.type == TokType::Eof) { lex_.next(); break; }
            if (t.value == "children") {
                lex_.next();
                if (lex_.peek().type == TokType::LBracket) {
                    lex_.next();
                    while (true) {
                        Token tc = lex_.peek();
                        if (tc.type==TokType::RBracket||tc.type==TokType::Eof){lex_.next();break;}
                        if (tc.type==TokType::Comma){lex_.next();continue;}
                        if (tc.value=="DEF"){lex_.next();Token nm=lex_.next();parseNode(nm.value);}
                        else parseNode("");
                    }
                } else if (lex_.peek().type==TokType::LBrace) {
                    parseNode("");
                } else {
                    // Single node without brackets (e.g., "children Shape { ... }")
                    parseNode("");
                }
            } else {
                lex_.next(); skipFieldValue();
            }
        }
    }

    void parseShape(const std::string& defName) {
        Material mat;
        std::shared_ptr<IndexedFaceSet> mesh;

        if (lex_.peek().type != TokType::LBrace) return;
        lex_.next();

        while (true) {
            Token t = lex_.peek();
            if (t.type==TokType::RBrace||t.type==TokType::Eof){lex_.next();break;}
            lex_.next();
            if (t.value == "appearance") {
                Token ap = lex_.peek();
                if (ap.value == "USE") {
                    lex_.next(); // consume USE
                    Token name = lex_.next(); // get name
                    auto it = defMaterialMap_.find(name.value);
                    if (it != defMaterialMap_.end()) mat = it->second;
                } else if (ap.value=="Appearance"||ap.value=="DEF") {
                    std::string appDef;
                    if (ap.value=="DEF"){
                        lex_.next(); // consume DEF
                        appDef=lex_.next().value; // get name
                        // Skip the Appearance keyword if present
                        if (lex_.peek().value=="Appearance") lex_.next();
                    } else {
                        lex_.next(); // consume Appearance
                    }
                    mat = parseAppearance(appDef);
                } else { skipFieldValue(); }
            } else if (t.value == "geometry") {
                mesh = parseGeometryField(defName);
            } else {
                skipFieldValue();
            }
        }

        if (mesh) {
            mesh->material = mat;
            mesh->triangulate();
            scene_->meshes.push_back(mesh);
        }
    }

    // Parse appearance { material Material { ... } }
    Material parseAppearance(const std::string& defName = "") {
        Material mat;
        if (lex_.peek().type != TokType::LBrace) return mat;
        lex_.next();
        while (true) {
            Token t = lex_.peek();
            if (t.type==TokType::RBrace||t.type==TokType::Eof){lex_.next();break;}
            lex_.next();
            if (t.value == "material") {
                Token mt = lex_.peek();
                if (mt.value == "USE") {
                    lex_.next(); // consume USE
                    Token name = lex_.next(); // get name
                    auto it = defMaterialMap_.find(name.value);
                    if (it != defMaterialMap_.end()) mat = it->second;
                } else if (mt.value=="Material"||mt.value=="DEF") {
                    std::string matDef;
                    if (mt.value=="DEF"){
                        lex_.next(); // consume DEF
                        matDef=lex_.next().value; // get name
                        // Skip the Material keyword if present
                        if (lex_.peek().value=="Material") lex_.next();
                    } else {
                        lex_.next(); // consume Material
                    }
                    mat = parseMaterial(matDef);
                } else { skipFieldValue(); }
            } else { skipFieldValue(); }
        }
        if (!defName.empty()) defMaterialMap_[defName] = mat;
        return mat;
    }

    Material parseMaterial(const std::string& defName = "") {
        Material mat;
        if (lex_.peek().type != TokType::LBrace) return mat;
        lex_.next();
        while (true) {
            Token t = lex_.peek();
            if (t.type==TokType::RBrace||t.type==TokType::Eof){lex_.next();break;}
            lex_.next();
            if      (t.value=="diffuseColor")    mat.diffuseColor    = readSFColor();
            else if (t.value=="specularColor")   mat.specularColor   = readSFColor();
            else if (t.value=="emissiveColor")   mat.emissiveColor   = readSFColor();
            else if (t.value=="ambientIntensity"){auto f=readMFFloat();if(!f.empty())mat.ambientIntensity=f[0];}
            else if (t.value=="shininess")       {auto f=readMFFloat();if(!f.empty())mat.shininess=f[0];}
            else if (t.value=="transparency")    {auto f=readMFFloat();if(!f.empty())mat.transparency=f[0];}
            else skipFieldValue();
        }
        if (!defName.empty()) defMaterialMap_[defName] = mat;
        return mat;
    }

    // Returns a freshly-parsed (or USE-referenced) mesh WITHOUT triangulation
    std::shared_ptr<IndexedFaceSet> parseGeometryField(const std::string& shapeDef) {
        Token gt = lex_.peek();

        // DEF name before geometry type
        std::string geomDef;
        if (gt.value == "DEF") {
            lex_.next(); geomDef = lex_.next().value; gt = lex_.peek();
        }

        if (gt.value == "USE") {
            lex_.next(); Token nm = lex_.next();
            auto it = defMeshMap_.find(nm.value);
            if (it != defMeshMap_.end()) {
                // Return a copy so each Shape gets its own material
                auto copy = std::make_shared<IndexedFaceSet>(*it->second);
                copy->vertices.clear(); // will re-triangulate after material is set
                return copy;
            }
            return nullptr;
        }

        if (gt.value == "IndexedFaceSet") {
            lex_.next();
            auto mesh = parseIndexedFaceSet();
            if (!geomDef.empty()) defMeshMap_[geomDef] = mesh;
            if (!shapeDef.empty()) defMeshMap_[shapeDef] = mesh;
            return mesh;
        }

        // Other geometry types (Box, Sphere…) – skip
        if (lex_.peek().type==TokType::LBrace){lex_.next();skipNodeBody();}
        return nullptr;
    }

    std::shared_ptr<IndexedFaceSet> parseIndexedFaceSet() {
        auto mesh = std::make_shared<IndexedFaceSet>();
        if (lex_.peek().type != TokType::LBrace) return mesh;
        lex_.next();

        while (true) {
            Token t = lex_.peek();
            if (t.type==TokType::RBrace||t.type==TokType::Eof){lex_.next();break;}
            lex_.next();

            if (t.value == "coord") {
                Token ct = lex_.peek();
                if (ct.value=="Coordinate"||ct.value=="DEF") {
                    if (ct.value=="DEF"){lex_.next();lex_.next();} else lex_.next();
                    mesh->coords = readCoordinateNode();
                } else if (ct.value=="USE") {
                    lex_.next(); lex_.next(); // skip USE name – would need DEF map
                } else { skipFieldValue(); }
            }
            else if (t.value == "normal") {
                Token ct = lex_.peek();
                if (ct.value=="Normal"||ct.value=="DEF") {
                    if (ct.value=="DEF"){lex_.next();lex_.next();} else lex_.next();
                    mesh->normals = readNormalNode();
                } else { skipFieldValue(); }
            }
            else if (t.value == "color") {
                Token ct = lex_.peek();
                if (ct.value=="Color"||ct.value=="DEF") {
                    if (ct.value=="DEF"){lex_.next();lex_.next();} else lex_.next();
                    mesh->colors = readColorNode();
                } else { skipFieldValue(); }
            }
            else if (t.value == "texCoord") {
                Token ct = lex_.peek();
                if (ct.value=="TextureCoordinate"||ct.value=="DEF") {
                    if (ct.value=="DEF"){lex_.next();lex_.next();} else lex_.next();
                    mesh->texCoords = readTexCoordNode();
                } else { skipFieldValue(); }
            }
            else if (t.value=="coordIndex")    mesh->coordIndex    = readMFInt32();
            else if (t.value=="normalIndex")   mesh->normalIndex   = readMFInt32();
            else if (t.value=="colorIndex")    mesh->colorIndex    = readMFInt32();
            else if (t.value=="texCoordIndex") mesh->texCoordIndex = readMFInt32();
            else if (t.value=="normalPerVertex") mesh->normalPerVertex = readSFBool();
            else if (t.value=="colorPerVertex")  mesh->colorPerVertex  = readSFBool();
            else if (t.value=="solid")           mesh->solid           = readSFBool();
            else if (t.value=="ccw")             mesh->ccw             = readSFBool();
            else if (t.value=="creaseAngle")    { auto f=readMFFloat(); if(!f.empty()) mesh->creaseAngle=f[0]; }
            else skipFieldValue();
        }
        return mesh;
    }
};

} // namespace detail
} // namespace vrml
