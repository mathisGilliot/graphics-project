#include <stdio.h>
#include "core/helpers.hpp"

namespace Quad
{
	//! \brief Create a quad consisting of two triangles and make it
	//!        available to OpenGL.
	//!
	//! @param width the width of the quad
	//! @param height the height of the quad
	//! @return wrapper around OpenGL objects' name containing the geometry
	//!         data
	bonobo::mesh_data createQuad(unsigned int width, unsigned int height, unsigned int res);
	
	
}
