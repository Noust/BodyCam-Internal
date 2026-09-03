#pragma once
#include "include.h"
#include <ImGui/imgui.h>
#include <algorithm>

namespace Render {

	struct DrawStyle {
		float lineThickness = 1.0f;
		float textScale = 1.0f;
		float outlineExtra = 2.0f;
		bool  outline = true;
	};
	inline DrawStyle g_Style;

	inline constexpr ImU32 kOutline = IM_COL32(0, 0, 0, 255);

	inline ImDrawList* List() { return ImGui::GetBackgroundDrawList(); }

	inline float FontSize() {
		return (std::max)(6.0f, ImGui::GetFontSize() * g_Style.textScale);
	}

	inline ImVec2 TextSize(const char* txt) {
		ImFont* f = ImGui::GetFont();
		if (!f || !txt) return ImVec2(0, 0);
		return f->CalcTextSizeA(FontSize(), FLT_MAX, 0.0f, txt);
	}

	inline void Text(const ImVec2& pos, const char* txt, ImU32 col, bool centered = true) {
		if (!txt || !*txt) return;
		ImDrawList* dl = List();
		ImFont* f = ImGui::GetFont();
		if (!dl || !f) return;

		const float fs = FontSize();
		ImVec2 p = pos;
		if (centered) p.x -= f->CalcTextSizeA(fs, FLT_MAX, 0.0f, txt).x * 0.5f;

		if (g_Style.outline) {
			dl->AddText(f, fs, ImVec2(p.x - 1, p.y), kOutline, txt);
			dl->AddText(f, fs, ImVec2(p.x + 1, p.y), kOutline, txt);
			dl->AddText(f, fs, ImVec2(p.x, p.y - 1), kOutline, txt);
			dl->AddText(f, fs, ImVec2(p.x, p.y + 1), kOutline, txt);
		}
		dl->AddText(f, fs, p, col, txt);
	}

	inline void Line(const ImVec2& a, const ImVec2& b, ImU32 col, float thick = -1.0f) {
		ImDrawList* dl = List();
		if (!dl) return;
		const float t = (thick > 0.0f) ? thick : g_Style.lineThickness;
		if (g_Style.outline)
			dl->AddLine(a, b, kOutline, t + g_Style.outlineExtra);
		dl->AddLine(a, b, col, t);
	}

	inline void Box(const ImVec2& tl, const ImVec2& br, ImU32 col, float thick = -1.0f) {
		ImDrawList* dl = List();
		if (!dl) return;
		const float t = (thick > 0.0f) ? thick : g_Style.lineThickness;
		if (g_Style.outline)
			dl->AddRect(tl, br, kOutline, 0.0f, 0, t + g_Style.outlineExtra);
		dl->AddRect(tl, br, col, 0.0f, 0, t);
	}

	inline void CornerBox(const ImVec2& tl, const ImVec2& br, ImU32 col, float thick = -1.0f) {
		const float w = br.x - tl.x, h = br.y - tl.y;
		if (w <= 0.0f || h <= 0.0f) return;
		const float lw = w * 0.25f, lh = h * 0.25f;

		Line(ImVec2(tl.x, tl.y), ImVec2(tl.x + lw, tl.y), col, thick);
		Line(ImVec2(tl.x, tl.y), ImVec2(tl.x, tl.y + lh), col, thick);
		Line(ImVec2(br.x - lw, tl.y), ImVec2(br.x, tl.y), col, thick);
		Line(ImVec2(br.x, tl.y), ImVec2(br.x, tl.y + lh), col, thick);
		Line(ImVec2(tl.x, br.y - lh), ImVec2(tl.x, br.y), col, thick);
		Line(ImVec2(tl.x, br.y), ImVec2(tl.x + lw, br.y), col, thick);
		Line(ImVec2(br.x, br.y - lh), ImVec2(br.x, br.y), col, thick);
		Line(ImVec2(br.x - lw, br.y), ImVec2(br.x, br.y), col, thick);
	}

	inline void Circle(const ImVec2& c, float r, ImU32 col, float thick = -1.0f) {
		ImDrawList* dl = List();
		if (!dl || r <= 0.0f) return;
		const float t = (thick > 0.0f) ? thick : g_Style.lineThickness;
		if (g_Style.outline)
			dl->AddCircle(c, r, kOutline, 0, t + g_Style.outlineExtra);
		dl->AddCircle(c, r, col, 0, t);
	}

	inline void CircleFilled(const ImVec2& c, float r, ImU32 col) {
		ImDrawList* dl = List();
		if (!dl || r <= 0.0f) return;
		if (g_Style.outline)
			dl->AddCircleFilled(c, r + 1.0f, kOutline);
		dl->AddCircleFilled(c, r, col);
	}

	inline ImU32 HealthColor(float pct) {
		if (pct > 75.0f) return IM_COL32(0, 255, 0, 255);
		if (pct > 50.0f) return IM_COL32(80, 190, 0, 255);
		if (pct > 25.0f) return IM_COL32(255, 165, 0, 255);
		return IM_COL32(255, 40, 40, 255);
	}

	inline void HealthBar(const ImVec2& tl, const ImVec2& br, float pct) {
		ImDrawList* dl = List();
		if (!dl) return;
		pct = (std::max)(0.0f, (std::min)(100.0f, pct));

		const float w = 3.0f;
		const float x = tl.x - w - 3.0f;
		const float h = br.y - tl.y;
		if (h <= 1.0f) return;

		dl->AddRectFilled(ImVec2(x - 1, tl.y - 1), ImVec2(x + w + 1, br.y + 1), kOutline);

		const float fill = h * (pct / 100.0f);
		dl->AddRectFilled(ImVec2(x, br.y - fill), ImVec2(x + w, br.y), HealthColor(pct));
	}

	inline void Snapline(const ImVec2& from, const ImVec2& to, ImU32 col) {
		Line(from, to, col);
	}

	inline void ApplyTheme() {
		ImGuiStyle& s = ImGui::GetStyle();
		s.WindowPadding = ImVec2(11, 12);
		s.FramePadding = ImVec2(8, 4);
		s.ItemSpacing = ImVec2(10, 6);
		s.ItemInnerSpacing = ImVec2(6, 4);
		s.FrameRounding = 4.0f;
		s.GrabRounding = 4.0f;
		s.PopupRounding = 6.0f;
		s.ScrollbarSize = 11.0f;
		s.ScrollbarRounding = 12.0f;
		s.WindowBorderSize = 0.0f;
		s.WindowRounding = 12.0f;
		s.ChildRounding = 8.0f;
		s.WindowTitleAlign = ImVec2(0.5f, 0.5f);

		ImVec4* c = s.Colors;
		c[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		c[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
		c[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.07f, 0.07f, 0.60f);
		c[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
		c[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
		c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
		c[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		c[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
		c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		c[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		c[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
		c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		c[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		c[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		c[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
		c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
		c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
		c[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		c[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
		c[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
		c[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
		c[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		c[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		c[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.22f, 0.23f, 0.60f);
		c[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.80f);
		c[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		c[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
		c[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
		c[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		c[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
		c[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
		c[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		c[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		c[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
		c[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		c[ImGuiCol_PlotLines] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		c[ImGuiCol_PlotLinesHovered] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		c[ImGuiCol_PlotHistogram] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		c[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		c[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		c[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		c[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		c[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
		c[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		c[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		c[ImGuiCol_NavHighlight] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		c[ImGuiCol_NavWindowingHighlight] = ImVec4(0.33f, 0.67f, 0.86f, 0.70f);
		c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
		c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);
	}

}
