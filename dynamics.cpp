#include "dynamics.h"

#include <glm/glm.hpp>

#include <memory>

static void dampVelocities(
    const glm::vec3* vs, int np)
{

}

static void generateCollisionConstraints(
    glm::vec3 x, glm::vec3 p,
    Constraint* colls, int* ncolls)
{

}

static void projectConstraints(
    const Constraint* cs, int nc,
    const Constraint* colls, int ncoll,
    const glm::vec3* ps, int np)
{

}

// Velocities of colliding vertices are modified
// according to friction and restitution coefficients.
static void velocityUpdate(
    const glm::vec3* vs, int np)
{

}

void SimulateDynamics(
    float dtsec,
    const float* x0s_f, const float* v0s_f,
    const float* ws,
    const float* fexts_f,
    int np, int ni,
    const Constraint* cs, int nc,
    float* xs_f, float* vs_f)
{
    const glm::vec3* x0s = (const glm::vec3*)&x0s_f[0];
    const glm::vec3* v0s = (const glm::vec3*)&v0s_f[0];
    const glm::vec3* fexts = (const  glm::vec3*)&fexts_f[0];
    glm::vec3* xs = (glm::vec3*)&xs_f[0];
    glm::vec3* vs = (glm::vec3*)&vs_f[0];

    std::unique_ptr<float[]> ps_f(new float[np]);
    glm::vec3* ps = (glm::vec3*)&ps_f[0];

    Constraint colls[1];
    int ncoll;

    for (int i = 0; i < np; i++)
    {
        xs[i] = x0s[i];
        vs[i] = v0s[i];
    }

    for (int i = 0; i < np; i++)
    {
        vs[i] = vs[i] + dtsec  * ws[i] * fexts[i];
    }
        
    dampVelocities(&vs[0], np);

    for (int i = 0; i < np; i++)
    {
        ps[i] = xs[i] + dtsec * vs[i];
    }

    for (int i = 0; i < np; i++)
    {
        generateCollisionConstraints(xs[i], ps[i], &colls[0], &ncoll);
    }

    for (int iter = 0; iter < ni; iter++)
    {
        projectConstraints(&cs[0], nc, &colls[0], ncoll, &ps[0], np);
    }

    for (int i = 0; i < np; i++)
    {
        vs[i] = (ps[i] - xs[i]) / dtsec;
        xs[i] = ps[i];
    }

    velocityUpdate(&vs[0], np);
}