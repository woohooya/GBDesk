#pragma once

#include <oboe/Oboe.h>
#include <cstdint>
#include <memory>
#include <mutex>

struct mAudioBuffer;

class AndroidAudio : public oboe::AudioStreamDataCallback {
public:
    AndroidAudio() = default;
    ~AndroidAudio() override;

    bool start(int sampleRate, mAudioBuffer* buffer);
    void stop();
    void setPaused(bool paused);

    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream* audioStream,
        void* audioData,
        int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream> m_stream;
    std::mutex m_mutex;
    mAudioBuffer* m_buffer = nullptr;
    bool m_paused = false;
};
