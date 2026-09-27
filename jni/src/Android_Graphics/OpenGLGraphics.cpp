#include <GLES3/gl3.h>
#include <android/native_window.h>
#include "OpenGLGraphics.h"
#include "imgui_impl_opengl3.h"


#if !defined(EGL_OPENGL_ES3_BIT)
    // 如果 EGL_OPENGL_ES3_BIT 未定义，则编译这部分代码
    #define EGL_OPENGL_ES3_BIT                0x00000040
#endif

bool OpenGLGraphics::Create() {
    const EGLint egl_attributes[] = {EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
                                     EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16,
                                     EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE,
                                     EGL_WINDOW_BIT, EGL_NONE};

    m_EglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(m_EglDisplay, nullptr, nullptr);
    EGLint num_configs = 0;
    eglChooseConfig(m_EglDisplay, egl_attributes, nullptr, 0, &num_configs);
    EGLConfig egl_config;
    eglChooseConfig(m_EglDisplay, egl_attributes, &egl_config, 1, &num_configs);
    EGLint egl_format;
    eglGetConfigAttrib(m_EglDisplay, egl_config, EGL_NATIVE_VISUAL_ID, &egl_format);
    ANativeWindow_setBuffersGeometry(m_Window, 0, 0, egl_format);

    const EGLint egl_context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    m_EglContext = eglCreateContext(m_EglDisplay, egl_config, EGL_NO_CONTEXT,
                                    egl_context_attributes);
    m_EglSurface = eglCreateWindowSurface(m_EglDisplay, egl_config, m_Window, nullptr);
    eglMakeCurrent(m_EglDisplay, m_EglSurface, m_EglSurface, m_EglContext);
    glClearColor(0.0, 0.0, 0.0, 0.0);

    // 创建时同步一次真实尺寸，避免首帧使用默认/过期尺寸导致裁剪
    int width  = ANativeWindow_getWidth(m_Window);
    int height = ANativeWindow_getHeight(m_Window);
    if (width > 0 && height > 0) {
        m_Width  = width;
        m_Height = height;
        m_LastWidth  = width;
        m_LastHeight = height;
        glViewport(0, 0, width, height);
    }
    return true;
}

void OpenGLGraphics::Setup() {
    ImGui_ImplOpenGL3_Init("#version 300 es");
}

void OpenGLGraphics::PrepareFrame(bool resize) {
    // 修复：之前此处直接丢弃 resize 参数，从不重新读取 ANativeWindow 尺寸，
    // 导致基类 m_Width / m_Height（上游用于设置 ImGuiIO::DisplaySize）长期停留在旧值。
    // 当 Surface 尺寸变化或与初始尺寸不一致时，ImGui 的投影矩阵 / 裁剪矩形会比真实屏幕小，
    // 控件被拖动到“旧边界”之外（表现为屏幕下半部分）就会被直接裁掉。
    int width  = ANativeWindow_getWidth(m_Window);
    int height = ANativeWindow_getHeight(m_Window);
    if (width > 0 && height > 0 &&
        (resize || width != m_LastWidth || height != m_LastHeight ||
         m_Width != width || m_Height != height)) {
        m_LastWidth  = width;
        m_LastHeight = height;
        m_Width      = width;
        m_Height     = height;
        // 显式设置视口，保证 glClear 与 ImGui 渲染都覆盖整个 Surface
        glViewport(0, 0, width, height);
    }
    ImGui_ImplOpenGL3_NewFrame();
}

void OpenGLGraphics::Render(ImDrawData *drawData) {
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(drawData);
    eglSwapBuffers(m_EglDisplay, m_EglSurface);
}

void OpenGLGraphics::PrepareShutdown() {
    ImGui_ImplOpenGL3_Shutdown();
}

void OpenGLGraphics::Cleanup() {
    eglMakeCurrent(m_EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(m_EglDisplay, m_EglContext);
    eglDestroySurface(m_EglDisplay, m_EglSurface);
    eglTerminate(m_EglDisplay);
    m_EglDisplay = EGL_NO_DISPLAY;
    m_EglSurface = EGL_NO_SURFACE;
    m_EglContext = EGL_NO_CONTEXT;
}

BaseTexData *OpenGLGraphics::LoadTexture(BaseTexData *tex, void *pixel_data) {
    auto tex_data = new OpenglTextureData();
    tex_data->Width = tex->Width;
    tex_data->Height = tex->Height;
    tex_data->Channels = tex->Channels;

    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_data->Width, tex_data->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);
    tex_data->DS = (void *) (intptr_t) textureId;
    return tex_data;
}

void OpenGLGraphics::RemoveTexture(BaseTexData *tex) {
    auto tex_data = (OpenglTextureData *) tex;
    auto textureId = (GLuint) (intptr_t) tex_data->DS;
    glDeleteTextures(1, &textureId);
    delete tex_data;
}
