// Copyright (C) 2026 Koh Swee Teck Dedrick. All rights reserved.

#include "imgui.cpp"
#include "imgui_demo.cpp"
#include "imgui_draw.cpp"
#include "imgui_impl_dx11.cpp"
#include "imgui_tables.cpp"
#include "imgui_widgets.cpp"

#include "desktop/desktop_rgfw.hpp"

#define RGFW_IMGUI_IMPLEMENTATION
#pragma warning(push, 3)
#include "imgui_impl_rgfw.h"
#pragma warning(pop)
#undef RGFW_IMGUI_IMPLEMENTATION
