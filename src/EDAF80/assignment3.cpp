#include "assignment3.hpp"
#include "interpolation.hpp"
#include "parametric_shapes.hpp"

#include "config.hpp"
#include "external/glad/glad.h"
#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/helpers.hpp"
#include "core/InputHandler.h"
#include "core/Log.h"
#include "core/LogView.h"
#include "core/Misc.h"
#include "core/utils.h"
#include "core/Window.h"
#include <imgui.h>
#include "external/imgui_impl_glfw_gl3.h"
#include "core/node.cpp"
#include "core/node.hpp"

#include "external/glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdlib>
#include <stdexcept>

enum class polygon_mode_t : unsigned int {
	fill = 0u,
	line,
	point
};

static polygon_mode_t get_next_mode(polygon_mode_t mode)
{
	return static_cast<polygon_mode_t>((static_cast<unsigned int>(mode) + 1u) % 3u);
}

edaf80::Assignment3::Assignment3()
{
	Log::View::Init();

	window = Window::Create("EDAF80: Assignment 3", config::resolution_x,
	                        config::resolution_y, config::msaa_rate, false);
	if (window == nullptr) {
		Log::View::Destroy();
		throw std::runtime_error("Failed to get a window: aborting!");
	}
	inputHandler = new InputHandler();
	window->SetInputHandler(inputHandler);
}

edaf80::Assignment3::~Assignment3()
{
	delete inputHandler;
	inputHandler = nullptr;

	Window::Destroy(window);
	window = nullptr;

	Log::View::Destroy();
}

void
edaf80::Assignment3::run()
{
    // load sphere geometry
    auto const sphere = parametric_shapes::createSphere(40u, 40u, 1.0f);
    if (sphere.vao == 0u) {
        LogError("Failed to retrieve the sphere mesh");
        return;
    }
    
    // load sky geometry
    auto const sky = parametric_shapes::createSphere(40u, 40u, 100.0f);

	// Set up the camera
	FPSCameraf mCamera(bonobo::pi / 4.0f,
	                   static_cast<float>(config::resolution_x) / static_cast<float>(config::resolution_y),
	                   0.01f, 1000.0f);
	mCamera.mWorld.SetTranslate(glm::vec3(0.0f, 0.0f, 6.0f));
	mCamera.mMouseSensitivity = 0.003f;
	mCamera.mMovementSpeed = 0.025;
	window->SetCamera(&mCamera);

	// Create the shader programs
    auto fallback_shader = bonobo::createProgram("fallback.vert", "fallback.frag");
    
    if (fallback_shader == 0u) {
        LogError("Failed to load fallback shader");
        return;
    }
    
    auto phong_shader = bonobo::createProgram("phong.vert", "phong.frag");
    
    if (phong_shader == 0u) {
        LogError("Failed to load phong shader");
        return;
    }
    
    auto skybox_shader = bonobo::createProgram("skybox.vert", "skybox.frag");
    
    if (skybox_shader == 0u) {
        LogError("Failed to load cube shader");
        return;
    }
    
    auto reflection_shader = bonobo::createProgram("reflection.vert", "reflection.frag");
    
    if (reflection_shader == 0u) {
        LogError("Failed to load reflection shader");
        return;
    }
    
    auto bumpmap_shader = bonobo::createProgram("bumpmap.vert", "bumpmap.frag");
    
    if (bumpmap_shader == 0u) {
        LogError("Failed to load bumpmap shader");
        return;
    }
    
    GLuint diffuse_shader = 0u, normal_shader = 0u, texcoord_shader = 0u;
	auto const reload_shaders = [&diffuse_shader,&normal_shader,&texcoord_shader](){
		if (diffuse_shader != 0u)
			glDeleteProgram(diffuse_shader);
		diffuse_shader = bonobo::createProgram("diffuse.vert", "diffuse.frag");
		if (diffuse_shader == 0u)
			LogError("Failed to load diffuse shader");

		if (normal_shader != 0u)
			glDeleteProgram(normal_shader);
		normal_shader = bonobo::createProgram("normal.vert", "normal.frag");
		if (normal_shader == 0u)
			LogError("Failed to load normal shader");

		if (texcoord_shader != 0u)
			glDeleteProgram(texcoord_shader);
		texcoord_shader = bonobo::createProgram("texcoord.vert", "texcoord.frag");
		if (texcoord_shader == 0u)
			LogError("Failed to load texcoord shader");
	};
	reload_shaders();

	auto light_position = glm::vec3(-2.0f, 4.0f, 2.0f);

	auto camera_position = mCamera.mWorld.GetTranslation();
	auto ambient = glm::vec3(0.1f, 0.1f, 0.1f);
	auto diffuse = glm::vec3(0.7f, 0.2f, 0.4f);
	auto specular = glm::vec3(1.0f, 1.0f, 1.0f);
	auto shininess = 1.0f;
    auto cubeTexture = bonobo::loadTextureCubeMap("/cloudyhills/posx.png", "/cloudyhills/negx.png", "/cloudyhills/posy.png", "/cloudyhills/negy.png", "/cloudyhills/posz.png", "/cloudyhills/negz.png" );
    auto bumpDiffuse = bonobo::loadTexture2D("fieldstone_diffuse.png");
    auto bumpTexture = bonobo::loadTexture2D("fieldstone_bump.png");
    
	auto const set_uniforms = [&light_position,&camera_position,&ambient,&diffuse,&specular,&shininess](GLuint program){
		glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(light_position));
		glUniform3fv(glGetUniformLocation(program, "camera_position"), 1, glm::value_ptr(camera_position));
		glUniform3fv(glGetUniformLocation(program, "ambient"), 1, glm::value_ptr(ambient));
		glUniform3fv(glGetUniformLocation(program, "diffuse"), 1, glm::value_ptr(diffuse));
		glUniform3fv(glGetUniformLocation(program, "specular"), 1, glm::value_ptr(specular));
		glUniform1f(glGetUniformLocation(program, "shininess"), shininess);
	};

	auto polygon_mode = polygon_mode_t::fill;

	auto testShape = Node();
	testShape.set_geometry(sphere);
    testShape.set_program(reflection_shader, set_uniforms);
    
    auto skyBox = Node();
    skyBox.set_geometry(sky);
    skyBox.set_program(skybox_shader, set_uniforms);
    skyBox.add_texture("diffuse_texture", cubeTexture, GL_TEXTURE_CUBE_MAP);

	glEnable(GL_DEPTH_TEST);

	// Enable face culling to improve performance:
//    glEnable(GL_CULL_FACE);
//    glCullFace(GL_FRONT);
//    glCullFace(GL_BACK);

	f64 ddeltatime;
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

		auto& io = ImGui::GetIO();
		inputHandler->SetUICapture(io.WantCaptureMouse, io.WantCaptureMouse);

		glfwPollEvents();
		inputHandler->Advance();
		mCamera.Update(ddeltatime, *inputHandler);

		ImGui_ImplGlfwGL3_NewFrame();

		if (inputHandler->GetKeycodeState(GLFW_KEY_1) & JUST_PRESSED) {
			testShape.set_program(fallback_shader, set_uniforms);
		}
		if (inputHandler->GetKeycodeState(GLFW_KEY_2) & JUST_PRESSED) {
			testShape.set_program(diffuse_shader, set_uniforms);
		}
		if (inputHandler->GetKeycodeState(GLFW_KEY_3) & JUST_PRESSED) {
			testShape.set_program(normal_shader, set_uniforms);
		}
		if (inputHandler->GetKeycodeState(GLFW_KEY_4) & JUST_PRESSED) {
			testShape.set_program(texcoord_shader, set_uniforms);
		}
        if (inputHandler->GetKeycodeState(GLFW_KEY_5) & JUST_PRESSED) {
            testShape.set_program(phong_shader, set_uniforms);
        }
        if (inputHandler->GetKeycodeState(GLFW_KEY_6) & JUST_PRESSED) {
            testShape.set_program(reflection_shader, set_uniforms);
        }
        if (inputHandler->GetKeycodeState(GLFW_KEY_7) & JUST_PRESSED) {
            testShape.set_program(bumpmap_shader, set_uniforms);
            testShape.add_texture("diffuse_texture", bumpDiffuse, GL_TEXTURE_2D);
            testShape.add_texture("bump_texture", bumpTexture, GL_TEXTURE_2D);
        }
		if (inputHandler->GetKeycodeState(GLFW_KEY_Z) & JUST_PRESSED) {
			polygon_mode = get_next_mode(polygon_mode);
		}
		if (inputHandler->GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
			reload_shaders();
		}
		switch (polygon_mode) {
			case polygon_mode_t::fill:
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				break;
			case polygon_mode_t::line:
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				break;
			case polygon_mode_t::point:
				glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
				break;
		}

		camera_position = mCamera.mWorld.GetTranslation();

		auto const window_size = window->GetDimensions();
		glViewport(0, 0, window_size.x, window_size.y);
		glClearDepthf(1.0f);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

        testShape.render(mCamera.GetWorldToClipMatrix(), testShape.get_transform());
        skyBox.render(mCamera.GetWorldToClipMatrix(), testShape.get_transform());

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		Log::View::Render();

		bool opened = ImGui::Begin("Scene Control", &opened, ImVec2(300, 100), -1.0f, 0);
		if (opened) {
			ImGui::ColorEdit3("Ambient", glm::value_ptr(ambient));
			ImGui::ColorEdit3("Diffuse", glm::value_ptr(diffuse));
			ImGui::ColorEdit3("Specular", glm::value_ptr(specular));
			ImGui::SliderFloat("Shininess", &shininess, 0.0f, 1000.0f);
			ImGui::SliderFloat3("Light Position", glm::value_ptr(light_position), -20.0f, 20.0f);
		}
		ImGui::End();

		ImGui::Begin("Render Time", &opened, ImVec2(120, 50), -1.0f, 0);
		if (opened)
			ImGui::Text("%.3f ms", ddeltatime);
		ImGui::End();

		ImGui::Render();

		window->Swap();
		lastTime = nowTime;
	}

	glDeleteProgram(texcoord_shader);
	texcoord_shader = 0u;
	glDeleteProgram(normal_shader);
	normal_shader = 0u;
	glDeleteProgram(diffuse_shader);
	diffuse_shader = 0u;
	glDeleteProgram(fallback_shader);
	diffuse_shader = 0u;
    glDeleteProgram(phong_shader);
    phong_shader = 0u;
}

int main()
{
	Bonobo::Init();
	try {
		edaf80::Assignment3 assignment3;
		assignment3.run();
	} catch (std::runtime_error const& e) {
		LogError(e.what());
	}
	Bonobo::Destroy();
}
