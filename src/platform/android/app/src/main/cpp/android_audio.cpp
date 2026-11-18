#include "android_audio.h"

extern "C" {
#include <mgba-util/audio-buffer.h>
}

#include <cstring>

AndroidAudio::~AndroidAudio() {
    stop();
}

bool AndroidAudio::start(int sampleRate, mAudioBuffer* buffer) {
    stop();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer = buffer;
        m_paused = false;
    }

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Shared)
        ->setFormat(oboe::AudioFormat::I16)
        ->setChannelCount(oboe::ChannelCount::Stereo)
        ->setSampleRate(sampleRate)
        ->setDataCallback(this);

    oboe::Result result = builder.openStream(m_stream);
    if (result != oboe::Result::OK || !m_stream) {
        return false;
    }
    return m_stream->requestStart() == oboe::Result::OK;
}

void AndroidAudio::stop() {
    if (m_stream) {
        m_stream->requestStop();
        m_stream->close();
        m_stream.reset();
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffer = nullptr;
}

void AndroidAudio::setPaused(bool paused) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_paused = paused;
}

oboe::DataCallbackResult AndroidAudio::onAudioReady(
    oboe::AudioStream*,
    void* audioData,
    int32_t numFrames) {
    auto* out = static_cast<int16_t*>(audioData);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_paused || !m_buffer) {
        std::memset(out, 0, static_cast<size_t>(numFrames) * 2 * sizeof(int16_t));
        return oboe::DataCallbackResult::Continue;
    }

    size_t read = mAudioBufferRead(m_buffer, out, static_cast<size_t>(numFrames));
    if (read < static_cast<size_t>(numFrames)) {
        std::memset(out + read * 2, 0, (static_cast<size_t>(numFrames) - read) * 2 * sizeof(int16_t));
    }
    return oboe::DataCallbackResult::Continue;
}
