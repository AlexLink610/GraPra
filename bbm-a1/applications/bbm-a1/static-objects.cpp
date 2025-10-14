#include "static-objects.h"
#include <cppgl.h>
#include "rendering.h"
#include "dynamic-objects.h"

using namespace cppgl;

// ------------------------------------------------
// globals

extern int player_id;
extern std::vector<std::shared_ptr<Player>> players;

// ------------------------------------------------
// prototypes

Drawelement Floor::prototype;

// HINT: you might need this for A12
static glm::vec3 polar2cartesian(float theta, float phi){
    float sinTheta = sinf(theta);
    float cosTheta = cosf(theta);
    float sinPhi = sinf(phi);
    float cosPhi = cosf(phi);
    glm::vec3 normal = glm::vec3(cosPhi * sinTheta,
        sinPhi * sinTheta,
        cosTheta);                 // z as north pole.
    return normal;
}

void init_static_prototypes() {
    { // init floor prototype
        // setup mesh
        glm::vec3 vertices[4] = { {0,0,0}, {0,0,1}, {1,0,1}, {1,0,0} };
        glm::vec3 normals[4] = { {0,1,0}, {0,1,0}, {0,1,0}, {0,1,0} };
        glm::vec2 texcoords[4] = { {0, 0}, {0, 1}, {1, 1}, {1, 0} };
        unsigned int indices[6] = { 0, 1, 2, 0, 2, 3 };
        auto mesh = Mesh("floor-mesh");
        mesh->add_vertex_buffer(GL_FLOAT, 3, 4, vertices);
        mesh->add_vertex_buffer(GL_FLOAT, 3, 4, normals);
        mesh->add_vertex_buffer(GL_FLOAT, 2, 4, texcoords);
        mesh->add_index_buffer(6, indices);
        // setup material
        auto mat = Material("floor-material");
        mat->add_texture("diffuse", Texture2D("floor-diffuse", "render-data/images/floor.png"));
        mat->add_texture("normalmap", Texture2D("floor-normalmap", "render-data/images/floor_normals.png"));
        mesh->material = mat;
        // setup shader
        auto shader = Shader("floor-shader", "shader/floor.vs", "shader/floor.fs");
        Floor::prototype = Drawelement("floor",shader,mesh);
    }
    { // init skysphere prototype
        const uint32_t stack_count = 32;
        const uint32_t sector_count = 32;
        const float stack_step = M_PI/stack_count;
        const float sector_step = 2*M_PI/sector_count;

        /* HINT: 
        1.
            * create a Mesh("skysphere-mesh")
            * add vertices and texcoords as vertex buffers (mesh->add_vertex_buffer())
            * create Shader using shader/sky.vs and shader/sky.fs
            * load "render-data/images/skysphere.png"
            * create Material and skysphere Texture2D
            * create a Drawelement and assign it to Skysphere::prototype 
        2.
            * calc vertices and texcoords, construct poles seperately
        */
        std::vector<glm::vec3> vertices = {{-0.273234, -1.039266, -0.662913}, {0.351766, -0.414266, 0.220971}, {-0.273234, -1.039266, 0.220971}};
        std::vector<glm::vec2> texcoords = {{0, 0}, {0, 1}, {1, 1}};

    }
}

void draw_gui() {
    const glm::ivec2 screen_size = Context::resolution();
    ImVec2 size = ImVec2(glm::clamp(screen_size.x / 4, 200, 400), glm::clamp(screen_size.y / 30, 20, 40));
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(size.x, size.y * (players.size()+1)));
    if (ImGui::Begin("GUI", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground)) {
        ImGui::SetWindowFontScale(1.5f);
        for (uint32_t i = 0; i < players.size(); ++i) {
            auto& player = players[i];
            const glm::vec3 col = glm::mix(glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), player->health / 100.f);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(col.x, col.y, col.z, 1));
            const std::string label = player->name + " (frags: " + std::to_string(player->frags) + ") ";
            ImGui::ProgressBar(player->health / 100.f, size, label.c_str());
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
    }
    ImGui::End();
}

// ------------------------------------------------
// Floor

Floor::Floor(int w, int h) : model(1) {
    // setup modelmatrix
    model[0][0] = float(w) * render_settings::tile_size;
    model[1][1] = 0.5 * render_settings::tile_size;
    model[2][2] = float(h) * render_settings::tile_size;
    model[3][0] = -0.5 * render_settings::tile_size;
    model[3][2] = -0.5 * render_settings::tile_size;
}

void Floor::draw() {
    prototype->model = model;
    prototype->bind();
    setup_light(prototype->shader);
    prototype->shader->uniform("tc_scale", glm::vec2(model[0][0], model[2][2]) / render_settings::tile_size);
    prototype->draw();
    prototype->unbind();
}

// ------------------------------------------------
// Skysphere

Skysphere::Skysphere(int w, int h, int d) : model(1) {
    // build modelmatrix
    // scale 
	model[0][0] *= d * render_settings::tile_size;
	model[1][1] *= d * render_settings::tile_size;
	model[2][2] *= d * render_settings::tile_size;
    // rotate
    model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f,2.f,0.f));
    // translate
	model[3][0] = 0.5f * w * render_settings::tile_size;
    model[3][1] = 0.5f * d * render_settings::tile_size;
	model[3][2] = 0.5f * h * render_settings::tile_size;
}

void Skysphere::draw() {
    glDepthFunc(GL_LEQUAL);
    // switch from counter clockwise convention to clockwise
    glFrontFace(GL_CW); 

    // HINT: here we might have skipped to draw the drawelement

    // switch back
    glFrontFace(GL_CCW);
    glDepthFunc(GL_LESS);
}
