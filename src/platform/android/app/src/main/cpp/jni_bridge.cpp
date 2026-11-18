#include "android_audio.h"
#include "android_video.h"

#include <android/log.h>
#include <android/native_window_jni.h>
#include <jni.h>

extern "C" {
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/directories.h>
#include <mgba/core/serialize.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/memory.h>
#include <mgba-util/vfs.h>
}

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "GBDeskJNI", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "GBDeskJNI", __VA_ARGS__)

namespace {

struct EmulatorInstance {
    mCore* core = nullptr;
    std::vector<uint32_t> videoBuffer;
    std::vector<int16_t> audioScratch;
    unsigned width = 240;
    unsigned height = 160;

    AndroidVideo video;
    AndroidAudio audio;

    std::thread thread;
    std::mutex coreMutex;
    std::atomic<bool> running{false};
    std::atomic<bool> paused{true};
    std::atomic<uint32_t> keys{0};
};

EmulatorInstance g_emu;

void emulationLoop() {
    using clock = std::chrono::steady_clock;
    const auto frameDuration = std::chrono::microseconds(16742); // ~59.73 fps

    while (g_emu.running.load(std::memory_order_acquire)) {
        if (g_emu.paused.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        auto start = clock::now();
        {
            std::lock_guard<std::mutex> lock(g_emu.coreMutex);
            if (g_emu.core) {
                g_emu.core->setKeys(g_emu.core, g_emu.keys.load(std::memory_order_relaxed));
                g_emu.core->runFrame(g_emu.core);

                mAudioBuffer* ab = g_emu.core->getAudioBuffer(g_emu.core);
                if (ab) {
                    size_t avail = mAudioBufferAvailable(ab);
                    if (avail > 0) {
                        if (g_emu.audioScratch.size() < avail * 2) {
                            g_emu.audioScratch.resize(avail * 2);
                        }
                        size_t read = mAudioBufferRead(ab, g_emu.audioScratch.data(), avail);
                        g_emu.audio.pushSamples(g_emu.audioScratch.data(), read);
                    }
                }

                g_emu.video.renderFrame(g_emu.videoBuffer.data());
            }
        }

        auto elapsed = clock::now() - start;
        if (elapsed < frameDuration) {
            std::this_thread::sleep_for(frameDuration - elapsed);
        }
    }
}

void stopEmulationInternal() {
    g_emu.running.store(false, std::memory_order_release);
    if (g_emu.thread.joinable()) {
        g_emu.thread.join();
    }
    g_emu.audio.stop();
    g_emu.video.reset();

    std::lock_guard<std::mutex> lock(g_emu.coreMutex);
    if (g_emu.core) {
        mCoreConfigDeinit(&g_emu.core->config);
        g_emu.core->deinit(g_emu.core);
        g_emu.core = nullptr;
    }
    g_emu.videoBuffer.clear();
    g_emu.audioScratch.clear();
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_gbdesk_android_native_CoreBridge_nativeLoadRomFd(
    JNIEnv* env,
    jobject,
    jint fd,
    jstring jBaseName,
    jstring jSaveDir) {
    stopEmulationInternal();

    VFile* vf = VFileFromFD(fd);
    if (!vf) {
        LOGE("Failed to open VFile from fd %d", fd);
        return JNI_FALSE;
    }

    mCore* core = mCoreFindVF(vf);
    if (!core) {
        vf->close(vf);
        LOGE("Unsupported ROM format");
        return JNI_FALSE;
    }

    core->init(core);
    mCoreInitConfig(core, nullptr);

    const char* baseName = env->GetStringUTFChars(jBaseName, nullptr);
    const char* saveDir = env->GetStringUTFChars(jSaveDir, nullptr);

    std::snprintf(core->dirs.baseName, sizeof(core->dirs.baseName), "%s", baseName ? baseName : "game");
    if (saveDir) {
        mDirectorySetAttachBase(&core->dirs, VDirOpen(saveDir));
    }

    env->ReleaseStringUTFChars(jBaseName, baseName);
    env->ReleaseStringUTFChars(jSaveDir, saveDir);

    // Preload into memory so SAF file descriptors that do not support mmap() still work reliably.
    if (!mCorePreloadVF(core, vf)) {
        mCoreConfigDeinit(&core->config);
        core->deinit(core);
        LOGE("mCorePreloadVF failed");
        return JNI_FALSE;
    }

    mCoreAutoloadSave(core);
    mCoreAutoloadCheats(core);

    unsigned w = 0, h = 0;
    core->baseVideoSize(core, &w, &h);
    if (w == 0 || h == 0) {
        w = 240;
        h = 160;
    }
    g_emu.width = w;
    g_emu.height = h;
    g_emu.videoBuffer.assign(static_cast<size_t>(w) * h, 0);
    core->setVideoBuffer(core, reinterpret_cast<mColor*>(g_emu.videoBuffer.data()), w);
    core->setAudioBufferSize(core, 2048);
    core->reset(core);

    g_emu.video.setFrameSize(w, h);
    g_emu.core = core;

    unsigned sampleRate = core->audioSampleRate(core);
    g_emu.audio.start(sampleRate ? static_cast<int>(sampleRate) : 32768);
    g_emu.audio.setPaused(true);

    g_emu.paused.store(true, std::memory_order_release);
    g_emu.running.store(true, std::memory_order_release);
    g_emu.thread = std::thread(emulationLoop);

    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_gbdesk_android_native_CoreBridge_nativeSetSurface(
    JNIEnv* env,
    jobject,
    jobject surface) {
    ANativeWindow* window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    g_emu.video.setWindow(window);
}

JNIEXPORT void JNICALL
Java_com_gbdesk_android_native_CoreBridge_nativeSetPaused(
    JNIEnv*,
    jobject,
    jboolean paused) {
    bool p = paused == JNI_TRUE;
    g_emu.paused.store(p, std::memory_order_release);
    g_emu.audio.setPaused(p);
}

JNIEXPORT void JNICALL
Java_com_gbdesk_android_native_CoreBridge_nativeSetKeys(
    JNIEnv*,
    jobject,
    jint keys) {
    g_emu.keys.store(static_cast<uint32_t>(keys), std::memory_order_relaxed);
}

JNIEXPORT void JNICALL
Java_com_gbdesk_android_native_CoreBridge_nativeReset(
    JNIEnv*,
    jobject) {
    std::lock_guard<std::mutex> lock(g_emu.coreMutex);
    if (g_emu.core) {
        g_emu.core->reset(g_emu.core);
    }
}

JNIEXPORT void JNICALL
Java_com_gbdesk_android_native_CoreBridge_nativeStop(
    JNIEnv*,
    jobject) {
    stopEmulationInternal();
}

JNIEXPORT jlong JNICALL
Java_com_gbdesk_android_native_CoreBridge_nativeGetStateTimestamp(
    JNIEnv*,
    jobject,
    jint slot) {
    std::lock_guard<std::mutex> lock(g_emu.coreMutex);
    if (!g_emu.core) {
        return -1;
    }
    VFile* vf = mCoreGetState(g_emu.core, slot, false);
    if (!vf) {
        return -1;
    }
    jlong timestampMs = 0;
    mStateExtdata extdata;
    mStateExtdataInit(&extdata);
    void* state = mCoreExtractState(g_emu.core, vf, &extdata);
    if (state) {
        mStateExtdataItem item;
        if (mStateExtdataGet(&extdata, EXTDATA_META_TIME, &item) && item.size == sizeof(uint64_t)) {
            uint64_t usec = 0;
            std::memcpy(&usec, item.data, sizeof(uint64_t));
            timestampMs = static_cast<jlong>(usec / 1000ULL);
        }
        mappedMemoryFree(state, g_emu.core->stateSize(g_emu.core));
    } else {
        timestampMs = -1;
    }
    mStateExtdataDeinit(&extdata);
    vf->close(vf);
    return timestampMs;
}

JNIEXPORT jboolean JNICALL
Java_com_gbdesk_android_native_CoreBridge_nativeLoadStateSlot(
    JNIEnv*,
    jobject,
    jint slot) {
    std::lock_guard<std::mutex> lock(g_emu.coreMutex);
    if (!g_emu.core) {
        return JNI_FALSE;
    }
    return mCoreLoadState(g_emu.core, slot, SAVESTATE_SCREENSHOT | SAVESTATE_RTC) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_gbdesk_android_native_CoreBridge_nativeSaveStateSlot(
    JNIEnv*,
    jobject,
    jint slot) {
    std::lock_guard<std::mutex> lock(g_emu.coreMutex);
    if (!g_emu.core) {
        return JNI_FALSE;
    }
    return mCoreSaveState(g_emu.core, slot, SAVESTATE_SCREENSHOT | SAVESTATE_SAVEDATA | SAVESTATE_CHEATS | SAVESTATE_RTC) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_gbdesk_android_native_CoreBridge_nativeLoadStateFd(
    JNIEnv*,
    jobject,
    jint fd) {
    std::lock_guard<std::mutex> lock(g_emu.coreMutex);
    if (!g_emu.core) {
        return JNI_FALSE;
    }
    VFile* vf = VFileFromFD(fd);
    if (!vf) {
        return JNI_FALSE;
    }
    bool ok = mCoreLoadStateNamed(g_emu.core, vf, SAVESTATE_SCREENSHOT | SAVESTATE_RTC);
    vf->close(vf);
    return ok ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
