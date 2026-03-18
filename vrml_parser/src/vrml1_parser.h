#pragma once
#include "lexer.h"
#include "../include/vrml_parser.h"
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

namespace vrml {
namespace detail {

// ─────────────────────────────────────────────────────────────────
//  VRML 1.0 parser
//  VRML 1.0 is scene-graph based with a state stack. We maintain
//  a minimal state (current Material, current coordinates) and
//  emit an IndexedFaceSet whenever we see one.
// ─────────────────────────────────────────────────────────────────
class Vrml1Parser {
public:
    explicit Vrml1Parser(const std::string& src) : lex_(src) {}

    std::shared_ptr<Scene> parse() {
        scene_ = std::make_shared<Scene>();
        scene_->vrmlVersion = 1;
        while (!lex_.atEnd()) {
            parseNode();
        }
        return scene_;
    }

private:
    Lexer  lex_;
    std::shared_ptr<Scene> scene_;

    // VRML1 traversal state
    std::vector<Vec3f>   currentCoords_;
    std::vector<Vec3f>   currentNormals_;
    std::vector<Vec2f>   currentTexCoords_;
    Material             currentMaterial_;
    bool                 normalPerVertex_ = true;
    bool                 colorPerVertex_  = true;

    static float toFloat(const std::string& s) { return std::stof(s); }
    static int   toInt  (const std::string& s) { return std::stoi(s); }

    // ── helpers ──────────────────────────────────────────────────

    std::vector<float> readFloatList() {
        // reads up to the closing ] or }
        std::vector<float> v;
        while (!lex_.atEnd()) {
            Token t = lex_.peek();
            if (t.type == TokType::RBracket || t.type == TokType::RBrace) break;
            if (t.type == TokType::Comma) { lex_.next(); continue; }
            if (t.type == TokType::Number || t.type == TokType::Word) {
                lex_.next();
                v.push_back(toFloat(t.value));
            } else break;
        }
        return v;
    }

    std::vector<int> readIntList() {
        std::vector<int> v;
        while (!lex_.atEnd()) {
            Token t = lex_.peek();
            if (t.type == TokType::RBracket || t.type == TokType::RBrace) break;
            if (t.type == TokType::Comma) { lex_.next(); continue; }
            if (t.type == TokType::Number || t.type == TokType::Word) {
                lex_.next();
                v.push_back(toInt(t.value));
            } else break;
        }
        return v;
    }

    // skip everything inside { } or [ ]
    void skipBlock(char open, char close) {
        TokType openT  = (open=='{')  ? TokType::LBrace   : TokType::LBracket;
        TokType closeT = (close=='}') ? TokType::RBrace   : TokType::RBracket;
        (void)openT;
        int depth = 1;
        while (!lex_.atEnd() && depth > 0) {
            Token t = lex_.next();
            if      (t.type == TokType::LBrace || t.type == TokType::LBracket) ++depth;
            else if (t.type == TokType::RBrace || t.type == TokType::RBracket) --depth;
            (void)closeT;
        }
    }

    // Consume the opening { then skipBlock
    void skipNodeBody() {
        if (lex_.peek().type == TokType::LBrace) {
            lex_.next();
            skipBlock('{','}');
        }
    }

    // ── node dispatch ────────────────────────────────────────────
    void parseNode() {
        Token t = lex_.peek();
        if (t.type == TokType::Eof) return;

        if (t.value == "DEF") {
            lex_.next(); // DEF
            lex_.next(); // name
            parseNode();
            return;
        }

        if (t.value == "USE") {
            lex_.next(); lex_.next(); return;
        }

        lex_.next(); // consume node type
        std::string nodeType = t.value;

        if      (nodeType == "Separator"         || nodeType == "Group"   ||
                 nodeType == "TransformSeparator" || nodeType == "Switch"  ||
                 nodeType == "WWWAnchor"          || nodeType == "LOD")
            parseSeparator();
        else if (nodeType == "Material")          parseMaterial1();
        else if (nodeType == "Coordinate3")       parseCoordinate3();
        else if (nodeType == "Normal")            parseNormal1();
        else if (nodeType == "TextureCoordinate2") parseTexCoord1();
        else if (nodeType == "NormalBinding")     parseNormalBinding();
        else if (nodeType == "MaterialBinding")   parseMaterialBinding();
        else if (nodeType == "IndexedFaceSet")    parseIndexedFaceSet1();
        else if (nodeType == "Transform"          ||
                 nodeType == "Translation"        ||
                 nodeType == "Rotation"           ||
                 nodeType == "Scale")             skipNodeBody();
        else                                      skipNodeBody();
    }

    // ── Separator (VRML1 scoping node) ───────────────────────────
    void parseSeparator() {
        // save state
        auto savedCoords    = currentCoords_;
        auto savedNormals   = currentNormals_;
        auto savedTex       = currentTexCoords_;
        auto savedMat       = currentMaterial_;
        bool savedNPV       = normalPerVertex_;
        bool savedCPV       = colorPerVertex_;

        if (lex_.peek().type != TokType::LBrace) return;
        lex_.next(); // {
        while (!lex_.atEnd()) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace) { lex_.next(); break; }
            parseNode();
        }

        // restore state
        currentCoords_   = savedCoords;
        currentNormals_  = savedNormals;
        currentTexCoords_= savedTex;
        currentMaterial_ = savedMat;
        normalPerVertex_ = savedNPV;
        colorPerVertex_  = savedCPV;
    }

    void parseMaterial1() {
        if (lex_.peek().type != TokType::LBrace) return;
        lex_.next();
        while (!lex_.atEnd()) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace) { lex_.next(); break; }
            lex_.next();
            if (t.value == "ambientColor") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                auto v = readFloatList();
                if (v.size()>=3) currentMaterial_.ambientColor={v[0],v[1],v[2]};
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else if (t.value == "diffuseColor") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                auto v = readFloatList();
                if (v.size()>=3) currentMaterial_.diffuseColor={v[0],v[1],v[2]};
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else if (t.value == "specularColor") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                auto v = readFloatList();
                if (v.size()>=3) currentMaterial_.specularColor={v[0],v[1],v[2]};
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else if (t.value == "emissiveColor") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                auto v = readFloatList();
                if (v.size()>=3) currentMaterial_.emissiveColor={v[0],v[1],v[2]};
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else if (t.value == "shininess") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                auto v = readFloatList();
                if (!v.empty()) currentMaterial_.shininess=v[0];
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else if (t.value == "transparency") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                auto v = readFloatList();
                if (!v.empty()) currentMaterial_.transparency=v[0];
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else {
                // skip unknown field value
                if (lex_.peek().type==TokType::LBracket){ lex_.next(); skipBlock('[',']'); }
                else lex_.next();
            }
        }
    }

    void parseCoordinate3() {
        currentCoords_.clear();
        if (lex_.peek().type != TokType::LBrace) return;
        lex_.next();
        while (!lex_.atEnd()) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace) { lex_.next(); break; }
            lex_.next();
            if (t.value == "point") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                auto v = readFloatList();
                for (size_t i=0; i+2<v.size(); i+=3)
                    currentCoords_.push_back({v[i],v[i+1],v[i+2]});
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else {
                if (lex_.peek().type==TokType::LBracket){ lex_.next(); skipBlock('[',']'); }
                else lex_.next();
            }
        }
    }

    void parseNormal1() {
        currentNormals_.clear();
        if (lex_.peek().type != TokType::LBrace) return;
        lex_.next();
        while (!lex_.atEnd()) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace) { lex_.next(); break; }
            lex_.next();
            if (t.value == "vector") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                auto v = readFloatList();
                for (size_t i=0; i+2<v.size(); i+=3)
                    currentNormals_.push_back({v[i],v[i+1],v[i+2]});
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else {
                if (lex_.peek().type==TokType::LBracket){ lex_.next(); skipBlock('[',']'); }
                else lex_.next();
            }
        }
    }

    void parseTexCoord1() {
        currentTexCoords_.clear();
        if (lex_.peek().type != TokType::LBrace) return;
        lex_.next();
        while (!lex_.atEnd()) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace) { lex_.next(); break; }
            lex_.next();
            if (t.value == "point") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                auto v = readFloatList();
                for (size_t i=0; i+1<v.size(); i+=2)
                    currentTexCoords_.push_back({v[i],v[i+1]});
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else {
                if (lex_.peek().type==TokType::LBracket){ lex_.next(); skipBlock('[',']'); }
                else lex_.next();
            }
        }
    }

    void parseNormalBinding() {
        if (lex_.peek().type != TokType::LBrace) return;
        lex_.next();
        while (!lex_.atEnd()) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace) { lex_.next(); break; }
            lex_.next();
            if (t.value == "value") {
                Token v = lex_.next();
                normalPerVertex_ = (v.value == "PER_VERTEX" ||
                                    v.value == "PER_VERTEX_INDEXED");
            } else lex_.next();
        }
    }

    void parseMaterialBinding() {
        if (lex_.peek().type != TokType::LBrace) return;
        lex_.next();
        while (!lex_.atEnd()) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace) { lex_.next(); break; }
            lex_.next();
            if (t.value == "value") {
                Token v = lex_.next();
                colorPerVertex_ = (v.value == "PER_VERTEX" ||
                                   v.value == "PER_VERTEX_INDEXED");
            } else lex_.next();
        }
    }

    void parseIndexedFaceSet1() {
        auto mesh = std::make_shared<IndexedFaceSet>();
        mesh->coords         = currentCoords_;
        mesh->normals        = currentNormals_;
        mesh->texCoords      = currentTexCoords_;
        mesh->material       = currentMaterial_;
        mesh->normalPerVertex= normalPerVertex_;
        mesh->colorPerVertex = colorPerVertex_;

        if (lex_.peek().type != TokType::LBrace) {
            mesh->triangulate();
            scene_->meshes.push_back(mesh);
            return;
        }
        lex_.next(); // {
        while (!lex_.atEnd()) {
            Token t = lex_.peek();
            if (t.type == TokType::RBrace) { lex_.next(); break; }
            lex_.next();
            if (t.value == "coordIndex") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                mesh->coordIndex = readIntList();
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else if (t.value == "normalIndex") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                mesh->normalIndex = readIntList();
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else if (t.value == "textureCoordIndex") {
                if (lex_.peek().type==TokType::LBracket) lex_.next();
                mesh->texCoordIndex = readIntList();
                if (lex_.peek().type==TokType::RBracket) lex_.next();
            } else {
                if (lex_.peek().type==TokType::LBracket){ lex_.next(); skipBlock('[',']'); }
                else lex_.next();
            }
        }

        mesh->triangulate();
        scene_->meshes.push_back(mesh);
    }
};

} // namespace detail
} // namespace vrml
