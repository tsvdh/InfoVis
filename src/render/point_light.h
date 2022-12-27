#ifndef _POINT_LIGHT_H_
#define _POINT_LIGHT_H_
#include <glm/vec3.hpp>

namespace render {

// This is a point light struct, used for raytracing
struct PointLight {
    glm::vec3 pos;
    glm::vec3 val;
};

}

#endif
