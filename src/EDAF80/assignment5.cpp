
#include "Assignment5.hpp"
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

#include <stack>
#include <stdexcept>
#include <cstdlib>
#include <iostream>

enum class polygon_mode_t : unsigned int {
    fill = 0u,
    line,
    point
    };
    
    static polygon_mode_t get_next_mode(polygon_mode_t mode)
    {
        return static_cast<polygon_mode_t>((static_cast<unsigned int>(mode) + 1u) % 3u);
    }
    
    edaf80::Assignment5::Assignment5()
    {
        Log::View::Init();
        
        window = Window::Create("EDAF80: Assignment 5", config::resolution_x,
                                config::resolution_y, config::msaa_rate, false);
        if (window == nullptr) {
            Log::View::Destroy();
            throw std::runtime_error("Failed to get a window: aborting!");
        }
        inputHandler = new InputHandler();
        window->SetInputHandler(inputHandler);
    }
    
    edaf80::Assignment5::~Assignment5()
    {
        delete inputHandler;
        inputHandler = nullptr;
        
        Window::Destroy(window);
        window = nullptr;
        
        Log::View::Destroy();
    }
    
    void
    edaf80::Assignment5::run()
    {
        // load ship
        bonobo::mesh_data const ship = bonobo::loadObjects("spaceship.obj")[0];
        
        // load sphere geometry
        auto const sphere = parametric_shapes::createSphere(10u, 10u, 0.4f);
        
        
        // load quad geometry
        auto const quad = parametric_shapes::createQuad(500, 500, 10);
        
        // Set up the camera
        
        FPSCameraf mCamera(bonobo::pi / 4.0f,
                           static_cast<float>(config::resolution_x) / static_cast<float>(config::resolution_y),
                           0.01f, 1000.0f);
        mCamera.mMouseSensitivity = 0.00f;
        mCamera.mMovementSpeed = 0.0;
        
        window->SetCamera(&mCamera);
        
        // Create the shader programs
        auto fallback_shader = bonobo::createProgram("fallback.vert", "fallback.frag");
        if (fallback_shader == 0u) {
            LogError("Failed to load fallback shader");
            return;
        }
        
        auto shader = bonobo::createProgram("default.vert", "default.frag");
        if (shader == 0u) {
            LogError("Failed to load shader");
            return;
        }
        
        auto light_position = glm::vec3(0.0f, 20.0f, 0.0f);
        auto camera_position = mCamera.mWorld.GetTranslation();
        auto ambient = glm::vec3(0.1f, 0.1f, 0.1f);
        auto diffuse = glm::vec3(0.7f, 0.2f, 0.4f);
        auto specular = glm::vec3(1.0f, 1.0f, 1.0f);
        auto shininess = 1.0f;
        auto time = 0.0f;
        auto spaceTexture = bonobo::loadTexture2D("space.png");
        auto sun_texture = bonobo::loadTexture2D("sunmap.png");
        
        // setting defaults for wave parameters
        // Amplitude, Frequency, Phase, Sharpness, DirectionX, DirectionZ
        float wave1Params[6] = {3.0, 0.05, 0.5, 1.3, -1.0, 0.0};
        float wave2Params[6] = {2.1, 0.15, 1.3, 1.3, -0.7, 0.7};
        
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
        
        GLuint diffuse_shader = 0u, normal_shader = 0u, texcoord_shader = 0u, water_shader = 0u, skybox_shader = 0u, reflection_shader = 0u;
        
        auto const reload_shaders = [&diffuse_shader, &normal_shader, &texcoord_shader, &water_shader, &skybox_shader, &reflection_shader]()
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
            
            if (reflection_shader != 0u)
                glDeleteProgram(reflection_shader);
            reflection_shader = bonobo::createProgram("reflection.vert", "reflection.frag");
            if (reflection_shader == 0u)
                LogError("Failed to load reflection shader");
        };
        
        reload_shaders();
        
        //
        // Create geometry, set up nodes
        //
        Node projectiles[20];
        Node comets[10];
        float cometradius[10];
        float speedcomets[10];
        
        auto player = Node();
        player.set_geometry(ship);
        player.set_program(normal_shader, set_uniforms);
        player.set_rotation_y(bonobo::pi);
        float playerradius = 2;
        float projectileradius = 0.4;
        
        auto space = Node();
        space.set_geometry(quad);
        space.set_program(shader, [](GLuint /*program*/){});
        space.add_texture("diffuse_texture", spaceTexture, GL_TEXTURE_2D);
        
        auto world = Node();
        world.add_child(&player);
        world.add_child(&space);
        
        for(int i=0; i<20; i++)
        {
            projectiles[i].set_geometry(sphere);
            projectiles[i].set_program(fallback_shader, set_uniforms);
            world.add_child(&projectiles[i]);
        }
        
        for (int i = 0; i<10; i++)
        {
            float r = 10 + (rand() % 30);
            r = r / 10;
            auto const cometsphere = parametric_shapes::createSphere(30u, 30u,r);
            comets[i].set_geometry(cometsphere);
            comets[i].set_program(shader, [](GLuint /*program*/) {});
            comets[i].add_texture("diffuse_texture", sun_texture, GL_TEXTURE_2D);
            world.add_child(&comets[i]);
            cometradius[i] = r;
            speedcomets[i] = (-rand() % 50) ;
            speedcomets[i] = speedcomets[i] / 100;
        }
        int counter = 0;
        int lives = 17;
        
        glEnable(GL_DEPTH_TEST);
        
        // Enable face culling to improve performance:
        //            glEnable(GL_CULL_FACE);
        //            glCullFace(GL_FRONT);
        //            glCullFace(GL_BACK);
        
        f64 ddeltatime;
        size_t fpsSamples = 0;
        double nowTime, lastTime = GetTimeMilliseconds();
        double fpsNextTick = lastTime + 1000.0;
        
        float x=-200, y=-10, z=-200;
        float rx=-1.6, ry=0, rz=0;
        
        // set camera location and backdrop
        mCamera.mWorld.SetRotateY(ry);
        mCamera.mWorld.SetRotateZ(rz);
        mCamera.mWorld.SetRotateX(rx);
        
        space.set_translation(glm::vec3(x, y, z));
        
        // velocity and acceleration
        float acceleration = 0.05;
        float maxSpeed = 0.4;
        glm::vec3 velocity = glm::vec3(0, 0, 0);
        
        
        // aiming directions
        bool up=false, down=false, left=false, right=false;
        glm::vec2 dir;
        
        while (!glfwWindowShouldClose(window->GetGLFW_Window())) {
            mCamera.mWorld.SetTranslate(glm::vec3(player.get_transform()[3]) + glm::vec3(-7, 2, 0));
            mCamera.mWorld.LookAt(player.get_transform()[3], glm::vec3(0, 1, 0));
            
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
            // Inputs and Movement
            //
            
            // keeping below max speed
            if(velocity.x>maxSpeed)
                velocity.x=maxSpeed;
            if(velocity.x<-maxSpeed)
                velocity.x=-maxSpeed;
            if(velocity.z>maxSpeed)
                velocity.z=maxSpeed;
            if(velocity.z<-maxSpeed)
                velocity.z=-maxSpeed;
            
            // check movement inputs
            if (inputHandler->GetKeycodeState(GLFW_KEY_S) & PRESSED)
            {
                velocity.x += -acceleration;
            }
            if (inputHandler->GetKeycodeState(GLFW_KEY_W) & PRESSED)
            {
                velocity.x += acceleration;
            }
            if (inputHandler->GetKeycodeState(GLFW_KEY_A) & PRESSED)
            {
                velocity.z += -acceleration;
            }
            if (inputHandler->GetKeycodeState(GLFW_KEY_D) & PRESSED)
            {
                velocity.z += acceleration;
            }
            
            // move
            player.translate(velocity * (static_cast<float>(ddeltatime)/15));
            
            // rotation
            up=false; down=false; left=false; right=false;
            dir.x=0; dir.y=0;
            
            if (inputHandler->GetKeycodeState(GLFW_KEY_UP) & PRESSED)
            {
                right=true;
            }
            if (inputHandler->GetKeycodeState(GLFW_KEY_DOWN) & PRESSED)
            {
                left=true;
            }
            if (inputHandler->GetKeycodeState(GLFW_KEY_LEFT) & PRESSED)
            {
                up=true;
            }
            if (inputHandler->GetKeycodeState(GLFW_KEY_RIGHT) & PRESSED)
            {
                down=true;
            }
            
            // rotate
            if(up)
                dir.y=1;
            if(down)
                dir.y=-1;
            if(left)
                dir.x=-1;
            if(right)
                dir.x=1;
            
            player.set_rotation_y(glm::atan(dir.y, dir.x) + glm::pi<float>()/2);
            
            // projectiles
            if (inputHandler->GetKeycodeState(GLFW_KEY_SPACE) & JUST_PRESSED)
            {
                for(int i=0; i<20; i++)
                {
                    if(!projectiles[i].visible)
                    {
                        projectiles[i].visible=true;
                        projectiles[i].set_translation(glm::vec3(player.get_transform()[3]));
                        projectiles[i].set_rotation_y(glm::atan(dir.y, dir.x));
                        break;
                    }
                }
            }
            
            for(int i=0; i<20; i++)
            {
                if(projectiles[i].visible)
                {
                    projectiles[i].translate(projectiles[i].get_transform()[0] * (static_cast<float>(ddeltatime)/15));
                    
                    // checking if out of bounds
                    if(projectiles[i].get_transform()[3].x < -100 || projectiles[i].get_transform()[3].x > 100 || projectiles[i].get_transform()[3].z > 200 ||projectiles[i].get_transform()[3].z < -200)
                        projectiles[i].visible = false;
                }
            }
            
            //comets
            for (int i = 0; i<10; i++)
            {
                if (!comets[i].visible)
                {
                    comets[i].visible = true;
                    comets[i].set_translation(glm::vec3(99,0, -40.0 + rand() % 80 + 1));
                    break;
                }
            }
            
            for (int i = 0; i<10; i++)
            {
                if (comets[i].visible)
                {
                    comets[i].translate(glm::vec3(speedcomets[i], 0, 0) * (static_cast<float>(ddeltatime)/15));
                    comets[i].set_rotation_z(nowTime);
                    
                    // checking if out of bounds
                    if (comets[i].get_transform()[3].x < -100 || comets[i].get_transform()[3].x > 100 || comets[i].get_transform()[3].z > 200 || comets[i].get_transform()[3].z < -200)
                        comets[i].visible = false;
                    
                    // checking for collision with projectiles
                    for (int j = 1; j < 20; j++)
                    {
                         if (projectiles[j].visible)
                         {
                             if (sqrt((projectiles[j].get_transform()[3].x - comets[i].get_transform()[3].x)*(projectiles[j].get_transform()[3].x - comets[i].get_transform()[3].x)
                                      + (projectiles[j].get_transform()[3].z - comets[i].get_transform()[3].z)*(projectiles[j].get_transform()[3].z - comets[i].get_transform()[3].z)) <= projectileradius + cometradius[i])
                             {
                                 comets[i].visible = false;
                                 projectiles[j].visible = false;
                                 counter++;
                             }
                         }
                    }
                    
                    // checking collision with player
                    if (sqrt((player.get_transform()[3].x - comets[i].get_transform()[3].x)*(player.get_transform()[3].x - comets[i].get_transform()[3].x)
                             + (player.get_transform()[3].z - comets[i].get_transform()[3].z)*(player.get_transform()[3].z - comets[i].get_transform()[3].z)) <= playerradius + cometradius[i])
                    {
                        lives--;
                        comets[i].visible = false;
                    }
                    
                    if (comets[i].get_transform()[3].x <= -90) {
                        counter--;
                        comets[i].visible = false;
                    }
                }
            }
            
            // other inputs
            if (inputHandler->GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
                reload_shaders();
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
            
            if(lives > 0)
            {
                player.render(mCamera.GetWorldToClipMatrix(), player.get_transform());
                
                space.render(mCamera.GetWorldToClipMatrix(), space.get_transform());
                
                for(int i=0; i<20; i++)
                {
                    if(projectiles[i].visible)
                        projectiles[i].render(mCamera.GetWorldToClipMatrix(), projectiles[i].get_transform());
                }
                for (int i = 0; i<10; i++)
                {
                    if (comets[i].visible)
                        comets[i].render(mCamera.GetWorldToClipMatrix(), comets[i].get_transform());
                }
            }
            
            bool opened = ImGui::Begin("Scoreboard", &opened, ImVec2(300, 100), -1.0f, 0);
            if (opened) {
                if(lives >0)
                {
                    ImGui::Text("Score: %d", counter);
                    ImGui::Text("Lives: %d", lives);
                }
                else
                {
                    ImGui::Text("Game Over");
                }
            }
            ImGui::End();
            //            auto node_stack = std::stack<Node const*>();
            //            auto matrix_stack = std::stack<glm::mat4>();
            //            node_stack.push(&world);
            //            matrix_stack.push(glm::mat4());
            //            do {
            //                auto const* const current_node = node_stack.top();
            //                node_stack.pop();
            //
            //                auto const parent_matrix = matrix_stack.top();
            //                matrix_stack.pop();
            //
            //                auto const current_node_matrix = current_node->get_transform();
            //
            //                //
            //                // Todo: Compute the current node's world matrix
            //                //
            //                auto const current_node_world_matrix = parent_matrix*current_node_matrix;
            //                current_node->render(mCamera.GetWorldToClipMatrix(), current_node_world_matrix);
            //
            //                for (int i = static_cast<int>(current_node->get_children_nb()) - 1; i >= 0; --i) {
            //                    node_stack.push(current_node->get_child(static_cast<size_t>(i)));
            //                    matrix_stack.push(current_node_world_matrix);
            //                }
            //            } while (!node_stack.empty());
            
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            
            Log::View::Render();
            
            //
            // Todo: If you want a custom ImGUI window, you can set it up
            //       here
            //
            
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
            edaf80::Assignment5 Assignment5;
            Assignment5.run();
        } catch (std::runtime_error const& e) {
            LogError(e.what());
        }
        Bonobo::Destroy();
    }
