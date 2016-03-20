#pragma once

#define DEFAULT_DYNAMICS_NUM_ITERATIONS 10

typedef enum ConstraintFunc
{
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
} Constraint;

extern "C"
void SimulateDynamics(
    float deltaTimeSeconds,
    const float* particleOldPositionXYZs, 
    const float* particleOldVelocityXYZs,
    const float* particleOneOverMassXYZs,
    const float* particleExternalForceXYZs,
    int numParticles, int numIterations,
    const Constraint* constraints,
    int numConstraints,
    float* particleNewPositionXYZs, 
    float* particleNewVelocityXYZs);