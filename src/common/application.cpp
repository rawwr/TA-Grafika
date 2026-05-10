/*
 * Physically Based Rendering
 * Copyright (c) 2017-2018 Michał Siejak
 */

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/quaternion.hpp>

#include "application.hpp"

namespace {
const int DisplaySizeX = 1920;
const int DisplaySizeY = 1080;
const int DisplaySamples = 16;

const float ViewDistance = 150.0f;
const float ViewFOV = 60.0f;
const float OrbitSpeed = 0.5f; // Reduced for smoother, more precise control
const float ZoomSpeed = 0.20f; // Proportional to distance (20% per scroll tick)
const float KeyboardRotationSpeed = 5.0f; // Rotation speed for WASD
const float SplitMoveSpeed = 0.02f; // Small discrete step for keyboard control
} // namespace

Application::Application()
    : m_window(nullptr), m_prevCursorX(0.0), m_prevCursorY(0.0),
      m_mode(InputMode::None) {
  if (!glfwInit()) {
    throw std::runtime_error("Failed to initialize GLFW library");
  }

  m_viewSettings.distance = ViewDistance;
  m_viewSettings.fov = ViewFOV;
  m_viewSettings.pitch = 0.0f;
  m_viewSettings.yaw = 0.0f;

  m_sceneSettings.lights[0].direction =
      glm::normalize(glm::vec3{-1.0f, 0.0f, 0.0f});
  m_sceneSettings.lights[1].direction =
      glm::normalize(glm::vec3{1.0f, 0.0f, 0.0f});
  m_sceneSettings.lights[2].direction =
      glm::normalize(glm::vec3{0.0f, -1.0f, 0.0f});

  m_sceneSettings.lights[0].radiance = glm::vec3{1.0f};
  m_sceneSettings.lights[1].radiance = glm::vec3{1.0f};
  m_sceneSettings.lights[2].radiance = glm::vec3{1.0f};

  for (int i = 0; i < SceneSettings::NumLights; ++i) {
    m_sceneSettings.lights[i].enabled = false; // Lights off by default
  }

  m_sceneSettings.exposure = 1.0f;
  m_sceneSettings.phongShininess = 16.0f;
}

Application::~Application() {
  if (m_window) {
    glfwDestroyWindow(m_window);
  }
  glfwTerminate();
}

void Application::run(const std::unique_ptr<RendererInterface> &renderer) {
  glfwWindowHint(GLFW_RESIZABLE, 0);
  m_window = renderer->initialize(DisplaySizeX, DisplaySizeY, DisplaySamples);

  glfwSetWindowUserPointer(m_window, this);
  glfwSetCursorPosCallback(m_window, Application::mousePositionCallback);
  glfwSetMouseButtonCallback(m_window, Application::mouseButtonCallback);
  glfwSetScrollCallback(m_window, Application::mouseScrollCallback);
  glfwSetKeyCallback(m_window, Application::keyCallback);

  renderer->setup();

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(m_window, true);
  ImGui_ImplOpenGL3_Init("#version 450");

  while (!glfwWindowShouldClose(m_window)) {
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    renderer->render(m_window, m_viewSettings, m_sceneSettings);
    renderer->gui(m_window, m_viewSettings, m_sceneSettings);

    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  renderer->shutdown();
}

void Application::mousePositionCallback(GLFWwindow *window, double xpos,
                                        double ypos) {
  Application *self =
      reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
  if (ImGui::GetIO().WantCaptureMouse)
    return;

  if (self->m_mode != InputMode::None) {
    const double dx = xpos - self->m_prevCursorX;
    const double dy = ypos - self->m_prevCursorY;

    switch (self->m_mode) {
    case InputMode::RotatingScene: {
      glm::mat4 viewRot = glm::eulerAngleYX(glm::radians(self->m_viewSettings.yaw), glm::radians(self->m_viewSettings.pitch));
      glm::vec3 camRight = glm::vec3(viewRot[0]);
      glm::vec3 camUp = glm::vec3(viewRot[1]);
      
      glm::quat qY = glm::angleAxis(glm::radians(float(-dx) * OrbitSpeed), camUp);
      glm::quat qX = glm::angleAxis(glm::radians(float(-dy) * OrbitSpeed), camRight);
      
      self->m_sceneSettings.rotation = qY * qX * self->m_sceneSettings.rotation;
      self->m_sceneSettings.rotation = glm::normalize(self->m_sceneSettings.rotation);
      break;
    }
    case InputMode::RotatingView: {
      // Natural orbit camera: drag right = rotate right, drag down = look down
      self->m_viewSettings.yaw   += OrbitSpeed * float(dx);
      self->m_viewSettings.pitch -= OrbitSpeed * float(dy); // Invert: drag down = look down (negative pitch)
      // Clamp pitch to prevent camera flipping over
      self->m_viewSettings.pitch = glm::clamp(self->m_viewSettings.pitch, -89.0f, 89.0f);
      break;
    }
    }

    self->m_prevCursorX = xpos;
    self->m_prevCursorY = ypos;
  }
}

void Application::mouseButtonCallback(GLFWwindow *window, int button,
                                      int action, int mods) {
  Application *self =
      reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
  if (ImGui::GetIO().WantCaptureMouse)
    return;

  const InputMode oldMode = self->m_mode;
  if (action == GLFW_PRESS && self->m_mode == InputMode::None) {
    switch (button) {
    case GLFW_MOUSE_BUTTON_1:
      self->m_mode = InputMode::RotatingView;
      break;
    }
  }
  if (action == GLFW_RELEASE && button == GLFW_MOUSE_BUTTON_1) {
    self->m_mode = InputMode::None;
  }

  if (oldMode != self->m_mode) {
    if (self->m_mode == InputMode::None) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      glfwGetCursorPos(window, &self->m_prevCursorX, &self->m_prevCursorY);
    }
  }
}

void Application::mouseScrollCallback(GLFWwindow *window, double xoffset,
                                      double yoffset) {
  Application *self =
      reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
  if (ImGui::GetIO().WantCaptureMouse)
    return;

  // Zoom proporsional terhadap jarak saat ini agar konsisten di semua level
  // zoom
  self->m_viewSettings.distance +=
      ZoomSpeed * self->m_viewSettings.distance * float(-yoffset);
  if (self->m_viewSettings.distance < 1.0f)
    self->m_viewSettings.distance = 1.0f;
}

void Application::keyCallback(GLFWwindow *window, int key, int scancode,
                              int action, int mods) {
  Application *self =
      reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
  if (ImGui::GetIO().WantCaptureKeyboard)
    return;

  if (action == GLFW_PRESS || action == GLFW_REPEAT) {
    SceneSettings::Light *light = nullptr;

    switch (key) {
    // WASD object rotation - simple and intuitive
    case GLFW_KEY_W:
    case GLFW_KEY_S:
    case GLFW_KEY_A:
    case GLFW_KEY_D: {
      // Use world-space axes for consistent, predictable rotation
      glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
      glm::vec3 worldRight = glm::vec3(1.0f, 0.0f, 0.0f);
      
      // Simple rotations: W/S tilt forward/back, A/D spin left/right
      if (key == GLFW_KEY_W) {
        // Tilt forward (top goes away)
        glm::quat rot = glm::angleAxis(glm::radians(-KeyboardRotationSpeed), worldRight);
        self->m_sceneSettings.rotation = rot * self->m_sceneSettings.rotation;
      } else if (key == GLFW_KEY_S) {
        // Tilt backward (top comes toward)
        glm::quat rot = glm::angleAxis(glm::radians(KeyboardRotationSpeed), worldRight);
        self->m_sceneSettings.rotation = rot * self->m_sceneSettings.rotation;
      } else if (key == GLFW_KEY_A) {
        // Spin counter-clockwise (left)
        glm::quat rot = glm::angleAxis(glm::radians(KeyboardRotationSpeed), worldUp);
        self->m_sceneSettings.rotation = rot * self->m_sceneSettings.rotation;
      } else if (key == GLFW_KEY_D) {
        // Spin clockwise (right)
        glm::quat rot = glm::angleAxis(glm::radians(-KeyboardRotationSpeed), worldUp);
        self->m_sceneSettings.rotation = rot * self->m_sceneSettings.rotation;
      }
      self->m_sceneSettings.rotation = glm::normalize(self->m_sceneSettings.rotation);
      break;
    }
    // Arrow keys for split screen position (discrete steps)
    case GLFW_KEY_LEFT:
      if (self->m_viewSettings.splitScreen) {
        self->m_viewSettings.splitPosition -= SplitMoveSpeed;
        if (self->m_viewSettings.splitPosition < 0.05f)
          self->m_viewSettings.splitPosition = 0.05f;
      }
      break;
    case GLFW_KEY_RIGHT:
      if (self->m_viewSettings.splitScreen) {
        self->m_viewSettings.splitPosition += SplitMoveSpeed;
        if (self->m_viewSettings.splitPosition > 0.95f)
          self->m_viewSettings.splitPosition = 0.95f;
      }
      break;
    // Light toggles
    case GLFW_KEY_F1:
      light = &self->m_sceneSettings.lights[0];
      break;
    case GLFW_KEY_F2:
      light = &self->m_sceneSettings.lights[1];
      break;
    case GLFW_KEY_F3:
      light = &self->m_sceneSettings.lights[2];
      break;
    // Texture toggles
    case GLFW_KEY_1:
      self->m_sceneSettings.useAlbedo = !self->m_sceneSettings.useAlbedo;
      break;
    case GLFW_KEY_2:
      self->m_sceneSettings.useNormalMap = !self->m_sceneSettings.useNormalMap;
      break;
    case GLFW_KEY_3:
      self->m_sceneSettings.useMetalness = !self->m_sceneSettings.useMetalness;
      break;
    case GLFW_KEY_4:
      self->m_sceneSettings.useRoughness = !self->m_sceneSettings.useRoughness;
      break;
    case GLFW_KEY_SPACE:
      self->m_viewSettings.splitScreen = !self->m_viewSettings.splitScreen;
      break;
    }

    if (light) {
      light->enabled = !light->enabled;
    }
  }
}
