#include "dynamics.h"

#include <glm/vec3.hpp>
#include <glm/mat3x3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_cross_product.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <vector>
#include <cstdio>
#include <cassert>

using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::make_vec3;

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
//        C(p) = (p - q) . n, k = 1
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
    const vec3* xs, const vec3* ps, const Hull* hs, int np,
    std::vector<ParticleCollision>& pcs,
    std::vector<Constraint>& coll_cs)
{
    auto sphereVSplane = [&pcs, &coll_cs](vec4 plane, int pidx, vec3 x, vec3 p, float r)
    {
        // offset plane in direction of radius
        plane.w -= r;

        bool x_in = dot(plane, vec4(x, 1.0f)) < 0.0f;
        bool p_in = dot(plane, vec4(p, 1.0f)) < 0.0f;
        if (p_in && !x_in)
        {
            // Compute intersection of x->p with plane
            float t = (plane.w - dot(vec3(plane), x))
                / dot(vec3(plane), (p - x));
            vec3 q = x + t * (p - x);

            ParticleCollision pc;
            pc.pidx = pidx;
            pc.normal = vec3(plane);
            pcs.push_back(pc);

            Constraint c;
            c.Func = CONSTRAINTFUNC_INTERSECTION;
            c.NumParticles = 1;
            // hack: vector gets resized, so I can't point to it
            // instead use indices, then switch to pointers
            // after all collision constraints are created
            c.ParticleIDs = (int*)0 + (int)pcs.size() - 1;
            c.Stiffness = 1.0f;
            c.Type = CONSTRAINTTYPE_INEQUALITY;
            *(vec3*)&c.Intersection.Qc = q;
            *(vec3*)&c.Intersection.Nc = vec3(plane);
            coll_cs.push_back(c);
        }
        else if (p_in && x_in)
        {
            // find surface point closest to p
            vec3 qs = p - vec3(plane) * dot(plane, vec4(p, 1.0f));

            ParticleCollision pc;
            pc.pidx = pidx;
            pc.normal = vec3(plane);
            pcs.push_back(pc);

            Constraint c;
            c.Func = CONSTRAINTFUNC_PROJECTION;
            c.NumParticles = 1;
            // hack: vector gets resized, so I can't point to it
            // instead use indices, then switch to pointers
            // after all collision constraints are created
            c.ParticleIDs = (int*)0 + (int)pcs.size() - 1;
            c.Stiffness = 1.0f;
            c.Type = CONSTRAINTTYPE_INEQUALITY;
            *(vec3*)&c.Projection.Qs = qs;
            *(vec3*)&c.Projection.Ns = vec3(plane);
            coll_cs.push_back(c);
        }
    };

    for (int i = 0; i < np; i++)
    {
        vec3 x = xs[i];
        vec3 p = ps[i];
        const Hull& h = hs[i];

        const vec4 kGroundPlane = vec4(0.0f, 1.0f, 0.0f, 0.0f);

        if (h.Type == HULLTYPE_NULL)
        {
            // No collisions happen if there's no hull
        }
        else if (h.Type == HULLTYPE_SPHERE)
        {
            sphereVSplane(kGroundPlane, i, x, p, h.Sphere.Radius);
        }
        else if (h.Type == HULLTYPE_CAPSULE)
        {
            vec3 x_other = xs[h.Capsule.OtherParticleID];
            vec3 p_other = xs[h.Capsule.OtherParticleID];
            sphereVSplane(kGroundPlane, i,                         x,       p,       h.Capsule.Radius);
            sphereVSplane(kGroundPlane, h.Capsule.OtherParticleID, x_other, p_other, h.Capsule.Radius);
        }
        else
        {
            assert(false && "Unhandled hull type");
        }
    }
}

// Want to solve dp (delta p) that maintains the constraint
// euler integration: C(p + dp) ~= C(p) + dC/dp(p) * dp
// do a bunch of math and you end up with:
//  dp[i] = -s * w[i] * dC/dp[i](p1,...,pn)
// where s = C(p1,...,pn) / sum_j(w[j] * lengthSquared(dC/dp[j](p1,...,pn)))
static void projectConstraint(
    const Constraint* c,
    const float* ws, int np,
    vec3* ps)
{
    if (c->Func == CONSTRAINTFUNC_DISTANCE)
    {
        assert(c->NumParticles == 2);

        float distance = c->Distance.Distance;
        int i0 = c->ParticleIDs[0];
        int i1 = c->ParticleIDs[1];

        if (c->Type == CONSTRAINTTYPE_EQUALITY)
        {
            // C(p1, p2) = length(p1 - p2) - d = 0
            float w0 = ws[i0];
            float w1 = ws[i1];
            vec3 p1_to_p0 = ps[i0] - ps[i1];
            float p1_to_p0_len = length(p1_to_p0);
            vec3 p1_to_p0_dir = p1_to_p0_len > 0.0f ? p1_to_p0 / p1_to_p0_len : vec3(0.0f);
            vec3 dp0 = -(w0 / (w0 + w1)) * (p1_to_p0 - distance * p1_to_p0_dir);
            vec3 dp1 = +(w1 / (w0 + w1)) * (p1_to_p0 - distance * p1_to_p0_dir);
            ps[i0] += dp0 * c->Stiffness;
            ps[i1] += dp1 * c->Stiffness;
        }
        else
        {
            assert(false && "Unhandled constraint type");
        }
    }
    else if (c->Func == CONSTRAINTFUNC_INTERSECTION)
    {
        assert(c->NumParticles == 1);
        int i = c->ParticleIDs[0];

        vec3 qc = make_vec3(c->Intersection.Qc);
        vec3 nc = make_vec3(c->Intersection.Nc);

        if (c->Type == CONSTRAINTTYPE_INEQUALITY)
        {
            vec3 to_intersect = qc - ps[i];
            if (dot(nc, to_intersect) >= 0.0f)
            {
                ps[i] += c->Stiffness * to_intersect;
            }
        }
        else
        {
            assert(false && "Unhandled constraint type");
        }
    }
    else if (c->Func == CONSTRAINTFUNC_PROJECTION)
    {
        assert(c->NumParticles == 1);
        int i = c->ParticleIDs[0];

        vec3 qs = make_vec3(c->Projection.Qs);
        vec3 ns = make_vec3(c->Projection.Ns);

        if (c->Type == CONSTRAINTTYPE_INEQUALITY)
        {
            vec3 to_intersect = qs - ps[i];
            if (dot(ns, to_intersect) >= 0.0f)
            {
                ps[i] += c->Stiffness * to_intersect;
            }
        }
        else
        {
            assert(false && "Unhandled constraint type");
        }
    }
    else if (c->Func == CONSTRAINTFUNC_ANGULAR)
    {
        assert(c->NumParticles == 3);
        
        float angle = c->Angle.Angle;
        int i0 = c->ParticleIDs[0];
        int i1 = c->ParticleIDs[1];
        int i2 = c->ParticleIDs[2];

        if (c->Type == CONSTRAINTTYPE_EQUALITY)
        {
            assert(!isnan(angle));
            
            vec3 p0 = ps[i0];
            vec3 p1 = ps[i1];
            vec3 p2 = ps[i2];
            
            if (length(p1 - p0) > 0.0001f && length(p2 - p0) > 0.0001f)
            {
                float curr_angle = std::acos(dot(normalize(p1 - p0), normalize(p2 - p0)));
                float delta_angle = (angle - curr_angle) * c->Stiffness;

                vec3 rotationAxis = normalize(cross(p1 - p0, p2 - p0));

                vec3 newP0P1 = rotate(normalize(p1 - p0), -delta_angle / 2.0f, rotationAxis);
                vec3 newP0P2 = rotate(normalize(p2 - p0), delta_angle / 2.0f, rotationAxis);

                // Probably more stable to move just p0 instead of p1 and p2
                // ps[i1] = p0 + newP0P1 * length(p1 - p0);
                // ps[i2] = p0 + newP0P2 * length(p2 - p0);

                // float w0 = ws[i1];
                // float w1 = ws[i2];
                // ps[i0] = (-w0/(w0+w1)*(p0 + newP0P1 * length(p1 - p0)) + w1 / (w0 + w1)*(p0 + newP0P2 * length(p2 - p0))) / 2.0f;

                float new_angle = std::acos(dot(normalize(ps[i1] - p0), normalize(ps[i2] - p0)));

                //static int asdf = 0;
                //if (asdf < 400)
                //{
                //    if (asdf == 0) printf("start asdf\n\n\n\n\n");
                //    printf("curr_angle: %f\n", curr_angle);
                //    printf("delta_angle: %f\n", delta_angle);
                //    printf("new_angle: %f\n", new_angle);
                //    printf("i0: %d, i1: %d, i2: %d\n", i0, i1, i2);
                //    printf("rotationAxis: {%f,%f,%f}\n", rotationAxis.x, rotationAxis.y, rotationAxis.z);
                //    // printf("p1: {%f,%f,%f}\n", p1.x, p1.y, p1.z);
                //    printf("newP0P1: {%f,%f,%f}\n", newP0P1.x, newP0P1.y, newP0P1.z);
                //    // printf("i0: %d, p0: {%f,%f,%f}\n", i0, p0.x, p0.y, p0.z);
                //    // printf("p1 - p0: {%f,%f,%f}\n", (p1 - p0).x, (p1 - p0).y, (p1 - p0).z);
                //    asdf++;
                //}
            }
        }
        else
        {
            assert(false && "Unhandled constraint type");
        }
    }
    else
    {
        assert(false && "Unhandled constraint function");
    }
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

        if (dot(vs[pidx], pcs[i].normal) < 0.0f)
        {
            vs[pidx] = reflect(vs[pidx], pcs[i].normal);
        }

        // Dampen perpendicular to collision normal
        vec3 normalpart = dot(vs[pidx], pcs[i].normal) * pcs[i].normal;
        vec3 nonnormalpart = vs[pidx] - normalpart;
        vs[pidx] = normalpart + nonnormalpart * 0.5f;

        lastpidx = pidx;
    }
}

void SimulateDynamics(
    float dtsec,
    const float* x0s_f, const float* v0s_f,
    const float* ms,
    const float* fexts_f,
    const Hull* hs,
    int np, int ni,
    const Constraint* cs, int nc,
    float kdamping,
    float* xs_f, float* vs_f)
{
    if (np == 0)
    {
        return;
    }

    const vec3* x0s = (const vec3*)&x0s_f[0];
    const vec3* v0s = (const vec3*)&v0s_f[0];
    const vec3* fexts = (const  vec3*)&fexts_f[0];
    vec3* xs = (vec3*)&xs_f[0];
    vec3* vs = (vec3*)&vs_f[0];

    float* ws = new float[np];
    for (int i = 0; i < np; i++)
    {
        ws[i] = 1.0f / ms[i];
    }

    float* ps_f = new float[np * 3];
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
        vs[i] = vs[i] + dtsec * ws[i] * fexts[i];
    }
     
    dampVelocities(&xs[0], &ms[0], kdamping, np, &vs[0]);

    for (int i = 0; i < np; i++)
    {
        ps[i] = xs[i] + dtsec * vs[i];
    }

    generateCollisionConstraints(&xs[0], &ps[0], &hs[0], np, pcs, coll_cs);

    // hack: since generateCollisionConstraints pushes onto vectors,
    // we can't keep any pointers to inside it while building it.
    // Instead, indices are stored, which are converted back to pointers here.
    assert(coll_cs.size() == pcs.size());
    for (int coll_i = 0; coll_i < (int)coll_cs.size(); coll_i++)
    {
        int collisionID = int(coll_cs[coll_i].ParticleIDs - (int*)0);
        coll_cs[coll_i].ParticleIDs = &pcs[collisionID].pidx;
    }

    for (int iter = 0; iter < ni; iter++)
    {
        for (int i = 0; i < nc; i++)
        {
            projectConstraint(&cs[i], &ws[0], np, &ps[0]);
        }
        for (int i = 0; i < (int)coll_cs.size(); i++)
        {
            projectConstraint(&coll_cs[i], &ws[0], np, &ps[0]);
        }
    }

    for (int i = 0; i < np; i++)
    { 
        vs[i] = (ps[i] - xs[i]) / dtsec;
        xs[i] = ps[i];
    }

    if (!pcs.empty())
    {
        velocityUpdate(&pcs[0], (int)pcs.size(), &vs[0]);
    }

    delete[] ps_f;
    delete[] ws;
}