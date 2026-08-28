#include "drag_widgets.h"
#include <SDL.h>

namespace
{
struct InfiniteDragState
{
    SDL_Window* window = nullptr;
    bool dragging = false;
    float relative_dx = 0.0f;
    int restore_x = 0, restore_y = 0;
};

InfiniteDragState g_drag;

void EndDrag()
{
    if (!g_drag.dragging)
        return;

    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_WarpMouseInWindow(g_drag.window, g_drag.restore_x, g_drag.restore_y);
    g_drag.dragging = false;
    g_drag.relative_dx = 0.0f;
}
} // namespace

void DragWidgets_Init(SDL_Window* window)
{
    g_drag.window = window;
}

void DragWidgets_ProcessEvent(const SDL_Event& event)
{
    if (!g_drag.dragging)
        return;

    if (event.type == SDL_MOUSEMOTION)
        g_drag.relative_dx += (float)event.motion.xrel;
    else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
        EndDrag();
}

bool DragFloatInfinite(const char* label, float* v, float speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    ImGuiIO& io = ImGui::GetIO();

    // DragFloat already has the desired activation, clamping, precision and rendering.
    // While in SDL relative mode, substitute its horizontal mouse delta with SDL's xrel.
    const float mouse_delta_x = io.MouseDelta.x;
    if (g_drag.dragging)
    {
        io.MouseDelta.x = g_drag.relative_dx;
        g_drag.relative_dx = 0.0f;
    }
    const bool changed = ImGui::DragFloat(label, v, speed, v_min, v_max, format, flags);
    io.MouseDelta.x = mouse_delta_x;

    // Do not enter relative mode until DragFloat's normal drag threshold has been crossed.
    if (!g_drag.dragging && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        SDL_GetMouseState(&g_drag.restore_x, &g_drag.restore_y);
        if (SDL_SetRelativeMouseMode(SDL_TRUE) == 0)
        {
            g_drag.dragging = true;
            g_drag.relative_dx = 0.0f; // discard motion preceding the mode switch
        }
    }
    else if (g_drag.dragging && ImGui::IsItemDeactivated())
    {
        EndDrag();
    }

    return changed;
}
