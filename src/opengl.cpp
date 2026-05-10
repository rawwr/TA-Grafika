/*
 * Physically Based Rendering
 * Copyright (c) 2017-2018 Michał Siejak
 *
 * OpenGL 4.5 renderer.
 */

#include <memory>
#include <stdexcept>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "common/image.hpp"
#include "common/mesh.hpp"
#include "common/utils.hpp"
#include "opengl.hpp"

namespace OpenGL {

struct TransformUB {
  glm::mat4 viewProjectionMatrix;
  glm::mat4 skyProjectionMatrix;
  glm::mat4 sceneRotationMatrix;
};

struct ShadingUB {
  struct {
    glm::vec4 direction;
    glm::vec4 radiance;
  } lights[SceneSettings::NumLights];
  glm::vec4 eyePosition;
  glm::vec4 flags; // x: albedo, y: normal, z: metalness, w: roughness
  glm::vec4 extra; // x: debugView, y: phongShininess
};

Renderer::Renderer()
	: m_emptyVAO(0)
	, m_tonemapProgram(0)
	, m_skyboxProgram(0)
	, m_pbrProgram(0)
	, m_phongProgram(0)
	, m_transformUB(0)
	, m_shadingUB(0)
	, m_loadIndex(0)
	, m_isTerminating(false)
{
}

GLFWwindow *Renderer::initialize(int width, int height, int maxSamples) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
#if _DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

  glfwWindowHint(GLFW_DEPTH_BITS, 0);
  glfwWindowHint(GLFW_STENCIL_BITS, 0);
  glfwWindowHint(GLFW_SAMPLES, 0);

  GLFWwindow* window = glfwCreateWindow(width, height, "Physically Based Rendering - Demo Pembelajaran", nullptr, nullptr);
  if (!window) {
    throw std::runtime_error("Failed to create OpenGL context");
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Standard VSync for better compatibility

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    throw std::runtime_error("Failed to initialize OpenGL extensions loader");
  }

  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &m_capabilities.maxAnisotropy);

#if _DEBUG
  glDebugMessageCallback(Renderer::logMessage, nullptr);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

  GLint maxSupportedSamples;
  glGetIntegerv(GL_MAX_SAMPLES, &maxSupportedSamples);

  const int samples = glm::min(maxSamples, maxSupportedSamples);
  m_framebuffer = createFrameBuffer(width, height, samples, GL_RGBA16F,
                                    GL_DEPTH24_STENCIL8);
  if (samples > 0) {
    m_resolveFramebuffer =
        createFrameBuffer(width, height, 0, GL_RGBA16F, GL_NONE);
  } else {
    m_resolveFramebuffer = m_framebuffer;
  }

  std::printf("OpenGL 4.5 Renderer [%s]\n", glGetString(GL_RENDERER));
  return window;
}

void Renderer::shutdown()
{
	m_isTerminating = true;
	if (m_loadingThread.joinable()) {
		m_loadingThread.join();
	}

	if(m_framebuffer.id != m_resolveFramebuffer.id) {
		deleteFrameBuffer(m_resolveFramebuffer);
	}
	deleteFrameBuffer(m_framebuffer);

	glDeleteVertexArrays(1, &m_emptyVAO);

	glDeleteProgram(m_tonemapProgram);
	glDeleteProgram(m_skyboxProgram);
	glDeleteProgram(m_pbrProgram);
	glDeleteProgram(m_phongProgram);

	deleteTexture(m_spBRDF_LUT);

	for(auto& m : m_availableModels) {
		deleteMeshBuffer(m.mesh);
		deleteTexture(m.albedo);
		deleteTexture(m.normal);
		deleteTexture(m.metalness);
		deleteTexture(m.roughness);
	}
	for(auto& h : m_availableHDRs) {
		deleteTexture(h.env);
		deleteTexture(h.irmap);
	}
}

void Renderer::setup()
{

	// Set global OpenGL state.
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glFrontFace(GL_CCW);

	// Create empty VAO for rendering full screen triangle.
	glCreateVertexArrays(1, &m_emptyVAO);

	// Create uniform buffers.
	m_transformUB = createUniformBuffer<TransformUB>();
	m_shadingUB = createUniformBuffer<ShadingUB>();

	// Initialize available models and HDRs
	m_availableModels = {
		{"F1 Wheel", "meshes/F1 Wheel.fbx", "textures/F1 Wheel_Albedo.png", "textures/F1 Wheel_Normal.png", "textures/F1 Wheel_Metalness.png", "textures/F1 Wheel_Roughness.png", 80.0f},
		{"Cerberus Gun", "meshes/cerberus.fbx", "textures/cerberus_A.png", "textures/cerberus_N.png", "textures/cerberus_M.png", "textures/cerberus_R.png", 90.0f},
		{"HDD", "meshes/working.obj", "textures/working_A.png", "textures/working_N.png", "textures/working_M.png", "textures/working_R.png", 60.0f}
	};
	m_availableHDRs = {
		{"Gedung FT Outdoor", "environment.hdr"},
		{"Indoor", "indoor.hdr"},
		{"Outdoor 2", "old.environment.hdr"}
	};

	// Load assets & compile/link rendering programs.
	m_tonemapProgram = linkProgram({
		compileShader("shaders/glsl/tonemap_vs.glsl", GL_VERTEX_SHADER),
		compileShader("shaders/glsl/tonemap_fs.glsl", GL_FRAGMENT_SHADER)
	});

	m_skybox = createMeshBuffer(Mesh::fromFile("meshes/skybox.obj"));
	m_skyboxProgram = linkProgram({
		compileShader("shaders/glsl/skybox_vs.glsl", GL_VERTEX_SHADER),
		compileShader("shaders/glsl/skybox_fs.glsl", GL_FRAGMENT_SHADER)
	});

	m_pbrProgram = linkProgram({
		compileShader("shaders/glsl/pbr_vs.glsl", GL_VERTEX_SHADER),
		compileShader("shaders/glsl/pbr_fs.glsl", GL_FRAGMENT_SHADER)
	});

	m_phongProgram = linkProgram({
		compileShader("shaders/glsl/pbr_vs.glsl", GL_VERTEX_SHADER),
		compileShader("shaders/glsl/phong_fs.glsl", GL_FRAGMENT_SHADER)
	});

	// Pre-load constant LUT
	// Compute Cook-Torrance BRDF 2D LUT for split-sum approximation.
	{
		GLuint spbrdfProgram = linkProgram({ compileShader("shaders/glsl/spbrdf_cs.glsl", GL_COMPUTE_SHADER) });
		m_spBRDF_LUT = createTexture(GL_TEXTURE_2D, kBRDF_LUT_Size, kBRDF_LUT_Size, GL_RG16F, 1);
		glUseProgram(spbrdfProgram);
		glBindImageTexture(0, m_spBRDF_LUT.id, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
		glDispatchCompute(kBRDF_LUT_Size / 32, kBRDF_LUT_Size / 32, 1);
		glDeleteProgram(spbrdfProgram);
	}

	// Start background loading
	startAsyncLoad();

	glFinish();
	std::printf("Initialization complete.\n");
}

void Renderer::render(GLFWwindow* window, const ViewSettings& view, const SceneSettings& scene)
{
	const_cast<Renderer*>(this)->processAsyncLoading(const_cast<SceneSettings&>(scene));

	if (scene.isLoading) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		return;
	}

	const ModelInfo& currentModel = m_availableModels[scene.currentModelIndex];
	const HDRInfo& currentHDR = m_availableHDRs[scene.currentHDRIndex];

	const glm::mat4 projectionMatrix = glm::perspectiveFov(glm::radians(view.fov), float(m_framebuffer.width), float(m_framebuffer.height), 0.01f, 10000.0f);
	const glm::mat4 viewRotationMatrix = glm::eulerAngleYX(glm::radians(view.yaw), glm::radians(view.pitch));
	const glm::mat4 sceneRotationMatrix = glm::mat4_cast(scene.rotation);

	const glm::mat4 sceneTransform = sceneRotationMatrix * currentModel.preRotation * currentModel.normalization;
	const glm::mat4 viewMatrix = glm::translate(glm::mat4{ 1.0f }, { 0.0f, 0.0f, -view.distance }) * viewRotationMatrix;
	const glm::vec3 eyePosition = glm::inverse(viewMatrix)[3];

	// Update transform uniform buffer.
	{
		TransformUB transformUniforms;
		transformUniforms.viewProjectionMatrix = projectionMatrix * viewMatrix;
		transformUniforms.skyProjectionMatrix = projectionMatrix * viewRotationMatrix;
		transformUniforms.sceneRotationMatrix = sceneTransform;
		glNamedBufferSubData(m_transformUB, 0, sizeof(TransformUB), &transformUniforms);
	}

	// Update shading uniform buffer.
	{
		ShadingUB shadingUniforms;
		shadingUniforms.eyePosition = glm::vec4(eyePosition, 0.0f);
		for (int i = 0; i < SceneSettings::NumLights; ++i) {
			const SceneSettings::Light& light = scene.lights[i];
			shadingUniforms.lights[i].direction = glm::vec4{ light.direction, 0.0f };
			if (light.enabled) {
				shadingUniforms.lights[i].radiance = glm::vec4{ light.radiance, 0.0f };
			}
			else {
				shadingUniforms.lights[i].radiance = glm::vec4{};
			}
		}
		shadingUniforms.flags = glm::vec4(
			scene.useAlbedo ? 1.0f : 0.0f, scene.useNormalMap ? 1.0f : 0.0f,
			scene.useMetalness ? 1.0f : 0.0f, scene.useRoughness ? 1.0f : 0.0f);
		shadingUniforms.extra = glm::vec4((float)scene.debugView, scene.phongShininess, 0.0f, 0.0f);
		glNamedBufferSubData(m_shadingUB, 0, sizeof(ShadingUB), &shadingUniforms);
	}

	// Prepare framebuffer for rendering.
	glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer.id);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Bind uniform buffers.
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_transformUB);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_shadingUB);

	auto drawScene = [&](GLuint program, bool pbr) {
		// Draw skybox.
		glDisable(GL_DEPTH_TEST);
		glUseProgram(m_skyboxProgram);
		glBindTextureUnit(0, currentHDR.env.id);
		glBindVertexArray(m_skybox.vao);
		glDrawElements(GL_TRIANGLES, m_skybox.numElements, GL_UNSIGNED_INT, 0);

		// Draw model.
		glEnable(GL_DEPTH_TEST);
		glUseProgram(program);
		glBindTextureUnit(0, currentModel.albedo.id);
		glBindTextureUnit(1, currentModel.normal.id);
		glBindTextureUnit(2, currentModel.metalness.id);
		glBindTextureUnit(3, currentModel.roughness.id);
		glBindTextureUnit(4, currentHDR.env.id);
		glBindTextureUnit(5, currentHDR.irmap.id);
		glBindTextureUnit(6, m_spBRDF_LUT.id);

		glBindVertexArray(currentModel.mesh.vao);
		glDrawElements(GL_TRIANGLES, currentModel.mesh.numElements, GL_UNSIGNED_INT, 0);
	};

  if (view.splitScreen) {
    int splitWidth = static_cast<int>(m_framebuffer.width * view.splitPosition);

    glEnable(GL_SCISSOR_TEST);
    glViewport(0, 0, m_framebuffer.width, m_framebuffer.height);

    // Left half: PBR
    glScissor(0, 0, splitWidth, m_framebuffer.height);
    drawScene(m_pbrProgram, true);

    // Right half: Phong
    glScissor(splitWidth, 0, m_framebuffer.width - splitWidth,
              m_framebuffer.height);
    drawScene(m_phongProgram, false);

    glDisable(GL_SCISSOR_TEST);
  } else {
    glViewport(0, 0, m_framebuffer.width, m_framebuffer.height);
    drawScene(m_pbrProgram, true);
  }

  // Resolve multisample framebuffer.
  resolveFramebuffer(m_framebuffer, m_resolveFramebuffer);

  // Draw a full screen triangle for postprocessing/tone mapping.
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_DEPTH_TEST);
  glUseProgram(m_tonemapProgram);
  glProgramUniform1f(m_tonemapProgram, 0, scene.exposure); // Set exposure
  glBindTextureUnit(0, m_resolveFramebuffer.colorTarget);
  glBindVertexArray(m_emptyVAO);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  if (view.splitScreen) {
    int splitWidth = static_cast<int>(m_framebuffer.width * view.splitPosition);
    glEnable(GL_SCISSOR_TEST);
    glScissor(splitWidth - 1, 0, 2, m_framebuffer.height);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);
	}
}

void Renderer::gui(GLFWwindow* window, ViewSettings& view, SceneSettings& scene)
{
	if (scene.isLoading) {
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
		ImGui::Begin("Loading", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
		
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetCursorPos(ImVec2(center.x - 200, center.y - 50));
		
		ImGui::BeginGroup();
		ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "MEMUAT ASET PBR...");
		ImGui::Dummy(ImVec2(0, 10));
		
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
		ImGui::ProgressBar(scene.loadingProgress, ImVec2(400, 30), "");
		ImGui::PopStyleColor();
		
		ImGui::Text("%s", scene.loadingStatus.c_str());
		ImGui::EndGroup();
		
		ImGui::End();
		return;
	}

	const ModelInfo& currentModel = m_availableModels[scene.currentModelIndex];

	// ============================================================
	// INTRO / WELCOME SCREEN
	// ============================================================
	if (scene.showIntro) {
		ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		ImVec2 winSize(700, 480);
		ImGui::SetNextWindowPos(ImVec2((displaySize.x - winSize.x) * 0.5f, (displaySize.y - winSize.y) * 0.5f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.95f);
		ImGui::Begin("## Intro", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);

		ImGui::SetCursorPosX((winSize.x - ImGui::CalcTextSize("PBR Renderer — Selamat Datang!").x) * 0.5f);
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "PBR Renderer -- Selamat Datang!");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[  MOUSE CONTROLS  ]");
		ImGui::BulletText("Klik Kiri + Drag    : Putar sudut kamera (orbit view)");
		ImGui::BulletText("Klik Kanan + Drag   : Putar objek di dunia (scene rotation)");
		ImGui::BulletText("Scroll Mouse        : Zoom in / zoom out");
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[  KEYBOARD CONTROLS  ]");
		ImGui::BulletText("W / S               : Putar objek naik / turun (pitch)");
		ImGui::BulletText("A / D               : Putar objek kiri / kanan (yaw)");
		ImGui::BulletText("SPACE               : Toggle Split-Screen (PBR vs Phong)");
		ImGui::BulletText("Arrow Left / Right  : Geser garis pemisah split-screen");
		ImGui::BulletText("F1 / F2 / F3        : Toggle lampu 1 / 2 / 3");
		ImGui::BulletText("1 / 2 / 3 / 4       : Toggle Albedo / Normal / Metalness / Roughness");
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "[  FITUR PANEL KONTROL  ]");
		ImGui::BulletText("Model Selection     : Ganti model 3D (Wheel / Cerberus / HDD)");
		ImGui::BulletText("Rendering Mode      : Split-screen & debug visualisasi");
		ImGui::BulletText("Material Components : Toggle peta Albedo/Normal/Metalness/Roughness");
		ImGui::BulletText("Light & Exposure    : Atur intensitas lampu & exposure");
		ImGui::BulletText("Texture Preview     : Pratinjau PBR texture maps");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float btnW = 180.0f;
		ImGui::SetCursorPosX((winSize.x - btnW) * 0.5f);
		if (ImGui::Button("Mulai Eksplorasi!", ImVec2(btnW, 36))) {
			scene.showIntro = false;
		}
		ImGui::End();
		return;
	}

	// ============================================================
	// MAIN CONTROL PANEL
	// ============================================================
	ImGui::Begin("Panel Kontrol");

	// --- ? Help button (small circle) in header area ---
	ImGui::SameLine(ImGui::GetWindowWidth() - 38);
	if (ImGui::Button("(?)", ImVec2(32, 22))) {
		scene.showHelp = !scene.showHelp;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Tampilkan panduan input interaksi");

	// ---- MODEL & HDR SELECTION ----
	if (ImGui::CollapsingHeader("Aset Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Model 3D:");
		for (int i = 0; i < (int)m_availableModels.size(); ++i) {
			if (ImGui::RadioButton(m_availableModels[i].name, &scene.currentModelIndex, i)) {
				scene.modelChanged = true;
			}
		}

		ImGui::Separator();

		ImGui::Text("Lingkungan HDR:");
		for (int i = 0; i < (int)m_availableHDRs.size(); ++i) {
			if (ImGui::RadioButton(m_availableHDRs[i].name, &scene.currentHDRIndex, i)) {
				scene.hdrChanged = true;
			}
		}
	}

	if (ImGui::CollapsingHeader("Mode Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Perbandingan Split-Screen", &view.splitScreen);

		int debugView = (int)scene.debugView;
		ImGui::Text("Visualisasi Debug:");
		ImGui::RadioButton("Tidak Ada", &debugView, (int)SceneSettings::DebugView::None);
		ImGui::RadioButton("Hanya Albedo", &debugView, (int)SceneSettings::DebugView::Albedo);
		ImGui::RadioButton("Hanya Normal", &debugView, (int)SceneSettings::DebugView::Normal);
		ImGui::RadioButton("Hanya Metalness", &debugView, (int)SceneSettings::DebugView::Metalness);
		ImGui::RadioButton("Hanya Roughness", &debugView, (int)SceneSettings::DebugView::Roughness);
		scene.debugView = (SceneSettings::DebugView)debugView;
	}

	if (ImGui::CollapsingHeader("Komponen Material", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Gunakan Albedo Map", &scene.useAlbedo);
		ImGui::Checkbox("Gunakan Normal Map", &scene.useNormalMap);
		ImGui::Checkbox("Gunakan Metalness Map", &scene.useMetalness);
		ImGui::Checkbox("Gunakan Roughness Map", &scene.useRoughness);
	}

	if (ImGui::CollapsingHeader("Pencahayaan & Eksposur", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Eksposur Skybox", &scene.exposure, 0.0f, 10.0f);
		ImGui::SliderFloat("Tingkat Kilap Phong", &scene.phongShininess, 1.0f, 256.0f);

		for (int i = 0; i < SceneSettings::NumLights; ++i) {
			char buf[32];
			std::sprintf(buf, "Lampu %d", i + 1);
			if (ImGui::TreeNode(buf)) {
				ImGui::Checkbox("Aktif", &scene.lights[i].enabled);
				float intensity = glm::length(scene.lights[i].radiance);
				glm::vec3 color = (intensity > 0.001f) ? (scene.lights[i].radiance / intensity) : glm::vec3(1.0f);
				if (ImGui::ColorEdit3("Warna", &color[0])) {
					scene.lights[i].radiance = color * intensity;
				}
				if (ImGui::DragFloat("Intensitas", &intensity, 0.1f, 0.0f, 100.0f)) {
					scene.lights[i].radiance = color * intensity;
				}
				ImGui::TreePop();
			}
		}
	}

	if (ImGui::CollapsingHeader("Pratinjau Tekstur (PiP)")) {
		float size = 120.0f;
		if (currentModel.albedo.id && ImGui::BeginTable("pip_table", 2)) {
			ImGui::TableNextColumn();
			ImGui::Text("Albedo");
			ImGui::Image((void*)(intptr_t)currentModel.albedo.id, ImVec2(size, size));

			ImGui::TableNextColumn();
			ImGui::Text("Normal");
			ImGui::Image((void*)(intptr_t)currentModel.normal.id, ImVec2(size, size));

			ImGui::TableNextColumn();
			ImGui::Text("Metalness");
			ImGui::Image((void*)(intptr_t)currentModel.metalness.id, ImVec2(size, size));

			ImGui::TableNextColumn();
			ImGui::Text("Roughness");
			ImGui::Image((void*)(intptr_t)currentModel.roughness.id, ImVec2(size, size));

			ImGui::EndTable();
		}
	}

	ImGui::End();

	// ============================================================
	// HELP POPUP (tombol ?)
	// ============================================================
	if (scene.showHelp) {
		ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		ImVec2 helpSize(420, 360);
		ImGui::SetNextWindowPos(ImVec2(displaySize.x - helpSize.x - 20, displaySize.y * 0.5f - helpSize.y * 0.5f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(helpSize, ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.90f);
		ImGui::Begin("Panduan Interaksi", &scene.showHelp, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "MOUSE");
		ImGui::BulletText("Klik Kiri + Drag  : Putar kamera");
		ImGui::BulletText("Klik Kanan + Drag : Putar objek");
		ImGui::BulletText("Scroll            : Zoom");
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "KEYBOARD");
		ImGui::BulletText("W/A/S/D           : Putar objek");
		ImGui::BulletText("SPACE             : Toggle Split-Screen");
		ImGui::BulletText("Arrow Left/Right  : Geser pemisah split");
		ImGui::BulletText("F1/F2/F3          : Toggle Lampu 1/2/3");
		ImGui::BulletText("1/2/3/4           : Toggle Albedo/Normal/Metal/Rough");
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "PANEL KONTROL");
		ImGui::BulletText("Model Selection   : Ganti model 3D");
		ImGui::BulletText("Rendering Mode    : Debug & split-screen");
		ImGui::BulletText("Material Comp.    : Toggle peta PBR");
		ImGui::BulletText("Light & Exposure  : Atur pencahayaan");
		ImGui::BulletText("Texture Preview   : Lihat PBR maps");
		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::Button("Tutup", ImVec2(-1, 0)))
			scene.showHelp = false;
		ImGui::End();
	}

	// Stats Overlay
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
	ImGui::Text("Performance: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();

	// Auto-Labeller for split-screen
	if (view.splitScreen) {
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		ImVec2 sz = ImGui::GetIO().DisplaySize;
		float splitX = sz.x * view.splitPosition;
		drawList->AddText(ImVec2(20, sz.y - 40), IM_COL32(255, 255, 255, 255), "SIDE A: PBR (Cook-Torrance)");
		drawList->AddText(ImVec2(splitX + 20, sz.y - 40), IM_COL32(255, 255, 255, 255), "SIDE B: Classic Phong");
	}
}

void Renderer::loadModel(int modelIndex)
{
	if (modelIndex < 0 || modelIndex >= (int)m_availableModels.size()) return;
	auto& info = m_availableModels[modelIndex];

	std::printf("Loading model: %s\n", info.name);

	auto mesh = Mesh::fromFile(info.meshPath);
	info.mesh = createMeshBuffer(mesh);

	info.albedo = createTexture(Image::fromFile(info.albedoPath, 3), GL_RGB, GL_SRGB8);
	info.normal = createTexture(Image::fromFile(info.normalPath, 3), GL_RGB, GL_RGB8);
	info.metalness = createTexture(Image::fromFile(info.metalnessPath, 1), GL_RED, GL_R8);
	info.roughness = createTexture(Image::fromFile(info.roughnessPath, 1), GL_RED, GL_R8);

	// Set swizzle mask for single-channel textures.
	GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
	glTextureParameteriv(info.metalness.id, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
	glTextureParameteriv(info.roughness.id, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

	// Calculate normalization matrix
	glm::vec3 min = mesh->min();
	glm::vec3 max = mesh->max();
	glm::vec3 size = max - min;
	glm::vec3 center = (min + max) * 0.5f;
	float maxDim = std::max({ size.x, size.y, size.z });
	float scale = (1.0f / maxDim) * info.scale; 

	info.normalization = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -center);

	// Special case for F1 Wheel: needs 90 deg rotation on X
	if (std::string(info.name).find("Wheel") != std::string::npos) {
		info.preRotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	}
	else {
		info.preRotation = glm::mat4(1.0f);
	}
}

void Renderer::loadHDREnvironment(int hdrIndex)
{
	if (hdrIndex < 0 || hdrIndex >= (int)m_availableHDRs.size()) return;
	auto& info = m_availableHDRs[hdrIndex];

	std::printf("Loading HDR environment: %s\n", info.name);

	GLuint equirectToCubeProgram = linkProgram({ compileShader("shaders/glsl/equirect2cube_cs.glsl", GL_COMPUTE_SHADER) });
	GLuint spmapProgram = linkProgram({ compileShader("shaders/glsl/spmap_cs.glsl", GL_COMPUTE_SHADER) });
	GLuint irmapProgram = linkProgram({ compileShader("shaders/glsl/irmap_cs.glsl", GL_COMPUTE_SHADER) });

	// Unfiltered environment cube map (temporary).
	Texture envTextureUnfiltered = createTexture(GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, GL_RGBA16F);

	// Convert equirectangular to cubemap
	Texture envTextureEquirect;
	if (info.cpuEquirect) {
		envTextureEquirect = createTexture(info.cpuEquirect, GL_RGB, GL_RGB16F, 1);
	}
	else {
		envTextureEquirect = createTexture(Image::fromFile(info.path, 3), GL_RGB, GL_RGB16F, 1);
	}
	glUseProgram(equirectToCubeProgram);
	glBindTextureUnit(0, envTextureEquirect.id);
	glBindImageTexture(0, envTextureUnfiltered.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
	glDispatchCompute(envTextureUnfiltered.width / 32, envTextureUnfiltered.height / 32, 6);
	glDeleteTextures(1, &envTextureEquirect.id);
	glGenerateTextureMipmap(envTextureUnfiltered.id);

	// Pre-filtering specular environment map
	info.env = createTexture(GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, GL_RGBA16F);
	glCopyImageSubData(envTextureUnfiltered.id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
		info.env.id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
		info.env.width, info.env.height, 6);
	glUseProgram(spmapProgram);
	glBindTextureUnit(0, envTextureUnfiltered.id);
	const float deltaRoughness = 1.0f / glm::max(float(info.env.levels - 1), 1.0f);
	for (int level = 1, size = kEnvMapSize / 2; level <= info.env.levels; ++level, size /= 2) {
		const GLuint numGroups = glm::max(1, size / 32);
		glBindImageTexture(0, info.env.id, level, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glProgramUniform1f(spmapProgram, 0, level * deltaRoughness);
		glDispatchCompute(numGroups, numGroups, 6);
	}
	glDeleteTextures(1, &envTextureUnfiltered.id);

	// Compute diffuse irradiance cubemap
	info.irmap = createTexture(GL_TEXTURE_CUBE_MAP, kIrradianceMapSize, kIrradianceMapSize, GL_RGBA16F, 1);
	glUseProgram(irmapProgram);
	glBindTextureUnit(0, info.env.id);
	glBindImageTexture(0, info.irmap.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
	glDispatchCompute(info.irmap.width / 32, info.irmap.height / 32, 6);

	glDeleteProgram(equirectToCubeProgram);
	glDeleteProgram(spmapProgram);
	glDeleteProgram(irmapProgram);
}

void Renderer::startAsyncLoad()
{
	m_isTerminating = false;
	m_loadIndex = 0;

	m_loadingThread = std::thread([this]() {
		for (auto& info : m_availableModels) {
			if (m_isTerminating) return;
			info.cpuMesh = Mesh::fromFile(info.meshPath);
			info.cpuAlbedo = Image::fromFile(info.albedoPath, 3);
			info.cpuNormal = Image::fromFile(info.normalPath, 3);
			info.cpuMetalness = Image::fromFile(info.metalnessPath, 1);
			info.cpuRoughness = Image::fromFile(info.roughnessPath, 1);
			info.cpuReady = true;
		}
		for (auto& info : m_availableHDRs) {
			if (m_isTerminating) return;
			info.cpuEquirect = Image::fromFile(info.path, 3);
			info.cpuReady = true;
		}
	});
}

void Renderer::processAsyncLoading(SceneSettings& scene)
{
	if (!scene.isLoading) return;

	int numModels = (int)m_availableModels.size();
	int numHDRs = (int)m_availableHDRs.size();
	int totalTasks = numModels + numHDRs;

	if (m_loadIndex < numModels) {
		auto& info = m_availableModels[m_loadIndex];
		if (info.cpuReady && !info.isLoaded) {
			// Upload to GPU (Main Thread)
			info.mesh = createMeshBuffer(info.cpuMesh);
			info.albedo = createTexture(info.cpuAlbedo, GL_RGB, GL_SRGB8);
			info.normal = createTexture(info.cpuNormal, GL_RGB, GL_RGB8);
			info.metalness = createTexture(info.cpuMetalness, GL_RED, GL_R8);
			info.roughness = createTexture(info.cpuRoughness, GL_RED, GL_R8);

			GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
			glTextureParameteriv(info.metalness.id, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
			glTextureParameteriv(info.roughness.id, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

			glm::vec3 min = info.cpuMesh->min();
			glm::vec3 max = info.cpuMesh->max();
			glm::vec3 size = max - min;
			glm::vec3 center = (min + max) * 0.5f;
			float maxDim = std::max({ size.x, size.y, size.z });
			float scale = (1.0f / maxDim) * info.scale;
			info.normalization = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -center);

			if (std::string(info.name).find("Wheel") != std::string::npos) {
				info.preRotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			}
			else {
				info.preRotation = glm::mat4(1.0f);
			}

			info.isLoaded = true;
			info.cpuMesh.reset();
			info.cpuAlbedo.reset();
			info.cpuNormal.reset();
			info.cpuMetalness.reset();
			info.cpuRoughness.reset();
			m_loadIndex++;
		}
	}
	else if (m_loadIndex < totalTasks) {
		int hdrIdx = m_loadIndex - numModels;
		auto& info = m_availableHDRs[hdrIdx];
		if (info.cpuReady && !info.isLoaded) {
			// Compute-heavy HDR tasks remain on main thread for GL context
			loadHDREnvironment(hdrIdx);
			info.isLoaded = true;
			info.cpuEquirect.reset();
			m_loadIndex++;
		}
	}

	scene.loadingProgress = (float)m_loadIndex / totalTasks;
	if (m_loadIndex >= totalTasks) {
		scene.isLoading = false;
		scene.loadingStatus = "Siap";
	}
	else {
		if (m_loadIndex < numModels)
			scene.loadingStatus = "Memproses Model: " + std::string(m_availableModels[m_loadIndex].name);
		else
			scene.loadingStatus = "Memproses HDR: " + std::string(m_availableHDRs[m_loadIndex - numModels].name);
	}
}

GLuint Renderer::compileShader(const std::string &filename, GLenum type) {
  const std::string src = File::readText(filename);
  if (src.empty()) {
    throw std::runtime_error("Cannot read shader source file: " + filename);
  }
  const GLchar *srcBufferPtr = src.c_str();

  std::printf("Compiling GLSL shader: %s\n", filename.c_str());

  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &srcBufferPtr, nullptr);
  glCompileShader(shader);

  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    GLsizei infoLogSize;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogSize);
    std::unique_ptr<GLchar[]> infoLog(new GLchar[infoLogSize]);
    glGetShaderInfoLog(shader, infoLogSize, nullptr, infoLog.get());
    throw std::runtime_error(std::string("Shader compilation failed: ") +
                             filename + "\n" + infoLog.get());
  }
  return shader;
}

GLuint Renderer::linkProgram(std::initializer_list<GLuint> shaders) {
  GLuint program = glCreateProgram();

  for (GLuint shader : shaders) {
    glAttachShader(program, shader);
  }
  glLinkProgram(program);
  for (GLuint shader : shaders) {
    glDetachShader(program, shader);
    glDeleteShader(shader);
  }

  GLint status;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (status == GL_TRUE) {
    glValidateProgram(program);
    glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
  }
  if (status != GL_TRUE) {
    GLsizei infoLogSize;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogSize);
    std::unique_ptr<GLchar[]> infoLog(new GLchar[infoLogSize]);
    glGetProgramInfoLog(program, infoLogSize, nullptr, infoLog.get());
    throw std::runtime_error(std::string("Program link failed\n") +
                             infoLog.get());
  }
  return program;
}

Texture Renderer::createTexture(GLenum target, int width, int height,
                                GLenum internalformat, int levels) const {
  Texture texture;
  texture.width = width;
  texture.height = height;
  texture.levels =
      (levels > 0) ? levels : Utility::numMipmapLevels(width, height);

  glCreateTextures(target, 1, &texture.id);
  glTextureStorage2D(texture.id, texture.levels, internalformat, width, height);
  glTextureParameteri(texture.id, GL_TEXTURE_MIN_FILTER,
                      texture.levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTextureParameteri(texture.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameterf(texture.id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                      m_capabilities.maxAnisotropy);
  return texture;
}

Texture Renderer::createTexture(const std::shared_ptr<class Image> &image,
                                GLenum format, GLenum internalformat,
                                int levels) const {
  Texture texture = createTexture(GL_TEXTURE_2D, image->width(),
                                  image->height(), internalformat, levels);
  if (image->isHDR()) {
    glTextureSubImage2D(texture.id, 0, 0, 0, texture.width, texture.height,
                        format, GL_FLOAT, image->pixels<float>());
  } else {
    glTextureSubImage2D(texture.id, 0, 0, 0, texture.width, texture.height,
                        format, GL_UNSIGNED_BYTE,
                        image->pixels<unsigned char>());
  }

  if (texture.levels > 1) {
    glGenerateTextureMipmap(texture.id);
  }
  return texture;
}

void Renderer::deleteTexture(Texture &texture) {
  glDeleteTextures(1, &texture.id);
  std::memset(&texture, 0, sizeof(Texture));
}

FrameBuffer Renderer::createFrameBuffer(int width, int height, int samples,
                                        GLenum colorFormat,
                                        GLenum depthstencilFormat) {
  FrameBuffer fb;
  fb.width = width;
  fb.height = height;
  fb.samples = samples;

  glCreateFramebuffers(1, &fb.id);

  if (colorFormat != GL_NONE) {
    if (samples > 0) {
      glCreateRenderbuffers(1, &fb.colorTarget);
      glNamedRenderbufferStorageMultisample(fb.colorTarget, samples,
                                            colorFormat, width, height);
      glNamedFramebufferRenderbuffer(fb.id, GL_COLOR_ATTACHMENT0,
                                     GL_RENDERBUFFER, fb.colorTarget);
    } else {
      glCreateTextures(GL_TEXTURE_2D, 1, &fb.colorTarget);
      glTextureStorage2D(fb.colorTarget, 1, colorFormat, width, height);
      glNamedFramebufferTexture(fb.id, GL_COLOR_ATTACHMENT0, fb.colorTarget, 0);
    }
  }
  if (depthstencilFormat != GL_NONE) {
    glCreateRenderbuffers(1, &fb.depthStencilTarget);
    if (samples > 0) {
      glNamedRenderbufferStorageMultisample(fb.depthStencilTarget, samples,
                                            depthstencilFormat, width, height);
    } else {
      glNamedRenderbufferStorage(fb.depthStencilTarget, depthstencilFormat,
                                 width, height);
    }
    glNamedFramebufferRenderbuffer(fb.id, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_RENDERBUFFER, fb.depthStencilTarget);
  }

  GLenum status = glCheckNamedFramebufferStatus(fb.id, GL_DRAW_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    throw std::runtime_error("Framebuffer completeness check failed: " +
                             std::to_string(status));
  }
  return fb;
}

void Renderer::resolveFramebuffer(const FrameBuffer &srcfb,
                                  const FrameBuffer &dstfb) {
  if (srcfb.id == dstfb.id) {
    return;
  }

  std::vector<GLenum> attachments;
  if (srcfb.colorTarget) {
    attachments.push_back(GL_COLOR_ATTACHMENT0);
  }
  if (srcfb.depthStencilTarget) {
    attachments.push_back(GL_DEPTH_STENCIL_ATTACHMENT);
  }
  assert(attachments.size() > 0);

  glBlitNamedFramebuffer(srcfb.id, dstfb.id, 0, 0, srcfb.width, srcfb.height, 0,
                         0, dstfb.width, dstfb.height, GL_COLOR_BUFFER_BIT,
                         GL_NEAREST);
  glInvalidateNamedFramebufferData(srcfb.id, (GLsizei)attachments.size(),
                                   &attachments[0]);
}

void Renderer::deleteFrameBuffer(FrameBuffer &fb) {
  if (fb.id) {
    glDeleteFramebuffers(1, &fb.id);
  }
  if (fb.colorTarget) {
    if (fb.samples == 0) {
      glDeleteTextures(1, &fb.colorTarget);
    } else {
      glDeleteRenderbuffers(1, &fb.colorTarget);
    }
  }
  if (fb.depthStencilTarget) {
    glDeleteRenderbuffers(1, &fb.depthStencilTarget);
  }
  std::memset(&fb, 0, sizeof(FrameBuffer));
}

MeshBuffer Renderer::createMeshBuffer(const std::shared_ptr<class Mesh> &mesh) {
  MeshBuffer buffer;
  buffer.numElements = static_cast<GLuint>(mesh->faces().size()) * 3;

  const size_t vertexDataSize = mesh->vertices().size() * sizeof(Mesh::Vertex);
  const size_t indexDataSize = mesh->faces().size() * sizeof(Mesh::Face);

  glCreateBuffers(1, &buffer.vbo);
  glNamedBufferStorage(buffer.vbo, vertexDataSize,
                       reinterpret_cast<const void *>(&mesh->vertices()[0]), 0);
  glCreateBuffers(1, &buffer.ibo);
  glNamedBufferStorage(buffer.ibo, indexDataSize,
                       reinterpret_cast<const void *>(&mesh->faces()[0]), 0);

  glCreateVertexArrays(1, &buffer.vao);
  glVertexArrayElementBuffer(buffer.vao, buffer.ibo);
  for (int i = 0; i < Mesh::NumAttributes; ++i) {
    glVertexArrayVertexBuffer(buffer.vao, i, buffer.vbo, i * sizeof(glm::vec3),
                              sizeof(Mesh::Vertex));
    glEnableVertexArrayAttrib(buffer.vao, i);
    glVertexArrayAttribFormat(buffer.vao, i,
                              i == (Mesh::NumAttributes - 1) ? 2 : 3, GL_FLOAT,
                              GL_FALSE, 0);
    glVertexArrayAttribBinding(buffer.vao, i, i);
  }
  return buffer;
}

void Renderer::deleteMeshBuffer(MeshBuffer &buffer) {
  if (buffer.vao) {
    glDeleteVertexArrays(1, &buffer.vao);
  }
  if (buffer.vbo) {
    glDeleteBuffers(1, &buffer.vbo);
  }
  if (buffer.ibo) {
    glDeleteBuffers(1, &buffer.ibo);
  }
  std::memset(&buffer, 0, sizeof(MeshBuffer));
}

GLuint Renderer::createUniformBuffer(const void *data, size_t size) {
  GLuint ubo;
  glCreateBuffers(1, &ubo);
  glNamedBufferStorage(ubo, size, data, GL_DYNAMIC_STORAGE_BIT);
  return ubo;
}

#if _DEBUG
void Renderer::logMessage(GLenum source, GLenum type, GLuint id,
                          GLenum severity, GLsizei length,
                          const GLchar *message, const void *userParam) {
  if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
    std::fprintf(stderr, "GL: %s\n", message);
  }
}
#endif

} // namespace OpenGL
