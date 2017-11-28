
#include "assignment4.hpp"
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

edaf80::Assignment4::Assignment4()
{
    Log::View::Init();

    window = Window::Create("EDAF80: Assignment 4", config::resolution_x,
                            config::resolution_y, config::msaa_rate, false);
    if (window == nullptr) {
        Log::View::Destroy();
        throw std::runtime_error("Failed to get a window: aborting!");
    }
    inputHandler = new InputHandler();
    window->SetInputHandler(inputHandler);
}

edaf80::Assignment4::~Assignment4()
{
    delete inputHandler;
    inputHandler = nullptr;

    Window::Destroy(window);
    window = nullptr;

    Log::View::Destroy();
}

void
edaf80::Assignment4::run()
{
    // load quad geometry
    auto const quad = parametric_shapes::createQuad(50, 50, 200);

    // load sky geometry
    auto const sky = parametric_shapes::createSphere(40u, 40u, 150.0f);

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

    auto light_position = glm::vec3(-2.0f, 0.0f, 2.0f);
    auto camera_position = mCamera.mWorld.GetTranslation();
    auto ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    auto diffuse = glm::vec3(0.7f, 0.2f, 0.4f);
    auto specular = glm::vec3(1.0f, 1.0f, 1.0f);
    auto shininess = 1.0f;
    auto time = 0.0f;
    auto cubeTexture = bonobo::loadTextureCubeMap("/cloudyhills/posx.png", "/cloudyhills/negx.png", "/cloudyhills/posy.png", "/cloudyhills/negy.png", "/cloudyhills/posz.png", "/cloudyhills/negz.png" );
    auto bumpTexture = bonobo::loadTexture2D("waves.png");
    
    // setting defaults for wave parameters
    // Amplitude, Frequency, Phase, Sharpness, DirectionX, DirectionZ
    float wave1Params[6] = {1.0, 0.2, 0.5, 2.0, -1.0, 0.0};
    float wave2Params[6] = {0.5, 0.4, 1.3, 2.0, -0.7, 0.7};
        
    //
    // Setting uniform values for shader
    //
    auto const set_uniforms = [&light_position, &camera_position, &ambient, &diffuse, &specular, &shininess, &time, &wave1Params, &wave2Params](GLuint program)
    {
        glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(light_position));
        glUniform3fv(glGetUniformLocation(program, "camera_position"), 1, glm::value_ptr(camera_position));
        glUniform3fv(glGetUniformLocation(program, "ambient"), 1, glm::value_ptr(ambient));
        glUniform3fv(glGetUniformLocation(program, "diffuse"), 1, glm::value_ptr(diffuse));
        glUniform3fv(glGetUniformLocation(program, "specular"), 1, glm::value_ptr(specular));
        glUniform1f(glGetUniformLocation(program, "shininess"), shininess);
        glUniform1f(glGetUniformLocation(program, "time"), time);
        glUniform1fv(glGetUniformLocation(program, "wave1Params"), 6, wave1Params);
        glUniform1fv(glGetUniformLocation(program, "wave2Params"), 6, wave2Params);
    };

    auto polygon_mode = polygon_mode_t::fill;

    //
    // Reload shaders
    //

    GLuint diffuse_shader = 0u, normal_shader = 0u, texcoord_shader = 0u, water_shader = 0u, skybox_shader = 0u;

    auto const reload_shaders = [&diffuse_shader, &normal_shader, &texcoord_shader, &water_shader, &skybox_shader]()
    {
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

        if (water_shader != 0u)
            glDeleteProgram(water_shader);
        water_shader = bonobo::createProgram("water.vert", "water.frag");
        if (water_shader == 0u)
            LogError("Failed to load water shader");

        if (skybox_shader != 0u)
            glDeleteProgram(skybox_shader);
        skybox_shader = bonobo::createProgram("skybox.vert", "skybox.frag");
        if (skybox_shader == 0u)
            LogError("Failed to load skybox shader");
    };

    reload_shaders();

    //
    // Create geometry
    //
    auto waves = Node();
    waves.set_geometry(quad);
    waves.set_program(water_shader, set_uniforms);
    waves.add_texture("SkyboxTexture", cubeTexture, GL_TEXTURE_CUBE_MAP);
    waves.add_texture("bump_texture", bumpTexture, GL_TEXTURE_2D);

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

        time += ddeltatime / 1000.0f;

        auto& io = ImGui::GetIO();
        inputHandler->SetUICapture(io.WantCaptureMouse, io.WantCaptureMouse);

        glfwPollEvents();
        inputHandler->Advance();
        mCamera.Update(ddeltatime, *inputHandler);

        ImGui_ImplGlfwGL3_NewFrame();

        //
        // Inputs
        //
        if (inputHandler->GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
            reload_shaders();
        }
        if(inputHandler->GetKeycodeState(GLFW_KEY_1) & JUST_PRESSED)
        {
            waves.set_program(fallback_shader, set_uniforms);
        }
        if(inputHandler->GetKeycodeState(GLFW_KEY_2) & JUST_PRESSED)
        {
            waves.set_program(diffuse_shader, set_uniforms);
        }
        if(inputHandler->GetKeycodeState(GLFW_KEY_3) & JUST_PRESSED)
        {
            waves.set_program(normal_shader, set_uniforms);
        }
        if(inputHandler->GetKeycodeState(GLFW_KEY_4) & JUST_PRESSED)
        {
            waves.set_program(texcoord_shader, set_uniforms);
        }
        if(inputHandler->GetKeycodeState(GLFW_KEY_5) & JUST_PRESSED)
        {
            waves.set_program(water_shader, set_uniforms);
        }
        if(inputHandler->GetKeycodeState(GLFW_KEY_Z) & JUST_PRESSED)
        {
            polygon_mode = get_next_mode(polygon_mode);
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

        //
        // Render geometry
        //
        waves.render(mCamera.GetWorldToClipMatrix(), waves.get_transform());
        skyBox.render(mCamera.GetWorldToClipMatrix(), skyBox.get_transform());

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        Log::View::Render();

        //
        // Todo: If you want a custom ImGUI window, you can set it up
        //       here
        //
        bool opened = ImGui::Begin("Wave 1 Control", &opened, ImVec2(300, 100), -1.0f, 0);
        if (opened) {
            ImGui::SliderFloat("Amplitude", &wave1Params[0], 0.0f, 5.0f);
            ImGui::SliderFloat("Frequency", &wave1Params[1], 0.0f, 5.0f);
            ImGui::SliderFloat("Phase", &wave1Params[2], 0.0f, 5.0f);
            ImGui::SliderFloat("Sharpness", &wave1Params[3], 0.0f, 5.0f);
            ImGui::SliderFloat("Direction X", &wave1Params[4], -5.0f, 5.0f);
            ImGui::SliderFloat("Direction Z", &wave1Params[5], -5.0f, 5.0f);
        }
        ImGui::End();
        
        bool opened1 = ImGui::Begin("Wave 2 Control", &opened, ImVec2(300, 100), -1.0f, 0);
        if (opened1) {
            ImGui::SliderFloat("Amplitude", &wave2Params[0], 0.0f, 5.0f);
            ImGui::SliderFloat("Frequency", &wave2Params[1], 0.0f, 5.0f);
            ImGui::SliderFloat("Phase", &wave2Params[2], 0.0f, 5.0f);
            ImGui::SliderFloat("Sharpness", &wave2Params[3], 0.0f, 5.0f);
            ImGui::SliderFloat("Direction X", &wave2Params[4], -5.0f, 5.0f);
            ImGui::SliderFloat("Direction Z", &wave2Params[5], -5.0f, 5.0f);
        }
        ImGui::End();

        ImGui::Render();

        window->Swap();
        lastTime = nowTime;
    }

    //
    // Todo: Do not forget to delete your shader programs, by calling
    //       `glDeleteProgram($your_shader_program)` for each of them.
    //
    glDeleteProgram(fallback_shader);
    fallback_shader = 0u;

    glDeleteProgram(diffuse_shader);
    diffuse_shader = 0u;

    glDeleteProgram(normal_shader);
    normal_shader = 0u;

    glDeleteProgram(texcoord_shader);
    texcoord_shader = 0u;
}

int main()
{
    Bonobo::Init();
    try {
        edaf80::Assignment4 assignment4;
        assignment4.run();
    } catch (std::runtime_error const& e) {
        LogError(e.what());
    }
    Bonobo::Destroy();
}

