#pragma once

// Small additions to imgui-node-editor that need its internals.

#include "imgui.h"
#include "imgui_node_editor.h"

namespace NodeEditorView {
    // Like ax::NodeEditor::NavigateToContent(), but never zooms in past about 100 %: the view is at
    // least `min_view` (canvas units) in size, so a mission with one or two nodes isn't blown up.
    // Call between ed::Begin() and ed::End().
    void FitContent(ax::NodeEditor::EditorContext* editor, ImVec2 min_view, float duration = 0.0f);
}
