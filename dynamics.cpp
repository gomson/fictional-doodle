#include "dynamics.h"

#include <glm/glm.hpp>
#include <glm/gtx/matrix_cross_product.hpp>

#include <memory>
#include <vector>

using glm::vec3;
using glm::mat3;

struct ParticleCollision
{
    int pidx;
    vec3 normal;
};

// Velocities are dampened before used for prediction of new positions.
// Damping method suggested in Muller 06
static void dampVelocities(
    const vec3* xs,
    const float* ms,
    float kdamping,
    int np,
    vec3* vs)
{
    vec3 xcm = vec3(0.0f);
    vec3 vcm = vec3(0.0f);
    float msum = 0.0f;
    for (int i = 0; i < np; i++)
    {
        xcm += xs[i] * ms[i];
        vcm += vs[i] * ms[i];
        msum += ms[i];
    }
    xcm /= msum;
    vcm /= msum;

    vec3 L = vec3(0.0f);
    for (int i = 0; i < np; i++)
    {
        L += cross(xs[i] - xcm, ms[i] * vs[i]);
    }

    mat3 I = mat3(0.0f);
    for (int i = 0; i < np; i++)
    {
        vec3 ri = xs[i] - xcm;
        mat3 ricross = matrixCross3(ri);
        I += ricross * transpose(ricross) * ms[i];
    }

    vec3 w = inverse(I) * L;

    for (int i = 0; i < np; i++)
    {
        vec3 dv = vcm + cross(w, xs[i] - xcm) - vs[i];
        vs[i] = vs[i] + kdamping * dv;
    }
}

// find the entry point of the ray x->p with an object
// if the ray enters an object:
//     1. Calculate entry point q and normal n there.
//     2. Add inequality constraint:
//        C(p) = (p-q) . n, k = 1
// if the ray is completely inside an object:
//     1. Compute surface point q (and normal n) closest to p
//     2. Add inequality constraint:
//        C(p) = (p - q) . n, k = 1
// if a point q moves through a triangle (p1,p2,p3):
//     1. Add inequality constraint:
//        C(q,p1,p2,p3) = (+/-) (q-p1) . [(p2 - p1) x (p3 - p1)]
//     * Note that the four vertices are represented by rays xi->pi
//       Therefore intersect a moving point with a moving triangle.
static void generateCollisionConstraints(
    const vec3* xs, const vec3* ps, int np,
    std::vector<ParticleCollision>& pcs,
    std::vector<Constraint>& coll_cs)
{

}

static void projectConstraints(
    const Constraint* cs, int nc,
    const Constraint* colls, int ncoll,
    const vec3* ps, const float* ws, int np)
{

}

// The velocity of each vertex for which a collision constraint
// was generated is dampened perpendicular to the collision normal
// and reflected in the direction of the collision normal
static void velocityUpdate(
    const ParticleCollision* pcs, int npcs,
    vec3* vs)
{
    int lastpidx = -1;
    for (int i = 0; i < npcs; i++)
    {
        if (pcs[i].pidx == lastpidx)
        {
            continue;
        }

        int pidx = pcs[i].pidx;

        // TODO: dampen perpendicular to collision normal

        if (dot(vs[pidx], pcs[i].normal) < 0.0f)
        {
            vs[pidx] = reflect(vs[pidx], pcs[i].normal);
        }

        lastpidx = pidx;
    }
}

void SimulateDynamics(
    float dtsec,
    const float* x0s_f, const float* v0s_f,
    const float* ms,
    const float* fexts_f,
    int np, int ni,
    const Constraint* cs, int nc,
    float* xs_f, float* vs_f)
{
    const vec3* x0s = (const vec3*)&x0s_f[0];
    const vec3* v0s = (const vec3*)&v0s_f[0];
    const vec3* fexts = (const  vec3*)&fexts_f[0];
    vec3* xs = (vec3*)&xs_f[0];
    vec3* vs = (vec3*)&vs_f[0];

    std::unique_ptr<float[]> ws(new float[np]);
    for (int i = 0; i < np; i++)
    {
        ws[i] = 1.0f / ms[i];
    }

    std::unique_ptr<float[]> ps_f(new float[np]);
    vec3* ps = (vec3*)&ps_f[0];

    std::vector<ParticleCollision> pcs;
    std::vector<Constraint> coll_cs;

    for (int i = 0; i < np; i++)
    {
        xs[i] = x0s[i];
        vs[i] = v0s[i];
    }

    for (int i = 0; i < np; i++)
    {
        vs[i] = vs[i] + dtsec  * ws[i] * fexts[i];
    }
     
    // TODO: Is damping different per particle or object or simulation?
    // kdamping = 1.0 means rigid body.
    float kdamping = 1.0f;
    dampVelocities(&xs[0], &ms[0], kdamping, np, &vs[0]);

    for (int i = 0; i < np; i++)
    {
        ps[i] = xs[i] + dtsec * vs[i];
    }

    for (int i = 0; i < np; i++)
    {
        generateCollisionConstraints(&xs[0], &ps[0], np, pcs, coll_cs);
    }

    for (int iter = 0; iter < ni; iter++)
    {
        projectConstraints(&cs[0], nc, &coll_cs[0], (int)coll_cs.size(), &ps[0], &ws[0], np);
    }

    for (int i = 0; i < np; i++)
    {
        vs[i] = (ps[i] - xs[i]) / dtsec;
        xs[i] = ps[i];
    }

    velocityUpdate(&pcs[0], (int)pcs.size(), &vs[0]);
}