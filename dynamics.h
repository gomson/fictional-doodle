#pragma once

#define DEFAULT_DYNAMICS_NUM_ITERATIONS 10

typedef enum ConstraintFunc
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
    CONSTRAINTFUNC_PROJECTION
} ConstraintFunc;

typedef enum ConstraintType
{
    CONSTRAINTTYPE_EQUALITY,
    CONSTRAINTTYPE_INEQUALITY
} ConstraintType;

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

typedef struct Constraint
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
    };

} Constraint;

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
    int numParticles, int numIterations,
    const Constraint* constraints, int numConstraints,
    float kdamping,
    float* particleNewPositionXYZs,
    float* particleNewVelocityXYZs);

using PFNSIMULATEDYNAMICSPROC = decltype(SimulateDynamics)*;