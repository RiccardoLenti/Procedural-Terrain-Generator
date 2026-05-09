// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// clang-format on

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <sstream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "camera.h"
#include "terrain_mesh.h"

const int WIN_W = 1920;
const int WIN_H = 1080;

const float TERRAIN_SIZE = 1024.f;
const int TERRAIN_DIVISIONS = 128;
const int CHUNK_COUNT = 8;  // 8x8 grid = 64 chunks
const float CHUNK_SIZE = TERRAIN_SIZE / CHUNK_COUNT;

struct Chunk {
    std::unique_ptr<TerrainMesh> mesh;
    float centerX, centerZ;
    int current_lod;
};

static std::string read_file(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::fprintf(stderr, "Cannot open shader: %s\n", path);
        std::exit(1);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader compile error:\n%s\n", log);
        std::exit(1);
    }
    return s;
}

static GLuint link_program(const char* vert_path, const char* frag_path) {
    std::string vsrc = read_file(vert_path);
    std::string fsrc = read_file(frag_path);
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vsrc.c_str());
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fsrc.c_str());
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Program link error:\n%s\n", log);
        std::exit(1);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

static GLuint load_texture(const char* path) {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &channels, 3);
    if (!data) {
        std::fprintf(stderr, "Failed to load texture %s\n", path);
        std::exit(1);
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return tex;
}

static int lod_for_distance(float dist) {
    if (dist < CHUNK_SIZE * 1.5f)
        return 64;
    if (dist < CHUNK_SIZE * 3.f)
        return 32;
    return 16;
}

// ---------------------------------------------------------------
// Global state for callbacks
// ---------------------------------------------------------------

static Camera g_camera;
static bool g_wireframe = false;

static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    static bool g_first_mouse = true;
    static double g_last_x = 0, g_last_y = 0;

    if (g_first_mouse) {
        g_last_x = xpos;
        g_last_y = ypos;
        g_first_mouse = false;
    }

    float dx = (float)(xpos - g_last_x);
    float dy = (float)(ypos - g_last_y);
    g_last_x = xpos;
    g_last_y = ypos;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        g_camera.process_mouse(-dx, -dy);
}

static void key_callback(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        g_wireframe = !g_wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, g_wireframe ? GL_LINE : GL_FILL);
    }
}

int main() {
    if (!glfwInit()) {
        std::fputs("glfwInit failed\n", stderr);
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIN_W, WIN_H, "Terrain Generator", nullptr, nullptr);

    if (!window) {
        std::fputs("glfwCreateWindow failed\n", stderr);
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // basically vsync

    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fputs("gladLoadGLLoader failed\n", stderr);
        return 1;
    }

    glEnable(GL_DEPTH_TEST);

    GLuint program = link_program("shaders/terrain.vert", "shaders/terrain.frag");

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uTexGrass"), 0);
    glUniform1i(glGetUniformLocation(program, "uTexRock"), 1);
    glUniform1i(glGetUniformLocation(program, "uTexSnow"), 2);

    GLuint tex_grass = load_texture("textures/grass.jpg");
    GLuint tex_snow = load_texture("textures/snow.jpg");
    GLuint tex_rock = load_texture("textures/rock.jpg");

    GLint loc_light_dir = glGetUniformLocation(program, "uLightDir");
    GLint loc_cam_pos = glGetUniformLocation(program, "uCamPos");

    std::vector<Chunk> chunks;
    float half_terrain = (CHUNK_COUNT * CHUNK_SIZE) / 2.f;

    for (int row = 0; row < CHUNK_COUNT; row++) {
        for (int col = 0; col < CHUNK_COUNT; col++) {
            float cx = (col + 0.5f) * CHUNK_SIZE - half_terrain;
            float cz = (row + 0.5f) * CHUNK_SIZE - half_terrain;
            float ox = col * CHUNK_SIZE - half_terrain;
            float oz = row * CHUNK_SIZE - half_terrain;
            int lod = 64;

            chunks.push_back({std::make_unique<TerrainMesh>(CHUNK_SIZE, lod, ox, oz), cx, cz, lod});
        }
    }

    GLint loc_model = glGetUniformLocation(program, "uModel");
    GLint loc_view = glGetUniformLocation(program, "uView");
    GLint loc_proj = glGetUniformLocation(program, "uProj");

    double last_time = glfwGetTime();

    std::printf("Controls:\n");
    std::printf("  WASD   – move\n");
    std::printf("  Q / E  – fly down / up\n");
    std::printf("  Mouse  – look\n");
    std::printf("  F      – toggle wireframe\n");
    std::printf("  ESC    – quit\n");

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - last_time);
        last_time = now;

        glfwPollEvents();
        g_camera.update(window, dt);

        int fb_w, fb_h;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);

        glClearColor(0.53f, 0.81f, 0.98f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUniform3f(loc_light_dir, 0.6, 1.0f, 0.4f);
        glUniform3fv(loc_cam_pos, 1, glm::value_ptr(g_camera.position));

        float aspect = (fb_h > 0) ? (float)fb_w / (float)fb_h : 1.f;
        glm::mat4 model = glm::mat4(1.f);
        glm::mat4 view = g_camera.view_matrix();
        glm::mat4 proj = g_camera.proj_matrix(aspect);

        // glm::value_ptr gives a float* in column-major order, which is
        // exactly what OpenGL expects, so transpose = GL_FALSE.
        glUniformMatrix4fv(loc_model, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(loc_view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(loc_proj, 1, GL_FALSE, glm::value_ptr(proj));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_grass);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, tex_rock);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, tex_snow);

        for (auto& chunk : chunks) {
            float dx = g_camera.position.x - chunk.centerX;
            float dz = g_camera.position.z - chunk.centerZ;
            float dist = std::sqrt(dx * dx + dz * dz);
            int lod = lod_for_distance(dist);

            if (lod != chunk.current_lod) {
                chunk.mesh->rebuild(lod);
                chunk.current_lod = lod;
            }

            chunk.mesh->draw();
        }

        glfwSwapBuffers(window);
    }

    glDeleteProgram(program);
    glfwTerminate();
    return 0;
}