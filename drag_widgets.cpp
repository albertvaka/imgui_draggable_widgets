#define IMGUI_DEFINE_MATH_OPERATORS
#include "drag_widgets.h"
#include "imgui_internal.h"
#include <SDL.h>
#include <math.h>

namespace {

// Fraction of the current order of magnitude that one pixel of drag motion represents.
// e.g. with MAGNITUDE_PER_PIXEL = 0.01, dragging 100 pixels changes the value by
// one full order of magnitude (1 -> 2, or 100 -> 200).
const double MAGNITUDE_PER_PIXEL = 0.01;
const double SHIFT_SLOWDOWN = 0.1;

struct DragState
{
    SDL_Window* Window = nullptr;

    bool        Dragging = false;      // true once the drag threshold has been exceeded and relative mouse mode engaged
    double      Accum = 0.0;           // pending fractional value change not yet applied (kept across frames for smooth sub-step accumulation)
    float       RelDeltaAccum = 0.0f;  // relative mouse motion accumulated since last consumed
    int         StartMouseX = 0;
    int         StartMouseY = 0;
};

DragState g_ds;

// Returns a drag-speed scale for a value of the given magnitude.
// - Scaling by |v| directly (like Blender's PROP_SCALE_LINEAR) makes the drag distance needed
//   to double the value grow fully proportionally with it (100->200 needs 100x the distance
//   1->2 needs).
// - Scaling exponentially, i.e. keeping the scale constant (like Blender's PROP_SCALE_LOG),
//   makes that distance constant everywhere (1->2 takes the same distance as 100->200).
// sqrt(|v|) sits halfway between those: the required distance still grows with the value,
// but only at half the rate. It's a continuous function of the value (no snapping to decade
// boundaries), so sensitivity changes smoothly as you drag instead of jumping.
double ComputeMagnitude(double v)
{
    return sqrt(ImMax(fabs(v), 1.0));
}

void StartDrag()
{
    g_ds.Dragging = true;
    g_ds.Accum = 0.0;
    g_ds.RelDeltaAccum = 0.0f;
    if (g_ds.Window)
    {
        SDL_GetMouseState(&g_ds.StartMouseX, &g_ds.StartMouseY);
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
}

void EndDrag()
{
    if (g_ds.Dragging && g_ds.Window)
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_WarpMouseInWindow(g_ds.Window, g_ds.StartMouseX, g_ds.StartMouseY);
    }
    g_ds.Dragging = false;
    g_ds.RelDeltaAccum = 0.0f;
}

// Applies one frame of drag input to *p_data (float or int). Returns true if the value changed.
// Mirrors the structure of ImGui::DragBehavior(), but with magnitude-proportional speed,
// Shift for finer control, Ctrl to snap floats to whole numbers, and infinite (relative) mouse motion.
bool ApplyDragBehavior(ImGuiID id, ImGuiDataType data_type, void* p_data, float speed, double v_min, double v_max, bool has_range, const char* format)
{
    ImGuiContext& g = *GImGui;
    if (g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_Mouse && !g.IO.MouseDown[0])
    {
        EndDrag();
        ImGui::ClearActiveID();
    }
    if (g.ActiveId != id)
        return false;

    const bool is_float = (data_type == ImGuiDataType_Float);
    const double v_old = is_float ? (double)*(float*)p_data : (double)*(int*)p_data;

    if (!g_ds.Dragging)
    {
        // Still just a (potential) click: don't engage relative-mouse drag mode until
        // the user has actually moved the mouse past the click threshold.
        if (!ImGui::IsMouseDragPastThreshold(0, g.IO.MouseDragThreshold * 1.0f))
            return false;
        StartDrag();
    }

    const float dx = g_ds.RelDeltaAccum;
    g_ds.RelDeltaAccum = 0.0f;
    if (dx == 0.0f)
        return false;

    double sensitivity = MAGNITUDE_PER_PIXEL * (double)speed;
    if (g.IO.KeyShift)
        sensitivity *= SHIFT_SLOWDOWN;

    const double magnitude = ComputeMagnitude(v_old);
    g_ds.Accum += (double)dx * magnitude * sensitivity;

    double v_target = v_old + g_ds.Accum;
    if (has_range)
    {
        if (v_target < v_min) v_target = v_min;
        if (v_target > v_max) v_target = v_max;
    }

    double v_new;
    if (is_float)
        v_new = g.IO.KeyCtrl ? floor(v_target + 0.5) : (double)ImGui::RoundScalarWithFormatT<float>(format, ImGuiDataType_Float, (float)v_target);
    else
        v_new = floor(v_target + 0.5);

    // Keep the leftover fractional part so slow drags still accumulate smoothly.
    g_ds.Accum -= (v_new - v_old);

    if (v_new == v_old)
        return false;

    if (is_float)
        *(float*)p_data = (float)v_new;
    else
        *(int*)p_data = (int)v_new;
    return true;
}

// The clickable/draggable/editable value box itself (without the side arrows).
bool DragBox(const char* str_id, const ImRect& bb, ImGuiDataType data_type, void* p_data, float speed, double v_min, double v_max, bool has_range, const char* format)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(str_id);

    if (!ImGui::ItemAdd(bb, id, &bb, ImGuiItemFlags_Inputable))
        return false;

    float clamp_min_f = (float)v_min, clamp_max_f = (float)v_max;
    int clamp_min_i = (int)v_min, clamp_max_i = (int)v_max;
    const void* p_clamp_min = has_range ? (data_type == ImGuiDataType_Float ? (const void*)&clamp_min_f : (const void*)&clamp_min_i) : nullptr;
    const void* p_clamp_max = has_range ? (data_type == ImGuiDataType_Float ? (const void*)&clamp_max_f : (const void*)&clamp_max_i) : nullptr;

    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    bool hovered = false;
    if (!temp_input_is_active)
    {
        hovered = ImGui::ItemHoverable(bb, id, g.LastItemData.ItemFlags);
        const bool clicked = hovered && ImGui::IsMouseClicked(0, ImGuiInputFlags_None, id);
        const bool double_clicked = (hovered && g.IO.MouseClickedCount[0] == 2 && ImGui::TestKeyOwner(ImGuiKey_MouseLeft, id));
        const bool make_active = (clicked || double_clicked || g.NavActivateId == id);

        if (make_active && (clicked || double_clicked))
            ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);

        if (make_active && !temp_input_is_active)
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
        }

        if (double_clicked || (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput)))
            temp_input_is_active = true;

        // A plain click released without dragging turns the field into a text input, like Blender (a click without a drag enters text-edit mode there too).
        if (g.ActiveId == id && hovered && g.IO.MouseReleased[0] && !g_ds.Dragging)
        {
            g.NavActivateId = id;
            g.NavActivateFlags = ImGuiActivateFlags_PreferInput;
            temp_input_is_active = true;
        }
    }

    if (temp_input_is_active)
        return ImGui::TempInputScalar(bb, id, str_id, data_type, p_data, format, p_clamp_min, p_clamp_max);

    const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImGui::RenderNavCursor(bb, id);
    ImGui::RenderFrame(bb.Min, bb.Max, frame_col, false, 0.0f);

    const bool value_changed = ApplyDragBehavior(id, data_type, p_data, speed, v_min, v_max, has_range, format);
    if (value_changed)
        ImGui::MarkItemEdited(id);

    char value_buf[64];
    const char* value_buf_end = value_buf + ImGui::DataTypeFormatString(value_buf, IM_COUNTOF(value_buf), data_type, p_data, format);
    ImGui::RenderTextClipped(bb.Min, bb.Max, value_buf, value_buf_end, nullptr, ImVec2(0.5f, 0.5f));

    return value_changed;
}

bool DragScalarEx(const char* label, ImGuiDataType data_type, void* p_data, float speed, double v_min, double v_max, bool has_range, const char* format, double step)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float w = ImGui::CalcItemWidth();
    const float btn_size = g.FontSize + style.FramePadding.y * 2.0f;
    const float box_w = ImMax(w - btn_size * 2.0f, 1.0f);

    ImGui::PushID(label);
    ImGui::BeginGroup();

    bool changed = false;

    auto apply_step = [&](int dir) {
        const bool is_float = (data_type == ImGuiDataType_Float);
        double v_old = is_float ? (double)*(float*)p_data : (double)*(int*)p_data;
        double s = step;
        if (g.IO.KeyShift)
            s *= 0.1;
        double v_new = v_old + dir * s;
        if (has_range)
        {
            if (v_new < v_min) v_new = v_min;
            if (v_new > v_max) v_new = v_max;
        }
        if (is_float)
        {
            v_new = g.IO.KeyCtrl ? floor(v_new + 0.5) : (double)ImGui::RoundScalarWithFormatT<float>(format, ImGuiDataType_Float, (float)v_new);
            if (v_new != v_old) *(float*)p_data = (float)v_new;
        }
        else
        {
            v_new = floor(v_new + 0.5);
            if (v_new != v_old) *(int*)p_data = (int)v_new;
        }
        changed |= (v_new != v_old);
    };

    ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
    if (ImGui::ArrowButtonEx("##l", ImGuiDir_Left, ImVec2(btn_size, btn_size), ImGuiButtonFlags_None))
        apply_step(-1);
    ImGui::SameLine(0.0f, 0.0f);

    const ImVec2 box_pos = window->DC.CursorPos;
    const ImRect box_bb(box_pos, box_pos + ImVec2(box_w, btn_size));
    ImGui::ItemSize(box_bb.GetSize());
    changed |= DragBox("##box", box_bb, data_type, p_data, speed, v_min, v_max, has_range, format);

    ImGui::SameLine(0.0f, 0.0f);
    if (ImGui::ArrowButtonEx("##r", ImGuiDir_Right, ImVec2(btn_size, btn_size), ImGuiButtonFlags_None))
        apply_step(1);
    ImGui::PopItemFlag();

    ImGui::PopID();

    const char* label_end = ImGui::FindRenderedTextEnd(label);
    if (label != label_end)
    {
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::TextEx(label, label_end);
    }

    ImGui::EndGroup();
    return changed;
}

} // anonymous namespace

void DragWidgets_Init(SDL_Window* window)
{
    g_ds.Window = window;
}

void DragWidgets_ProcessEvent(const SDL_Event& event)
{
    if (g_ds.Dragging && event.type == SDL_MOUSEMOTION)
        g_ds.RelDeltaAccum += (float)event.motion.xrel;
}

bool DragFloatEx(const char* label, float* v, float speed, float v_min, float v_max, const char* format, float step)
{
    return DragScalarEx(label, ImGuiDataType_Float, v, speed, (double)v_min, (double)v_max, v_min < v_max, format, (double)step);
}

bool DragIntEx(const char* label, int* v, float speed, int v_min, int v_max, const char* format, int step)
{
    return DragScalarEx(label, ImGuiDataType_S32, v, speed, (double)v_min, (double)v_max, v_min < v_max, format, (double)step);
}
