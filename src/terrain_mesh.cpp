#include "terrain_mesh.h"

#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <stdexcept>

TerrainMesh::TerrainMesh(float size, int divisions, float xOffset, float zOffset)
    : size(size), xOffset(xOffset), zOffset(zOffset), divisions(divisions) {
    const siv::PerlinNoise::seed_type seed = 123456790u;
    perlin = siv::PerlinNoise(seed);

    build(size, divisions);
    upload();
}

TerrainMesh::~TerrainMesh() { free_gpu(); }

void TerrainMesh::rebuild(int newDivisions) {
    divisions = newDivisions;
    vertices.clear();
    indices.clear();
    free_gpu();
    build(size, newDivisions);
    upload();
}

void TerrainMesh::free_gpu() {
    if (vao) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (vbo) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }

    if (ebo) {
        glDeleteBuffers(1, &ebo);
        ebo = 0;
    }
}

void TerrainMesh::build(float size, int divisions) {
    int verts = divisions + 1;
    int vertsExt = verts + 2;  // one-cell border so edge normals use real centered diffs
    float step = size / (float)divisions;
    float half = size / 2.f;

    // extended grid: index (r,c) maps to chunk vertex (r-1, c-1)
    std::vector<float> heights(vertsExt * vertsExt);
    for (int r = 0; r < vertsExt; r++) {
        for (int c = 0; c < vertsExt; c++) {
            float wx = ((c - 1) * step - half) + xOffset;
            float wz = ((r - 1) * step - half) + zOffset;
            heights[r * vertsExt + c] =
                perlin.normalizedOctave2D_01(wx * frequency, wz * frequency, octaves) * amplitude;
        }
    }

    vertices.reserve(verts * verts * 6 + 4 * verts * 6);
    for (int row = 0; row < verts; row++) {
        for (int col = 0; col < verts; col++) {
            float wx = col * step - half + xOffset;
            float wz = row * step - half + zOffset;
            int er = row + 1, ec = col + 1;  // coords in extended grid
            float wy = heights[er * vertsExt + ec];

            // finite difference across neighbors
            float hL = heights[er * vertsExt + (ec - 1)];
            float hR = heights[er * vertsExt + (ec + 1)];
            float hD = heights[(er - 1) * vertsExt + ec];
            float hU = heights[(er + 1) * vertsExt + ec];
            glm::vec3 horizontal = glm::vec3(2.f * step, hR - hL, 0.f);
            glm::vec3 vertical = glm::vec3(0.f, hU - hD, 2.f * step);
            glm::vec3 normal = glm::normalize(glm::cross(vertical, horizontal));

            vertices.push_back(wx);
            vertices.push_back(wy);
            vertices.push_back(wz);
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
        }
    }

    indices.reserve(divisions * divisions * 6 + 4 * divisions * 6);
    for (int row = 0; row < divisions; row++) {
        for (int col = 0; col < divisions; col++) {
            unsigned int tl = row * verts + col;
            unsigned int tr = tl + 1;
            unsigned int bl = tl + verts;
            unsigned int br = bl + 1;

            // Triangle 1
            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            // Triangle 2
            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    // skirts: drop each chunk edge to SKIRT_Y to hide LOD cracks between neighbors
    struct Edge { int row0, col0, dRow, dCol; bool windingA; };
    Edge edges[4] = {
        {0,         0,         0, 1, true },   // top
        {verts - 1, 0,         0, 1, false},   // bottom
        {0,         0,         1, 0, false},   // left
        {0,         verts - 1, 1, 0, true },   // right
    };

    for (const Edge& e : edges) {
        unsigned int skirtStart = (unsigned int)(vertices.size() / 6);

        for (int i = 0; i < verts; i++) {
            int r = e.row0 + e.dRow * i;
            int c = e.col0 + e.dCol * i;
            const float* top = &vertices[(r * verts + c) * 6];
            vertices.push_back(top[0]);
            vertices.push_back(SKIRT_Y);
            vertices.push_back(top[2]);
            vertices.push_back(top[3]);
            vertices.push_back(top[4]);
            vertices.push_back(top[5]);
        }

        for (int i = 0; i < verts - 1; i++) {
            unsigned int topA  = (e.row0 + e.dRow * i)       * verts + (e.col0 + e.dCol * i);
            unsigned int topB  = (e.row0 + e.dRow * (i + 1)) * verts + (e.col0 + e.dCol * (i + 1));
            unsigned int skirtA = skirtStart + i;
            unsigned int skirtB = skirtStart + i + 1;

            if (e.windingA) {
                indices.push_back(topA);   indices.push_back(topB);   indices.push_back(skirtA);
                indices.push_back(topB);   indices.push_back(skirtB);  indices.push_back(skirtA);
            } else {
                indices.push_back(topA);   indices.push_back(skirtA);  indices.push_back(topB);
                indices.push_back(topB);   indices.push_back(skirtA);  indices.push_back(skirtB);
            }
        }
    }
}

void TerrainMesh::upload() {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void TerrainMesh::draw() const {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}