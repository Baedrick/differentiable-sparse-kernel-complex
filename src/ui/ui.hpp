// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef UI_HPP__
#define UI_HPP__

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_internal.h"
#include "thirdparty/imgui/imgui_impl_dx11.h"
#include "thirdparty/imgui/imgui_impl_rgfw.h"

namespace ImGui {
	inline void PushDisabled() {
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	inline void PopDisabled() {
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
}

#endif // UI_HPP__
