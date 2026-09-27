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


static bool g_need_reset_interaction = false;


// =====================================================================
//  UI 动画状态
//  - alpha: 窗口淡入进度 0..1
//  - colors_current / colors_target: 主题颜色平滑过渡
//  全部为文件内 static，不对外暴露
// =====================================================================
struct GlassAnimState {
    float  alpha          = 0.0f;
    float  target_alpha   = 0.98f;
    bool   colors_inited  = false;
    ImVec4 colors_current[ImGuiCol_COUNT];
    ImVec4 colors_target[ImGuiCol_COUNT];
};
static GlassAnimState g_anim;

// 把当前 style 里的颜色快照到 target（主题切换时调用）
static void CaptureCurrentColorsAsTarget() {
    ImGuiStyle &style = ImGui::GetStyle();
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        g_anim.colors_target[i] = style.Colors[i];
    }
}

// 重置淡入（重建 / 切换方向时调用）
static void ResetGlassFadeIn() {
    g_anim.alpha = 0.0f;
    g_anim.colors_inited = false;   // 首次会直接吸附到目标色，避免黑屏渐变
}

// 每帧调用：颜色 lerp + alpha 递增
static void UpdateGlassAnimation() {
    ImGuiIO &io = ImGui::GetIO();
    float dt = io.DeltaTime;
    if (dt <= 0.0f || dt > 0.1f) dt = 1.0f / 60.0f;   // 防止切换时大跳变

    // ---------- 1) 窗口淡入 ----------
    if (g_anim.alpha < g_anim.target_alpha) {
        // 1.5 秒左右收敛（speed = 0.7/s 起点慢，后面快）
        const float speed = 0.7f + 0.5f * (1.0f - g_anim.alpha / g_anim.target_alpha);
        g_anim.alpha += dt * speed;
        if (g_anim.alpha > g_anim.target_alpha) g_anim.alpha = g_anim.target_alpha;
    }

    // ---------- 2) 主题颜色平滑过渡 ----------
    if (!g_anim.colors_inited) {
        // 首帧：直接吸附到目标，避免从黑渐变
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            g_anim.colors_current[i] = g_anim.colors_target[i];
        }
        g_anim.colors_inited = true;
    } else {
        // 用"指数逼近"做平滑，收敛速度与帧率无关
        // t = 1 - exp(-k * dt)，k 越大越快
        const float k = 9.0f;
        float t = 1.0f - expf(-k * dt);
        if (t > 1.0f) t = 1.0f;

        ImGuiStyle &style = ImGui::GetStyle();
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            ImVec4 &cur       = g_anim.colors_current[i];
            const ImVec4 &tgt = g_anim.colors_target[i];

            cur.x += (tgt.x - cur.x) * t;
            cur.y += (tgt.y - cur.y) * t;
            cur.z += (tgt.z - cur.z) * t;
            cur.w += (tgt.w - cur.w) * t;

            style.Colors[i] = cur;
        }
    }

    // ---------- 3) 应用全局 alpha ----------
    ImGui::GetStyle().Alpha = g_anim.alpha;
}


// =====================================================================
//  重建 / 切屏后给新上下文一个中性的鼠标初始状态
// =====================================================================
static void ResetMouseToNeutralPosition() {
    if (ImGui::GetCurrentContext() == nullptr)
        return;

    ImGuiIO &io = ImGui::GetIO();
    const float cx = (float)(native_window_screen_x) * 0.5f;
    const float cy = (float)(native_window_screen_y) * 0.5f;

    io.MousePos     = ImVec2(cx, cy);
    io.MousePosPrev = ImVec2(cx, cy);
    io.MouseDelta   = ImVec2(0.0f, 0.0f);
    io.MouseDown[0] = false;
    io.MouseDown[1] = false;
    io.MouseDown[2] = false;
}


// =====================================================================
//  液态玻璃：公共形状与间距（先重置再缩放，幂等）
// =====================================================================
static void ApplyLiquidGlassShapeAndSpacing() {
    ImGuiStyle &style = ImGui::GetStyle();
    style = ImGuiStyle();
    style.ScaleAllSizes(3.25f);

    style.WindowRounding    = 36.0f;
    style.ChildRounding     = 28.0f;
    style.FrameRounding     = 18.0f;
    style.PopupRounding     = 28.0f;
    style.ScrollbarRounding = 18.0f;
    style.GrabRounding      = 14.0f;
    style.TabRounding       = 18.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.WindowPadding     = ImVec2(28.0f, 28.0f);
    style.FramePadding      = ImVec2(20.0f, 10.0f);
    style.ItemSpacing       = ImVec2(16.0f, 14.0f);
    style.ScrollbarSize     = 24.0f;
    style.WindowMinSize     = ImVec2(220.0f, 120.0f);

    style.AntiAliasedLines = true;
    style.AntiAliasedFill  = true;
}


// =====================================================================
//  液态玻璃：深色配色
// =====================================================================
static void ApplyLiquidGlassDarkColors() {
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *c = style.Colors;

    c[ImGuiCol_WindowBg]       = ImVec4(0.05f, 0.06f, 0.10f, 0.45f);
    c[ImGuiCol_ChildBg]        = ImVec4(0.08f, 0.09f, 0.14f, 0.35f);
    c[ImGuiCol_PopupBg]        = ImVec4(0.05f, 0.06f, 0.10f, 0.55f);

    c[ImGuiCol_Border]         = ImVec4(1.00f, 1.00f, 1.00f, 0.28f);
    c[ImGuiCol_BorderShadow]   = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    c[ImGuiCol_TitleBg]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);

    c[ImGuiCol_FrameBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.16f);
    c[ImGuiCol_FrameBgActive]  = ImVec4(1.00f, 1.00f, 1.00f, 0.22f);

    c[ImGuiCol_Button]         = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
    c[ImGuiCol_ButtonHovered]  = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    c[ImGuiCol_ButtonActive]   = ImVec4(1.00f, 1.00f, 1.00f, 0.28f);

    const ImVec4 glow = ImVec4(0.45f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_CheckMark]        = glow;
    c[ImGuiCol_SliderGrab]       = glow;
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.90f, 1.00f, 1.00f);
    c[ImGuiCol_Header]           = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(1.00f, 1.00f, 1.00f, 0.18f);
    c[ImGuiCol_HeaderActive]     = ImVec4(1.00f, 1.00f, 1.00f, 0.26f);

    c[ImGuiCol_Text]         = ImVec4(0.98f, 0.99f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.75f, 0.78f, 0.85f, 0.75f);

    c[ImGuiCol_Separator]        = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.24f);
    c[ImGuiCol_SeparatorActive]  = glow;

    c[ImGuiCol_ScrollbarBg]          = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(1.00f, 1.00f, 1.00f, 0.22f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.34f);
    c[ImGuiCol_ScrollbarGrabActive]  = glow;
    c[ImGuiCol_ResizeGrip]           = ImVec4(1.00f, 1.00f, 1.00f, 0.18f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    c[ImGuiCol_ResizeGripActive]     = glow;
}


// =====================================================================
//  液态玻璃：亮色配色
// =====================================================================
static void ApplyLiquidGlassLightColors() {
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *c = style.Colors;

    c[ImGuiCol_WindowBg]       = ImVec4(0.98f, 0.98f, 1.00f, 0.55f);
    c[ImGuiCol_ChildBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.40f);
    c[ImGuiCol_PopupBg]        = ImVec4(0.99f, 0.99f, 1.00f, 0.65f);

    c[ImGuiCol_Border]         = ImVec4(0.60f, 0.65f, 0.80f, 0.35f);
    c[ImGuiCol_BorderShadow]   = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    c[ImGuiCol_TitleBg]          = ImVec4(0.90f, 0.92f, 0.98f, 0.30f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.85f, 0.90f, 0.98f, 0.45f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.90f, 0.92f, 0.98f, 0.30f);

    c[ImGuiCol_FrameBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.55f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    c[ImGuiCol_FrameBgActive]  = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);

    c[ImGuiCol_Button]         = ImVec4(0.92f, 0.94f, 1.00f, 0.70f);
    c[ImGuiCol_ButtonHovered]  = ImVec4(0.85f, 0.90f, 1.00f, 0.85f);
    c[ImGuiCol_ButtonActive]   = ImVec4(0.78f, 0.86f, 1.00f, 0.95f);

    const ImVec4 accent = ImVec4(0.20f, 0.50f, 0.95f, 1.00f);
    c[ImGuiCol_CheckMark]        = accent;
    c[ImGuiCol_SliderGrab]       = accent;
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.10f, 0.40f, 0.85f, 1.00f);
    c[ImGuiCol_Header]           = ImVec4(0.85f, 0.90f, 1.00f, 0.60f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(0.78f, 0.86f, 1.00f, 0.80f);
    c[ImGuiCol_HeaderActive]     = ImVec4(0.70f, 0.80f, 1.00f, 0.90f);

    c[ImGuiCol_Text]         = ImVec4(0.10f, 0.12f, 0.18f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.44f, 0.52f, 0.75f);

    c[ImGuiCol_Separator]        = ImVec4(0.60f, 0.65f, 0.80f, 0.30f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.50f, 0.58f, 0.78f, 0.50f);
    c[ImGuiCol_SeparatorActive]  = accent;

    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.90f, 0.92f, 0.98f, 0.20f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.70f, 0.75f, 0.85f, 0.55f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.68f, 0.82f, 0.75f);
    c[ImGuiCol_ScrollbarGrabActive]  = accent;
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.60f, 0.68f, 0.82f, 0.30f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.50f, 0.58f, 0.78f, 0.55f);
    c[ImGuiCol_ResizeGripActive]     = accent;
}


static void ApplyLiquidGlassWindowStyle() {
    ApplyLiquidGlassShapeAndSpacing();
    ApplyLiquidGlassDarkColors();
    CaptureCurrentColorsAsTarget();   // 记录目标色，用于平滑过渡
}

static void ApplyLiquidGlassLightStyle() {
    ApplyLiquidGlassShapeAndSpacing();
    ApplyLiquidGlassLightColors();
    CaptureCurrentColorsAsTarget();
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
    ImGui::StyleColorsDark();
    ImGui::My_Android_LoadSystemFont(25.0f);
    M_Android_LoadFont(25.0f);

    ::Aekun_image = graphics->LoadTextureFromMemory((void *)picture_ZhenAiKun_PNG_H, sizeof(picture_ZhenAiKun_PNG_H));

    // 液态玻璃样式（默认深色）
    ApplyLiquidGlassWindowStyle();

    // 首次构建/重建后都从透明淡入
    ResetGlassFadeIn();
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
        ::init_My_drawdata();   // 内部会 ResetGlassFadeIn()

        g_window = NULL;
        ResetMouseToNeutralPosition();
        g_need_reset_interaction = true;
    }


    static uint32_t orientation = -1;
    screen_config();
    if (orientation != displayInfo.orientation) {
        orientation = displayInfo.orientation;
        Touch::setOrientation((int)displayInfo.orientation);
        ResetMouseToNeutralPosition();
        g_need_reset_interaction = true;

        // 方向切换也播一次淡入
        ResetGlassFadeIn();
    }
}


void Layout_tick_UI(bool *main_thread_flag) {
    // ===== 每帧驱动 UI 动画（颜色插值 + 淡入）=====
    UpdateGlassAnimation();

    // 打断 ImGui 残留交互
    if (g_need_reset_interaction) {
        g_need_reset_interaction = false;

        ImGui::ClearActiveID();
        if (ImGuiContext *ctx = ImGui::GetCurrentContext()) {
            ctx->HoveredId = 0;
        }
    }

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

        // ============= 顶部标题栏 =============
        ImGui::TextDisabled(ICON_FA_MICROCHIP "  %s", graphics->RenderName);
        ImGui::SameLine();
        ImGui::TextDisabled("  ·  " ICON_FA_CODE "  %s", ImGui::GetVersion());
        ImGui::Spacing();

        // ============= 分组 1：外观主题 =============
        if (ImGui::CollapsingHeader(ICON_FA_PALETTE "  外观主题", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(16.0f);
            ImGui::TextDisabled("整体视觉风格（切换带平滑过渡）");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##theme_combo", &style_idx, "深色主题\0亮色主题\0经典主题\0")) {
                // 注意：调用 ApplyLiquidGlass* 时会 CaptureCurrentColorsAsTarget，
                // 但当前 colors_current 仍是旧色，动画会从旧色平滑过渡到新色。
                switch (style_idx) {
                    case 0: ApplyLiquidGlassWindowStyle(); break;
                    case 1: ApplyLiquidGlassLightStyle();  break;
                    case 2:
                        ImGui::StyleColorsClassic();
                        CaptureCurrentColorsAsTarget();
                        break;
                }
            }
            ImGui::Unindent(16.0f);
            ImGui::Spacing();
        }

        // ============= 分组 2：窗口控制 =============
        if (ImGui::CollapsingHeader(ICON_FA_WINDOW_RESTORE "  窗口控制", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(16.0f);
            if (ImGui::Checkbox(ICON_FA_VIDEO "  过录制", &::permeate_record)) {
                ::permeate_record_ini = true;
            }
            ImGui::Checkbox(ICON_FA_IMAGE "  演示窗口", &show_demo_window);
            ImGui::Checkbox(ICON_FA_DRAW_POLYGON "  绘制射线", &show_draw_Line);
            ImGui::Checkbox(ICON_FA_CAT "  坤坤窗口", &show_another_window);
            ImGui::Unindent(16.0f);
            ImGui::Spacing();
        }

        // ============= 分组 3：控件测试 =============
        if (ImGui::CollapsingHeader(ICON_FA_SLIDERS_H "  控件测试", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(16.0f);

            ImGui::TextDisabled("滑块");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderFloat("##float_slider", &f, 0.0f, 1.0f, "数值 = %.2f");

            ImGui::TextDisabled("取色器");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::ColorEdit4("##color_picker", (float *) &clear_color);

            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_PLUS "  点击 +1", ImVec2(0, 0))) {
                counter++;
            }
            ImGui::SameLine();
            ImGui::Text("计数 = %d", counter);

            ImGui::Unindent(16.0f);
            ImGui::Spacing();
        }

        // ============= 分组 4：运行状态 =============
        if (ImGui::CollapsingHeader(ICON_FA_INFO_CIRCLE "  运行状态", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(16.0f);
            ImGui::Text("窗口集中 = %d", ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow));
            ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f),
                               ICON_FA_TACHOMETER_ALT "  %.1f FPS   (%.3f ms/frame)",
                               ImGui::GetIO().Framerate,
                               1000.0f / ImGui::GetIO().Framerate);
            ImGui::Unindent(16.0f);
            ImGui::Spacing();
        }

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
        ImGui::GetForegroundDrawList()->AddLine(ImVec2(0, 0), ImVec2(displayInfo.width, displayInfo.height), IM_COL32(255, 0, 0, 255), 4);

}
