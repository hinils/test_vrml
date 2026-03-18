/**
 * VRML Viewer  –  GLFW + OpenGL 3.3 Core
 *
 * Controls:
 *   Left-drag   : orbit
 *   Right-drag  : pan
 *   Scroll      : zoom
 *   R           : reset camera
 *   W           : toggle wireframe
 *   Esc/Q       : quit
 */

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../vrml_parser/include/vrml_parser.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────
//  Shader helpers (inline GLSL to avoid file-path issues)
// ─────────────────────────────────────────────────────────────────
static const char* kVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMat;

out vec3 vFragPos;
out vec3 vNormal;
out vec3 vColor;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos  = worldPos.xyz;
    vNormal   = normalize(uNormalMat * aNormal);
    vColor    = aColor;
    gl_Position = uProj * uView * worldPos;
}
)";

static const char* kFragSrc = R"(
#version 330 core
in vec3 vFragPos;
in vec3 vNormal;
in vec3 vColor;

uniform vec3  uLightPos;
uniform vec3  uViewPos;
uniform vec3  uDiffuse;
uniform vec3  uSpecular;
uniform vec3  uEmissive;
uniform float uShininess;
uniform float uAmbientIntensity;
uniform int   uUseVertexColor;
uniform int   uWireframe;

out vec4 FragColor;

void main() {
    if (uWireframe == 1) {
        FragColor = vec4(0.9, 0.9, 0.9, 1.0);
        return;
    }

    vec3 base  = (uUseVertexColor == 1) ? vColor : uDiffuse;
    vec3 light = vec3(1.0);

    vec3 ambient = uAmbientIntensity * light * base;

    vec3  norm     = normalize(vNormal);
    vec3  lightDir = normalize(uLightPos - vFragPos);
    float diff     = max(dot(norm, lightDir), 0.0);
    vec3  diffuse  = diff * light * base;

    vec3  viewDir  = normalize(uViewPos - vFragPos);
    vec3  halfVec  = normalize(lightDir + viewDir);
    float spec     = pow(max(dot(norm, halfVec), 0.0),
                         max(uShininess * 128.0, 1.0));
    vec3  specular = spec * light * uSpecular;

    // soft fill from below
    float fill = max(dot(norm, normalize(vec3(-0.3,-1.0,-0.3))), 0.0) * 0.25;
    vec3  fillC = fill * vec3(0.35, 0.4, 0.55) * base;

    vec3 result = ambient + diffuse + specular + fillC + uEmissive;
    FragColor   = vec4(result, 1.0);
}
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log);
        throw std::runtime_error(std::string("Shader compile error:\n") + log);
    }
    return s;
}

static GLuint createProgram(const char* vert, const char* frag) {
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag);
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetProgramInfoLog(p, 1024, nullptr, log);
        throw std::runtime_error(std::string("Program link error:\n") + log);
    }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

// ─────────────────────────────────────────────────────────────────
//  GPU mesh wrapper
// ─────────────────────────────────────────────────────────────────
struct GpuMesh {
    GLuint vao = 0, vbo = 0;
    GLsizei count = 0;
    vrml::Material material;
    bool hasVertexColors = false;

    void upload(const vrml::IndexedFaceSet& ifs) {
        material = ifs.material;

        if (ifs.vertices.empty()) return;

        // Check if any vertex color differs from default
        hasVertexColors = !ifs.colors.empty();

        struct GPUVert {
            float pos[3];
            float nor[3];
            float col[3];
            float uv[2];
        };
        std::vector<GPUVert> data;
        data.reserve(ifs.vertices.size());
        for (auto& v : ifs.vertices) {
            GPUVert g;
            g.pos[0]=v.pos.x;    g.pos[1]=v.pos.y;    g.pos[2]=v.pos.z;
            g.nor[0]=v.normal.x; g.nor[1]=v.normal.y; g.nor[2]=v.normal.z;
            g.col[0]=v.color.r;  g.col[1]=v.color.g;  g.col[2]=v.color.b;
            g.uv[0] =v.uv.x;     g.uv[1] =v.uv.y;
            data.push_back(g);
        }
        count = (GLsizei)data.size();

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     (GLsizeiptr)(data.size() * sizeof(GPUVert)),
                     data.data(), GL_STATIC_DRAW);

        constexpr int S = sizeof(GPUVert);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,S,(void*)offsetof(GPUVert,pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,S,(void*)offsetof(GPUVert,nor));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,S,(void*)offsetof(GPUVert,col));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,S,(void*)offsetof(GPUVert,uv));
        glBindVertexArray(0);
    }

    void free() {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = vbo = 0;
    }
};

// ─────────────────────────────────────────────────────────────────
//  Camera (arcball-style)
// ─────────────────────────────────────────────────────────────────
struct Camera {
    glm::vec3 target  = {0,0,0};
    float     distance = 5.0f;
    float     yaw      = 30.0f;   // degrees
    float     pitch    = 20.0f;   // degrees

    // Returns eye position
    glm::vec3 eye() const {
        float yawR   = glm::radians(yaw);
        float pitchR = glm::radians(pitch);
        return target + distance * glm::vec3(
            std::cos(pitchR) * std::sin(yawR),
            std::sin(pitchR),
            std::cos(pitchR) * std::cos(yawR));
    }

    glm::mat4 view() const {
        return glm::lookAt(eye(), target, glm::vec3(0,1,0));
    }
};

// ─────────────────────────────────────────────────────────────────
//  Globals (input state)
// ─────────────────────────────────────────────────────────────────
static Camera   g_cam;
static bool     g_wireframe   = false;
static bool     g_lmb         = false;
static bool     g_rmb         = false;
static double   g_lastX       = 0, g_lastY = 0;
static int      g_winW        = 1280, g_winH = 800;

// ─────────────────────────────────────────────────────────────────
//  GLFW callbacks
// ─────────────────────────────────────────────────────────────────
static void cbFramebufferSize(GLFWwindow*, int w, int h) {
    g_winW = w; g_winH = h;
    glViewport(0, 0, w, h);
}

static void cbKey(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q)
        glfwSetWindowShouldClose(win, GLFW_TRUE);
    if (key == GLFW_KEY_W) g_wireframe = !g_wireframe;
    if (key == GLFW_KEY_R) {
        g_cam.yaw = 30.0f; g_cam.pitch = 20.0f; g_cam.distance = 5.0f;
    }
}

static void cbMouseButton(GLFWwindow*, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        g_lmb = (action == GLFW_PRESS);
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
        g_rmb = (action == GLFW_PRESS);
}

static void cbMouseMove(GLFWwindow*, double x, double y) {
    double dx = x - g_lastX;
    double dy = y - g_lastY;
    g_lastX = x; g_lastY = y;

    if (g_lmb) {
        g_cam.yaw   += (float)dx * 0.4f;
        g_cam.pitch  = std::clamp(g_cam.pitch - (float)dy * 0.4f, -89.0f, 89.0f);
    }
    if (g_rmb) {
        // pan in camera's local right/up
        glm::vec3 fwd = glm::normalize(g_cam.target - g_cam.eye());
        glm::vec3 up  = {0,1,0};
        glm::vec3 right = glm::normalize(glm::cross(fwd, up));
        glm::vec3 camUp = glm::normalize(glm::cross(right, fwd));
        float speed = g_cam.distance * 0.001f;
        g_cam.target -= right * (float)dx * speed;
        g_cam.target += camUp * (float)dy * speed;
    }
}

static void cbScroll(GLFWwindow*, double, double dy) {
    g_cam.distance *= std::pow(0.9f, (float)dy);
    g_cam.distance  = std::clamp(g_cam.distance, 0.1f, 1000.0f);
}

// ─────────────────────────────────────────────────────────────────
//  Compute scene bounding box and auto-fit camera
// ─────────────────────────────────────────────────────────────────
static void fitCamera(const std::vector<GpuMesh>& meshes,
                      const std::shared_ptr<vrml::Scene>& scene) {
    glm::vec3 bmin(1e30f), bmax(-1e30f);
    for (auto& ifs : scene->meshes) {
        for (auto& v : ifs->vertices) {
            bmin = glm::min(bmin, glm::vec3(v.pos.x, v.pos.y, v.pos.z));
            bmax = glm::max(bmax, glm::vec3(v.pos.x, v.pos.y, v.pos.z));
        }
    }
    glm::vec3 center = (bmin + bmax) * 0.5f;
    float radius = glm::length(bmax - bmin) * 0.5f;
    if (radius < 1e-5f) radius = 1.0f;
    g_cam.target   = center;
    g_cam.distance = radius * 2.5f;
    g_cam.yaw      = 30.0f;
    g_cam.pitch    = 20.0f;
    (void)meshes;
}

// ─────────────────────────────────────────────────────────────────
//  Overlay HUD text (very small OpenGL-based)
// ─────────────────────────────────────────────────────────────────
// We skip font rendering to keep zero external deps beyond GLFW/glad/glm.
// Instead we draw a small legend quad + print info to console.

// ─────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    try {
    // ── Load scene ───────────────────────────────────────────────
    std::string vrmlPath;
    if (argc >= 2) {
        vrmlPath = argv[1];
    } else {
        // Use built-in test scene
        vrmlPath = ""; // will generate procedural mesh
    }

    std::shared_ptr<vrml::Scene> scene;

    if (!vrmlPath.empty()) {
        std::cout << "[viewer] Loading: " << vrmlPath << "\n";
        try {
            scene = vrml::parseFile(vrmlPath);
        } catch (const std::exception& e) {
            std::cerr << "Error loading file: " << e.what() << "\n";
            return 1;
        }
    } else {
        // Procedural test: simple cube made of triangles
        scene = std::make_shared<vrml::Scene>();
        auto mesh = std::make_shared<vrml::IndexedFaceSet>();

        // 8 vertices of a unit cube
        mesh->coords = {
            {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
            {-1,-1, 1},{1,-1, 1},{1,1, 1},{-1,1, 1}
        };
        // 6 faces, each as quad (4 indices + -1)
        mesh->coordIndex = {
            0,1,2,3,-1,   4,7,6,5,-1,   0,4,5,1,-1,
            2,6,7,3,-1,   0,3,7,4,-1,   1,5,6,2,-1
        };
        mesh->material.diffuseColor = {0.3f, 0.6f, 0.9f};
        mesh->creaseAngle = 0.0f;
        mesh->triangulate();
        scene->meshes.push_back(mesh);
        std::cout << "[viewer] No file given – showing built-in cube.\n";
    }

    if (scene->meshes.empty()) {
        std::cerr << "[viewer] Scene contains no meshes.\n";
        return 1;
    }

    size_t totalTris = 0;
    for (auto& m : scene->meshes)
        totalTris += m->vertices.size() / 3;
    std::cout << "[viewer] VRML v" << scene->vrmlVersion
              << " | " << scene->meshes.size() << " mesh(es)"
              << " | " << totalTris << " triangles\n";
    std::cout << "Controls: Left-drag=orbit  Right-drag=pan  Scroll=zoom  W=wireframe  R=reset  Q/Esc=quit\n";

    // ── GLFW init ────────────────────────────────────────────────
    if (!glfwInit()) { std::cerr << "glfwInit failed\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    std::string title = "VRML Viewer  [" +
        (vrmlPath.empty() ? "built-in cube" : vrmlPath) + "]";
    GLFWwindow* window = glfwCreateWindow(g_winW, g_winH, title.c_str(), nullptr, nullptr);
    if (!window) { std::cerr << "glfwCreateWindow failed\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // ── glad ─────────────────────────────────────────────────────
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "gladLoadGL failed\n"; glfwTerminate(); return 1;
    }
    std::cout << "[viewer] OpenGL " << glGetString(GL_VERSION) << "\n";

    // ── callbacks ────────────────────────────────────────────────
    glfwSetFramebufferSizeCallback(window, cbFramebufferSize);
    glfwSetKeyCallback(window,         cbKey);
    glfwSetMouseButtonCallback(window, cbMouseButton);
    glfwSetCursorPosCallback(window,   cbMouseMove);
    glfwSetScrollCallback(window,      cbScroll);
    glfwGetCursorPos(window, &g_lastX, &g_lastY);

    // ── OpenGL state ─────────────────────────────────────────────
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);

    // ── Shader ───────────────────────────────────────────────────
    GLuint prog = createProgram(kVertSrc, kFragSrc);

    // uniform locations
    auto uloc = [&](const char* name){ return glGetUniformLocation(prog, name); };
    GLint uModel   = uloc("uModel");
    GLint uView    = uloc("uView");
    GLint uProj    = uloc("uProj");
    GLint uNormMat = uloc("uNormalMat");
    GLint uLightPos= uloc("uLightPos");
    GLint uViewPos = uloc("uViewPos");
    GLint uDiffuse = uloc("uDiffuse");
    GLint uSpecular= uloc("uSpecular");
    GLint uEmissive= uloc("uEmissive");
    GLint uShiny   = uloc("uShininess");
    GLint uAmbInt  = uloc("uAmbientIntensity");
    GLint uUseVC   = uloc("uUseVertexColor");
    GLint uWire    = uloc("uWireframe");

        // ── Upload meshes to GPU ─────────────────────────────────────
        std::vector<GpuMesh> gpuMeshes(scene->meshes.size());
        for (size_t i = 0; i < scene->meshes.size(); ++i)
            gpuMeshes[i].upload(*scene->meshes[i]);
    
        fitCamera(gpuMeshes, scene);
    
        // ── Render loop ──────────────────────────────────────────────
        while (!glfwWindowShouldClose(window)) {

            glfwPollEvents();

    

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    

            float aspect = (g_winH > 0) ? (float)g_winW / (float)g_winH : 1.0f;

            glm::mat4 proj  = glm::perspective(glm::radians(45.0f), aspect, 0.001f, 10000.0f);

            glm::mat4 view  = g_cam.view();

            glm::mat4 model = glm::mat4(1.0f);

            glm::mat3 normM = glm::transpose(glm::inverse(glm::mat3(model)));

    

            // Light slightly above and to the right of the camera

            glm::vec3 lightPos = g_cam.eye() + glm::vec3(2, 3, 1) * g_cam.distance * 0.5f;

    

            glUseProgram(prog);

            glUniformMatrix4fv(uModel,   1, GL_FALSE, glm::value_ptr(model));

            glUniformMatrix4fv(uView,    1, GL_FALSE, glm::value_ptr(view));

            glUniformMatrix4fv(uProj,    1, GL_FALSE, glm::value_ptr(proj));

            glUniformMatrix3fv(uNormMat, 1, GL_FALSE, glm::value_ptr(normM));

            glUniform3fv(uLightPos, 1, glm::value_ptr(lightPos));

            glUniform3fv(uViewPos,  1, glm::value_ptr(g_cam.eye()));

    

            if (g_wireframe) {

                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

                glDisable(GL_CULL_FACE);

            } else {

                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                glEnable(GL_CULL_FACE);

            }

    

            for (auto& gm : gpuMeshes) {

                if (gm.count == 0) continue;

                auto& mat = gm.material;

                glUniform3f(uDiffuse,  mat.diffuseColor.r,  mat.diffuseColor.g,  mat.diffuseColor.b);

                glUniform3f(uSpecular, mat.specularColor.r, mat.specularColor.g, mat.specularColor.b);

                glUniform3f(uEmissive, mat.emissiveColor.r, mat.emissiveColor.g, mat.emissiveColor.b);

                glUniform1f(uShiny,    mat.shininess);

                glUniform1f(uAmbInt,   mat.ambientIntensity);

                glUniform1i(uUseVC,    gm.hasVertexColors ? 1 : 0);

                glUniform1i(uWire,     g_wireframe ? 1 : 0);

    

                glBindVertexArray(gm.vao);

                glDrawArrays(GL_TRIANGLES, 0, gm.count);

            }

            glBindVertexArray(0);

    

            glfwSwapBuffers(window);

        }

    // ── Cleanup ──────────────────────────────────────────────────
    for (auto& gm : gpuMeshes) gm.free();
    glDeleteProgram(prog);
    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "[viewer] Normal exit.\n";
    return 0;
    } catch (const std::exception& e) {
        std::cerr << "[viewer] Exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "[viewer] Unknown exception.\n";
        return 1;
    }
}
