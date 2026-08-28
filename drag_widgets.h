#pragma once

#include "imgui.h"

struct SDL_Window;
union SDL_Event;

// Call once after creating the SDL window, then forward every SDL event.
void DragWidgets_Init(SDL_Window* window);
void DragWidgets_ProcessEvent(const SDL_Event& event);

// ImGui::DragFloat() with unbounded horizontal mouse motion.
bool DragFloatInfinite(const char* label, float* v, float speed = 1.0f,
                       float v_min = 0.0f, float v_max = 0.0f,
                       const char* format = "%.3f",
                       ImGuiSliderFlags flags = 0);
