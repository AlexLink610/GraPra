#pragma once

#include <cppgl.h>

void init_static_prototypes();
void draw_gui();

class Floor {
public:
	Floor(int w, int h);

	void draw();

    // data
    glm::mat4 model;
    static cppgl::Drawelement prototype;
};

class Skysphere {
public:
	Skysphere(int w, int h, int d);

	void draw();

    glm::mat4 model;
    // HINT: add a static cppgl::Drawelement member
};
