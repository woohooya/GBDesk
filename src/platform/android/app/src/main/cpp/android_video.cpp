#include "android_video.h"

#include <android/log.h>
#include <algorithm>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "GBDeskVideo", __VA_ARGS__)

namespace {

const char* VERTEX_SHADER = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
out vec2 vTex;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTex = aTex;
}
)";

const char* FRAGMENT_SHADER = R"(#version 300 es
precision mediump float;
in vec2 vTex;
uniform sampler2D uTex;
out vec4 fragColor;
void main() {
    vec3 rgb = texture(uTex, vTex).rgb;
    fragColor = vec4(rgb, 1.0);
}
)";

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(shader, sizeof(buf), nullptr, buf);
        LOGE("Shader compile error: %s", buf);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

AndroidVideo::~AndroidVideo() {
    setWindow(nullptr);
}

void AndroidVideo::setWindow(ANativeWindow* window) {
    std::lock_guard<std::mutex> lock(m_mutex);
    destroyEGL();
    if (m_window) {
        ANativeWindow_release(m_window);
        m_window = nullptr;
    }
    if (window) {
        m_window = window;
        initEGL();
    }
}

void AndroidVideo::setFrameSize(unsigned width, unsigned height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_frameWidth != width || m_frameHeight != height) {
        m_frameWidth = width;
        m_frameHeight = height;
        m_sizeChanged = true;
    }
}

bool AndroidVideo::initEGL() {
    if (!m_window) {
        return false;
    }
    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_display == EGL_NO_DISPLAY || !eglInitialize(m_display, nullptr, nullptr)) {
        return false;
    }

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(m_display, attribs, &config, 1, &numConfigs) || numConfigs == 0) {
        destroyEGL();
        return false;
    }

    m_surface = eglCreateWindowSurface(m_display, config, m_window, nullptr);
    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    m_context = eglCreateContext(m_display, config, EGL_NO_CONTEXT, ctxAttribs);
    if (m_surface == EGL_NO_SURFACE || m_context == EGL_NO_CONTEXT) {
        destroyEGL();
        return false;
    }
    if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context)) {
        destroyEGL();
        return false;
    }
    eglSwapInterval(m_display, 1);

    GLuint vs = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    const float verts[] = {
        -1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
    };
    glGenVertexArrays(1, &m_vao);
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_sizeChanged = true;

    return true;
}

void AndroidVideo::destroyEGL() {
    if (m_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_texture) {
            glDeleteTextures(1, &m_texture);
            m_texture = 0;
        }
        if (m_vao) {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }
        if (m_program) {
            glDeleteProgram(m_program);
            m_program = 0;
        }
        if (m_surface != EGL_NO_SURFACE) {
            eglDestroySurface(m_display, m_surface);
            m_surface = EGL_NO_SURFACE;
        }
        if (m_context != EGL_NO_CONTEXT) {
            eglDestroyContext(m_display, m_context);
            m_context = EGL_NO_CONTEXT;
        }
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
    }
}

void AndroidVideo::renderFrame(const uint32_t* pixels) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_display == EGL_NO_DISPLAY || m_surface == EGL_NO_SURFACE || !pixels) {
        return;
    }
    if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context)) {
        return;
    }

    EGLint winW = 0;
    EGLint winH = 0;
    eglQuerySurface(m_display, m_surface, EGL_WIDTH, &winW);
    eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &winH);
    if (winW <= 0 || winH <= 0) {
        return;
    }

    glViewport(0, 0, winW, winH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Preserve aspect ratio
    float targetAspect = static_cast<float>(m_frameWidth) / static_cast<float>(m_frameHeight);
    float winAspect = static_cast<float>(winW) / static_cast<float>(winH);
    int vpW = winW;
    int vpH = winH;
    int vpX = 0;
    int vpY = 0;
    if (winAspect > targetAspect) {
        vpW = static_cast<int>(winH * targetAspect);
        vpX = (winW - vpW) / 2;
    } else {
        vpH = static_cast<int>(winW / targetAspect);
        vpY = (winH - vpH) / 2;
    }
    glViewport(vpX, vpY, vpW, vpH);

    glBindTexture(GL_TEXTURE_2D, m_texture);
    if (m_sizeChanged) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_frameWidth, m_frameHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        m_sizeChanged = false;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }

    glUseProgram(m_program);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    eglSwapBuffers(m_display, m_surface);
}
