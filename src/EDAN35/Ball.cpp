#include "Ball.hpp"
#include "core/Log.h"
#include "core/utils.h"

#include <glm/glm.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

bonobo::mesh_data
Ball::createSphere(unsigned int const res_theta,
								unsigned int const res_phi, float const radius)
{
	auto const vertices_nb = res_theta * res_phi;
	
	auto vertices  = std::vector<glm::vec3>(vertices_nb);
	auto normals   = std::vector<glm::vec3>(vertices_nb);
	auto texcoords = std::vector<glm::vec3>(vertices_nb);
	auto tangents  = std::vector<glm::vec3>(vertices_nb);
	auto binormals = std::vector<glm::vec3>(vertices_nb);
	
	// variables to step through theta, 0-2PI stepping by dtheta
	float theta = 0.0f,
	dtheta = 2.0f * bonobo::pi / (static_cast<float>(res_theta) - 1.0f);
	
	// variables to step through phi
	float phi = 0.0f,
	dphi = bonobo::pi / (static_cast<float>(res_phi) - 1.0f);
	
	// generate vertices
	size_t index = 0u;
	for(unsigned int i = 0u; i < res_phi; ++i)
	{
		float cos_phi = std::cos(phi), sin_phi = std::sin(phi);
		
		for (unsigned int j = 0u; j < res_theta; ++j)
		{
			
			float cos_theta = std::cos(theta), sin_theta = std::sin(theta);
			
			
			// vertices
			vertices[index] = glm::vec3(radius * sin_theta * sin_phi,
										-radius * cos_phi,
										radius * cos_theta * sin_phi
										);
			
			// texture coordinates
			texcoords[index] = glm::vec3(static_cast<float>(j) / (static_cast<float>(res_phi) - 1.0f),
										 static_cast<float>(i) / (static_cast<float>(res_theta)  - 1.0f),
										 0.0f);
			
			// tangents
			auto t = glm::vec3(cos_theta,
							   0.0f,
							   -sin_theta);
			
			tangents[index] = t;
			
			// binormals
			auto b = glm::vec3(radius * sin_theta * cos_phi,
							   radius * sin_phi,
							   radius * cos_theta * cos_phi);
			
			b = glm::normalize(b);
			binormals[index] = b;
			
			// normals
			auto const n = glm::cross(t, b);
			normals[index] = n;
			
			++index;
			theta += dtheta;
		}
		phi += dphi;
		theta=0;
	}
	
	
	// create index array
	auto indices = std::vector<glm::uvec3>(2u * (res_theta - 1u) * (res_phi - 1u));
	
	// generate indices
	index = 0u;
	for (unsigned int i = 0u; i < res_phi - 1u; ++i)
	{
		for (unsigned int j = 0u; j < res_theta - 1u; ++j)
		{
			indices[index] = glm::uvec3(res_theta * i + j,
										res_theta * i + j + 1u,
										res_theta * i + j + 1u + res_theta);
			++index;
			
			indices[index] = glm::uvec3(res_theta * i + j,
										res_theta * i + j + res_theta + 1u,
										res_theta * i + j + res_theta );
			++index;
		}
	}
	
	bonobo::mesh_data data;
	glGenVertexArrays(1, &data.vao);
	assert(data.vao != 0u);
	glBindVertexArray(data.vao);
	
	auto const vertices_offset = 0u;
	auto const vertices_size = static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3));
	auto const normals_offset = vertices_size;
	auto const normals_size = static_cast<GLsizeiptr>(normals.size() * sizeof(glm::vec3));
	auto const texcoords_offset = normals_offset + normals_size;
	auto const texcoords_size = static_cast<GLsizeiptr>(texcoords.size() * sizeof(glm::vec3));
	auto const tangents_offset = texcoords_offset + texcoords_size;
	auto const tangents_size = static_cast<GLsizeiptr>(tangents.size() * sizeof(glm::vec3));
	auto const binormals_offset = tangents_offset + tangents_size;
	auto const binormals_size = static_cast<GLsizeiptr>(binormals.size() * sizeof(glm::vec3));
	auto const bo_size = static_cast<GLsizeiptr>(vertices_size
												 + normals_size
												 + texcoords_size
												 + tangents_size
												 + binormals_size
												 );
	glGenBuffers(1, &data.bo);
	assert(data.bo != 0u);
	glBindBuffer(GL_ARRAY_BUFFER, data.bo);
	glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);
	
	glBufferSubData(GL_ARRAY_BUFFER, vertices_offset, vertices_size, static_cast<GLvoid const*>(vertices.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::vertices));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));
	
	glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size, static_cast<GLvoid const*>(normals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::normals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(normals_offset));
	
	glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size, static_cast<GLvoid const*>(texcoords.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(texcoords_offset));
	
	glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size, static_cast<GLvoid const*>(tangents.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::tangents));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(tangents_offset));
	
	glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size, static_cast<GLvoid const*>(binormals.data()));
	glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::binormals));
	glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(binormals_offset));
	
	glBindBuffer(GL_ARRAY_BUFFER, 0u);
	
	data.indices_nb = indices.size() * 3u;
	glGenBuffers(1, &data.ibo);
	assert(data.ibo != 0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(glm::uvec3)), reinterpret_cast<GLvoid const*>(indices.data()), GL_STATIC_DRAW);
	
	glBindVertexArray(0u);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);
	
	return data;
}
