#include "draw.h"

#include "My_font/zh_Font.h"
#include "My_font/fontawesome-brands.h"
#include "My_font/fontawesome-regular.h"
#include "My_font/fontawesome-solid.h"
#include "My_font/gui_icon.h"
   
#include "My_icon/pic_ZhenAiKun_png.h"

bool permeate_record = false;
bool permeate_record_ini = false;
struct Last_ImRect LastCoordinate = {0, 0, 0, 0};


std::unique_ptr<AndroidImgui> graphics;
ANativeWindow *window = NULL; 
android::ANativeWindowCreator::DisplayInfo displayInfo;// 屏幕信息
ImGuiWindow *g_window = NULL;// 窗口信息
int abs_ScreenX = 0, abs_ScreenY = 0;// 绝对屏幕X _ Y
int native_window_screen_x = 0, native_window_screen_y = 0;

TextureInfo Aekun_image{};

ImFont* zh_font = NULL;
ImFont* icon_font_0 = NULL;
ImFont* icon_font_1 = NULL;
ImFont* icon_font_2 = NULL;


// =====================================================================
//  现代主题配色 —— 仅切换颜色，不重新 Scale，可反复调用
//  全部为文件内 static 函数，不改变任何对外接口
// =====================================================================

static void ApplyModernDarkColors() {
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *c = style.Colors;

    const ImVec4 accent    = ImVec4(0.36f, 0.56f, 0.94f, 1.00f); // #5B8DEF
    const ImVec4 accentHov = ImVec4(0.46f, 0.66f, 1.00f, 1.00f);
    const ImVec4 accentAct = ImVec4(0.28f, 0.46f, 0.84f, 1.00f);

    c[ImGuiCol_Text]                  = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
    c[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.56f, 1.00f);
    c[ImGuiCol_WindowBg]              = ImVec4(0.09f, 0.09f, 0.12f, 0.96f);
    c[ImGuiCol_ChildBg]               = ImVec4(0.12f, 0.12f, 0.16f, 0.60f);
    c[ImGuiCol_PopupBg]               = ImVec4(0.14f, 0.14f, 0.18f, 0.98f);
    c[ImGuiCol_Border]                = ImVec4(0.24f, 0.24f, 0.30f, 0.50f);
    c[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]               = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
    c[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
    c[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.26f, 0.34f, 1.00f);
    c[ImGuiCol_TitleBg]               = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    c[ImGuiCol_TitleBgActive]         = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.07f, 0.07f, 0.09f, 0.80f);
    c[ImGuiCol_MenuBarBg]             = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.08f, 0.10f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.28f, 0.28f, 0.34f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.36f, 0.36f, 0.44f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]   = accent;
    c[ImGuiCol_CheckMark]             = accent;
    c[ImGuiCol_SliderGrab]            = accent;
    c[ImGuiCol_SliderGrabActive]      = accentAct;
    c[ImGuiCol_Button]                = ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
    c[ImGuiCol_ButtonHovered]         = accentHov;
    c[ImGuiCol_ButtonActive]          = accentAct;
    c[ImGuiCol_Header]                = ImVec4(0.20f, 0.22f, 0.30f, 1.00f);
    c[ImGuiCol_HeaderHovered]         = accentHov;
    c[ImGuiCol_HeaderActive]          = accentAct;
    c[ImGuiCol_Separator]             = ImVec4(0.24f, 0.24f, 0.30f, 0.60f);
    c[ImGuiCol_SeparatorHovered]      = accentHov;
    c[ImGuiCol_SeparatorActive]       = accentAct;
    c[ImGuiCol_ResizeGrip]            = ImVec4(0.36f, 0.56f, 0.94f, 0.40f);
    c[ImGuiCol_ResizeGripHovered]     = accentHov;
    c[ImGuiCol_ResizeGripActive]      = accentAct;
    c[ImGuiCol_Tab]                   = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
    c[ImGuiCol_TabHovered]            = accentHov;
    c[ImGuiCol_TabActive]             = ImVec4(0.22f, 0.30f, 0.46f, 1.00f);
    c[ImGuiCol_TabUnfocused]          = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.18f, 0.18f, 0.24f, 1.00f);
    c[ImGuiCol_PlotLines]             = ImVec4(0.70f, 0.70f, 0.78f, 1.00f);
    c[ImGuiCol_PlotLinesHovered]      = accentHov;
    c[ImGuiCol_PlotHistogram]         = accent;
    c[ImGuiCol_PlotHistogramHovered]  = accentHov;
    c[ImGuiCol_TableHeaderBg]         = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
    c[ImGuiCol_TableBorderStrong]     = ImVec4(0.24f, 0.24f, 0.30f, 1.00f);
    c[ImGuiCol_TableBorderLight]      = ImVec4(0.18f, 0.18f, 0.24f, 1.00f);
    c[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    c[ImGuiCol_TextSelectedBg]        = ImVec4(0.36f, 0.56f, 0.94f, 0.35f);
    c[ImGuiCol_DragDropTarget]        = accent;
    c[ImGuiCol_NavHighlight]          = accent;
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.10f, 0.10f, 0.13f, 0.60f);
}

static void ApplyModernLightColors() {
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *c = style.Colors;

    const ImVec4 accent    = ImVec4(0.23f, 0.51f, 0.96f, 1.00f); // #3B82F6
    const ImVec4 accentHov = ImVec4(0.36f, 0.62f, 0.98f, 1.00f);
    const ImVec4 accentAct = ImVec4(0.16f, 0.42f, 0.86f, 1.00f);

    c[ImGuiCol_Text]                  = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);
    c[ImGuiCol_TextDisabled]          = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);
    c[ImGuiCol_WindowBg]              = ImVec4(0.96f, 0.96f, 0.98f, 0.96f);
    c[ImGuiCol_ChildBg]               = ImVec4(0.99f, 0.99f, 1.00f, 0.60f);
    c[ImGuiCol_PopupBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    c[ImGuiCol_Border]                = ImVec4(0.80f, 0.80f, 0.84f, 0.80f);
    c[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]               = ImVec4(0.90f, 0.90f, 0.93f, 1.00f);
    c[ImGuiCol_FrameBgHovered]        = ImVec4(0.84f, 0.86f, 0.92f, 1.00f);
    c[ImGuiCol_FrameBgActive]         = ImVec4(0.78f, 0.82f, 0.92f, 1.00f);
    c[ImGuiCol_TitleBg]               = ImVec4(0.88f, 0.88f, 0.92f, 1.00f);
    c[ImGuiCol_TitleBgActive]         = ImVec4(0.82f, 0.86f, 0.95f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.88f, 0.88f, 0.92f, 0.80f);
    c[ImGuiCol_MenuBarBg]             = ImVec4(0.94f, 0.94f, 0.96f, 1.00f);
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0.94f, 0.94f, 0.96f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.72f, 0.72f, 0.78f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.60f, 0.60f, 0.68f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]   = accent;
    c[ImGuiCol_CheckMark]             = accent;
    c[ImGuiCol_SliderGrab]            = accent;
    c[ImGuiCol_SliderGrabActive]      = accentAct;
    c[ImGuiCol_Button]                = ImVec4(0.86f, 0.88f, 0.94f, 1.00f);
    c[ImGuiCol_ButtonHovered]         = accentHov;
    c[ImGuiCol_ButtonActive]          = accentAct;
    c[ImGuiCol_Header]                = ImVec4(0.86f, 0.90f, 0.98f, 1.00f);
    c[ImGuiCol_HeaderHovered]         = accentHov;
    c[ImGuiCol_HeaderActive]          = accentAct;
    c[ImGuiCol_Separator]             = ImVec4(0.78f, 0.78f, 0.82f, 0.60f);
    c[ImGuiCol_SeparatorHovered]      = accentHov;
    c[ImGuiCol_SeparatorActive]       = accentAct;
    c[ImGuiCol_ResizeGrip]            = ImVec4(0.23f, 0.51f, 0.96f, 0.40f);
    c[ImGuiCol_ResizeGripHovered]     = accentHov;
    c[ImGuiCol_ResizeGripActive]      = accentAct;
    c[ImGuiCol_Tab]                   = ImVec4(0.90f, 0.90f, 0.94f, 1.00f);
    c[ImGuiCol_TabHovered]            = accentHov;
    c[ImGuiCol_TabActive]             = ImVec4(0.78f, 0.86f, 0.98f, 1.00f);
    c[ImGuiCol_TabUnfocused]          = ImVec4(0.92f, 0.92f, 0.96f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.86f, 0.90f, 0.96f, 1.00f);
    c[ImGuiCol_PlotLines]             = ImVec4(0.40f, 0.40f, 0.48f, 1.00f);
    c[ImGuiCol_PlotLinesHovered]      = accentHov;
    c[ImGuiCol_PlotHistogram]         = accent;
    c[ImGuiCol_PlotHistogramHovered]  = accentHov;
    c[ImGuiCol_TableHeaderBg]         = ImVec4(0.90f, 0.90f, 0.94f, 1.00f);
    c[ImGuiCol_TableBorderStrong]     = ImVec4(0.78f, 0.78f, 0.82f, 1.00f);
    c[ImGuiCol_TableBorderLight]      = ImVec4(0.86f, 0.86f, 0.90f, 1.00f);
    c[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(0.00f, 0.00f, 0.00f, 0.03f);
    c[ImGuiCol_TextSelectedBg]        = ImVec4(0.23f, 0.51f, 0.96f, 0.35f);
    c[ImGuiCol_DragDropTarget]        = accent;
    c[ImGuiCol_NavHighlight]          = accent;
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(0.30f, 0.30f, 0.30f, 0.70f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.60f);
}

static void ApplyModernPurpleColors() {
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *c = style.Colors;

    const ImVec4 accent    = ImVec4(0.66f, 0.33f, 0.97f, 1.00f); // #A855F7
    const ImVec4 accentHov = ImVec4(0.76f, 0.46f, 1.00f, 1.00f);
    const ImVec4 accentAct = ImVec4(0.54f, 0.24f, 0.86f, 1.00f);

    c[ImGuiCol_Text]                  = ImVec4(0.94f, 0.92f, 0.98f, 1.00f);
    c[ImGuiCol_TextDisabled]          = ImVec4(0.55f, 0.52f, 0.62f, 1.00f);
    c[ImGuiCol_WindowBg]              = ImVec4(0.11f, 0.09f, 0.15f, 0.96f);
    c[ImGuiCol_ChildBg]               = ImVec4(0.14f, 0.11f, 0.19f, 0.60f);
    c[ImGuiCol_PopupBg]               = ImVec4(0.15f, 0.12f, 0.20f, 0.98f);
    c[ImGuiCol_Border]                = ImVec4(0.30f, 0.24f, 0.42f, 0.55f);
    c[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]               = ImVec4(0.18f, 0.14f, 0.24f, 1.00f);
    c[ImGuiCol_FrameBgHovered]        = ImVec4(0.24f, 0.18f, 0.32f, 1.00f);
    c[ImGuiCol_FrameBgActive]         = ImVec4(0.30f, 0.22f, 0.40f, 1.00f);
    c[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.06f, 0.12f, 1.00f);
    c[ImGuiCol_TitleBgActive]         = ImVec4(0.16f, 0.11f, 0.24f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.08f, 0.06f, 0.12f, 0.80f);
    c[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.10f, 0.19f, 1.00f);
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.06f, 0.12f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.30f, 0.24f, 0.40f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.40f, 0.32f, 0.52f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]   = accent;
    c[ImGuiCol_CheckMark]             = accent;
    c[ImGuiCol_SliderGrab]            = accent;
    c[ImGuiCol_SliderGrabActive]      = accentAct;
    c[ImGuiCol_Button]                = ImVec4(0.24f, 0.18f, 0.32f, 1.00f);
    c[ImGuiCol_ButtonHovered]         = accentHov;
    c[ImGuiCol_ButtonActive]          = accentAct;
    c[ImGuiCol_Header]                = ImVec4(0.22f, 0.18f, 0.32f, 1.00f);
    c[ImGuiCol_HeaderHovered]         = accentHov;
    c[ImGuiCol_HeaderActive]          = accentAct;
    c[ImGuiCol_Separator]             = ImVec4(0.30f, 0.24f, 0.42f, 0.60f);
    c[ImGuiCol_SeparatorHovered]      = accentHov;
    c[ImGuiCol_SeparatorActive]       = accentAct;
    c[ImGuiCol_ResizeGrip]            = ImVec4(0.66f, 0.33f, 0.97f, 0.40f);
    c[ImGuiCol_ResizeGripHovered]     = accentHov;
    c[ImGuiCol_ResizeGripActive]      = accentAct;
    c[ImGuiCol_Tab]                   = ImVec4(0.16f, 0.12f, 0.22f, 1.00f);
    c[ImGuiCol_TabHovered]            = accentHov;
    c[ImGuiCol_TabActive]             = ImVec4(0.32f, 0.20f, 0.48f, 1.00f);
    c[ImGuiCol_TabUnfocused]          = ImVec4(0.13f, 0.10f, 0.18f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.20f, 0.15f, 0.28f, 1.00f);
    c[ImGuiCol_PlotLines]             = ImVec4(0.72f, 0.66f, 0.82f, 1.00f);
    c[ImGuiCol_PlotLinesHovered]      = accentHov;
    c[ImGuiCol_PlotHistogram]         = accent;
    c[ImGuiCol_PlotHistogramHovered]  = accentHov;
    c[ImGuiCol_TableHeaderBg]         = ImVec4(0.16f, 0.12f, 0.22f, 1.00f);
    c[ImGuiCol_TableBorderStrong]     = ImVec4(0.30f, 0.24f, 0.42f, 1.00f);
    c[ImGuiCol_TableBorderLight]      = ImVec4(0.20f, 0.16f, 0.28f, 1.00f);
    c[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    c[ImGuiCol_TextSelectedBg]        = ImVec4(0.66f, 0.33f, 0.97f, 0.35f);
    c[ImGuiCol_DragDropTarget]        = accent;
    c[ImGuiCol_NavHighlight]          = accent;
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.11f, 0.09f, 0.15f, 0.60f);
}

// 按索引切换主题颜色（只改颜色，不重 Scale）
static void ApplyThemeColors(int theme_idx) {
    switch (theme_idx) {
        case 0: ApplyModernLightColors();  break;
        case 2: ApplyModernPurpleColors(); break;
        case 1:
        default: ApplyModernDarkColors();  break;
    }
}

// 应用现代样式（形状 + 间距 + 缩放）。只在 init_My_drawdata() 里调用一次。
static void ApplyModernShapeAndSpacing() {
    ImGuiStyle &style = ImGui::GetStyle();

    // 1) 缩放：与原有行为保持一致（3.25x）
    style.ScaleAllSizes(3.25f);

    // 2) 形状：圆角与边框（绝对值，不受 Scale 影响）
    style.WindowRounding    = 12.0f;
    style.ChildRounding     = 10.0f;
    style.FrameRounding     = 8.0f;
    style.PopupRounding     = 10.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding      = 8.0f;
    style.TabRounding       = 8.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;

    // 3) 间距：呼吸感
    style.WindowPadding     = ImVec2(20.0f, 20.0f);
    style.FramePadding      = ImVec2(14.0f, 8.0f);
    style.CellPadding       = ImVec2(8.0f, 6.0f);
    style.ItemSpacing       = ImVec2(14.0f, 12.0f);
    style.ItemInnerSpacing  = ImVec2(10.0f, 8.0f);
    style.IndentSpacing     = 24.0f;
    style.ScrollbarSize     = 20.0f;
    style.GrabMinSize       = 14.0f;

    // 4) 对齐
    style.WindowTitleAlign         = ImVec2(0.5f, 0.5f); // 标题居中
    style.WindowMenuButtonPosition = ImGuiDir_None;      // 隐藏左侧菜单按钮
    style.ButtonTextAlign          = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign      = ImVec2(0.0f, 0.5f);

    // 5) 抗锯齿与透明度
    style.AntiAliasedLines = true;
    style.AntiAliasedFill  = true;
    style.Alpha            = 0.98f;

    // 6) 窗口最小尺寸（触摸友好）
    style.WindowMinSize    = ImVec2(220.0f, 120.0f);
}


bool M_Android_LoadFont(float SizePixels) {
    ImGuiIO &io = ImGui::GetIO();
    
    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.OversampleH = 3.0;
    icons_config.OversampleV = 3.0;		
    icons_config.SizePixels = SizePixels;
	::icon_font_0 = io.Fonts->AddFontFromMemoryCompressedTTF((const void *)&font_awesome_brands_compressed_data, sizeof(font_awesome_brands_compressed_data), 0.0f, &icons_config, icons_ranges);
	::icon_font_1 = io.Fonts->AddFontFromMemoryCompressedTTF((const void *)&font_awesome_regular_compressed_data, sizeof(font_awesome_regular_compressed_data), 0.0f, &icons_config, icons_ranges);
	::icon_font_2 = io.Fonts->AddFontFromMemoryCompressedTTF((const void *)&font_awesome_solid_compressed_data, sizeof(font_awesome_solid_compressed_data), 0.0f, &icons_config, icons_ranges);

    io.Fonts->AddFontDefault();
    return zh_font != nullptr;
}

void init_My_drawdata() {
    // 1) 基础风格（作为兜底）
    ImGui::StyleColorsDark();

    // 2) 加载字体（保持原样）
    ImGui::My_Android_LoadSystemFont(25.0f); // 加载系统字体
    M_Android_LoadFont(25.0f);               // 加载字体 + 图标

    // 3) 现代样式：形状 + 间距 + 缩放（只做一次，避免累积）
    ApplyModernShapeAndSpacing();

    // 4) 现代深色配色（默认）
    ApplyThemeColors(1);

    // 5) 加载图片资源（保持原样）
    ::Aekun_image = graphics->LoadTextureFromMemory((void *)picture_ZhenAiKun_PNG_H, sizeof(picture_ZhenAiKun_PNG_H));
}


void screen_config() {
    ::displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
}

void drawBegin() {
    if (::permeate_record_ini) {
        LastCoordinate.Pos_x = ::g_window->Pos.x;
        LastCoordinate.Pos_y = ::g_window->Pos.y;
        LastCoordinate.Size_x = ::g_window->Size.x;
        LastCoordinate.Size_y = ::g_window->Size.y;

        graphics->Shutdown();
        android::ANativeWindowCreator::Destroy(::window);
        ::window = android::ANativeWindowCreator::Create("AImGui", native_window_screen_x, native_window_screen_y, permeate_record);
        graphics->Init_Render(::window, native_window_screen_x, native_window_screen_y);
        ::init_My_drawdata(); //初始化绘制数据
    } 


    static uint32_t orientation = -1;
    screen_config();
    if (orientation != displayInfo.orientation) {
        orientation = displayInfo.orientation;
        Touch::setOrientation((int)displayInfo.orientation);
        if (g_window != NULL) {
            g_window->Pos.x = 100;
            g_window->Pos.y = 125;        
        }        
    }
}


void Layout_tick_UI(bool *main_thread_flag) {
    static bool show_draw_Line = false;
    static bool show_demo_window = false;
    static bool show_another_window = false;
    { 
        static float f = 0.0f;
        static int counter = 0;
        static int style_idx = 1; // 默认深色
        static ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

        ImGui::Begin("AndroidSurfaceImguiEnhanced", main_thread_flag);

        if (::permeate_record_ini) {
            ImGui::SetWindowPos({LastCoordinate.Pos_x, LastCoordinate.Pos_y});
            ImGui::SetWindowSize({LastCoordinate.Size_x, LastCoordinate.Size_y});
            permeate_record_ini = false;   
        }

        // ===== 头部信息 =====
        ImGui::TextDisabled(ICON_FA_MICROCHIP "  渲染接口 : %s", graphics->RenderName);
        ImGui::SameLine();
        ImGui::TextDisabled(ICON_FA_CODE "  GUI : %s", ImGui::GetVersion());
        ImGui::Separator();
        ImGui::Spacing();

        // ===== 主题切换 =====
        ImGui::Text(ICON_FA_PALETTE "  主题");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##theme_combo", &style_idx, "亮色主题\0深色主题\0紫罗兰主题\0")) {
            // 只改配色，不重新 Scale —— 避免反复切换导致样式爆炸
            ApplyThemeColors(style_idx);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ===== 窗口控制 =====
        ImGui::Text(ICON_FA_WINDOW_RESTORE "  窗口控制");
        if (ImGui::Checkbox(ICON_FA_VIDEO "  过录制", &::permeate_record)) {
            ::permeate_record_ini = true;
        }
        ImGui::Checkbox(ICON_FA_IMAGE "  演示窗口", &show_demo_window);
        ImGui::SameLine();
        ImGui::Checkbox(ICON_FA_DRAW_POLYGON "  绘制射线", &show_draw_Line);
        ImGui::Checkbox(ICON_FA_CAT "  坤坤窗口", &show_another_window);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ===== 控件测试 =====
        ImGui::Text(ICON_FA_SLIDERS_H "  控件测试");
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        ImGui::ColorEdit4("取色器", (float *) &clear_color);

        if (ImGui::Button(ICON_FA_PLUS "  Button", ImVec2(0, 0))) {
            counter++;
        }
        ImGui::SameLine();
        ImGui::Text("计数 = %d", counter);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ===== 状态信息 =====
        ImGui::Text(ICON_FA_INFO_CIRCLE "  窗口集中 = %d",
                    ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow));
        ImGui::TextColored(ImVec4(0.36f, 0.56f, 0.94f, 1.0f),
                           ICON_FA_TACHOMETER_ALT "  %.3f ms/frame  (%.1f FPS)",
                           1000.0f / ImGui::GetIO().Framerate,
                           ImGui::GetIO().Framerate);

        g_window = ImGui::GetCurrentWindow();
        ImGui::End();
    }
    
        
    if (show_another_window) {
        ImGui::Begin(ICON_FA_CAT "  另一个窗口", &show_another_window);
        ImGui::Text("另一个窗口的 爱坤!");
        ImGui::Image(Aekun_image.DS, ImVec2(170, 170));
        if (ImGui::Button(ICON_FA_TIMES "  关闭这个坤口")) {
            show_another_window = false;
        }
        ImGui::End();
    }
            
    if (show_demo_window) {
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    if (show_draw_Line)
        ImGui::GetForegroundDrawList()->AddLine(ImVec2(0,0),ImVec2(displayInfo.width, displayInfo.height),IM_COL32(255,0,0,255),4);
}
