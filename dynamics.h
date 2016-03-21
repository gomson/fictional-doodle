#pragma once

#define DEFAULT_DYNAMICS_NUM_ITERATIONS 10

typedef enum ConstraintFunc
{
    CONSTRAINT_FUNC_DISTANCE
} ConstraintFunc;

typedef enum ConstraintType
{
    CONSTRAINT_TYPE_EQUALITY,
    CONSTRAINT_TYPE_INEQUALITY
} ConstraintType;

typedef struct Constraint
{
    ConstraintFunc Func;
    int NumParticles;
    int* ParticleIDs;
    float Stiffness;
    ConstraintType Type;

    // Rest length for distance constraints
    float Distance;
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
    float* particleNewPositionXYZs,
    float* particleNewVelocityXYZs);

using PFNSIMULATEDYNAMICSPROC = decltype(SimulateDynamics)*;