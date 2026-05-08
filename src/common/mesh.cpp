/*
 * Physically Based Rendering
 * Copyright (c) 2017-2018 Michał Siejak
 */

#include <cstdio>
#include <stdexcept>
#include <cassert>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "mesh.hpp"

namespace {
	const unsigned int ImportFlags = 
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_SortByPType |
		aiProcess_PreTransformVertices |
		aiProcess_GenNormals |
		aiProcess_GenUVCoords |
		aiProcess_OptimizeMeshes |
		aiProcess_Debone |
		aiProcess_ValidateDataStructure;
}

Mesh::Mesh(const aiMesh* mesh)
{
	assert(mesh->HasPositions());
	assert(mesh->HasNormals());

	m_vertices.reserve(mesh->mNumVertices);
	for(size_t i=0; i<m_vertices.capacity(); ++i) {
		Vertex vertex;
		vertex.position = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
		vertex.normal = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};
		if(mesh->HasTangentsAndBitangents()) {
			vertex.tangent = {mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z};
			vertex.bitangent = {mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z};
		}
		else {
			// Default tangent space if not provided
			vertex.tangent = {1.0f, 0.0f, 0.0f};
			vertex.bitangent = {0.0f, 1.0f, 0.0f};
		}
		if(mesh->HasTextureCoords(0)) {
			vertex.texcoord = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
		}
		else {
			// Default UV coordinates if not provided
			vertex.texcoord = {0.0f, 0.0f};
		}
		m_vertices.push_back(vertex);
	}
	
	m_faces.reserve(mesh->mNumFaces);
	for(size_t i=0; i<m_faces.capacity(); ++i) {
		assert(mesh->mFaces[i].mNumIndices == 3);
		m_faces.push_back({mesh->mFaces[i].mIndices[0], mesh->mFaces[i].mIndices[1], mesh->mFaces[i].mIndices[2]});
	}
}

std::shared_ptr<Mesh> Mesh::fromFile(const std::string& filename)
{
	std::printf("Loading mesh: %s\n", filename.c_str());
	
	const aiScene* scene = aiImportFile(filename.c_str(), ImportFlags);
	if(!scene || !scene->HasMeshes()) {
		throw std::runtime_error("Failed to load mesh file: " + filename + " (" + aiGetErrorString() + ")");
	}

	std::printf("Scene contains %u meshes\n", scene->mNumMeshes);
	
	// Load first mesh as base
	std::shared_ptr<Mesh> mesh = std::shared_ptr<Mesh>(new Mesh{scene->mMeshes[0]});
	
	// Append all additional meshes
	for(unsigned int i = 1; i < scene->mNumMeshes; ++i) {
		const aiMesh* aiMesh = scene->mMeshes[i];
		if(!aiMesh->HasPositions() || !aiMesh->HasNormals()) {
			std::printf("Skipping mesh %u (missing positions or normals)\n", i);
			continue;
		}
		
		std::printf("Merging mesh %u (%u vertices, %u faces)\n", i, aiMesh->mNumVertices, aiMesh->mNumFaces);
		
		// Store the current vertex offset for index adjustment
		uint32_t vertexOffset = static_cast<uint32_t>(mesh->m_vertices.size());
		
		// Append vertices
		for(size_t v = 0; v < aiMesh->mNumVertices; ++v) {
			Vertex vertex;
			vertex.position = {aiMesh->mVertices[v].x, aiMesh->mVertices[v].y, aiMesh->mVertices[v].z};
			vertex.normal = {aiMesh->mNormals[v].x, aiMesh->mNormals[v].y, aiMesh->mNormals[v].z};
			if(aiMesh->HasTangentsAndBitangents()) {
				vertex.tangent = {aiMesh->mTangents[v].x, aiMesh->mTangents[v].y, aiMesh->mTangents[v].z};
				vertex.bitangent = {aiMesh->mBitangents[v].x, aiMesh->mBitangents[v].y, aiMesh->mBitangents[v].z};
			}
			else {
				vertex.tangent = {1.0f, 0.0f, 0.0f};
				vertex.bitangent = {0.0f, 1.0f, 0.0f};
			}
			if(aiMesh->HasTextureCoords(0)) {
				vertex.texcoord = {aiMesh->mTextureCoords[0][v].x, aiMesh->mTextureCoords[0][v].y};
			}
			else {
				vertex.texcoord = {0.0f, 0.0f};
			}
			mesh->m_vertices.push_back(vertex);
		}
		
		// Append faces with adjusted indices
		for(size_t f = 0; f < aiMesh->mNumFaces; ++f) {
			assert(aiMesh->mFaces[f].mNumIndices == 3);
			mesh->m_faces.push_back({
				aiMesh->mFaces[f].mIndices[0] + vertexOffset,
				aiMesh->mFaces[f].mIndices[1] + vertexOffset,
				aiMesh->mFaces[f].mIndices[2] + vertexOffset
			});
		}
	}
	
	std::printf("Final mesh: %zu vertices, %zu faces\n", mesh->m_vertices.size(), mesh->m_faces.size());
	
	aiReleaseImport(scene);
	return mesh;
}

std::shared_ptr<Mesh> Mesh::fromString(const std::string& data)
{
	const aiScene* scene = aiImportFileFromMemory(data.c_str(), (unsigned int)data.length(), ImportFlags, "nff");
	if(!scene || !scene->HasMeshes()) {
		throw std::runtime_error("Failed to create mesh from string (" + std::string(aiGetErrorString()) + ")");
	}

	std::shared_ptr<Mesh> mesh = std::shared_ptr<Mesh>(new Mesh{scene->mMeshes[0]});
	aiReleaseImport(scene);
	return mesh;
}
