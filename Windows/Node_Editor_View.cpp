// The internal header must come first: it enables ImGui's vector math operators before imgui.h.
#include "imgui_node_editor_internal.h"

#include "Node_Editor_View.h"

namespace NodeEditorView {
    void FitContent(ax::NodeEditor::EditorContext* editor, ImVec2 min_view, float duration)
    {
        auto* ctx = reinterpret_cast<ax::NodeEditor::Detail::EditorContext*>(editor);
        ImRect bounds = ctx->GetContentBounds();
        if (bounds.GetWidth() <= 0.0f || bounds.GetHeight() <= 0.0f)
            return;
        // Grow the target to the minimum view (keeping the content centered); the editor then adds its margin.
        const ImVec2 c = bounds.GetCenter();
        const float w = ImMax(bounds.GetWidth(), min_view.x * 0.95f);
        const float h = ImMax(bounds.GetHeight(), min_view.y * 0.95f);
        bounds = ImRect(ImVec2(c.x - w * 0.5f, c.y - h * 0.5f), ImVec2(c.x + w * 0.5f, c.y + h * 0.5f));
        ctx->NavigateTo(bounds, true, duration);
    }
}
