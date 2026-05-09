#pragma once
#include <glad/glad.h>
#include "PerlinNoise.hpp"

#include <vector>

const float TERRAIN_AMPLITUDE = 110.f;
const float SKIRT_Y = -20.f;

class TerrainMesh {
   public:
    TerrainMesh(float size, int divisions, float xOffset, float zOffset);
    ~TerrainMesh();

    // rebuilds the mesh with a new division count  
    void rebuild(int newDivisions);

    void draw() const;

    int get_divisions() const {return divisions;}

   private:
    void build(float size, int divisions);
    void upload();
    void free_gpu();

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // NOISE
    siv::PerlinNoise perlin;
    float frequency = 0.008f;
    int octaves = 8;
    float amplitude = TERRAIN_AMPLITUDE;

    float size, xOffset, zOffset;
    int divisions;
    GLuint vao = 0, vbo = 0, ebo = 0;
};