#pragma once

#define DEFAULT_DYNAMICS_NUM_ITERATIONS 10

enum ConstraintFunc
{
    // Maintains a distance between two points
    // C(P0, P1) = |P1 - P0| - Distance
    CONSTRAINTFUNC_DISTANCE,
    // Maintains a direction (Nc) of a vector (P - Qc) from a point (Qc)
    // Resolves by intersecting the ray (Qc, P - Qc) with the plane
    // C(P) = (P - Qc) . Nc
    CONSTRAINTFUNC_INTERSECTION,
    // Maintains a direction (Ns) of a vector (P - Qs) from a point (Qs)
    // Resolves by projecting P onto the plane (Ns, Qs)
    // C(P) = (P - Qs) . Ns
    CONSTRAINTFUNC_PROJECTION,
    // Maintains a minimum angle at a joint between two points
    // Resolves by rotating the points away from each other
    // C(P) = acos((P2 - P1) . (P3 - P1)) - angle
    CONSTRAINTFUNC_ANGULAR
};

enum ConstraintType
{
    CONSTRAINTTYPE_EQUALITY,
    CONSTRAINTTYPE_INEQUALITY
};

struct DistanceConstraint
{
    float Distance;
};

struct IntersectionConstraint
{
    float Qc[3];
    float Nc[3];
};

struct ProjectionConstraint
{
    float Qs[3];
    float Ns[3];
};

struct AngleConstraint
{
    float Angle;
};

struct Constraint
{
    ConstraintFunc Func;
    int NumParticles;
    int* ParticleIDs;
    float Stiffness;
    ConstraintType Type;

    union
    {
        DistanceConstraint Distance;
        IntersectionConstraint Intersection;
        ProjectionConstraint Projection;
        AngleConstraint Angle;
    };
};

enum HullType
{
    HULLTYPE_NULL,
    HULLTYPE_SPHERE,
    HULLTYPE_CAPSULE
};

struct NullHull
{
    // nothing!
};

struct SphereHull
{
    float Radius;
};

struct CapsuleHull
{
    float Radius;
    // Consider the capsule below, with endpoints a and b:
    //     ------------------
    // (  a                 b  )
    //     -----------------
    // The particle with this hull is particle a
    // The other particle is the ID of particle b (can have a null hull)
    // To maintain the distance between the two points,
    // you must also add a distance constraint between the two particles.
    int OtherParticleID;
};

struct Hull
{
    HullType Type;
    union
    {
        NullHull Null;
        SphereHull Sphere;
        CapsuleHull Capsule;
    };
};

#ifdef _MSC_VER
extern "C"
_declspec(dllexport)
#endif
void SimulateDynamics(
    float deltaTimeSeconds,
    const float* particleOldPositionXYZs,
    const float* particleOldVelocityXYZs,
    const float* particleMasses,
    const float* particleExternalForceXYZs,
    const Hull* particleHulls,
    int numParticles, int numIterations,
    const Constraint* constraints, int numConstraints,
    float kdamping,
    float* particleNewPositionXYZs,
    float* particleNewVelocityXYZs);

using PFNSIMULATEDYNAMICSPROC = decltype(SimulateDynamics)*;