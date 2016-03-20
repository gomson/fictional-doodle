#pragma once

#include <glm/glm.hpp>

void SimulateDynamics(
    const glm::vec3* oldVertexPositionXYZs,
    const glm::vec3* oldVertexVelocityXYZs,
    glm::vec3* newVertexPositionXYZs,
    glm::vec3* newVertexVelocityXYZs,
    int numParticles);