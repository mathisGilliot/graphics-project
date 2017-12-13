#pragma once
#include <glm/glm.hpp>

class InputHandler;
class Window;


namespace edan35
{
	//! \brief Wrapper class for Assignment 2
	class Assignment2 {
	public:
		//! \brief Default constructor.
		//!
		//! It will initialise various modules of bonobo and retrieve a
		//! window to draw to.
		Assignment2();

		//! \brief Default destructor.
		//!
		//! It will release the bonobo modules initialised by the
		//! constructor, as well as the window.
		~Assignment2();

		//! \brief Contains the logic of the assignment, along with the
		//! render loop.
		void run();
		
		glm::vec3 symetry(glm::vec3 originalPoint, glm::vec3 planePoint, glm::vec3 normalVector);

	private:
		InputHandler *inputHandler;
		Window       *window;
	};
}
