#include "interpolation.hpp"

#include <iostream>

glm::vec3
interpolation::evalLERP(glm::vec3 const& p0, glm::vec3 const& p1, float const x)
{
	return (1-x)*p0 + x*p1;
}

glm::vec3
interpolation::evalCatmullRom(glm::vec3 const& p0, glm::vec3 const& p1,
                              glm::vec3 const& p2, glm::vec3 const& p3,
                              float const t, float const x)
{
    return (-t*x + 2*t*x*x - t*x*x*x)*p0 + (1+(t-3)*x*x + (2-t)*x*x*x)*p1 + (t*x + (3-2*t)*x*x + (t-2)*x*x*x)*p2 + (-t*x*x + t*x*x*x)*p3;
}
