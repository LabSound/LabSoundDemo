// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2020, The LabSound Authors. All rights reserved.

#if defined(_MSC_VER)
    #if !defined(_CRT_SECURE_NO_WARNINGS)
        #define _CRT_SECURE_NO_WARNINGS
    #endif
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
#endif

#include "LabSound/LabSound.h"
#include "LabSoundDemo.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace lab;
\

std::shared_ptr<AudioBus> MakeBusFromSampleFile(char const* const name, int argc, char** argv)
{
    std::string path_prefix = asset_base;
    const std::string path = path_prefix + name;
    std::shared_ptr<AudioBus> bus = MakeBusFromFile(path, false);
    if (!bus) 
        throw std::runtime_error("couldn't open " + path);

    return bus;
}

template <typename Duration>
void Wait(Duration duration)
{
    std::this_thread::sleep_for(duration);
}


int main(int argc, char *argv[]) try
{   
    AudioStreamConfig offlineConfig;
    offlineConfig.device_index = 0;
    offlineConfig.desired_samplerate = LABSOUND_DEFAULT_SAMPLERATE;
    offlineConfig.desired_channels = LABSOUND_DEFAULT_CHANNELS;

    const float recording_time_ms = 1000.f;

    std::unique_ptr<lab::AudioContext> context = lab::MakeOfflineAudioContext(offlineConfig, recording_time_ms);
    lab::AudioContext& ac = *context.get();

    std::shared_ptr<OscillatorNode> oscillator;
    std::shared_ptr<AudioBus> musicClip = MakeBusFromSampleFile("samples/stereo-music-clip.wav", argc, argv);
    std::shared_ptr<SampledAudioNode> musicClipNode;
    std::shared_ptr<GainNode> gain;

    auto recorder = std::make_shared<RecorderNode>(ac, offlineConfig);

    context->connect(ac.device(), recorder, 1, 0);

    recorder->startRecording();

    {
        ContextRenderLock r(context.get(), "ex_offline_rendering");

        gain = std::make_shared<GainNode>(ac);
        gain->gain()->setValue(0.125f);

        // osc -> gain -> recorder
        oscillator = std::make_shared<OscillatorNode>(ac);
        context->connect(gain, oscillator, 0, 0);
        context->connect(recorder, gain, 0, 0);
        oscillator->frequency()->setValue(880.f);
        oscillator->setType(OscillatorType::SINE);
        oscillator->start(0.0f);

        musicClipNode = std::make_shared<SampledAudioNode>(ac);
        context->connect(recorder, musicClipNode, 0, 0);
        musicClipNode->setBus(r, musicClip);
        musicClipNode->schedule(0.0);
    }

    // the completion callback will be called when the 1000ms sample buffer has been filled.
    bool complete = false;
    context->offlineRenderCompleteCallback = [&context, &recorder, &complete]() {
        recorder->stopRecording();

        printf("Recorded %f seconds of audio\n", recorder->recordedLengthInSeconds());

        context->disconnectInput(recorder);
        recorder->writeRecordingToWav("ex_offline_rendering.wav", false);
        complete = true;
    };

    // Offline rendering happens in a separate thread and blocks until complete.
    // It needs to acquire the graph + render locks, so it must
    // be outside the scope of where we make changes to the graph.
    context->startOfflineRendering();

    while (!complete)
    {
        Wait(std::chrono::milliseconds(100));
    }
}
catch (const std::exception & e) 
{
    std::cerr << "unhandled fatal exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
}

