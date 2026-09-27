#include "draw.h"    //绘制套
#include "AndroidImgui.h"     //创建绘制套
#include "GraphicsManager.h" //获取 当前渲染模式


// =====================================================================
// 新增：方向 / 分辨率变化时重建 Surface 与所有相关子系统
// 仅作为文件内静态辅助函数，不改变任何已有函数签名
// =====================================================================
static void RebuildSurfaceForDisplayChange() {
    // 1. 关闭旧渲染上下文（内部会销毁 ImGui 上下文、Vulkan 资源、释放旧 Surface 引用）
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(::window);

    // 2. 刷新屏幕信息（内部会调用 GetDisplayInfo，并按 orientation 计算物理方向尺寸）
    ::screen_config();

    // 3. 直接使用 displayInfo 的物理方向尺寸，横竖屏自适应
    //    竖屏 -> 1080 x 2400；横屏 -> 2400 x 1080
    ::native_window_screen_x = ::displayInfo.width;
    ::native_window_screen_y = ::displayInfo.height;
    ::abs_ScreenX = ::displayInfo.width;
    ::abs_ScreenY = ::displayInfo.height;

    // 4. 重建窗口与渲染（顺序与 main 初始化时一致）
    ::window = android::ANativeWindowCreator::Create(
            "AImGui", ::native_window_screen_x, ::native_window_screen_y, permeate_record);
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

    //获取屏幕信息
    ::screen_config();

    // 自动检测：直接使用 displayInfo 已按 orientation 计算好的物理方向尺寸
    // 竖屏手机 -> 1080 x 2400；横屏设备 -> 2400 x 1080
    // 之前用 max/min 强制转横屏，导致 Surface 与物理屏幕错位、下半屏无法显示
    ::native_window_screen_x = ::displayInfo.width;
    ::native_window_screen_y = ::displayInfo.height;
    ::abs_ScreenX = ::displayInfo.width;
    ::abs_ScreenY = ::displayInfo.height;

    ::window = android::ANativeWindowCreator::Create("AImGui", native_window_screen_x, native_window_screen_y, permeate_record);
    graphics->Init_Render(::window, native_window_screen_x, native_window_screen_y);

    Touch::Init({(float)::abs_ScreenX, (float)::abs_ScreenY}, false); //最后一个参数改成true 只监听
    Touch::setOrientation(displayInfo.orientation);


    ::init_My_drawdata(); //初始化绘制数据

    // 记录初始方向 / 尺寸，用于检测运行中的变化
    int lastOrientation = (int)::displayInfo.orientation;
    int lastWidth       = (int)::displayInfo.width;
    int lastHeight      = (int)::displayInfo.height;

    static bool flag = true;
    while (flag) {
        // ===== 新增：自动检测横竖屏 / 显示设备变化 =====
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
                lastWidth       = (int)::displayInfo.width;
                lastHeight      = (int)::displayInfo.height;
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
