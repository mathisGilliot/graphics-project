#include "assignment2.hpp"

#include "config.hpp"
#include "external/glad/glad.h"
#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/GLStateInspection.h"
#include "core/GLStateInspectionView.h"
#include "core/helpers.hpp"
#include "core/InputHandler.h"
#include "core/Log.h"
#include "core/LogView.h"
#include "core/Misc.h"
#include "core/node.hpp"
#include "core/utils.h"
#include "core/Window.h"
#include <imgui.h>
#include "external/imgui_impl_glfw_gl3.h"
#include "Ball.hpp"
#include "Quad.hpp"

#include "external/glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstdlib>
#include <stdexcept>

#define PI 3.14159265359

enum class polygon_mode_t : unsigned int {
	fill = 0u,
	line,
	point
	};
	
	static polygon_mode_t get_next_mode(polygon_mode_t mode)
	{
		return static_cast<polygon_mode_t>((static_cast<unsigned int>(mode) + 1u) % 3u);
	}
	
	namespace constant
	{
		constexpr uint32_t shadowmap_res_x = 1024;
		constexpr uint32_t shadowmap_res_y = 1024;
		
		constexpr size_t lights_nb           = 1;
		constexpr size_t mirrors_nb     	 = 2;
		constexpr float  light_intensity     = 720000.0f;
		constexpr float  light_angle_falloff = glm::radians(30.0f);
		constexpr float  light_cutoff        = 0.05f;
	}
	
	static bonobo::mesh_data loadCone();
	
	edan35::Assignment2::Assignment2()
	{
		Log::View::Init();
		
		window = Window::Create("EDAN35: Assignment 2", config::resolution_x,
								config::resolution_y, config::msaa_rate, false, false);
		if (window == nullptr) {
			Log::View::Destroy();
			throw std::runtime_error("Failed to get a window: aborting!");
		}
		inputHandler = new InputHandler();
		window->SetInputHandler(inputHandler);
		
		GLStateInspection::Init();
		GLStateInspection::View::Init();
		
		bonobo::init();
	}
	
	edan35::Assignment2::~Assignment2()
	{
		bonobo::deinit();
		
		GLStateInspection::View::Destroy();
		GLStateInspection::Destroy();
		
		delete inputHandler;
		inputHandler = nullptr;
		
		Window::Destroy(window);
		window = nullptr;
		
		Log::View::Destroy();
	}
	
	glm::vec3
	edan35::Assignment2::symetry(glm::vec3 originalPoint, glm::vec3 planePoint, glm::vec3 normalVector)
	{
		glm::vec3 PM = originalPoint - planePoint;
		float distToPlane = glm::dot(PM, normalVector);
		glm::vec3 sym = originalPoint - 2 * distToPlane * normalVector;
		return sym;
	}
	
	void
	edan35::Assignment2::run()
	{
		// Load the geometry of Sponza
		auto const sponza_geometry = bonobo::loadObjects("../crysponza/sponza.obj");
		if (sponza_geometry.empty()) {
			LogError("Failed to load the Sponza model");
			return;
		}
		std::vector<Node> sponza_elements;
		sponza_elements.reserve(sponza_geometry.size());
		for (auto const& shape : sponza_geometry) {
			Node node;
			node.set_geometry(shape);
			sponza_elements.push_back(node);
		}
		
		auto const cone_geometry = loadCone();
		Node cone;
		cone.set_geometry(cone_geometry);
		
		auto const window_size = window->GetDimensions();
		auto const mirror_width = 50;
		auto const mirror_height = 50;
		
		//
		// Setup the camera
		//
		FPSCameraf mCamera(bonobo::pi / 4.0f,
						   static_cast<float>(window_size.x) / static_cast<float>(window_size.y),
						   1.0f, 10000.0f);
		mCamera.mWorld.SetTranslate(glm::vec3(0.0f, 100.0f, 180.0f));
		mCamera.mWorld.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));
		mCamera.mMouseSensitivity = 0.003f;
		mCamera.mMovementSpeed = 0.25f;
		window->SetCamera(&mCamera);
		
		//
		// Load all the shader programs used
		//
		// Build Program
		auto mirrorShader = bonobo::createProgram("mirror.vert", "mirror.frag");
		if (mirrorShader == 0u) {
			LogError("Failed to load mirror shader");
			return;
		}
		auto fallback_shader = bonobo::createProgram("fallback.vert", "fallback.frag");
		if (fallback_shader == 0u) {
			LogError("Failed to load fallback shader");
			return;
		}
		auto const reload_shader = [fallback_shader](std::string const& vertex_path, std::string const& fragment_path, GLuint& program){
			if (program != 0u && program != fallback_shader)
				glDeleteProgram(program);
			program = bonobo::createProgram("../EDAN35/" + vertex_path, "../EDAN35/" + fragment_path);
			if (program == 0u) {
				LogError("Failed to load \"%s\" and \"%s\"", vertex_path.c_str(), fragment_path.c_str());
				program = fallback_shader;
			}
		};
		GLuint fill_gbuffer_shader = 0u, fill_shadowmap_shader = 0u, accumulate_lights_shader = 0u, resolve_deferred_shader = 0u;
		GLuint accumulate_lights_mirror_shader = 0u;
		auto const reload_shaders = [&reload_shader,&fill_gbuffer_shader,&fill_shadowmap_shader,&accumulate_lights_shader, &accumulate_lights_mirror_shader, &resolve_deferred_shader](){
			LogInfo("Reloading shaders");
			reload_shader("fill_gbuffer.vert",      "fill_gbuffer.frag",      fill_gbuffer_shader);
			reload_shader("fill_shadowmap.vert",    "fill_shadowmap.frag",    fill_shadowmap_shader);
			reload_shader("accumulate_lights.vert", "accumulate_lights.frag", accumulate_lights_shader);
			reload_shader("accumulate_lights_mirror.vert", "accumulate_lights_mirror.frag", accumulate_lights_mirror_shader);
			reload_shader("resolve_deferred.vert",  "resolve_deferred.frag",  resolve_deferred_shader);
		};
		reload_shaders();
		
		auto const set_uniforms = [](GLuint /*program*/){};
		
		
		//
		// Setup textures
		//
		auto const diffuse_texture                     = bonobo::createTexture(window_size.x, window_size.y);
		auto const specular_texture                    = bonobo::createTexture(window_size.x, window_size.y);
		auto const normal_texture                      = bonobo::createTexture(window_size.x, window_size.y);
		auto const light_diffuse_contribution_texture  = bonobo::createTexture(window_size.x, window_size.y);
		auto const light_specular_contribution_texture = bonobo::createTexture(window_size.x, window_size.y);
		auto const depth_texture                       = bonobo::createTexture(window_size.x, window_size.y, GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
		auto const shadowmap_texture                   = bonobo::createTexture(constant::shadowmap_res_x, constant::shadowmap_res_y, GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
		
		//
		// Setup FBOs
		//
		auto const deferred_fbo  = bonobo::createFBO({diffuse_texture, specular_texture, normal_texture}, depth_texture);
		auto const shadowmap_fbo = bonobo::createFBO({}, shadowmap_texture);
		auto const light_fbo     = bonobo::createFBO({light_diffuse_contribution_texture, light_specular_contribution_texture}, depth_texture);
		
		//
		// Setup samplers
		//
		auto const default_sampler = bonobo::createSampler([](GLuint sampler){
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		});
		auto const depth_sampler = bonobo::createSampler([](GLuint sampler){
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		});
		auto const shadow_sampler = bonobo::createSampler([](GLuint sampler){
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
			GLfloat border_color[4] = { 1.0f, 0.0f, 0.0f, 0.0f};
			glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, border_color);
		});
		auto const bind_texture_with_sampler = [](GLenum target, unsigned int slot, GLuint program, std::string const& name, GLuint texture, GLuint sampler){
			glActiveTexture(GL_TEXTURE0 + slot);
			glBindTexture(target, texture);
			glUniform1i(glGetUniformLocation(program, name.c_str()), static_cast<GLint>(slot));
			glBindSampler(slot, sampler);
		};
		
		// Load the cube geometry
		auto const quad = Quad::createQuad(mirror_width, mirror_height, 10);
		std::array<glm::vec3, constant::mirrors_nb> P0;
		std::array<glm::vec3, constant::mirrors_nb> P1;
		std::array<glm::vec3, constant::mirrors_nb> P2;
		std::array<glm::vec3, constant::mirrors_nb> P3;
		
		// Quad 0
		P0[0] = glm::vec3(120.0,40.0,0.0);
		float angle1X = PI/2;
		float angle1Y = 3*PI/5;
		
		auto quad0 = Node();
		quad0.set_geometry(quad);
		quad0.set_rotation_x(angle1X);
		quad0.set_rotation_y(angle1Y);
		quad0.set_translation(P0[0]);
		quad0.set_program(mirrorShader, [](GLuint /*program*/){});
		// Matze job : add a golden texture
		//auto quadTexture = bonobo::loadTexture2D("checkers.png");
		//quadTest.add_texture("quad_texture", quadTexture, GL_TEXTURE_2D);
		P1[0] = glm::vec3(mirror_width,0.0,0.0);
		P2[0] = glm::vec3(mirror_width,0.0,mirror_height);
		P3[0] = glm::vec3(0.0,0.0,mirror_height);
		auto const rotation1X = glm::rotate(glm::mat4(), angle1X, glm::vec3(1.0, 0.0, 0.0));
		auto const rotation1Y = glm::rotate(glm::mat4(), angle1Y, glm::vec3(0.0, 1.0, 0.0));
		P1[0] = P0[0] + glm::vec3(rotation1Y*rotation1X*glm::vec4(P1[0], 1.0));
		P2[0] = P0[0] + glm::vec3(rotation1Y*rotation1X*glm::vec4(P2[0], 1.0));
		P3[0] = P0[0] + glm::vec3(rotation1Y*rotation1X*glm::vec4(P3[0], 1.0));
		
		
		// Quad 1
		P0[1] = glm::vec3(120.0,40.0,0.0);
		float angle2X = PI/2;
		float angle2Y = 0.0;
		
		auto quad1 = Node();
		quad1.set_geometry(quad);
		quad1.set_rotation_x(angle2X);
		quad1.set_rotation_y(angle2Y);
		quad1.set_translation(P0[1]);
		quad1.set_program(mirrorShader, [](GLuint /*program*/){});
		// Matze job : add a golden texture
		//auto quadTexture = bonobo::loadTexture2D("checkers.png");
		//quadTest.add_texture("quad_texture", quadTexture, GL_TEXTURE_2D);
		P1[1] = glm::vec3(mirror_width,0.0,0.0);
		P2[1] = glm::vec3(mirror_width,0.0,mirror_height);
		P3[1] = glm::vec3(0.0,0.0,mirror_height);
		auto const rotation2X = glm::rotate(glm::mat4(), angle2X, glm::vec3(1.0, 0.0, 0.0));
		auto const rotation2Y = glm::rotate(glm::mat4(), angle2Y, glm::vec3(0.0, 1.0, 0.0));
		P1[1] = P0[1] + glm::vec3(rotation2Y*rotation2X*glm::vec4(P1[1], 1.0));
		P2[1] = P0[1] + glm::vec3(rotation2Y*rotation2X*glm::vec4(P2[1], 1.0));
		P3[1] = P0[1] + glm::vec3(rotation2Y*rotation2X*glm::vec4(P3[1], 1.0));
		
		std::vector<Node> quads;
		quads.reserve(2);
		quads.push_back(quad0);
		quads.push_back(quad1);
		
		//
		// Setup lights properties
		//
		std::array<TRSTransform<float, glm::defaultp>, constant::lights_nb*(constant::mirrors_nb+1)> lightTransforms;
		std::array<glm::vec3, constant::lights_nb*(constant::mirrors_nb+1)> lightColors;
		
		glm::vec3 originalLightPoint = glm::vec3(0.0, 75.0, 20.0);
		
		// Symetric lights (mirrors_nb fake lights)
		std::array<glm::vec3, constant::mirrors_nb> symetricPoint;
		std::array<glm::vec3, constant::mirrors_nb> mirror_normal;
		
		
		glm::vec3 rotation1;
		rotation1.x = angle1X;
		rotation1.y = angle1Y;
		auto const rotation1_x = glm::rotate(glm::mat4(), rotation1.x, glm::vec3(1.0, 0.0, 0.0));
		auto const rotation1_y = glm::rotate(glm::mat4(), rotation1.y, glm::vec3(0.0, 1.0, 0.0));
		auto const rotation1_z = glm::rotate(glm::mat4(), rotation1.z, glm::vec3(0.0, 0.0, 1.0));
		auto const rotating1 = rotation1_z * rotation1_y * rotation1_x;
		mirror_normal[0] = rotating1*glm::vec4(0.0, 1.0, 0.0, 1.0);
		symetricPoint[0] = symetry(originalLightPoint, P0[0], mirror_normal[0]);
		
		glm::vec3 rotation2;
		rotation2.x = angle2X;
		rotation2.y = angle2Y;
		auto const rotation2_x = glm::rotate(glm::mat4(), rotation2.x, glm::vec3(1.0, 0.0, 0.0));
		auto const rotation2_y = glm::rotate(glm::mat4(), rotation2.y, glm::vec3(0.0, 1.0, 0.0));
		auto const rotation2_z = glm::rotate(glm::mat4(), rotation2.z, glm::vec3(0.0, 0.0, 1.0));
		auto const rotating2 = rotation2_z * rotation2_y * rotation2_x;
		mirror_normal[1] = rotating2*glm::vec4(0.0, 1.0, 0.0, 1.0);
		symetricPoint[1] = symetry(originalLightPoint, P0[1], mirror_normal[1]);
		
		// Original lights
		for (size_t i = 0; i < constant::lights_nb; ++i) {
			lightTransforms[i].SetTranslate(originalLightPoint);
			lightColors[i] = glm::vec3(0.5f + 0.5f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)),
									   0.5f + 0.5f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)),
									   0.5f + 0.5f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)));
		}
		// Symetric fake lights
		for (size_t i = constant::lights_nb; i < constant::lights_nb*(constant::mirrors_nb+1); ++i) {
			lightTransforms[i].SetTranslate(symetricPoint[i-constant::lights_nb]);
			lightColors[i] = lightColors[0];
		}
		
		TRSTransform<f32, glm::defaultp> coneScaleTransform = TRSTransform<f32, glm::defaultp>();
		coneScaleTransform.SetScale(glm::vec3(sqrt(constant::light_intensity / constant::light_cutoff)));
		
		TRSTransform<f32, glm::defaultp> lightOffsetTransform = TRSTransform<f32, glm::defaultp>();
		lightOffsetTransform.SetTranslate(glm::vec3(0.0f, 0.0f, -40.0f));
		
		auto lightProjection = glm::perspective(bonobo::pi * 0.5f,
												static_cast<float>(constant::shadowmap_res_x) / static_cast<float>(constant::shadowmap_res_y),
												1.0f, 10000.0f);
		
		auto seconds_nb = 0.0f;
		
		
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		
		
		double ddeltatime;
		size_t fpsSamples = 0;
		double nowTime, lastTime = GetTimeMilliseconds();
		double fpsNextTick = lastTime + 1000.0;
		
		while (!glfwWindowShouldClose(window->GetGLFW_Window())) {
			nowTime = GetTimeMilliseconds();
			ddeltatime = nowTime - lastTime;
			if (nowTime > fpsNextTick) {
				fpsNextTick += 1000.0;
				fpsSamples = 0;
			}
			fpsSamples++;
			seconds_nb += static_cast<float>(ddeltatime / 1000.0);
			
			auto& io = ImGui::GetIO();
			inputHandler->SetUICapture(io.WantCaptureMouse, io.WantCaptureMouse);
			
			glfwPollEvents();
			inputHandler->Advance();
			mCamera.Update(ddeltatime, *inputHandler);
			
			ImGui_ImplGlfwGL3_NewFrame();
			
			if (inputHandler->GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
				reload_shaders();
			}
			
			// Load the sphere geometry
			auto sphereLight = Node();
			auto const sphere = Ball::createSphere(15u, 15u, 7.0f);
			sphereLight.set_geometry(sphere);
			sphereLight.set_translation(originalLightPoint);
			sphereLight.set_program(fallback_shader, set_uniforms);
			
			auto symetricSphere0 = Node();
			symetricSphere0.set_geometry(sphere);
			symetricSphere0.set_translation(symetricPoint[0]);
			symetricSphere0.set_program(fallback_shader, set_uniforms);
			
			auto symetricSphere1 = Node();
			symetricSphere1.set_geometry(sphere);
			symetricSphere1.set_translation(symetricPoint[1]);
			symetricSphere1.set_program(fallback_shader, set_uniforms);
			
			//auto sphereP0 = Node();
			//sphereP0.set_geometry(sphere);
			//sphereP0.set_translation(P0[0]);
			//sphereP0.set_program(fallback_shader, set_uniforms);
			
			
			// Default shader
			auto defaultShader = bonobo::createProgram("default.vert", "default.frag");
			if (defaultShader == 0u) {
				LogError("Failed to load shader");
				return;
			}
			
			glDepthFunc(GL_LESS);
			//
			// Pass 1: Render scene into the g-buffer
			//
			glBindFramebuffer(GL_FRAMEBUFFER, deferred_fbo);
			GLenum const deferred_draw_buffers[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
			glDrawBuffers(3, deferred_draw_buffers);
			auto const status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE)
				LogError("Something went wrong with framebuffer %u", deferred_fbo);
			glViewport(0, 0, window_size.x, window_size.y);
			glClear(GL_DEPTH_BUFFER_BIT| GL_COLOR_BUFFER_BIT);
			// XXX: Is any other clearing needed?
			
			
			GLStateInspection::CaptureSnapshot("Filling Pass");
			
			for (auto const& element : sponza_elements)
				element.render(mCamera.GetWorldToClipMatrix(), element.get_transform(), fill_gbuffer_shader, set_uniforms);
			sphereLight.render(mCamera.GetWorldToClipMatrix(), sphereLight.get_transform());
			 /*sphereSymLight.render(mCamera.GetWorldToClipMatrix(), sphereSymLight.get_transform());
			 sphereIntersection.render(mCamera.GetWorldToClipMatrix(), sphereIntersection.get_transform());
			 sphereP0.render(mCamera.GetWorldToClipMatrix(), sphereP0.get_transform());
			 sphereP1.render(mCamera.GetWorldToClipMatrix(), sphereP1.get_transform());
			 sphereP2.render(mCamera.GetWorldToClipMatrix(), sphereP2.get_transform());
			 sphereP3.render(mCamera.GetWorldToClipMatrix(), sphereP3.get_transform());
			 sphereA.render(mCamera.GetWorldToClipMatrix(), sphereA.get_transform());*/
			symetricSphere0.render(mCamera.GetWorldToClipMatrix(), symetricSphere0.get_transform());
			//sphereP0.render(mCamera.GetWorldToClipMatrix(), sphereP0.get_transform());
			symetricSphere1.render(mCamera.GetWorldToClipMatrix(), symetricSphere1.get_transform());
			
			
			glDisable(GL_CULL_FACE);
			//for (auto const& quad : quads)
				//quad.render(mCamera.GetWorldToClipMatrix(), quad.get_transform(), fill_gbuffer_shader, set_uniforms);
			quad0.render(mCamera.GetWorldToClipMatrix(), quad0.get_transform(), fill_gbuffer_shader, set_uniforms);
			quad1.render(mCamera.GetWorldToClipMatrix(), quad1.get_transform(), fill_gbuffer_shader, set_uniforms);
			glEnable(GL_CULL_FACE);
			
			glCullFace(GL_FRONT);
			//
			// Pass 2: Generate shadowmaps and accumulate lights' contribution
			//
			glBindFramebuffer(GL_FRAMEBUFFER, light_fbo);
			GLenum light_draw_buffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
			glDrawBuffers(2, light_draw_buffers);
			glViewport(0, 0, window_size.x, window_size.y);
			// XXX: Is any clearing needed?
			glClear(GL_COLOR_BUFFER_BIT);
			
			float alpha = -PI/2;
			for (size_t i = 0; i < constant::lights_nb; ++i) {
				auto& lightTransform = lightTransforms[i];
				//lightTransform.SetRotate(seconds_nb * 0.1f + i * 1.57f, glm::vec3(0.0f, 1.0f, 0.0f)); //rotation of light
				lightTransform.SetRotate(alpha, glm::vec3(0.0f, 1.0f, 0.0f));
			}
			// Symtric fake lights
			for (size_t i = constant::lights_nb; i < constant::lights_nb*(constant::mirrors_nb+1); ++i) {
				auto& reflectedLightTransform = lightTransforms[i];
				auto const rotationToIncident = glm::rotate(glm::mat4(), alpha, glm::vec3(0.0, 1.0, 0.0));
				auto const rotationToReflected = glm::rotate(glm::mat4(), 3.14f, mirror_normal[i-constant::lights_nb]);
				glm::vec4 incidentRay = rotationToIncident*glm::vec4(0.0, 0.0, -1.0, 1.0); // default light - z-oriented
				glm::vec3 reflectedRay = rotationToReflected*(-incidentRay);
				reflectedLightTransform.LookTowards(reflectedRay);
			}
			
			for (size_t i = 0; i < constant::lights_nb*(constant::mirrors_nb+1); ++i) {
				auto& lightTransform = lightTransforms[i];
				//lightTransform.SetRotate(seconds_nb * 0.1f + i * 1.57f, glm::vec3(0.0f, 1.0f, 0.0f)); //rotation of light
				//lightTransform.SetRotate(-90*3.14 / 180, glm::vec3(0.0f, 1.0f, 0.0f));
				
				auto light_matrix = lightProjection * lightOffsetTransform.GetMatrixInverse() * lightTransform.GetMatrixInverse();
				
				//
				// Pass 2.1: Generate shadow map for light i
				//
				glBindFramebuffer(GL_FRAMEBUFFER, shadowmap_fbo);
				glViewport(0, 0, constant::shadowmap_res_x, constant::shadowmap_res_y);
				// XXX: Is any clearing needed?
				glClear(GL_DEPTH_BUFFER_BIT);
				
				GLStateInspection::CaptureSnapshot("Shadow Map Generation");
				
				for (auto const& element : sponza_elements)
					element.render(light_matrix, glm::mat4(), fill_gbuffer_shader, set_uniforms);
				
				if (i < constant::lights_nb){
					glDisable(GL_CULL_FACE);
					//for (auto const& quad : quads)
						//quad.render(mCamera.GetWorldToClipMatrix(), quad.get_transform(), fill_gbuffer_shader, set_uniforms);
					quad0.render(light_matrix, quad0.get_transform(), fill_gbuffer_shader, set_uniforms);
					quad1.render(light_matrix, quad1.get_transform(), fill_gbuffer_shader, set_uniforms);
					glEnable(GL_CULL_FACE);
				}
				
				
				glEnable(GL_BLEND);
				glDepthFunc(GL_GREATER);
				glDepthMask(GL_FALSE);
				glBlendEquationSeparate(GL_FUNC_ADD, GL_MIN);
				glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
				//
				// Pass 2.2: Accumulate light i contribution
				glBindFramebuffer(GL_FRAMEBUFFER, light_fbo);
				glDrawBuffers(2, light_draw_buffers);
				glViewport(0, 0, window_size.x, window_size.y);
				// XXX: Is any clearing needed?
				
				if (i < constant::lights_nb){
					
					glUseProgram(accumulate_lights_shader);
					
					auto const spotlight_set_uniforms = [&window_size,&mCamera,&light_matrix,&lightColors,&lightTransform,&i](GLuint program){
						glUniform2f(glGetUniformLocation(program, "inv_res"),
									1.0f / static_cast<float>(window_size.x),
									1.0f / static_cast<float>(window_size.y));
						glUniformMatrix4fv(glGetUniformLocation(program, "view_projection_inverse"), 1, GL_FALSE,
										   glm::value_ptr(mCamera.GetClipToWorldMatrix()));
						glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
									 glm::value_ptr(mCamera.mWorld.GetTranslation()));
						glUniformMatrix4fv(glGetUniformLocation(program, "shadow_view_projection"), 1, GL_FALSE,
										   glm::value_ptr(light_matrix));
						glUniform3fv(glGetUniformLocation(program, "light_color"), 1, glm::value_ptr(lightColors[i]));
						glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(lightTransform.GetTranslation()));
						glUniform3fv(glGetUniformLocation(program, "light_direction"), 1, glm::value_ptr(lightTransform.GetFront()));
						glUniform1f(glGetUniformLocation(program, "light_intensity"), constant::light_intensity);
						glUniform1f(glGetUniformLocation(program, "light_angle_falloff"), constant::light_angle_falloff);
						glUniform2f(glGetUniformLocation(program, "shadowmap_texel_size"),
									1.0f / static_cast<float>(constant::shadowmap_res_x),
									1.0f / static_cast<float>(constant::shadowmap_res_y));
					};
					
					bind_texture_with_sampler(GL_TEXTURE_2D, 0, accumulate_lights_shader, "depth_texture", depth_texture, depth_sampler);
					bind_texture_with_sampler(GL_TEXTURE_2D, 1, accumulate_lights_shader, "normal_texture", normal_texture, default_sampler);
					bind_texture_with_sampler(GL_TEXTURE_2D, 2, accumulate_lights_shader, "shadow_texture", shadowmap_texture, shadow_sampler);
					
					GLStateInspection::CaptureSnapshot("Accumulating");
					
					cone.render(mCamera.GetWorldToClipMatrix(),
								lightTransform.GetMatrix() * lightOffsetTransform.GetMatrix() * coneScaleTransform.GetMatrix(),
								accumulate_lights_shader, spotlight_set_uniforms);
				}
				// For all symetric lights
				else {
				//if (i >= constant::lights_nb){
					glUseProgram(accumulate_lights_mirror_shader);
					
					auto const reflected_spotlight_set_uniforms = [&window_size,&mCamera,&light_matrix,&lightColors,&lightTransform,&i,&mirror_normal,&P0,&P1,&P2,&P3](GLuint program){
						glUniform2f(glGetUniformLocation(program, "inv_res"),
									1.0f / static_cast<float>(window_size.x),
									1.0f / static_cast<float>(window_size.y));
						glUniformMatrix4fv(glGetUniformLocation(program, "view_projection_inverse"), 1, GL_FALSE,
										   glm::value_ptr(mCamera.GetClipToWorldMatrix()));
						glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
									 glm::value_ptr(mCamera.mWorld.GetTranslation()));
						glUniformMatrix4fv(glGetUniformLocation(program, "shadow_view_projection"), 1, GL_FALSE,
										   glm::value_ptr(light_matrix));
						glUniform3fv(glGetUniformLocation(program, "light_color"), 1, glm::value_ptr(lightColors[i]));
						glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(lightTransform.GetTranslation()));
						glUniform3fv(glGetUniformLocation(program, "light_direction"), 1, glm::value_ptr(lightTransform.GetFront()));
						glUniform1f(glGetUniformLocation(program, "light_intensity"), constant::light_intensity);
						glUniform1f(glGetUniformLocation(program, "light_angle_falloff"), constant::light_angle_falloff);
						glUniform2f(glGetUniformLocation(program, "shadowmap_texel_size"),
									1.0f / static_cast<float>(constant::shadowmap_res_x),
									1.0f / static_cast<float>(constant::shadowmap_res_y));
						glUniform3fv(glGetUniformLocation(program, "normalVector"), 1, glm::value_ptr(mirror_normal[i-constant::lights_nb]));
						glUniform3fv(glGetUniformLocation(program, "P0"), 1, glm::value_ptr(P0[i-constant::lights_nb]));
						glUniform3fv(glGetUniformLocation(program, "P1"), 1, glm::value_ptr(P1[i-constant::lights_nb]));
						glUniform3fv(glGetUniformLocation(program, "P2"), 1, glm::value_ptr(P2[i-constant::lights_nb]));
						glUniform3fv(glGetUniformLocation(program, "P3"), 1, glm::value_ptr(P3[i-constant::lights_nb]));
					};
					
					bind_texture_with_sampler(GL_TEXTURE_2D, 0, accumulate_lights_mirror_shader, "depth_texture", depth_texture, depth_sampler);
					bind_texture_with_sampler(GL_TEXTURE_2D, 1, accumulate_lights_mirror_shader, "normal_texture", normal_texture, default_sampler);
					bind_texture_with_sampler(GL_TEXTURE_2D, 2, accumulate_lights_mirror_shader, "shadow_texture", shadowmap_texture, shadow_sampler);
					
					GLStateInspection::CaptureSnapshot("Accumulating");
					
					cone.render(mCamera.GetWorldToClipMatrix(),
								lightTransform.GetMatrix() * lightOffsetTransform.GetMatrix() * coneScaleTransform.GetMatrix(),
								accumulate_lights_mirror_shader, reflected_spotlight_set_uniforms);
				}
				
				glBindSampler(2u, 0u);
				glBindSampler(1u, 0u);
				glBindSampler(0u, 0u);
				
				glDepthMask(GL_TRUE);
				glDepthFunc(GL_LESS);
				glDisable(GL_BLEND);
			}
			
			
			glCullFace(GL_BACK);
			glDepthFunc(GL_ALWAYS);
			//
			// Pass 3: Compute final image using both the g-buffer and  the light accumulation buffer
			//
			glBindFramebuffer(GL_FRAMEBUFFER, 0u);
			glUseProgram(resolve_deferred_shader);
			glViewport(0, 0, window_size.x, window_size.y);
			// XXX: Is any clearing needed?
			
			bind_texture_with_sampler(GL_TEXTURE_2D, 0, resolve_deferred_shader, "diffuse_texture", diffuse_texture, default_sampler);
			bind_texture_with_sampler(GL_TEXTURE_2D, 1, resolve_deferred_shader, "specular_texture", specular_texture, default_sampler);
			bind_texture_with_sampler(GL_TEXTURE_2D, 2, resolve_deferred_shader, "light_d_texture", light_diffuse_contribution_texture, default_sampler);
			bind_texture_with_sampler(GL_TEXTURE_2D, 3, resolve_deferred_shader, "light_s_texture", light_specular_contribution_texture, default_sampler);
			
			GLStateInspection::CaptureSnapshot("Resolve Pass");
			
			bonobo::drawFullscreen();
			
			glBindSampler(3, 0u);
			glBindSampler(2, 0u);
			glBindSampler(1, 0u);
			glBindSampler(0, 0u);
			glUseProgram(0u);
			
			
			//
			// Pass 4: Draw wireframe cones on top of the final image for debugging purposes
			//
			//		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			//		for (size_t i = 0; i < constant::lights_nb; ++i) {
			//			cone.render(mCamera.GetWorldToClipMatrix(),
			//			            lightTransforms[i].GetMatrix() * lightOffsetTransform.GetMatrix() * coneScaleTransform.GetMatrix(),
			//			            fill_shadowmap_shader, set_uniforms);
			//		}
			//		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			
			
			//
			// Output content of the g-buffer as well as of the shadowmap, for debugging purposes
			//
			bonobo::displayTexture({-0.95f, -0.95f}, {-0.55f, -0.55f}, diffuse_texture,                     default_sampler, {0, 1, 2, -1}, window_size);
			bonobo::displayTexture({-0.45f, -0.95f}, {-0.05f, -0.55f}, specular_texture,                    default_sampler, {0, 1, 2, -1}, window_size);
			bonobo::displayTexture({ 0.05f, -0.95f}, { 0.45f, -0.55f}, normal_texture,                      default_sampler, {0, 1, 2, -1}, window_size);
			bonobo::displayTexture({ 0.55f, -0.95f}, { 0.95f, -0.55f}, depth_texture,                       default_sampler, {0, 0, 0, -1}, window_size, &mCamera);
			bonobo::displayTexture({-0.95f,  0.55f}, {-0.55f,  0.95f}, shadowmap_texture,                   default_sampler, {0, 0, 0, -1}, window_size, &mCamera);
			bonobo::displayTexture({-0.45f,  0.55f}, {-0.05f,  0.95f}, light_diffuse_contribution_texture,  default_sampler, {0, 1, 2, -1}, window_size);
			bonobo::displayTexture({ 0.05f,  0.55f}, { 0.45f,  0.95f}, light_specular_contribution_texture, default_sampler, {0, 1, 2, -1}, window_size);
			//
			// Reset viewport back to normal
			//
			glViewport(0, 0, window_size.x, window_size.y);
			
			GLStateInspection::View::Render();
			Log::View::Render();
			
			bool opened = ImGui::Begin("Render Time", nullptr, ImVec2(120, 50), -1.0f, 0);
			if (opened)
				ImGui::Text("%.3f ms", ddeltatime);
			ImGui::End();
			
			ImGui::Render();
			
			window->Swap();
			lastTime = nowTime;
		}
		
		glDeleteProgram(mirrorShader);
		mirrorShader = 0u;
		glDeleteProgram(resolve_deferred_shader);
		resolve_deferred_shader = 0u;
		glDeleteProgram(accumulate_lights_shader);
		accumulate_lights_shader = 0u;
		glDeleteProgram(accumulate_lights_mirror_shader);
		accumulate_lights_mirror_shader = 0u;
		glDeleteProgram(fill_shadowmap_shader);
		fill_shadowmap_shader = 0u;
		glDeleteProgram(fill_gbuffer_shader);
		fill_gbuffer_shader = 0u;
		glDeleteProgram(fallback_shader);
		fallback_shader = 0u;
	}
	
	int main()
	{
		Bonobo::Init();
		try {
			edan35::Assignment2 assignment2;
			assignment2.run();
		} catch (std::runtime_error const& e) {
			LogError(e.what());
		}
		Bonobo::Destroy();
	}
	
	static
	bonobo::mesh_data
	loadCone()
	{
		bonobo::mesh_data cone;
		cone.vertices_nb = 65;
		cone.drawing_mode = GL_TRIANGLE_STRIP;
		float vertexArrayData[65 * 3] = {
			0.f, 1.f, -1.f,
			0.f, 0.f, 0.f,
			0.38268f, 0.92388f, -1.f,
			0.f, 0.f, 0.f,
			0.70711f, 0.70711f, -1.f,
			0.f, 0.f, 0.f,
			0.92388f, 0.38268f, -1.f,
			0.f, 0.f, 0.f,
			1.f, 0.f, -1.f,
			0.f, 0.f, 0.f,
			0.92388f, -0.38268f, -1.f,
			0.f, 0.f, 0.f,
			0.70711f, -0.70711f, -1.f,
			0.f, 0.f, 0.f,
			0.38268f, -0.92388f, -1.f,
			0.f, 0.f, 0.f,
			0.f, -1.f, -1.f,
			0.f, 0.f, 0.f,
			-0.38268f, -0.92388f, -1.f,
			0.f, 0.f, 0.f,
			-0.70711f, -0.70711f, -1.f,
			0.f, 0.f, 0.f,
			-0.92388f, -0.38268f, -1.f,
			0.f, 0.f, 0.f,
			-1.f, 0.f, -1.f,
			0.f, 0.f, 0.f,
			-0.92388f, 0.38268f, -1.f,
			0.f, 0.f, 0.f,
			-0.70711f, 0.70711f, -1.f,
			0.f, 0.f, 0.f,
			-0.38268f, 0.92388f, -1.f,
			0.f, 1.f, -1.f,
			0.f, 1.f, -1.f,
			0.38268f, 0.92388f, -1.f,
			0.f, 1.f, -1.f,
			0.70711f, 0.70711f, -1.f,
			0.f, 0.f, -1.f,
			0.92388f, 0.38268f, -1.f,
			0.f, 0.f, -1.f,
			1.f, 0.f, -1.f,
			0.f, 0.f, -1.f,
			0.92388f, -0.38268f, -1.f,
			0.f, 0.f, -1.f,
			0.70711f, -0.70711f, -1.f,
			0.f, 0.f, -1.f,
			0.38268f, -0.92388f, -1.f,
			0.f, 0.f, -1.f,
			0.f, -1.f, -1.f,
			0.f, 0.f, -1.f,
			-0.38268f, -0.92388f, -1.f,
			0.f, 0.f, -1.f,
			-0.70711f, -0.70711f, -1.f,
			0.f, 0.f, -1.f,
			-0.92388f, -0.38268f, -1.f,
			0.f, 0.f, -1.f,
			-1.f, 0.f, -1.f,
			0.f, 0.f, -1.f,
			-0.92388f, 0.38268f, -1.f,
			0.f, 0.f, -1.f,
			-0.70711f, 0.70711f, -1.f,
			0.f, 0.f, -1.f,
			-0.38268f, 0.92388f, -1.f,
			0.f, 0.f, -1.f,
			0.f, 1.f, -1.f,
			0.f, 0.f, -1.f
		};
		
		glGenVertexArrays(1, &cone.vao);
		assert(cone.vao != 0u);
		glBindVertexArray(cone.vao);
		{
			glGenBuffers(1, &cone.bo);
			assert(cone.bo != 0u);
			glBindBuffer(GL_ARRAY_BUFFER, cone.bo);
			glBufferData(GL_ARRAY_BUFFER, cone.vertices_nb * 3 * sizeof(float), vertexArrayData, GL_STATIC_DRAW);
			
			glVertexAttribPointer(static_cast<int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));
			glEnableVertexAttribArray(static_cast<int>(bonobo::shader_bindings::vertices));
			
			glBindBuffer(GL_ARRAY_BUFFER, 0u);
		}
		glBindVertexArray(0u);
		
		return cone;
	}
