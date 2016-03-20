#include "dynamics.h"

void SimulateDynamics(
    const glm::vec3* oldVertexPositionXYZs,
    const glm::vec3* oldVertexVelocityXYZs,
    glm::vec3* newVertexPositionXYZs,
    glm::vec3* newVertexVelocityXYZs,
    int np,
    int numiter)
{
    for (int i = 0; i < np; i++)
    {
        newVertexPositionXYZs[i] = oldVertexPositionXYZs[i];
        newVertexVelocityXYZs[i] = oldVertexVelocityXYZs[i];
    }

    for (int iter = 0; iter < numiter; iter++)
    {

    }
}