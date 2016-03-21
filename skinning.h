#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>
#include "opengl.h"

struct VertexWeights
{
    glm::u8vec4 BoneIDs;
    glm::vec4   Weights;
};
