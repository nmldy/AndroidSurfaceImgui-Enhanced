#include "draw.h"    //绘制套
#include "AndroidImgui.h"     //创建绘制套
#include "GraphicsManager.h" //获取 当前渲染模式

#include "imgui.h"           // 用于重建后重置鼠标输入状态
#include <unistd.h>          // usleep


// =====================================================================
// 重建后重置 ImGui 鼠标输入状态。
// 必须清全所有与"上一帧鼠标状态"有关的字段，否则第一次触摸会被
// ImGui 误识别为"从窗口边缘开始拖动"，表现为"切换后只能朝一个方向伸缩"。
// 仅作为文件内静态辅助函数，不改变任何已有函数签名。
// =====================================================================
static void ResetImGuiInputState() {
    ImGuiIO &io = ImGui::GetIO();

    // 1) 推送"鼠标不在屏幕"事件，让 ImGui 内部事件队列从干净状态起步
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    io.AddMouseButtonEvent(0, false);
    io.AddMouseButtonEvent(1, false);
    io.AddMouseButtonEvent(2, false);
    io.AddMouseWheelEvent(0.0f, 0.0f);

    // 2) 直接清空所有与"上一帧鼠标状态"相关的字段
    const ImVec2 offscreen(-FLT_MAX, -FLT_MAX);
    io.MousePos     = offscreen;
    io.MousePosPrev = offscreen;
    io.MouseDelta   = ImVec2(0.0f, 0.0f);
    io.MouseWheel   = 0.0f;
    io.MouseWheelH  = 0.0f;
    for (int i = 0; i < 5; ++i) {
        io.MouseDown[i]            = false;
        io.MouseClicked[i]         = false;
        io.MouseReleased[i]        = false;
        io.MouseDoubleClicked[i]   = false;
        io.MouseClickedCount[i]    = 0;
        io.MouseClickedLastCount[i]= 0;
        io.MouseClickedPos[i]      = offscreen;
    }
}


// =====================================================================
// 方向 / 分辨率变化时重建 Surface 与所有相关子系统
// =====================================================================
static void RebuildSurfaceForDisplayChange() {
    // 1. 关闭旧渲染上下文
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(::window);

    // 2. 刷新屏幕信息（displayInfo.width/height 是按 orientation 重算后的物理方向尺寸）
    ::screen_config();

    // 3. 明确传入 displayInfo 的物理方向尺寸（竖屏 1080x2400 / 横屏 2400x1080）
    //    不要传 -1，否则 Create 内部会用 layerStackSpaceRect（横屏 2400x1080），
    //    与物理屏幕方向不一致，导致 UI 只显示在屏幕一部分。
    ::window = android::ANativeWindowCreator::Create(
            "AImGui", (int32_t)::displayInfo.width, (int32_t)::displayInfo.height, permeate_record);

    ::native_window_screen_x = ::displayInfo.width;
    ::native_window_screen_y = ::displayInfo.height;
    ::abs_ScreenX = ::displayInfo.width;
    ::abs_ScreenY = ::displayInfo.height;

    graphics->Init_Render(::window, ::native_window_screen_x, ::native_window_screen_y);

    // 4. 重建触摸子系统
    Touch::Close();
    Touch::Init({(float)::abs_ScreenX, (float)::abs_ScreenY}, false);
    Touch::setOrientation(::displayInfo.orientation);

    // 5. 重建绘制数据
    ::init_My_drawdata();

    // 6. 等一小会儿，让 Touch::Init 内部清空残留事件、让用户手指（如果还按着）松开
    usleep(80000);   // 80ms

    // 7. 清空 ImGui 鼠标输入状态，避免第一次触摸被当成拖动窗口边缘
    ResetImGuiInputState();
}


int main(int argc, char *argv[]) {
    ::graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);

    // 获取屏幕信息
    ::screen_config();

    // 明确传入 displayInfo 的物理方向尺寸
    ::window = android::ANativeWindowCreator::Create(
            "AImGui", (int32_t)::displayInfo.width, (int32_t)::displayInfo.height, permeate_record);

    ::native_window_screen_x = ::displayInfo.width;
    ::native_window_screen_y = ::displayInfo.height;
    ::abs_ScreenX = ::displayInfo.width;
    ::abs_ScreenY = ::displayInfo.height;

    graphics->Init_Render(::window, ::native_window_screen_x, ::native_window_screen_y);

    Touch::Init({(float)::abs_ScreenX, (float)::abs_ScreenY}, false);
    Touch::setOrientation(displayInfo.orientation);

    ::init_My_drawdata();

    // 首次启动也清一次
    ResetImGuiInputState();

    // 记录初始方向 / 尺寸
    int lastOrientation = (int)::displayInfo.orientation;
    int lastWidth       = (int)::native_window_screen_x;
    int lastHeight      = (int)::native_window_screen_y;

    static bool flag = true;
    while (flag) {
        // ===== 自动检测横竖屏 / 显示设备变化 =====
        {
            auto currentDisplayInfo = android::ANativeWindowCreator::GetDisplayInfo();
            if ((int)currentDisplayInfo.orientation != lastOrientation ||
                (int)currentDisplayInfo.width       != lastWidth ||
                (int)currentDisplayInfo.height      != lastHeight) {
                RebuildSurfaceForDisplayChange();

                lastOrientation = (int)::displayInfo.orientation;
                lastWidth       = (int)::native_window_screen_x;
                lastHeight      = (int)::native_window_screen_y;
            }
        }
        // ================================================

        drawBegin();
        if (permeate_record == false) {
            android::ANativeWindowCreator::ProcessMirrorDisplay();
        }
        graphics->NewFrame();

        Layout_tick_UI(&flag);

        graphics->EndFrame();
    }

    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(::window);
    return 0;
}
