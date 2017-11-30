//
//  Ball.hpp
//  Project
//
//  Created by Mathis Gilliot on 28/11/2017.
//

#ifndef Ball_hpp
#define Ball_hpp

#include <stdio.h>
#include "core/helpers.hpp"

namespace Ball
{
	//! \brief Create a sphere for some tesselation level and make it
	//!        available to OpenGL.
	//!
	//! @param res_theta tessellation resolution (nbr of vertices) in the latitude direction ( 0 < theta < PI/2 )
	//! @param res_phi tessellation resolution (nbr of vertices) in the longitude direction ( 0 < phi < 2PI )
	//! @param radius radius of the sphere
	//! @return wrapper around OpenGL objects' name containing the geometry
	//!         data
	bonobo::mesh_data createSphere(unsigned int const res_theta, unsigned int const res_phi, float const radius);
	
}



#endif /* Ball_hpp */
