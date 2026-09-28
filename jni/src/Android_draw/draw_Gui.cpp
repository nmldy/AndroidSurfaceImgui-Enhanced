#include "draw.h"

#include "My_font/zh_Font.h"
#include "My_font/fontawesome-brands.h"
#include "My_font/fontawesome-regular.h"
#include "My_font/fontawesome-solid.h"
#include "My_font/gui_icon.h"

#include "My_icon/pic_ZhenAiKun_png.h"

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <vector>
#include <cmath>

bool permeate_record = false;
bool permeate_record_ini = false;
struct Last_ImRect LastCoordinate = {0, 0, 0, 0};

std::unique_ptr<AndroidImgui> graphics;
ANativeWindow *window = NULL;
android::ANativeWindowCreator::DisplayInfo displayInfo;
ImGuiWindow *g_window = NULL;
int abs_ScreenX = 0, abs_ScreenY = 0;
int native_window_screen_x = 0, native_window_screen_y = 0;

TextureInfo Aekun_image{};

ImFont* zh_font = NULL;
ImFont* icon_font_0 = NULL;
ImFont* icon_font_1 = NULL;
ImFont* icon_font_2 = NULL;

static bool g_need_reset_interaction = false;
static bool g_wait_release = false;

static ImVec2 g_user_exp_pos  = ImVec2(-1.0f, -1.0f);
static ImVec2 g_user_exp_size = ImVec2(-1.0f, -1.0f);
static bool   g_user_exp_placed = false;

struct GlassAnimState {
    float  alpha          = 0.0f;
    float  target_alpha   = 0.98f;
    bool   colors_inited  = false;
    ImVec4 colors_current[ImGuiCol_COUNT];
    ImVec4 colors_target[ImGuiCol_COUNT];
};
static GlassAnimState g_anim;

static volatile int g_volume_toggle_request = 0;
static bool         g_volume_thread_started = false;
static bool         g_ui_hidden             = false;
static float        g_hide_progress         = 0.0f;

static bool IsLandscape() {
    return native_window_screen_x > native_window_screen_y;
}

static ImVec2 CalcDefaultExpandedSize() {
    float sx = (float)native_window_screen_x;
    float sy = (float)native_window_screen_y;
    if (IsLandscape()) return ImVec2(sx * 0.72f, sy * 0.78f);
    return ImVec2(sx * 0.86f, sy * 0.62f);
}

static ImVec2 CalcDefaultExpandedPos(ImVec2 sz) {
    float x = ((float)native_window_screen_x - sz.x) * 0.5f;
    float y = ((float)native_window_screen_y - sz.y) * 0.55f;
    return ImVec2(x, y);
}

static ImVec2 CalcCapsuleSize() {
    float sx = (float)native_window_screen_x;
    if (IsLandscape()) {
        float w = sx * 0.45f;
        if (w > 1200.0f) w = 1200.0f;
        return ImVec2(w, 140.0f);
    }
    float w = sx * 0.55f;
    if (w > 900.0f) w = 900.0f;
    return ImVec2(w, 140.0f);
}

static ImVec2 CalcCapsulePos(ImVec2 sz) {
    float x = ((float)native_window_screen_x - sz.x) * 0.5f;
    float y = IsLandscape() ? 40.0f : 60.0f;
    return ImVec2(x, y);
}

static void ResetUserExpandedState() {
    g_user_exp_size   = ImVec2(-1.0f, -1.0f);
    g_user_exp_pos    = ImVec2(-1.0f, -1.0f);
    g_user_exp_placed = false;
}

static void* VolumeKeyThread(void*) {
    std::vector<int> fds;
    for (int i = 0; i < 32; ++i) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        uint8_t keybit[(KEY_MAX / 8) + 1] = {0};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) {
            close(fd);
            continue;
        }
        bool has_vol_up = keybit[KEY_VOLUMEUP   / 8] & (1 << (KEY_VOLUMEUP   % 8));
        bool has_vol_dn = keybit[KEY_VOLUMEDOWN / 8] & (1 << (KEY_VOLUMEDOWN % 8));
        if (has_vol_up || has_vol_dn) fds.push_back(fd);
        else close(fd);
    }
    if (fds.empty()) return nullptr;
    while (true) {
        for (int fd : fds) {
            struct input_event ev;
            while (read(fd, &ev, sizeof(ev)) == (ssize_t) sizeof(ev)) {
                if (ev.type == EV_KEY &&
                    (ev.code == KEY_VOLUMEUP || ev.code == KEY_VOLUMEDOWN) &&
                    ev.value == 1) {
                    __sync_fetch_and_add(&g_volume_toggle_request, 1);
                }
            }
        }
        usleep(20000);
    }
    return nullptr;
}

static void UpdateHideAnimation() {
    ImGuiIO &io = ImGui::GetIO();
    float dt = io.DeltaTime;
    if (dt <= 0.0f || dt > 0.1f) dt = 1.0f / 60.0f;
    const float target = g_ui_hidden ? 1.0f : 0.0f;
    const float k = 6.0f;
    float t = 1.0f - expf(-k * dt);
    if (t > 1.0f) t = 1.0f;
    g_hide_progress += (target - g_hide_progress) * t;
    if (fabsf(g_hide_progress - target) < 0.0015f) g_hide_progress = target;
}

static void ResetMouseToNeutralPosition() {
    if (ImGui::GetCurrentContext() == nullptr) return;
    ImGuiIO &io = ImGui::GetIO();
    io.MousePos     = ImVec2(-FLT_MAX, -FLT_MAX);
    io.MousePosPrev = ImVec2(-FLT_MAX, -FLT_MAX);
    io.MouseDelta   = ImVec2(0.0f, 0.0f);
    io.MouseDown[0] = false;
    io.MouseDown[1] = false;
    io.MouseDown[2] = false;
    g_wait_release = true;
}

static void CaptureCurrentColorsAsTarget() {
    ImGuiStyle &style = ImGui::GetStyle();
    for (int i = 0; i < ImGuiCol_COUNT; ++i) g_anim.colors_target[i] = style.Colors[i];
}

static void ResetGlassFadeIn() {
    g_anim.alpha = 0.0f;
    g_anim.colors_inited = false;
}

static void UpdateGlassAnimation() {
    ImGuiIO &io = ImGui::GetIO();
    float dt = io.DeltaTime;
    if (dt <= 0.0f || dt > 0.1f) dt = 1.0f / 60.0f;

    if (g_anim.alpha < g_anim.target_alpha) {
        const float speed = 0.7f + 0.5f * (1.0f - g_anim.alpha / g_anim.target_alpha);
        g_anim.alpha += dt * speed;
        if (g_anim.alpha > g_anim.target_alpha) g_anim.alpha = g_anim.target_alpha;
    }

    if (!g_anim.colors_inited) {
        for (int i = 0; i < ImGuiCol_COUNT; ++i) g_anim.colors_current[i] = g_anim.colors_target[i];
        g_anim.colors_inited = true;
    } else {
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
    ImGui::GetStyle().Alpha = g_anim.alpha;
}

static void ApplyLiquidGlassShapeAndSpacing() {
    ImGuiStyle &style = ImGui::GetStyle();
    style = ImGuiStyle();

    style.WindowRounding    = 22.0f;
    style.ChildRounding     = 16.0f;
    style.FrameRounding     = 10.0f;
    style.PopupRounding     = 16.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding      = 8.0f;
    style.TabRounding       = 12.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(16.0f, 16.0f);
    style.FramePadding      = ImVec2(10.0f, 5.0f);
    style.ItemSpacing       = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 6.0f);

    style.ScrollbarSize     = 14.0f;
    style.WindowMinSize     = ImVec2(120.0f, 80.0f);

    style.AntiAliasedLines = true;
    style.AntiAliasedFill  = true;
}

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
    c[ImGuiCol_Header]           = ImVec4(0.45f, 0.75f, 1.00f, 0.28f);
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
    c[ImGuiCol_Header]           = ImVec4(0.20f, 0.50f, 0.95f, 0.35f);
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
    CaptureCurrentColorsAsTarget();
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

    ApplyLiquidGlassWindowStyle();
    ResetGlassFadeIn();

    g_ui_hidden     = false;
    g_hide_progress = 0.0f;

    ResetUserExpandedState();
}

void screen_config() {
    ::displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
}

static void RebuildForOrientation() {
    if (g_window != nullptr) {
        LastCoordinate.Pos_x = g_window->Pos.x;
        LastCoordinate.Pos_y = g_window->Pos.y;
        LastCoordinate.Size_x = g_window->Size.x;
        LastCoordinate.Size_y = g_window->Size.y;
    }

    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(::window);

    native_window_screen_x = displayInfo.width;
    native_window_screen_y = displayInfo.height;
    abs_ScreenX = displayInfo.width;
    abs_ScreenY = displayInfo.height;

    ::window = android::ANativeWindowCreator::Create("AImGui", native_window_screen_x, native_window_screen_y, permeate_record);
    graphics->Init_Render(::window, native_window_screen_x, native_window_screen_y);

    ::init_My_drawdata();

    Touch::Close();
    Touch::Init({(float)abs_ScreenX, (float)abs_ScreenY}, false);
    Touch::setOrientation((int)displayInfo.orientation);

    g_window = NULL;
    ResetMouseToNeutralPosition();
    g_need_reset_interaction = true;

    ResetGlassFadeIn();
    g_ui_hidden     = false;
    g_hide_progress = 0.0f;

    ResetUserExpandedState();
}

void drawBegin() {
    if (::permeate_record_ini) {
        if (g_window != nullptr) {
            LastCoordinate.Pos_x = ::g_window->Pos.x;
            LastCoordinate.Pos_y = ::g_window->Pos.y;
            LastCoordinate.Size_x = ::g_window->Size.x;
            LastCoordinate.Size_y = ::g_window->Size.y;
        } else {
            LastCoordinate = {0, 0, 0, 0};
        }

        graphics->Shutdown();
        android::ANativeWindowCreator::Destroy(::window);
        ::window = android::ANativeWindowCreator::Create("AImGui", native_window_screen_x, native_window_screen_y, permeate_record);
        graphics->Init_Render(::window, native_window_screen_x, native_window_screen_y);
        ::init_My_drawdata();

        g_window = NULL;
        ResetMouseToNeutralPosition();
        g_need_reset_interaction = true;
    }

    static uint32_t orientation = -1;
    screen_config();
    if (orientation != displayInfo.orientation) {
        orientation = displayInfo.orientation;

        const bool size_changed =
            (displayInfo.width  != native_window_screen_x) ||
            (displayInfo.height != native_window_screen_y);

        if (size_changed) {
            RebuildForOrientation();
        } else {
            Touch::setOrientation((int)displayInfo.orientation);
        }
    }
}

// =====================================================================
//  主 UI
//  布局：左侧导航 + 右侧内容
// =====================================================================
void Layout_tick_UI(bool *main_thread_flag) {
    if (!g_volume_thread_started) {
        g_volume_thread_started = true;
        pthread_t t;
        if (pthread_create(&t, nullptr, VolumeKeyThread, nullptr) == 0) {
            pthread_detach(t);
        }
    }

    int volume_presses = __sync_lock_test_and_set(&g_volume_toggle_request, 0);
    while (volume_presses-- > 0) {
        g_ui_hidden = !g_ui_hidden;
    }

    UpdateGlassAnimation();
    UpdateHideAnimation();

    if (g_wait_release) {
        ImGuiIO &io = ImGui::GetIO();
        if (!io.MouseDown[0] && !io.MouseDown[1] && !io.MouseDown[2]) {
            g_wait_release = false;
        } else {
            io.MousePos     = ImVec2(-FLT_MAX, -FLT_MAX);
            io.MousePosPrev = ImVec2(-FLT_MAX, -FLT_MAX);
            io.MouseDelta   = ImVec2(0.0f, 0.0f);
            io.MouseDown[0] = false;
            io.MouseDown[1] = false;
            io.MouseDown[2] = false;
        }
    }

    if (g_need_reset_interaction) {
        g_need_reset_interaction = false;
        ImGui::ClearActiveID();
        if (ImGuiContext *ctx = ImGui::GetCurrentContext()) {
            ctx->HoveredId = 0;
        }
    }

    if (g_user_exp_size.x < 0.0f) {
        g_user_exp_size   = CalcDefaultExpandedSize();
        g_user_exp_pos    = CalcDefaultExpandedPos(g_user_exp_size);
        g_user_exp_placed = false;
    }

    static bool show_draw_Line = false;
    static bool show_demo_window = false;
    static bool show_another_window = false;

    {
        static float f = 0.0f;
        static int counter = 0;
        static int style_idx = 0;
        static ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

        // 左侧导航当前选中页
        static int g_current_page = 0;

        const bool fully_expanded = (!g_ui_hidden && g_hide_progress < 0.01f);

        ImVec2 cap_size = CalcCapsuleSize();
        ImVec2 cap_pos  = CalcCapsulePos(cap_size);

        float move_t = g_hide_progress / 0.6f;
        if (move_t > 1.0f) move_t = 1.0f;

        float shrink_t = (g_hide_progress - 0.6f) / 0.4f;
        if (shrink_t < 0.0f) shrink_t = 0.0f;
        if (shrink_t > 1.0f) shrink_t = 1.0f;

        ImVec2 cur_size = ImVec2(
            g_user_exp_size.x + (cap_size.x - g_user_exp_size.x) * shrink_t,
            g_user_exp_size.y + (cap_size.y - g_user_exp_size.y) * shrink_t
        );

        float pos_y = g_user_exp_pos.y + (cap_pos.y - g_user_exp_pos.y) * move_t;
        float cur_x = ((float)native_window_screen_x - cur_size.x) * 0.5f;
        ImVec2 cur_pos = ImVec2(cur_x, pos_y);

        // 残影
        if (g_hide_progress > 0.05f && g_hide_progress < 0.95f) {
            ImDrawList *dl = ImGui::GetForegroundDrawList();
            float intensity = 1.0f - fabsf(g_hide_progress - 0.5f) * 2.0f;
            if (intensity < 0.0f) intensity = 0.0f;

            const int TRAIL_COUNT = 6;
            for (int i = 1; i <= TRAIL_COUNT; ++i) {
                float t = (float)i / (float)TRAIL_COUNT;
                float trail_offset_y = (float)i * 26.0f;
                float size_lerp = t * 0.35f;
                ImVec2 trail_size = ImVec2(
                    cur_size.x + (g_user_exp_size.x - cur_size.x) * size_lerp,
                    cur_size.y + (g_user_exp_size.y - cur_size.y) * size_lerp
                );
                ImVec2 trail_pos = ImVec2(
                    ((float)native_window_screen_x - trail_size.x) * 0.5f,
                    cur_pos.y + trail_offset_y
                );
                float alpha = (1.0f - t) * 0.22f * intensity;
                float trail_rounding = trail_size.y * 0.5f;

                dl->AddRectFilled(
                    trail_pos,
                    ImVec2(trail_pos.x + trail_size.x, trail_pos.y + trail_size.y),
                    IM_COL32(70, 130, 220, (int)(alpha * 255)),
                    trail_rounding
                );
            }
        }

        float dyn_rounding = 22.0f + (cap_size.y * 0.5f - 22.0f) * shrink_t;
        ImVec2 dyn_padding(
            16.0f + (8.0f - 16.0f) * shrink_t,
            16.0f + (8.0f - 16.0f) * shrink_t
        );

        if (!fully_expanded || !g_user_exp_placed) {
            ImGui::SetNextWindowPos(cur_pos);
            ImGui::SetNextWindowSize(cur_size);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, dyn_rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, dyn_padding);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;

        if (!fully_expanded) {
            flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        }

        ImGui::Begin("AndroidSurfaceImguiEnhanced", main_thread_flag, flags);

        if (shrink_t < 0.5f) {
            // ============ 完整内容 ============
            if (::permeate_record_ini) {
                if (LastCoordinate.Size_x > 1.0f && LastCoordinate.Size_y > 1.0f) {
                    ImGui::SetWindowPos({LastCoordinate.Pos_x, LastCoordinate.Pos_y});
                    ImGui::SetWindowSize({LastCoordinate.Size_x, LastCoordinate.Size_y});
                }
                permeate_record_ini = false;
            }

            // 顶部标题栏
            ImGui::TextDisabled(ICON_FA_MICROCHIP "  %s", graphics->RenderName);
            ImGui::SameLine();
            ImGui::TextDisabled("  ·  " ICON_FA_CODE "  %s", ImGui::GetVersion());
            ImGui::SameLine();
            ImGui::TextDisabled("  ·  " ICON_FA_MOBILE_ALT "  %s",
                                IsLandscape() ? "横屏" : "竖屏");
            ImGui::Separator();
            ImGui::Spacing();

            // ============ 左右分栏 ============
            // 左侧导航宽度按屏幕宽 16% 自适应，限制 160~300px
            const float nav_w = ImClamp((float)native_window_screen_x * 0.16f, 160.0f, 300.0f);

            // 左侧导航
            ImGui::BeginChild("##left_nav", ImVec2(nav_w, 0), false);
            {
                ImGui::TextDisabled(ICON_FA_LIST "  功能");
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Selectable(ICON_FA_PALETTE "  外观主题", g_current_page == 0))
                    g_current_page = 0;
                ImGui::Spacing();

                if (ImGui::Selectable(ICON_FA_WINDOW_RESTORE "  窗口控制", g_current_page == 1))
                    g_current_page = 1;
                ImGui::Spacing();

                if (ImGui::Selectable(ICON_FA_SLIDERS_H "  控件测试", g_current_page == 2))
                    g_current_page = 2;
                ImGui::Spacing();

                if (ImGui::Selectable(ICON_FA_INFO_CIRCLE "  运行状态", g_current_page == 3))
                    g_current_page = 3;
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // 右侧内容
            ImGui::BeginChild("##right_content", ImVec2(0, 0), false);
            {
                switch (g_current_page) {
                    case 0: {
                        ImGui::Text(ICON_FA_PALETTE "  外观主题");
                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::TextDisabled("整体视觉风格（切换带平滑过渡）");
                        ImGui::SetNextItemWidth(-1.0f);
                        if (ImGui::Combo("##theme_combo", &style_idx, "深色主题\0亮色主题\0经典主题\0")) {
                            switch (style_idx) {
                                case 0: ApplyLiquidGlassWindowStyle(); break;
                                case 1: ApplyLiquidGlassLightStyle();  break;
                                case 2:
                                    ImGui::StyleColorsClassic();
                                    CaptureCurrentColorsAsTarget();
                                    break;
                            }
                        }
                    } break;

                    case 1: {
                        ImGui::Text(ICON_FA_WINDOW_RESTORE "  窗口控制");
                        ImGui::Separator();
                        ImGui::Spacing();

                        if (ImGui::Checkbox(ICON_FA_VIDEO "  过录制", &::permeate_record)) {
                            ::permeate_record_ini = true;
                        }
                        ImGui::Checkbox(ICON_FA_IMAGE "  演示窗口", &show_demo_window);
                        ImGui::Checkbox(ICON_FA_DRAW_POLYGON "  绘制射线", &show_draw_Line);
                        ImGui::Checkbox(ICON_FA_CAT "  坤坤窗口", &show_another_window);
                    } break;

                    case 2: {
                        ImGui::Text(ICON_FA_SLIDERS_H "  控件测试");
                        ImGui::Separator();
                        ImGui::Spacing();

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
                    } break;

                    case 3: {
                        ImGui::Text(ICON_FA_INFO_CIRCLE "  运行状态");
                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::Text("窗口集中 = %d", ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow));
                        const float fps = ImGui::GetIO().Framerate;
                        const float ms_per_frame = (fps > 0.0f) ? (1000.0f / fps) : 0.0f;
                        ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f),
                                           ICON_FA_TACHOMETER_ALT "  %.1f FPS   (%.3f ms/frame)",
                                           fps, ms_per_frame);
                        ImGui::Spacing();
                        ImGui::TextDisabled(ICON_FA_VOLUME_UP " 音量上/下 : 收起 / 展开");
                    } break;
                }
            }
            ImGui::EndChild();

            g_window = ImGui::GetCurrentWindow();

        } else {
            // 胶囊内容
            float text_h = ImGui::GetTextLineHeight();
            ImGui::SetCursorPosY((cur_size.y - text_h) * 0.5f);

            char buf[128];
            snprintf(buf, sizeof(buf), "%s   %s   %.0f FPS",
                     ICON_FA_MICROCHIP,
                     graphics->RenderName,
                     ImGui::GetIO().Framerate);
            float text_w = ImGui::CalcTextSize(buf).x;
            ImGui::SetCursorPosX((cur_size.x - text_w) * 0.5f);

            ImGui::TextUnformatted(buf);

            g_window = ImGui::GetCurrentWindow();
        }

        if (fully_expanded) {
            g_user_exp_pos  = ImGui::GetWindowPos();
            g_user_exp_size = ImGui::GetWindowSize();
            g_user_exp_placed = true;
        }

        ImGui::End();

        ImGui::PopStyleVar(3);
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
