#include "dynamics.h"

void SimulateDynamics(
    const glm::vec3* oldVertexPositionXYZs,
    const glm::vec3* oldVertexVelocityXYZs,
    glm::vec3* newVertexPositionXYZs,
    glm::vec3* newVertexVelocityXYZs,
    int np)
{
    for (int i = 0; i < np; i++)
    {
        newVertexPositionXYZs[i] = oldVertexPositionXYZs[i];
        newVertexVelocityXYZs[i] = oldVertexVelocityXYZs[i];
    }
}