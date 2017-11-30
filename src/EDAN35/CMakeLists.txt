cmake_minimum_required (VERSION 3.0)

set (
	ASSIGNMENT2_SOURCES

	"assignment2.cpp"
	"assignment2.hpp"
	"Ball.cpp"
	"Ball.hpp"
	"Quad.cpp"
	"Quad.hpp"
)

source_group (
	EDAN35${PATH_SEP}Assignment2

	FILES
	${PROJECT_SOURCE_DIR}/assignment2.cpp
	${PROJECT_SOURCE_DIR}/assignment2.hpp
	${PROJECT_SOURCE_DIR}/Ball.cpp
	${PROJECT_SOURCE_DIR}/Ball.hpp
	${PROJECT_SOURCE_DIR}/Quad.cpp
	${PROJECT_SOURCE_DIR}/Quad.hpp
)

luggcgl_new_assignment ("EDAN35_Assignment2" "${ASSIGNMENT2_SOURCES}" "")
