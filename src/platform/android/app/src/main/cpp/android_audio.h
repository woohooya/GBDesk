#pragma once

#include <oboe/Oboe.h>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>

class AndroidAudio : public oboe::AudioStreamDataCallback {
public:
    AndroidAudio() = default;
    ~AndroidAudio() override;

    bool start(int sampleRate);
    void stop();
    void setPaused(bool paused);
    void pushSamples(const int16_t* samples, size_t frameCount);

    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream* audioStream,
        void* audioData,
        int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream> m_stream;
    std::mutex m_mutex;
    std::deque<int16_t> m_ring;
    bool m_paused = false;
};
