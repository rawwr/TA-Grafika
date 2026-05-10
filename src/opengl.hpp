/*
 * Physically Based Rendering
 * Copyright (c) 2017-2018 Michał Siejak
 *
 * OpenGL 4.5 renderer.
 */

#pragma once


#include <string>
#include <vector>
#include <glad/glad.h>

#include "common/renderer.hpp"

namespace OpenGL {

struct MeshBuffer
{
	MeshBuffer() : vbo(0), ibo(0), vao(0) {}
	GLuint vbo, ibo, vao;
	GLuint numElements;
};

struct FrameBuffer
{
	FrameBuffer() : id(0), colorTarget(0), depthStencilTarget(0) {}
	GLuint id;
	GLuint colorTarget;
	GLuint depthStencilTarget;
	int width, height;
	int samples;
};

struct Texture
{
	Texture() : id(0) {}
	GLuint id;
	int width, height;
	int levels;
};

class Renderer final : public RendererInterface
{
public:
	Renderer();
	GLFWwindow* initialize(int width, int height, int maxSamples) override;
	void shutdown() override;
	void setup() override;
	void render(GLFWwindow* window, const ViewSettings& view, const SceneSettings& scene) override;
	void gui(GLFWwindow* window, ViewSettings& view, SceneSettings& scene) override;

private:
	static GLuint compileShader(const std::string& filename, GLenum type);
	static GLuint linkProgram(std::initializer_list<GLuint> shaders);

	Texture createTexture(GLenum target, int width, int height, GLenum internalformat, int levels=0) const;
	Texture createTexture(const std::shared_ptr<class Image>& image, GLenum format, GLenum internalformat, int levels=0) const;
	static void deleteTexture(Texture& texture);

	static FrameBuffer createFrameBuffer(int width, int height, int samples, GLenum colorFormat, GLenum depthstencilFormat);
	static void resolveFramebuffer(const FrameBuffer& srcfb, const FrameBuffer& dstfb);
	static void deleteFrameBuffer(FrameBuffer& fb);

	static MeshBuffer createMeshBuffer(const std::shared_ptr<class Mesh>& mesh);
	static void deleteMeshBuffer(MeshBuffer& buffer);

	static GLuint createUniformBuffer(const void* data, size_t size);
	template<typename T> GLuint createUniformBuffer(const T* data=nullptr)
	{
		return createUniformBuffer(data, sizeof(T));
	}

	void loadModel(int modelIndex);
	void loadHDREnvironment(int hdrIndex);

#if _DEBUG
	static void logMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
#endif

	struct {
		float maxAnisotropy = 1.0f;
	} m_capabilities;

	FrameBuffer m_framebuffer;
	FrameBuffer m_resolveFramebuffer;

	MeshBuffer m_skybox;

	GLuint m_emptyVAO;

	GLuint m_tonemapProgram;
	GLuint m_skyboxProgram;
	GLuint m_pbrProgram;
	GLuint m_phongProgram;

	Texture m_spBRDF_LUT;

	GLuint m_transformUB;
	GLuint m_shadingUB;

	// Constants for texture sizes
	static constexpr int kEnvMapSize = 1024;
	static constexpr int kIrradianceMapSize = 32;
	static constexpr int kBRDF_LUT_Size = 256;

	// Dynamic loading lists
	struct ModelInfo {
		const char* name;
		const char* meshPath;
		const char* albedoPath;
		const char* normalPath;
		const char* metalnessPath;
		const char* roughnessPath;
		float scale;

		// GPU Resources
		MeshBuffer mesh;
		Texture albedo;
		Texture normal;
		Texture metalness;
		Texture roughness;
		glm::mat4 normalization;
		glm::mat4 preRotation;
	};
	struct HDRInfo {
		const char* name;
		const char* path;

		// GPU Resources
		Texture env;
		Texture irmap;
	};
	std::vector<ModelInfo> m_availableModels;
	std::vector<HDRInfo> m_availableHDRs;
};

} // OpenGL
