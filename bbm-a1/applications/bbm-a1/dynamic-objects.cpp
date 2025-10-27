#include "dynamic-objects.h"
#include "clientside-networking.h"
#include "particles.h"

#include <cppgl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
#include <iostream>

using namespace std;
using namespace cppgl;

// ------------------------------------------------
// globals

extern int player_id;
extern std::shared_ptr<Particles> particles, particles_small;

// ------------------------------------------------
// prototypes

std::vector<Drawelement> Player::prototype;
Material Box::wood_material;
std::vector<Material> Box::stone_materials;
std::vector<Drawelement> Box::prototype_idle;
std::vector<Drawelement> Box::prototype_scatter;
std::vector<Drawelement> Bomb::prototype;

void init_dynamic_prototypes() {
	{ // load player prototype
		auto shader_norm = Shader("bbm: pos+norm", "shader/pos+norm.vs", "shader/pos+norm.fs");
		auto shader_norm_tc = Shader("bbm: pos+norm+tc,modulated", "shader/pos+norm+tc.vs", "shader/pos+norm+tc.fs");
		auto shader_callback = [shader_norm, shader_norm_tc](const Material& mat) {
			return mat->texture_map.empty() ? shader_norm : shader_norm_tc;
		};
		auto meshes = load_meshes_gpu("render-data/bbm/bbm-nolegs.obj", true);
		for (auto m : meshes) {
			Player::prototype.push_back(Drawelement(m->name, shader_callback(m->material), m));
		}
	}
	{ // load bomb prototype
		auto shader_norm = Shader("bomb: pos+norm", "shader/pos+norm.vs", "shader/pos+norm.fs");
		auto shader_norm_tc = Shader("bomb: pos+norm+tc", "shader/pos+norm+tc.vs", "shader/pos+norm+tc.fs");
		auto shader_callback = [shader_norm, shader_norm_tc](const Material& mat) {
			return mat->texture_map.empty() ? shader_norm : shader_norm_tc;
		};
		auto meshes = load_meshes_gpu("render-data/bomb/bomb.obj", true);
		for (auto m : meshes) {
			Bomb::prototype.push_back(Drawelement(m->name, shader_callback(m->material), m));
		}
	}
	{ // load box prototypes
		auto shader = Shader("box-shader", "shader/box.vs", "shader/box.fs");
		auto shader_cb = [shader](const Material&) { return shader; };
		// load idle and scatter box
		{
			auto meshes = load_meshes_gpu("render-data/cube/cube.obj", true);
			for (auto m : meshes) {
				Box::prototype_idle.push_back(Drawelement(m->name, shader_cb(m->material), m));
			}
		}
		{
			auto meshes = load_meshes_gpu("render-data/crate/wooden_crate.obj", true);
			for (auto m : meshes) {
				Box::prototype_scatter.push_back(Drawelement(m->name, shader_cb(m->material), m));
			}
		}
		// load materials
		auto overlay = Texture2D("crate_overlay", "render-data/images/crate_overlay2.png");
		Box::wood_material = Material("box-wood");
		Box::wood_material->add_texture("diffuse", Texture2D("box-wood", "render-data/crate/crate.png"));
		Box::wood_material->add_texture("normalmap", Texture2D("box-wood-normals", "render-data/crate/crate_normals.png"));
		Box::wood_material->add_texture("overlay", overlay);
		auto mat0 = Material("box-wall_0");
		mat0->add_texture("diffuse", Texture2D("box-wall_diff_0", "render-data/images/wall_0.png"));
		mat0->add_texture("normalmap", Texture2D("box-wall_normals_0", "render-data/images/wall_normals_0.png"));
		mat0->add_texture("overlay", overlay);
		Box::stone_materials.push_back(mat0);
		auto mat1 = Material("box-wall_1");
		mat1->add_texture("diffuse", Texture2D("box-wall_diff_1", "render-data/images/wall_1.png"));
		mat1->add_texture("normalmap", Texture2D("box-wall_normals_1", "render-data/images/wall_normals_1.png"));
		mat1->add_texture("overlay", overlay);
		Box::stone_materials.push_back(mat1);
		auto mat2 = Material("box-wall_2");
		mat2->add_texture("diffuse", Texture2D("box-wall_diff_2", "render-data/images/wall_2.png"));
		mat2->add_texture("normalmap", Texture2D("box-wall_normals_2", "render-data/images/wall_normals_2.png"));
		mat2->add_texture("overlay", overlay);
		Box::stone_materials.push_back(mat2);
	}
}

// ------------------------------------------------
// Player

Player::Player(const std::string& name, int id) : moving(false), look_dir(0, 1),
name(name), model(1), model_rot(1), health(100), id(id), respawning(false), frags(0) {
	model[0][0] = render_settings::character_radius;
	model[1][1] = render_settings::character_radius;
	model[2][2] = render_settings::character_radius;
	model[3][1] = render_settings::character_float_h;
	// Start wobble timer
	wobble_timer.begin();
}

float Player::rotation(Dir dir) {
	if (dir == Dir(0, 1))  return 0;
	if (dir == Dir(0, -1)) return float(M_PI);
	if (dir == Dir(1, 0))  return -float(M_PI) / 2.0f;
	if (dir == Dir(-1, 0)) return float(M_PI) / 2.0f;
	return 0;
}

float Player::rotation_angle_between(Dir from, Dir to) {
	float old_rot = rotation(from);
	float new_rot = rotation(to);
	float diff = old_rot - new_rot;
	if (abs(diff) > M_PI)		// if rotation angle > 180, rotate in other direction
		diff -= glm::sign(diff) * 2.f * float(M_PI);
	return diff;
}

void Player::force_position(int x, int y) {
	moving = false;
	glm::vec3 axis(0, 1, 0);
	base_rotation = -rotation(look_dir);
	model_rot = glm::rotate(glm::mat4(1), base_rotation, glm::vec3(0, 1, 0));
	model[3][0] = x * render_settings::tile_size;
	model[3][2] = y * render_settings::tile_size;
	base = glm::vec3(x, 0, y);
}

void Player::start_moving(int dir_x, int dir_y, int est_duration) {
	if (moving) {	// the last movement is not finished: reset base
		base = base + move_dir;
		model[3][0] = base.x * render_settings::tile_size;
		model[3][2] = base.z * render_settings::tile_size;
		base_rotation = -rotation(look_dir);
	}

	move_dir = glm::vec3(dir_x, 0, dir_y);
	move_duration = float(est_duration);
	move_rotation = rotation_angle_between(look_dir, Dir(dir_x, dir_y));

	look_dir.x = dir_x;
	look_dir.y = dir_y;
	moving = true;
	movement_timer.begin();
}

void Player::update() {
	if (health <= 0) return;

	if (moving) {
		const float elapsed = float(movement_timer.look());
		float t = elapsed / move_duration;

		glm::vec3 pos_tiles = base + t * move_dir;

		model[3][0] = pos_tiles.x * render_settings::tile_size;
		model[3][2] = pos_tiles.z * render_settings::tile_size;

		model_rot = glm::rotate(glm::mat4(1), base_rotation + t * move_rotation, glm::vec3(0, 1, 0));

		if (elapsed >= move_duration) {
			moving = false;
			base = base + move_dir;
			model[3][0] = base.x * render_settings::tile_size;
			model[3][2] = base.z * render_settings::tile_size;
			base_rotation += move_rotation;
			move_rotation = 0.f;
		}

		const glm::vec2& cam_offset = render_settings::character_camera_offset;

		//Camera
		if (id == player_id) {
			auto cam = Camera::find("playercam");
			if (cam) {
				cam->pos = glm::vec3(model[3][0], cam_offset.y, model[3][2] + cam_offset.x);
				glm::vec3 look_target = glm::vec3(model[3][0], 0.f, model[3][2]);
				cam->dir = glm::normalize(look_target - cam->pos);
				cam->up = glm::vec3(0, 1, 0);
				cam->update();
			}
		}

	}
	//Idle Animation
	if (!moving) {
		const float wobble_speed = 0.005f;
		const float wobble_amp = 0.2f;
		float t = float(wobble_timer.look());
		float offset = sinf(t * wobble_speed) * wobble_amp;
		model[3][1] = render_settings::character_float_h + offset;
	}
	else { //reset "animation"
		wobble_timer.begin();
		model[3][1] = render_settings::character_float_h;
	}


	// player particles
	if (particle_timer.look() >= render_settings::particle_emitter_timeslice) {
		particle_timer.begin();
		const float offset = 0.9f * render_settings::character_radius;
		const glm::vec3 pos = glm::vec3(model[3][0], model[3][1] - offset, model[3][2]);
		const glm::vec3 dir = glm::normalize(glm::vec3(random_float(), -3, random_float()));
		particles->add(pos, dir, (rand() % 1000) + 1000);
		if (moving) {
			for (int i = 0; i < 3; ++i) {
				glm::vec3 v = glm::normalize(glm::vec3(
					random_float() - 0.5f,
					random_float() * 0.3f,
					random_float() - 0.5f
				));
				v *= 0.3f * render_settings::tile_size;
				particles_small->add(pos, v, 300 + rand() % 300);
			}
		}

	}

}

void Player::draw() {
	if (health <= 0) return;
	for (auto& elem : prototype) {
		elem->model = model * model_rot;
		elem->bind();
		setup_light(elem->shader);
		elem->draw();
		elem->unbind();
	}
}

// ------------------------------------------------
// Box

Box::Box(unsigned posx, unsigned posy, bool is_stone) : model(1.f), is_stone(is_stone), exploding(false) {
	stone_type = is_stone ? rand() % 3 : 0;
	uv_offset.x = float(rand() % 100) / 100.0f;
	uv_offset.y = float(rand() % 100) / 100.0f;
	model[0][0] = 0.5 * render_settings::tile_size;
	model[1][1] = 0.5 * render_settings::tile_size;
	model[2][2] = 0.5 * render_settings::tile_size;
	model[3][0] = posx * render_settings::tile_size;
	model[3][1] = 0.5 * render_settings::tile_size;
	model[3][2] = posy * render_settings::tile_size;
}

void Box::set_exploding(const glm::vec2& dir) {
	if (exploding) return;
	exploding = true;
	explo_timer.begin();

	const size_t n = prototype_scatter.size();
	explo_rot_axis.clear();
	explo_rot_axis.reserve(n);
	explo_rot_angle.clear();
	explo_rot_angle.reserve(n);
	explo_translation.clear();
	explo_translation.reserve(n);

	glm::vec3 launch = glm::normalize(glm::vec3(dir.x, 0.f, dir.y));
	if (glm::length(launch) < 0.01f) launch = glm::vec3(1, 0, 0);

	for (size_t i = 0; i < n; ++i) {
		glm::vec3 axis = glm::normalize(glm::vec3(
			random_float() - 0.5f,
			random_float() + 0.25f,
			random_float() - 0.5f
		));
		float ang_speed = (2.5f + 3.5f * random_float());
		
		glm::vec3 vel = 0.8f * render_settings::tile_size * (0.5f + random_float()) *
			glm::normalize(launch + 0.35f * glm::vec3(random_float() - 0.5f, 0.2f * random_float(), random_float() - 0.5f));
		
		float lift = (1.0f+ 1.4f * random_float()) * render_settings::tile_size;
		vel.y += lift;

		explo_rot_axis.push_back(axis);
		explo_rot_angle.push_back(ang_speed);
		explo_translation.push_back(vel);		
	}

//more particles		
	for (int i = 0; i < 120; ++i) {
		glm::vec3 p = glm::vec3(model[3][0], model[3][1], model[3][2]);
		glm::vec3 v = glm::normalize(glm::vec3(random_float() - 0.5f, random_float()*0.7f, random_float() - 0.5f));
		v *= (0.8f + random_float() * 2.0f) * render_settings::tile_size;
		particles_small->add(p, v, 400 + rand() % 400);
	}
	


}

bool Box::to_destroy() { 
	if (!exploding) return false;
	return explo_timer.look() > 1200;

}

void Box::draw() {
	if (!exploding) {
		for (auto& elem : prototype_idle) {
			elem->model = model;
			elem->bind();
			// bind wood or stone material
			if (is_stone)
				Box::stone_materials[stone_type]->bind(elem->shader);
			else
				Box::wood_material->bind(elem->shader);
			elem->shader->uniform("uv_offset", uv_offset);
			setup_light(elem->shader);
			elem->draw();
			elem->unbind();
		}
		return;
	}
	const float t = float(explo_timer.look()) * 0.001f;
	const float g = 18.0f;

	const size_t n = prototype_scatter.size();
	for (size_t i = 0; i < n; ++i) {
		glm::vec3 trans = explo_translation[i] * t + glm::vec3(0, -0.5f * g * t * t, 0);

		float angle = explo_rot_angle[i] * t;

		glm::mat4 M = model;
		M = glm::translate(M, trans);
		M = glm::rotate(M, angle, explo_rot_axis[i]);

		auto& elem = prototype_scatter[i];
		elem->model = M;
		elem->bind();

		Box::wood_material->bind(elem->shader);
		elem->shader->uniform("uv_offset", uv_offset);
		setup_light(elem->shader);
		elem->draw();
		elem->unbind();
	}

	
}

// ------------------------------------------------
// Bomb

Bomb::Bomb(int posx, int posy, int id) : x(posx), y(posy), id(id), model(1) {
	float scale = render_settings::character_radius * 0.8f;
	model[0][0] = scale;
	model[1][1] = scale;
	model[2][2] = scale;
	model[3][0] = posx * render_settings::tile_size;
	model[3][1] = 0.5f * render_settings::tile_size;
	model[3][2] = posy * render_settings::tile_size;
}

void Bomb::update(){
	float t = float(glfwGetTime());
	float scale_factor = 0.8f + 0.1f * sinf(t * 10.0f);
	model[0][0] = render_settings::character_radius * scale_factor;
	model[1][1] = render_settings::character_radius * scale_factor;
	model[2][2] = render_settings::character_radius * scale_factor;
	for (int i = 0; i < 6; ++i) {
		glm::vec3 p = glm::vec3(
			model[3][0],
			model[3][1] + 0.6f * render_settings::tile_size,
			model[3][2]
		);

		glm::vec3 v = glm::normalize(glm::vec3(
			random_float() - 0.5f,
			random_float() * 0.8f + 0.6f,
			random_float() - 0.5f
		));

		v *= 0.1f * render_settings::tile_size;
		int lifetime = 300 + rand() % 300;
		particles_small->add(p, v, lifetime);
	}
}

void Bomb::draw() {

	for (auto& elem : prototype) {
		elem->model = model;
		elem->bind();
		setup_light(elem->shader);
		elem->draw();
		elem->unbind();
	}
}

// ------------------------------------------------
// Board

Board::Board(int tiles_x, int tiles_y) : tiles_x(tiles_x), tiles_y(tiles_y), camera_shake(false) {
	// init cells
	grid = std::vector<std::vector<std::shared_ptr<Box>>>(tiles_y);
	for (int y = 0; y < tiles_y; ++y) {
		grid[y] = std::vector<std::shared_ptr<Box>>(tiles_x);
	}
}

void Board::add_box(int x, int y, int type) {
	const bool is_stone = type == msg::box_type::stone;
	auto box = std::make_shared<Box>(x, y, is_stone);
	grid[y][x] = box;
	if (is_stone)
		stone_boxes.push_back(box);
	else
		crates.push_back(box);
}

void Board::add_bomb(int x, int y, int id) {
	bombs.push_back(std::make_shared<Bomb>(x, y, id));
}

void Board::update() {
	//erase exploded crates
	for (auto it = crates.begin(); it != crates.end(); ) {
		if ((*it)->to_destroy()) {
			it = crates.erase(it);
		}
		else
		{
			++it;
		}
	}
	
	//camera shake
	if (camera_shake) {
		float t = static_cast<float>(camera_shake_timer.look());
		float duration = 200.0f;
		auto cam = Camera::find("playercam");
		if (cam) {
			if (t > duration) {
				camera_shake = false;
				cam->pos = camera_original_pos;
				cam->update();
			}
			else
			{
				float strength = (0.3f - (t / duration)) * 0.4f * render_settings::tile_size;
				float shake_x = (random_float() - 0.5f) * strength;
				float shake_y = (random_float() - 0.5f) * strength;

				cam->pos = camera_original_pos + glm::vec3(shake_x, 0.0f, shake_y);
				cam->update();
			}
		}
	}
	for(auto& elem : bombs)
		elem->update();
	

}

void Board::draw() {
	for (auto& elem : stone_boxes)
		elem->draw();
	for (auto& elem : crates)
		elem->draw();
	for (auto& elem : bombs)
		elem->draw();
}

void Board::explosion(int bomb_id, unsigned int codes) {
	auto cam = Camera::find("playercam");
	if (cam) camera_original_pos = cam->pos;
	// Camera wobble on explode
	camera_shake_timer.begin();
	camera_shake = true;

	std::shared_ptr<Bomb> the_bomb;
	for (auto it = bombs.begin(); it != bombs.end(); ++it) {
		if ((*it)->id == bomb_id) {
			the_bomb = *it;
			bombs.erase(it);
			break;
		}
	}

	if (!the_bomb) {
		cerr << "ERROR: The server triggered an explosion for a bomb we don't have recorded!" << endl;
		return;
	}
	glm::vec3 bomb_pos = glm::vec3(the_bomb->model[3][0], the_bomb->model[3][1], the_bomb->model[3][2]);

	//more particles
	for (int i = 0; i < 200; i++) {
		glm::vec3 dir = glm::normalize(glm::vec3(
			random_float() - 0.5f,
			random_float(),
			random_float() - 0.5f
		));
		particles->add(bomb_pos, dir, 300 + rand() % 500);
	}
	//more particles
	for (int i = 0; i < 100; ++i) {
		glm::vec3 v = glm::normalize(glm::vec3(
			random_float() - 0.5f,
			random_float(),
			random_float() - 0.5f
		));
		v *= (0.5f + random_float() * 1.8f) * render_settings::tile_size;
		particles_small->add(bomb_pos, v, 600 + rand() % 600);
	}




	//decoding codes
	const int len_posx = codes & 0x3;
	const int len_negx = (codes >> 2) & 0x3;
	const int len_posy = (codes >> 4) & 0x3;
	const int len_negy = (codes >> 6) & 0x3;

	//bomb tile coordinates
	int bx = int(std::floor(bomb_pos.x / render_settings::tile_size + 0.5f));
	int by = int(std::floor(bomb_pos.z / render_settings::tile_size + 0.5f));

	auto process_dir = [&](int dx, int dy, int length) { //helper func to process one direction
		for (int step = 1; step <= length; ++step) {
			//current tile coordinates
			int tx = bx + dx * step;
			int ty = by + dy * step;

			if (ty < 0 || ty >=tiles_y || tx < 0 || tx >= tiles_x) //check for OOB
				break;
			auto& cell = grid[ty][tx];
			if (cell) {
				if (cell->is_stone) break; //stop on stone
				else{
					cell->set_exploding(glm::vec2(dx, dy));
					cell.reset();
				}
			}
			//spawns in particles
			glm::vec3 flame_pos = glm::vec3(
				tx * render_settings::tile_size,
				0.5f * render_settings::tile_size,
				ty * render_settings::tile_size
			);
			for (int i = 0; i < 40; ++i) {
				glm::vec3 dir = glm::normalize(glm::vec3(
					random_float() - 0.5f,
					random_float(),
					random_float() - 0.5f
				));
				particles->add(flame_pos, dir, 300 + rand() % 400);
			}
		}
	};
	process_dir(+1, 0, len_posx);
	process_dir(-1, 0, len_negx);
	process_dir(0, +1, len_posy);
	process_dir(0, -1, len_negy);


}
