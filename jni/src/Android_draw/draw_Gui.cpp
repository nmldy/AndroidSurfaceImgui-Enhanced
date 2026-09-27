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
//  方向切换时需要打断 ImGui 残留交互（例如 resize grip 被锁住）
//  仅作为文件内 static 变量，不对外暴露
// =====================================================================
static bool g_need_reset_interaction = false;


// =====================================================================
//  第 1 步：液态玻璃窗口的视觉基础
//  关键修复：函数开头先重置为 ImGui 默认样式，避免多次调用时
//  ScaleAllSizes 累积放大导致布局爆炸（表现为"卡住"）。
// =====================================================================
static void ApplyLiquidGlassWindowStyle() {
    ImGuiStyle &style = ImGui::GetStyle();

    // ---------- 1) 重置为 ImGui 默认样式（防止累积）----------
    style = ImGuiStyle();

    // ---------- 2) 基础缩放 ----------
    // 只在 ApplyLiquidGlassWindowStyle 里做一次缩放，函数是幂等的
    style.ScaleAllSizes(3.25f);

    // ---------- 3) 大圆角（覆盖默认值，绝对值）----------
    style.WindowRounding    = 36.0f;
    style.ChildRounding     = 28.0f;
    style.FrameRounding     = 18.0f;
    style.PopupRounding     = 28.0f;
    style.ScrollbarRounding = 18.0f;
    style.GrabRounding      = 14.0f;
    style.TabRounding       = 18.0f;

    // ---------- 4) 描边、间距 ----------
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.WindowPadding     = ImVec2(28.0f, 28.0f);
    style.FramePadding      = ImVec2(20.0f, 10.0f);
    style.ItemSpacing       = ImVec2(16.0f, 14.0f);
    style.ScrollbarSize     = 24.0f;
    style.WindowMinSize     = ImVec2(220.0f, 120.0f);

    ImVec4 *c = style.Colors;

    // ---------- 5) 窗口背景 = 磨砂深色玻璃 ----------
    c[ImGuiCol_WindowBg]       = ImVec4(0.05f, 0.06f, 0.10f, 0.45f);
    c[ImGuiCol_ChildBg]        = ImVec4(0.08f, 0.09f, 0.14f, 0.35f);
    c[ImGuiCol_PopupBg]        = ImVec4(0.05f, 0.06f, 0.10f, 0.55f);

    // ---------- 6) 边框 = 亮色高光 ----------
    c[ImGuiCol_Border]         = ImVec4(1.00f, 1.00f, 1.00f, 0.28f);
    c[ImGuiCol_BorderShadow]   = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // ---------- 7) 标题栏 ----------
    c[ImGuiCol_TitleBg]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);

    // ---------- 8) 玻璃感控件 ----------
    c[ImGuiCol_FrameBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.16f);
    c[ImGuiCol_FrameBgActive]  = ImVec4(1.00f, 1.00f, 1.00f, 0.22f);

    c[ImGuiCol_Button]         = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
    c[ImGuiCol_ButtonHovered]  = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    c[ImGuiCol_ButtonActive]   = ImVec4(1.00f, 1.00f, 1.00f, 0.28f);

    // ---------- 9) 强调色：冰蓝 ----------
    const ImVec4 glow = ImVec4(0.45f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_CheckMark]        = glow;
    c[ImGuiCol_SliderGrab]       = glow;
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.90f, 1.00f, 1.00f);
    c[ImGuiCol_Header]           = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(1.00f, 1.00f, 1.00f, 0.18f);
    c[ImGuiCol_HeaderActive]     = ImVec4(1.00f, 1.00f, 1.00f, 0.26f);

    // ---------- 10) 文字 ----------
    c[ImGuiCol_Text]         = ImVec4(0.98f, 0.99f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.75f, 0.78f, 0.85f, 0.75f);

    // ---------- 11) 分隔线 ----------
    c[ImGuiCol_Separator]        = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.24f);
    c[ImGuiCol_SeparatorActive]  = glow;

    // ---------- 12) 滚动条 / 拖拽点 ----------
    c[ImGuiCol_ScrollbarBg]          = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(1.00f, 1.00f, 1.00f, 0.22f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.34f);
    c[ImGuiCol_ScrollbarGrabActive]  = glow;
    c[ImGuiCol_ResizeGrip]           = ImVec4(1.00f, 1.00f, 1.00f, 0.18f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    c[ImGuiCol_ResizeGripActive]     = glow;

    // ---------- 13) 抗锯齿 / 透明度 ----------
    style.AntiAliasedLines = true;
    style.AntiAliasedFill  = true;
    style.Alpha            = 0.98f;
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
    ImGui::StyleColorsDark(); // 兜底
    ImGui::My_Android_LoadSystemFont(25.0f); // 加载系统字体
    M_Android_LoadFont(25.0f);               // 加载字体 + 图标

    // 注意：ScaleAllSizes(3.25f) 已移入 ApplyLiquidGlassWindowStyle 内部。
    // 之前放在这里会导致每次 init_My_drawdata 调用时在上一次的基础上继续放大，
    // 切换方向反复重建时样式会爆炸。现在 ApplyLiquidGlassWindowStyle 内部先重置
    // 再缩放，是幂等的。

    ::Aekun_image = graphics->LoadTextureFromMemory((void *)picture_ZhenAiKun_PNG_H, sizeof(picture_ZhenAiKun_PNG_H));

    // 液态玻璃样式（第 1 步：外观基础）
    ApplyLiquidGlassWindowStyle();
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

        // 关键修复：ImGui 上下文已被销毁重建，g_window 指向的旧内存已失效。
        // 必须清空，否则下一帧写 g_window->Pos 会破坏新上下文的状态。
        g_window = NULL;

        // 重建后也打断一次残留交互
        g_need_reset_interaction = true;
    } 


    static uint32_t orientation = -1;
    screen_config();
    if (orientation != displayInfo.orientation) {
        orientation = displayInfo.orientation;
        Touch::setOrientation((int)displayInfo.orientation);

        // 关键修复：这里不再直接写 g_window->Pos.x/y（悬空指针风险）。
        // 只置一个标志位，让 Layout_tick_UI 在 Begin 之前用 ImGui 公开 API 处理，
        // 同时打断可能残留在 resize grip 上的拖拽状态。
        g_need_reset_interaction = true;
    }
}


void Layout_tick_UI(bool *main_thread_flag) {
    // ===== 关键修复：在 Begin 之前打断 ImGui 残留交互 =====
    // 方向切换时手指可能还按在屏幕边缘，ImGui 的 ActiveId 会锁在 resize grip 上，
    // 导致"只能朝一个方向伸缩"。
    // 注意：ClearHoveredID 在部分 ImGui 版本里不存在，改为直接写上下文内部字段。
    if (g_need_reset_interaction) {
        g_need_reset_interaction = false;

        ImGui::ClearActiveID();

        if (ImGuiContext *ctx = ImGui::GetCurrentContext()) {
            ctx->HoveredId = 0;
        }
    }
    // ====================================================

    static bool show_draw_Line = false;
    static bool show_demo_window = false;
    static bool show_another_window = false;
    { 
        static float f = 0.0f;
        static int counter = 0;
        static int style_idx = 0;
        static ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        ImGui::Begin("AndroidSurfaceImguiEnhanced", main_thread_flag);
        if (::permeate_record_ini) {
            ImGui::SetWindowPos({LastCoordinate.Pos_x, LastCoordinate.Pos_y});
            ImGui::SetWindowSize({LastCoordinate.Size_x, LastCoordinate.Size_y});
            permeate_record_ini = false;   
        }
        ImGui::Text("渲染接口 : %s, gui版本 : %s", graphics->RenderName, ImGui::GetVersion());
		if (ImGui::Combo("##主题", &style_idx, "白色主题\0蓝色主题\0紫色主题\0")) {
			switch (style_idx) {
				case 0: ImGui::StyleColorsLight(); break;
				case 1: ImGui::StyleColorsDark(); break;
				case 2: ImGui::StyleColorsClassic(); break;
			}
		}
		
        if (ImGui::Checkbox("过录制", &::permeate_record)) {
            ::permeate_record_ini = true;
        }
            
        ImGui::Checkbox("演示窗口", &show_demo_window);
        ImGui::SameLine();
        ImGui::Checkbox("绘制射线", &show_draw_Line);
        ImGui::Checkbox("坤坤窗口", &show_another_window);
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        ImGui::ColorEdit4("取色器", (float *) &clear_color);
        if (ImGui::Button("Button")) {
            counter++;
        }
        
        ImGui::SameLine();
        ImGui::Text("计数 = %d", counter);
        ImGui::Text("窗口集中 = %d", ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow));
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 1.0f, 1.0f), "应用平均 %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        g_window = ImGui::GetCurrentWindow();
        ImGui::End();
    }
    
        
    if (show_another_window) {
        ImGui::Begin("另一个窗口", &show_another_window);
        ImGui::Text("另一个窗口的 爱坤!");
        ImGui::Image(Aekun_image.DS, ImVec2(170, 170));
        if (ImGui::Button("关闭这个坤口")) {
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
