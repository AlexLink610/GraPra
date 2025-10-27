#include <iostream>

#include <cppgl.h>
#include <fstream>
#include "cmdline.h"
#include "particles.h"
#include "rendering.h"
#include "static-objects.h"
#include "dynamic-objects.h"
#include "clientside-networking.h"
#undef far
#undef near

using namespace std;
using namespace cppgl;

// ---------------------------------------
// globals

bool game_is_running = false;
std::shared_ptr<Board> the_board;
std::shared_ptr<Floor> the_floor;
std::shared_ptr<Skysphere> the_skysphere;
std::vector<std::shared_ptr<Player>> players;
std::shared_ptr<Particles> particles, particles_small;
Framebuffer fbo;
int player_id = -1;
boost::asio::ip::tcp::socket* server_connection = 0;
client_message_reader* reader = 0;
boost::asio::io_service io_service;
// ---------------------------------------
// callbacks

void keyboard_callback(int key, int scancode, int action, int mods) {
    if (!reader || !reader->prologue_over()) return;
    if (key == GLFW_KEY_F2 && action == GLFW_PRESS) make_camera_current(Camera::find("default"));
    if (key == GLFW_KEY_F3 && action == GLFW_PRESS) make_camera_current(Camera::find("playercam"));
    if (key == GLFW_KEY_F5 && action == GLFW_PRESS) {
        static bool wireframe = false;
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    }
    if (current_camera()->name != "playercam") return;

    // HINT: https://www.glfw.org/docs/latest/input_guide.html

	// HINT key handling missing

    //helper function that maps key input to direction
    auto dir_from_key = [](int key) -> int {
        switch (key) {
        case GLFW_KEY_W:
        case GLFW_KEY_UP:    return msg::key_code::up;
        case GLFW_KEY_S:
        case GLFW_KEY_DOWN:  return msg::key_code::down;
        case GLFW_KEY_A:
        case GLFW_KEY_LEFT:  return msg::key_code::left;
        case GLFW_KEY_D:
        case GLFW_KEY_RIGHT: return msg::key_code::right;
        default:             return -1;
        }
    };


    const int dir = dir_from_key(key);
    if (dir != -1) {
        //tracks previous state to avoid spamming
        static bool pressed[4] = { false, false, false, false };

        if (action == GLFW_PRESS) {
            if (!pressed[dir]) {
                pressed[dir] = true;
                auto m = make_message<msg::key_updown>();
                m.k = static_cast<uint8_t>(dir);
                m.down = 1;
                send_message(m);
            }
        }
        else if (action == GLFW_RELEASE) {
            if (pressed[dir]) {
                pressed[dir] = false;
                auto m = make_message<msg::key_updown>();
                m.k = static_cast<uint8_t>(dir);
                m.down = 0;
                send_message(m);
            }
        }
    }
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        auto m = make_message<msg::key_drop>();
        send_message(m);
    }
}

void resize_callback(int w, int h) {
    if (fbo) fbo->resize(w, h);
}

// ---------------------------------------
// main

int main(int argc, char** argv) {



    if(!parse_cmdline(argc, argv)) return 0;

    // init context and set parameters
    ContextParameters params;
    params.title = "bbm";
    params.font_ttf_filename = EXECUTABLE_DIR + std::string("/render-data/fonts/DroidSansMono.ttf");
    params.font_size_pixels = 15;
    params.width = cmdline.res_x;
    params.height = cmdline.res_y;
    Context::init(params);
    Context::set_keyboard_callback(keyboard_callback);
    Context::set_resize_callback(resize_callback);

    // EXECUTABLE_DIR set via cmake, paths now relative to source/executable directory
    std::filesystem::current_path(EXECUTABLE_DIR);

    auto playercam = Camera("playercam");
    playercam->far = 250;
    make_camera_current(playercam);

    const glm::ivec2 res = Context::resolution();
    fbo = Framebuffer("target_fbo", res.x, res.y);
    fbo->attach_depthbuffer(Texture2D("fbo_depth", res.x, res.y, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT));
    fbo->attach_colorbuffer(Texture2D("fbo_color", res.x, res.y, GL_RGBA32F, GL_RGBA, GL_FLOAT));
    fbo->check();

    //Tests if render-data was correctly found in applications/bbm-a1/render-data
    std::ifstream test_folders("render-data/images/gameover.png");
    if(!test_folders.is_open()) {
        std::cerr << "IMAGE IN RENDER DATA NOT FOUND. Please check if the bbm folder contains the entire render-data folder and download it otherwise." << std::endl;
        throw std::runtime_error("IMAGE IN RENDER DATA NOT FOUND. \
            Please check if the bbm folder contains the entire render-data folder and download it otherwise.; \
            Failed to open image file: render-data/images/gameover.png; ");
        return 1;
    }
    else {
        test_folders.close();
    }

    auto game_over_tex = Texture2D("game-over", "render-data/images/gameover.png");

    init_static_prototypes();
    init_dynamic_prototypes();
    particles = std::make_shared<Particles>(2000, render_settings::particle_size);
    particles_small = std::make_shared<Particles>(3000, render_settings::particle_size * 0.1);

    networking_prologue();

    TimerQuery input_timer("input");
    TimerQuery update_timer("update");
    TimerQueryGL render_timer("render");

    glClearColor(.75f, .1f, .75f, 1.f);

    while (Context::running() && game_is_running) {
        // input handling
        input_timer.begin();
        if (current_camera()->name != "playercam")
            CameraImpl::default_input_handler(Context::frame_time());

        reader->read_and_handle();
        current_camera()->update();
        reload_modified_shaders();
        input_timer.end();

        // update
        update_timer.begin();
        for (auto& player : players)
            player->update();
        the_board->update();
        particles->update();
        particles_small->update();
        update_timer.end();

        // render
        render_timer.begin();
        fbo->bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        for (auto& player : players)
            player->draw();
        the_board->draw();
        the_floor->draw();
        fbo->unbind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        blit_depth(fbo->depth_texture, fbo->color_textures[0]);
        // HINT: we might have skipped a draw call here
        particles->draw();
        particles_small->draw();
        draw_gui();
        render_timer.end();

        // finish frame
        Context::swap_buffers();
    }

    Timer game_over_timer;
    while (Context::running() && game_over_timer.look() < 1337) {
        blit(game_over_tex);
        Context::swap_buffers();
    }

    return 0;
}
