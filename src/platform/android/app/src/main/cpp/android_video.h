#pragma once

#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstdint>
#include <mutex>

class AndroidVideo {
public:
    AndroidVideo() = default;
    ~AndroidVideo();

    void setWindow(ANativeWindow* window);
    void setFrameSize(unsigned width, unsigned height);
    void renderFrame(const uint32_t* pixels);
    void reset();

private:
    bool initEGL();
    void destroyEGL();

    std::mutex m_mutex;
    ANativeWindow* m_window = nullptr;
    ANativeWindow* m_pendingWindow = nullptr;
    bool m_windowChanged = false;

    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
    EGLContext m_context = EGL_NO_CONTEXT;
    GLuint m_program = 0;
    GLuint m_texture = 0;
    GLuint m_vao = 0;

    unsigned m_frameWidth = 240;
    unsigned m_frameHeight = 160;
    bool m_sizeChanged = true;
};
