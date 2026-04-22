/*
 * Physically Based Rendering
 * Copyright (c) 2017-2018 Michał Siejak
 *
 * OpenGL 4.5 demo entry point.
 */

#include <cstdio>
#include <memory>

#include "application.hpp"
#include "../opengl.hpp"

int main(int, char*[])
{
	try {
		Application().run(std::unique_ptr<RendererInterface>{ new OpenGL::Renderer });
	}
	catch(const std::exception& e) {
		std::fprintf(stderr, "Error: %s\n", e.what());
		return 1;
	}
	return 0;
}
