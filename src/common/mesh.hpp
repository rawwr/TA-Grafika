/*
 * Physically Based Rendering
 * Copyright (c) 2017-2018 Michał Siejak
 */

#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class Mesh
{
public:
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec3 tangent;
		glm::vec3 bitangent;
		glm::vec2 texcoord;
	};
	static_assert(sizeof(Vertex) == 14 * sizeof(float));
	static const int NumAttributes = 5;

	struct Face
	{
		uint32_t v1, v2, v3;
	};
	static_assert(sizeof(Face) == 3 * sizeof(uint32_t));

	static std::shared_ptr<Mesh> fromFile(const std::string& filename);
	static std::shared_ptr<Mesh> fromString(const std::string& data);

	const std::vector<Vertex>& vertices() const { return m_vertices; }
	const std::vector<Face>& faces() const { return m_faces; }

	glm::vec3 min() const { return m_min; }
	glm::vec3 max() const { return m_max; }

private:
	Mesh(const struct aiMesh* mesh);

	std::vector<Vertex> m_vertices;
	std::vector<Face> m_faces;

	glm::vec3 m_min;
	glm::vec3 m_max;
};
