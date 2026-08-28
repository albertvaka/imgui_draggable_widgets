// Draggable numeric input widgets for Dear ImGui.
//
// Click to type a value, use the small arrows on the sides to step it,
// or click-drag anywhere on the field to change it. Drag speed grows with
// the square root of the value's magnitude, so bigger numbers move faster
// but not fully proportionally. Hold Shift while dragging for finer control,
// hold Ctrl while dragging a float to snap it to whole numbers. The mouse
// cursor is locked in place for the duration of the drag so it never hits
// the screen edge.
#pragma once

#include "imgui.h"

struct SDL_Window;
union SDL_Event;

// Call once after creating the SDL window, before using DragFloatEx/DragIntEx.
void DragWidgets_Init(SDL_Window* window);

// Call for every SDL event polled by the application (e.g. right after
// ImGui_ImplSDL2_ProcessEvent), so drags can track raw relative mouse motion.
void DragWidgets_ProcessEvent(const SDL_Event& event);

// v_min == v_max (with v_min == 0) means "no bounds", matching ImGui::DragFloat/DragInt conventions.
bool DragFloatEx(const char* label, float* v, float speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", float step = 1.0f);
bool DragIntEx(const char* label, int* v, float speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d", int step = 1);
