#pragma once

#include <glm/glm.hpp>

#define DEFAULT_DYNAMICS_NUM_ITERATIONS 10

void SimulateDynamics(
    const glm::vec3* oldPositions, const glm::vec3* oldVelocitys,
    glm::vec3* newPositions, glm::vec3* newVelocitys,
    int numParticles,
    int numIterations);