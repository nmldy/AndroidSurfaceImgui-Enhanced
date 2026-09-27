#include "draw.h"    //绘制套
#include "AndroidImgui.h"     //创建绘制套
#include "GraphicsManager.h" //获取 当前渲染模式

#include <android/native_window.h>   // ANativeWindow_getWidth/Height


// =====================================================================
// 方向 / 分辨率变化时重建 Surface 与所有相关子系统
// 仅作为文件内静态辅助函数，不改变任何已有函数签名
// =====================================================================
static void RebuildSurfaceForDisplayChange() {
    // 1. 关闭旧渲染上下文（内部会销毁 ImGui 上下文、Vulkan 资源、释放旧 Surface 引用）
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(::window);

    // 2. 刷新屏幕信息（用于后续 Touch::setOrientation）
    ::screen_config();

    // 3. 让 ANativeWindowCreator 内部直接使用 SurfaceFlinger 返回的 layerStackSpaceRect
    //    作为 Surface buffer 尺寸，不再经过 displayInfo 的 max/min 重算。
    //    这是修复横屏错位的关键：Surface buffer 尺寸必须与 layerStackSpaceRect 一致。
    ::window = android::ANativeWindowCreator::Create(
            "AImGui", -1, -1, permeate_record);

    // 4. 读回 Surface 的真实 buffer 尺寸，作为 ImGui 的 DisplaySize。
    //    这样无论横屏竖屏，Surface 与 ImGui 坐标系都严格一致，不会出现下半屏被裁。
    ::native_window_screen_x = ANativeWindow_getWidth(::window);
    ::native_window_screen_y = ANativeWindow_getHeight(::window);
    ::abs_ScreenX = ::native_window_screen_x;
    ::abs_ScreenY = ::native_window_screen_y;

    graphics->Init_Render(::window, ::native_window_screen_x, ::native_window_screen_y);

    // 5. 重建触摸子系统（内部会根据新尺寸重算 touch_scale）
    Touch::Close();
    Touch::Init({(float)::abs_ScreenX, (float)::abs_ScreenY}, false);
    Touch::setOrientation(::displayInfo.orientation);

    // 6. 重建绘制数据（界面元素坐标依赖屏幕尺寸）
    ::init_My_drawdata();
}


int main(int argc, char *argv[]) {
    ::graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);

    // 获取屏幕信息
    ::screen_config();

    // 让 ANativeWindowCreator 内部只从 SurfaceFlinger 的 layerStackSpaceRect 取尺寸，
    // 避免经过 displayInfo 的 max/min 重算造成横屏方向错位。
    ::window = android::ANativeWindowCreator::Create("AImGui", -1, -1, permeate_record);

    // 读回 Surface 的真实 buffer 尺寸作为 ImGui DisplaySize
    ::native_window_screen_x = ANativeWindow_getWidth(::window);
    ::native_window_screen_y = ANativeWindow_getHeight(::window);
    ::abs_ScreenX = ::native_window_screen_x;
    ::abs_ScreenY = ::native_window_screen_y;

    graphics->Init_Render(::window, ::native_window_screen_x, ::native_window_screen_y);

    Touch::Init({(float)::abs_ScreenX, (float)::abs_ScreenY}, false); //最后一个参数改成true 只监听
    Touch::setOrientation(::displayInfo.orientation);


    ::init_My_drawdata(); //初始化绘制数据

    // 记录初始方向 / 尺寸，用于检测运行中的变化
    int lastOrientation = (int)::displayInfo.orientation;
    int lastWidth       = (int)::native_window_screen_x;
    int lastHeight      = (int)::native_window_screen_y;

    static bool flag = true;
    while (flag) {
        // ===== 自动检测横竖屏 / 显示设备变化 =====
        // 仅在方向或分辨率变化时才触发重建，正常帧仅一次 IPC 查询（微秒级开销）
        {
            auto currentDisplayInfo = android::ANativeWindowCreator::GetDisplayInfo();
            if ((int)currentDisplayInfo.orientation != lastOrientation ||
                (int)currentDisplayInfo.width       != lastWidth ||
                (int)currentDisplayInfo.height      != lastHeight) {
                // 方向 / 分辨率变了 -> 完整重建
                RebuildSurfaceForDisplayChange();

                // 更新缓存，避免下一帧重复触发
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

    // graphics->DeleteTexture(image);
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(::window);
    return 0;
}
