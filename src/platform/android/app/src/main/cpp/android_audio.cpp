#include "android_audio.h"

#include <algorithm>
#include <cstring>

namespace {
constexpr size_t MAX_RING_SAMPLES = 16384 * 2; // ~0.5s stereo at 32kHz
}

AndroidAudio::~AndroidAudio() {
    stop();
}

bool AndroidAudio::start(int sampleRate) {
    stop();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ring.clear();
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
    m_ring.clear();
}

void AndroidAudio::setPaused(bool paused) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_paused = paused;
}

void AndroidAudio::pushSamples(const int16_t* samples, size_t frameCount) {
    if (!samples || frameCount == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t sampleCount = frameCount * 2;
    if (m_ring.size() + sampleCount > MAX_RING_SAMPLES) {
        size_t drop = (m_ring.size() + sampleCount) - MAX_RING_SAMPLES;
        drop = std::min(drop, m_ring.size());
        m_ring.erase(m_ring.begin(), m_ring.begin() + static_cast<std::ptrdiff_t>(drop));
    }
    m_ring.insert(m_ring.end(), samples, samples + sampleCount);
}

oboe::DataCallbackResult AndroidAudio::onAudioReady(
    oboe::AudioStream*,
    void* audioData,
    int32_t numFrames) {
    auto* out = static_cast<int16_t*>(audioData);
    size_t neededSamples = static_cast<size_t>(numFrames) * 2;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_paused || m_ring.empty()) {
        std::memset(out, 0, neededSamples * sizeof(int16_t));
        return oboe::DataCallbackResult::Continue;
    }

    size_t avail = std::min(neededSamples, m_ring.size());
    for (size_t i = 0; i < avail; ++i) {
        out[i] = m_ring.front();
        m_ring.pop_front();
    }
    if (avail < neededSamples) {
        std::memset(out + avail, 0, (neededSamples - avail) * sizeof(int16_t));
    }
    return oboe::DataCallbackResult::Continue;
}
