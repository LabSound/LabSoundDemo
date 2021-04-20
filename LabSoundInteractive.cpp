
// SPDX-License-Identifier: BSD-2-Clause
// Copyright () 2020, Nick Porcino & Dimitri Diakopolous. All rights reserved.

#include "imgui-app/imgui.h"

#include "LabSound/LabSound.h"
#include "LabSound/extended/Util.h"
#include "LabSoundDemo.h"
#include "ImGuiGridSlider.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <iostream>
#include <string>


#if defined(_MSC_VER)
# pragma warning(disable : 4996)
# if !defined(NOMINMAX)
#  define NOMINMAX
# endif
#endif

using namespace lab;

struct NodeLocation
{
    float x = 0;
    std::string name;
};


struct Demo
{
    std::unique_ptr<lab::AudioContext> context;
    std::map<std::string, std::shared_ptr<AudioBus>> sample_cache;
    std::shared_ptr<RecorderNode> recorder;
    bool use_live = false;

    void shutdown()
    {
        context.reset();
    }

    std::shared_ptr<AudioBus> MakeBusFromSampleFile(char const* const name, float sampleRate)
    {
        auto it = sample_cache.find(name);
        if (it != sample_cache.end())
            return it->second;

        std::string path_prefix = asset_base;

        const std::string path = path_prefix + name;
        std::shared_ptr<AudioBus> bus = MakeBusFromFile(path, false, sampleRate);
        if (!bus)
            throw std::runtime_error("couldn't open " + path);

        sample_cache[name] = bus;

        return bus;
    }

};



/////////////////////////////////////
//    Graph Traversal              //
/////////////////////////////////////

// traveral chart
std::vector<NodeLocation> displayNodes;
std::set<uintptr_t> nodes;

void traverse(ContextRenderLock* r, AudioNode* root, char const* const prefix, int tab)
{
    nodes.insert(reinterpret_cast<uintptr_t>(root));
    for (int i = 0; i < tab; ++i)
        printf(" ");

    displayNodes.push_back({ static_cast<float>(tab), std::string(root->name()) });

    bool inputs_silent = root->numberOfInputs() > 0 && root->inputsAreSilent(*r);

    const char* state_name = root->isScheduledNode() ? schedulingStateName(root->_scheduler._playbackState) : "active";
    const char* input_status = root->numberOfInputs() > 0 ? (root->inputsAreSilent(*r) ? "inputs silent" : "inputs active") : "no inputs";
    printf("%s%s (%s) (%s)\n", prefix, root->name(), state_name, input_status);

    auto params = root->params();
    for (auto& p : params)
    {
        if (p->isConnected())
        {
            AudioBus const* const bus = p->bus();
            if (bus)
            {
                const char* input_is_zero = bus->maxAbsValue() > 0.f ? "non-zero" : "zero";
                for (int i = 0; i < tab; ++i)
                    printf(" ");
                printf("driven param has %s values\n", input_is_zero);
            }

            int c = p->numberOfRenderingConnections(*r);
            for (int j = 0; j < c; ++j)
            {
                AudioNode* n = p->renderingOutput(*r, j)->sourceNode();
                if (n)
                {
                    if (nodes.find(reinterpret_cast<uintptr_t>(n)) == nodes.end())
                    {
                        char buff[64];
                        strncpy(buff, p->name().c_str(), 64);
                        buff[63] = '\0';
                        traverse(r, n, buff, tab + 3);
                    }
                    else
                    {
                        for (int i = 0; i < tab; ++i)
                            printf(" ");
                        printf("*--> %s\n", n->name());   // just show gotos to previous nodes
                    }
                }
            }
        }
    }
    for (int i = 0; i < root->numberOfInputs(); ++i)
    {
        auto input = root->input(i);
        if (input)
        {
            const char* input_is_zero = input->bus(*r)->maxAbsValue() > 0.f ? "active signal" : "zero signal";
            for (int i = 0; i < tab; ++i)
                printf(" ");
            printf("input %d: %s\n", i, input_is_zero);
            int c = input->numberOfRenderingConnections(*r);
            for (int j = 0; j < c; ++j)
            {
                AudioNode* n = input->renderingOutput(*r, j)->sourceNode();
                if (n)
                {
                    if (nodes.find(reinterpret_cast<uintptr_t>(n)) == nodes.end())
                        traverse(r, n, "", tab + 3);
                    else
                    {
                        for (int i = 0; i < tab; ++i)
                            printf(" ");
                        printf("*--> %s\n", n->name());   // just show gotos to previous nodes
                    }
                }
            }
        }
    }
}

void traverse_ui(lab::AudioContext& context)
{
    displayNodes.clear();
    nodes.clear();
    printf("\n");
    context.synchronizeConnections();
    ContextRenderLock r(&context, "traverse");
    traverse(&r, context.device().get(), "", 0);
}



//////////////////////////////
//    example base class    //
//////////////////////////////

struct labsound_example
{
    explicit labsound_example(Demo& demo) : _demo(&demo) {}

    Demo* _demo;
    std::shared_ptr<lab::AudioNode> _root_node;

    void connect()
    {
        if (!_root_node)
            return;

        auto& ac = *_demo->context.get();
        if (!ac.isConnected(_demo->recorder, _root_node))
        {
            // connect synchronously
            ac.connect(_demo->recorder, _root_node, 0, 0);
            ac.synchronizeConnections();
            _root_node->_scheduler.start(0);
        }
    }

    void disconnect()
    {
        if (!_root_node)
            return;

        auto& ac = *_demo->context.get();
        if (!ac.isConnected(_demo->recorder, _root_node))
        {
            ac.disconnect(_demo->recorder, _root_node);
            ac.synchronizeConnections();
        }
    }

    virtual void play() = 0;
    virtual void update() {}
    virtual char const* const name() const = 0;

    virtual void ui()
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        ImGui::BeginChild("###EXAMPLE", ImVec2{ 0, 100 }, true);
        ImGui::TextUnformatted("Example");
        if (ImGui::Button("Disconnect"))
        {
            disconnect();
        }
        ImGui::EndChild();
    }
};



/////////////////////
//    ex_simple    //
/////////////////////

// ex_simple demonstrate the use of an audio clip loaded from disk and a basic sine oscillator. 
struct ex_simple : public labsound_example
{
    std::shared_ptr<SampledAudioNode> musicClipNode;
    std::shared_ptr<GainNode> gain;
    std::shared_ptr<PeakCompNode> peakComp;

    virtual char const* const name() const override { return "Simple"; }

    explicit ex_simple(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        auto musicClip = demo.MakeBusFromSampleFile("samples/stereo-music-clip.wav", ac.sampleRate());
        if (!musicClip)
            return;

        gain = std::make_shared<GainNode>(ac);
        gain->gain()->setValue(0.5f);
        peakComp = std::make_shared<PeakCompNode>(ac);
        _root_node = peakComp;
        ac.connect(peakComp, gain, 0, 0);

        musicClipNode = std::make_shared<SampledAudioNode>(ac);
        {
            ContextRenderLock r(&ac, "ex_simple");
            musicClipNode->setBus(r, musicClip);
        }
        ac.connect(_root_node, musicClipNode, 0, 0);
    }

    virtual void play() override final
    {
        connect();
        musicClipNode->schedule(0.0);
    }
};



/////////////////////
//    ex_sfxr      //
/////////////////////

// ex_simple demonstrate the use of an audio clip loaded from disk and a basic sine oscillator. 
struct ex_sfxr : public labsound_example
{
    std::shared_ptr<SfxrNode> sfxr;

    virtual char const* const name() const override { return "Sfxr"; }

    explicit ex_sfxr(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        sfxr = std::make_shared<SfxrNode>(ac);
        _root_node = sfxr;
    }

    virtual void play() override final
    {
        connect();
        sfxr->start(0.0);
    }

    virtual void ui() override final
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        ImGui::BeginChild("###SFXR", ImVec2{ 0, 300 }, true);
        if (ImGui::Button("Default"))
        {
            sfxr->preset()->setUint32(99);  // notifications only occur on change, so send a nonsense value
            sfxr->preset()->setUint32(0);
            sfxr->start(0.0);
        }
        if (ImGui::Button("Coin"))
        {
            sfxr->preset()->setUint32(99);  // notifications only occur on change, so send a nonsense value
            sfxr->preset()->setUint32(1);
            sfxr->start(0.0);
        }
        if (ImGui::Button("Laser"))
        {
            sfxr->preset()->setUint32(99);  // notifications only occur on change, so send a nonsense value
            sfxr->preset()->setUint32(2);
            sfxr->start(0.0);
        }
        if (ImGui::Button("Explosion"))
        {
            sfxr->preset()->setUint32(99);  // notifications only occur on change, so send a nonsense value
            sfxr->preset()->setUint32(3);
            sfxr->start(0.0);
        }
        if (ImGui::Button("Power Up"))
        {
            sfxr->preset()->setUint32(99);  // notifications only occur on change, so send a nonsense value
            sfxr->preset()->setUint32(4);
            sfxr->start(0.0);
        }
        if (ImGui::Button("Hit"))
        {
            sfxr->preset()->setUint32(99);  // notifications only occur on change, so send a nonsense value
            sfxr->preset()->setUint32(5);
            sfxr->start(0.0);
        }
        if (ImGui::Button("Jump"))
        {
            sfxr->preset()->setUint32(99);  // notifications only occur on change, so send a nonsense value
            sfxr->preset()->setUint32(6);
            sfxr->start(0.0);
        }
        if (ImGui::Button("Select"))
        {
            sfxr->preset()->setUint32(99);  // notifications only occur on change, so send a nonsense value
            sfxr->preset()->setUint32(7);
            sfxr->start(0.0);
        }
        if (ImGui::Button("Mutate"))
        {
            sfxr->preset()->setUint32(99);  // notifications only occur on change, so send a nonsense value
            sfxr->preset()->setUint32(8);
            sfxr->start(0.0);
        }
        if (ImGui::Button("Random"))
        {
            sfxr->preset()->setUint32(99);  // notifications only occur on change, so send a nonsense value
            sfxr->preset()->setUint32(9);
            sfxr->start(0.0);
        }
        ImGui::EndChild();
    }
};



/////////////////////
//    ex_osc_pop   //
/////////////////////

// ex_osc_pop to test oscillator start/stop popping (it shouldn't pop). 
struct ex_osc_pop : public labsound_example
{
    std::shared_ptr<OscillatorNode> oscillator;
    std::shared_ptr<GainNode> gain;
//    std::shared_ptr<RecorderNode> recorder;

    virtual char const* const name() const override { return "Oscillator"; }

    ex_osc_pop(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        oscillator = std::make_shared<OscillatorNode>(ac);

        gain = std::make_shared<GainNode>(ac);
        _root_node = gain;

        gain->gain()->setValue(1);

        // osc -> destination
        ac.connect(gain, oscillator, 0, 0);

        oscillator->frequency()->setValue(1000.f);
        oscillator->setType(OscillatorType::SINE);

 //       AudioStreamConfig outputConfig { -1, }
 //       recorder = std::make_shared<RecorderNode>(ac, outputConfig);
    }

    virtual void play() override final
    {
        connect();
        auto& ac = *_demo->context.get();
 //       ac.addAutomaticPullNode(recorder);
        oscillator->start(0);
        oscillator->stop(0.5f);
 //       recorder->startRecording();
 //       ac.connect(recorder, gain, 0, 0);
 //       recorder->stopRecording();
 //       ac.removeAutomaticPullNode(recorder);
 //       recorder->writeRecordingToWav("ex_osc_pop.wav", false);
    }

    virtual void ui() override final
    {
        ImGui::BeginChild("###OSCPOP", ImVec2{ 0, 100 }, true);
        if (ImGui::Button("Play"))
        {
            oscillator->start(0);
            oscillator->stop(0.5f);
        }
        static float f = 1000.f;
        if (ImGui::InputFloat("Frequency", &f))
        {
            oscillator->frequency()->setValue(f);
        }
        ImGui::EndChild();
    }

};



//////////////////////////////
//    ex_playback_events    //
//////////////////////////////

// ex_playback_events showcases the use of a `setOnEnded` callback on a `SampledAudioNode`
struct ex_playback_events : public labsound_example
{
    std::shared_ptr<SampledAudioNode> sampledAudio;
    bool waiting = true;

    virtual char const* const name() const override { return "Events"; }

    explicit ex_playback_events(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        auto musicClip = _demo->MakeBusFromSampleFile("samples/mono-music-clip.wav", ac.sampleRate());
        if (!musicClip)
            return;

        sampledAudio = std::make_shared<SampledAudioNode>(ac);
        _root_node = sampledAudio;
        {
            ContextRenderLock r(&ac, "ex_playback_events");
            sampledAudio->setBus(r, musicClip);
        }

        sampledAudio->setOnEnded([this]() {
            std::cout << "sampledAudio finished..." << std::endl;
            waiting = false;
        });
    }

    ~ex_playback_events()
    {
        if (sampledAudio)
        {
            sampledAudio->setOnEnded([]() {});
            sampledAudio->stop(0);
        }
    }

    virtual void play() override final
    {
        connect();
        sampledAudio->schedule(0.0);
    }

    virtual void ui() override final
    {
        ImGui::BeginChild("###EVENT", ImVec2{ 0, 100 }, true);
        if (ImGui::Button("Play"))
        {
            connect();
            sampledAudio->schedule(0.0);
            waiting = true;
        }
        if (waiting)
        {
            ImGui::TextUnformatted("Waiting for end of clip");
        }
        else
        {
            ImGui::TextUnformatted("End of clip detected");
        }
        ImGui::EndChild();
    }

};



////////////////////////////////
//    ex_offline_rendering    //
////////////////////////////////

// This sample illustrates how LabSound can be used "offline," where the graph is not
// pulled by an actual audio device, but rather a null destination. This sample shows
// how a `RecorderNode` can be used to capture the rendered audio to disk.
struct ex_offline_rendering : public labsound_example
{
    std::shared_ptr<AudioBus> musicClip;
    std::string path;

    virtual char const* const name() const override { return "Offline"; }

    explicit ex_offline_rendering(Demo& demo) : labsound_example(demo) 
    {
        auto& ac = *_demo->context.get();
        musicClip = _demo->MakeBusFromSampleFile("samples/stereo-music-clip.wav", ac.sampleRate());
        path = "ex_offiline_rendering.wav";
    }

    virtual void play() override
    {
        auto& ac = *_demo->context.get();
        std::shared_ptr<SampledAudioNode> musicClipNode;
        std::shared_ptr<OscillatorNode> oscillator;
        std::shared_ptr<GainNode> gain;

        AudioStreamConfig offlineConfig;
        offlineConfig.device_index = 0;
        offlineConfig.desired_samplerate = LABSOUND_DEFAULT_SAMPLERATE;
        offlineConfig.desired_channels = LABSOUND_DEFAULT_CHANNELS;

        const float recording_time_ms = 1000.f;

        std::unique_ptr<lab::AudioContext> context = lab::MakeOfflineAudioContext(offlineConfig, recording_time_ms);

        auto recorder = std::make_shared<RecorderNode>(*context.get(), offlineConfig);
        context->addAutomaticPullNode(recorder);

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

        // make the recorder ready, and set up a completion callback to write the result
        recorder->startRecording();
        bool complete = false;
        context->offlineRenderCompleteCallback = [this, &context, &recorder, &complete]() 
        {
            recorder->stopRecording();

            printf("Recorded %f seconds of audio\n", recorder->recordedLengthInSeconds());

            context->removeAutomaticPullNode(recorder);
            recorder->writeRecordingToWav(path.c_str(), false);
            complete = true;
        };

        // Offline rendering happens in a separate thread and blocks until complete.
        // It needs to acquire the graph + render locks, so it must
        // be outside the scope of where we make changes to the graph.
        context->startOfflineRendering();
    }
};



//////////////////////
//    ex_tremolo    //
//////////////////////

// This demonstrates the use of `connectParam` as a way of modulating one node through another. 
// Params are control signals that operate at audio rate.
struct ex_tremolo : public labsound_example
{
    std::shared_ptr<OscillatorNode> oscillator;
    std::shared_ptr<OscillatorNode> modulator;
    std::shared_ptr<GainNode> modulatorGain;
    float freq;
    float mod_freq;
    float variance;

    virtual char const* const name() const override { return "Tremolo"; }

    explicit ex_tremolo(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        modulator = std::make_shared<OscillatorNode>(ac);
        modulator->setType(OscillatorType::SINE);
        variance = 8;
        modulator->frequency()->setValue(variance);
        modulator->start(0);

        mod_freq = 10.f;
        modulatorGain = std::make_shared<GainNode>(ac);
        modulatorGain->gain()->setValue(mod_freq);

        freq = 440.f;
        oscillator = std::make_shared<OscillatorNode>(ac);
        _root_node = oscillator;
        oscillator->setType(OscillatorType::TRIANGLE);
        oscillator->frequency()->setValue(freq);

        // Set up processing chain
        // modulator > modulatorGain ---> osc frequency
        //                                osc > context
        ac.connect(modulatorGain, modulator, 0, 0);
        ac.connectParam(oscillator->detune(), modulatorGain, 0);
    }
 
    virtual void play() override final
    {
        connect();
        oscillator->start(0);
    }

    virtual void ui() override final
    {
        ImGui::BeginChild("###TREMOLO", ImVec2{ 0, 100 }, true);
        if (ImGui::Button("Play"))
        {
            oscillator->start(0);
            oscillator->stop(0.5f);
        }
        if (ImGui::InputFloat("Frequency", &freq))
        {
            oscillator->frequency()->setValue(freq);
        }
        if (ImGui::InputFloat("Speed", &mod_freq))
        {
            modulator->frequency()->setValue(mod_freq);
        }
        if (ImGui::InputFloat("Variance", &variance))
        {
            modulatorGain->gain()->setValue(variance);
        }
        ImGui::EndChild();
    }

};



///////////////////////////////////
//    ex_frequency_modulation    //
///////////////////////////////////

// This is inspired by a patch created in the ChucK audio programming language. It showcases
// LabSound's ability to construct arbitrary graphs of oscillators a-la FM synthesis.
struct ex_frequency_modulation : public labsound_example
{
    std::shared_ptr<OscillatorNode> modulator;
    std::shared_ptr<GainNode> modulatorGain;
    std::shared_ptr<OscillatorNode> osc;
    std::shared_ptr<ADSRNode> trigger;

    std::shared_ptr<GainNode> signalGain;
    std::shared_ptr<GainNode> feedbackTap;
    std::shared_ptr<DelayNode> chainDelay;

    std::chrono::steady_clock::time_point prev;
    UniformRandomGenerator fmrng;

    bool on = true;

    virtual char const* const name() const override { return "Frequence Modulation"; }

    explicit ex_frequency_modulation(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        modulator = std::make_shared<OscillatorNode>(ac);
        modulator->setType(OscillatorType::SQUARE);
        const float mod_freq = fmrng.random_float(4.f, 512.f);
        modulator->frequency()->setValue(mod_freq);
        modulator->start(0);

        modulatorGain = std::make_shared<GainNode>(ac);

        osc = std::make_shared<OscillatorNode>(ac);
        osc->setType(OscillatorType::SQUARE);
        const float carrier_freq = fmrng.random_float(80.f, 440.f);
        osc->frequency()->setValue(carrier_freq);
        osc->start(0);

        trigger = std::make_shared<ADSRNode>(ac);
        trigger->oneShot()->setBool(false);

        signalGain = std::make_shared<GainNode>(ac);
        signalGain->gain()->setValue(1.0f);

        feedbackTap = std::make_shared<GainNode>(ac);
        feedbackTap->gain()->setValue(0.5f);

        chainDelay = std::make_shared<DelayNode>(ac, 4);
        chainDelay->delayTime()->setFloat(0.0f);  // passthrough delay, not sure if this has the same DSP semantic as ChucK

        // Set up FM processing chain:
        ac.connect(modulatorGain, modulator, 0, 0);  // Modulator to Gain
        ac.connectParam(osc->frequency(), modulatorGain, 0);  // Gain to frequency parameter
        ac.connect(trigger, osc, 0, 0);  // Osc to ADSR
        ac.connect(signalGain, trigger, 0, 0);  // ADSR to signalGain
        ac.connect(feedbackTap, signalGain, 0, 0);  // Signal to Feedback
        ac.connect(chainDelay, feedbackTap, 0, 0);  // Feedback to Delay
        ac.connect(signalGain, chainDelay, 0, 0);  // Delay to signalGain
        _root_node = signalGain;// signalGain;
    }

    virtual void play() override final
    {
        connect();
        prev = std::chrono::steady_clock::now();
    }

    virtual void update() override
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        auto now = std::chrono::steady_clock::now();
        if (now - prev < std::chrono::milliseconds(500))
            return;

        if (on)
        {
            trigger->gate()->setValue(0.f);
            on = false;
            return;
        }
        on = true;

        prev = now;

        auto& ac = *_demo->context.get();

        const float carrier_freq = fmrng.random_float(80.f, 440.f);
        osc->frequency()->setValue(carrier_freq);

        const float mod_freq = fmrng.random_float(4.f, 512.f);
        modulator->frequency()->setValue(mod_freq);

        const float mod_gain = fmrng.random_float(16.f, 1024.f);
        modulatorGain->gain()->setValue(mod_gain);

        const float attack_length = fmrng.random_float(0.25f, 0.5f);
        trigger->set(attack_length, 0.50f, 0.50f, 0.25f, 0.50f, 0.1f);

        double t = ac.currentTime();
        trigger->gate()->setValueAtTime(0, static_cast<float>(t));
        trigger->gate()->setValueAtTime(1, static_cast<float>(t + 0.1));

        //std::cout << "[ex_frequency_modulation] car_freq: " << carrier_freq << std::endl;
        //std::cout << "[ex_frequency_modulation] mod_freq: " << mod_freq << std::endl;
        //std::cout << "[ex_frequency_modulation] mod_gain: " << mod_gain << std::endl;
    }
};



///////////////////////////////////
//    ex_runtime_graph_update    //
///////////////////////////////////

// In most examples, nodes are not disconnected during playback. This sample shows how nodes
// can be arbitrarily connected/disconnected during runtime while the graph is live. 
struct ex_runtime_graph_update : public labsound_example
{
    std::shared_ptr<OscillatorNode> oscillator1, oscillator2;
    std::shared_ptr<GainNode> gain;
    std::chrono::steady_clock::time_point prev;
    int disconnect;

    virtual char const* const name() const override { return "Graph Update"; }

    explicit ex_runtime_graph_update(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        oscillator1 = std::make_shared<OscillatorNode>(ac);
        oscillator2 = std::make_shared<OscillatorNode>(ac);

        gain = std::make_shared<GainNode>(ac);
        gain->gain()->setValue(0.50);
        _root_node = gain;

        // osc -> gain -> destination
        ac.connect(gain, oscillator1, 0, 0);
        ac.connect(gain, oscillator2, 0, 0);

        oscillator1->setType(OscillatorType::SINE);
        oscillator1->frequency()->setValue(220.f);
        oscillator1->start(0.00f);

        oscillator2->setType(OscillatorType::SINE);
        oscillator2->frequency()->setValue(440.f);
        oscillator2->start(0.00);
        disconnect = 4;
    }

    virtual void play() override
    {
        connect();
        prev = std::chrono::steady_clock::now();
        disconnect = 1;
    }

    virtual void update() override
    {
        if (disconnect >= 4)
            return;

        auto now = std::chrono::steady_clock::now();
        auto duration = now - prev;

        auto& ac = *_demo->context.get();
        if (disconnect == 1 && duration > std::chrono::milliseconds(500))
        {
            disconnect = 2;
            ac.disconnect(nullptr, oscillator1, 0, 0);
            ac.connect(gain, oscillator2, 0, 0);
        }

        if (disconnect == 2 && duration > std::chrono::milliseconds(1000))
        {
            disconnect = 3;
            ac.disconnect(nullptr, oscillator2, 0, 0);
            ac.connect(gain, oscillator1, 0, 0);
        }

        if (disconnect == 3 && duration > std::chrono::milliseconds(1500))
        {
            ac.disconnect(nullptr, oscillator1, 0, 0);
            ac.disconnect(nullptr, oscillator2, 0, 0);
            ac.disconnect(gain, _demo->recorder);
            disconnect = 4;
            std::cout << "OscillatorNode 1 use_count: " << oscillator1.use_count() << std::endl;
            std::cout << "OscillatorNode 2 use_count: " << oscillator2.use_count() << std::endl;
            std::cout << "GainNode use_count:         " << gain.use_count() << std::endl;
        }
    }

};



//////////////////////////////////
//    ex_microphone_loopback    //
//////////////////////////////////

// This example simply connects an input device (e.g. a microphone) to the output audio device (e.g. your speakers). 
// DANGER! This sample creates an open feedback loop. It is best used when the output audio device is a pair of headphones. 
struct ex_microphone_loopback : public labsound_example
{
    std::shared_ptr<AudioHardwareInputNode> input;

    virtual char const* const name() const override { return "Mic Loopback"; }

    explicit ex_microphone_loopback(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();

        ContextRenderLock r(&ac, "ex_microphone_loopback");
        input = lab::MakeAudioHardwareInputNode(r);
        _root_node = input;
    }

    virtual void play() override final
    {
        connect();
    }

    virtual void ui() override final
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        ImGui::BeginChild("###LOOPBACK", ImVec2{ 0, 100 }, true);
        ImGui::TextUnformatted("Input connected directly to output");
        if (ImGui::Button("Disconnect"))
        {
            disconnect();
        }
        ImGui::EndChild();
    }

};



////////////////////////////////
//    ex_microphone_reverb    //
////////////////////////////////

// This sample takes input from a microphone and convolves it with an impulse response to create reverb (i.e. use of the `ConvolverNode`).
// The sample convolution is for a rather large room, so there is a delay.
// DANGER! This sample creates an open feedback loop. It is best used when the output audio device is a pair of headphones. 
struct ex_microphone_reverb : public labsound_example
{
    std::shared_ptr<AudioBus> impulseResponseClip;
    std::shared_ptr<AudioHardwareInputNode> input;
    std::shared_ptr<ConvolverNode> convolve;
    std::shared_ptr<GainNode> wetGain;

    virtual char const* const name() const override { return "Mic Reverb"; }

    explicit ex_microphone_reverb(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        std::shared_ptr<AudioBus> impulseResponseClip = _demo->MakeBusFromSampleFile("impulse/cardiod-rear-levelled.wav", ac.sampleRate());

        ContextRenderLock r(&ac, "ex_microphone_reverb");

        input = lab::MakeAudioHardwareInputNode(r);

        convolve = std::make_shared<ConvolverNode>(ac);
        convolve->setImpulse(impulseResponseClip);

        wetGain = std::make_shared<GainNode>(ac);
        wetGain->gain()->setValue(0.6f);

        ac.connect(convolve, input, 0, 0);
        ac.connect(wetGain, convolve, 0, 0);
        _root_node = wetGain;
    }

    virtual void play() override
    {
        connect();
    }

    virtual void ui() override final
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        ImGui::BeginChild("###MICREVERB", ImVec2{ 0, 100 }, true);
        ImGui::TextUnformatted("Mic reverb active");
        if (ImGui::Button("Disconnect mic"))
        {
            disconnect();
        }
        ImGui::EndChild();
    }
};



//////////////////////////////
//    ex_peak_compressor    //
//////////////////////////////

// Demonstrates the use of the `PeakCompNode` and many scheduled audio sources.
struct ex_peak_compressor : public labsound_example
{
    std::shared_ptr<SampledAudioNode> kick_node;
    std::shared_ptr<SampledAudioNode> hihat_node;
    std::shared_ptr<SampledAudioNode> snare_node;

    std::shared_ptr<BiquadFilterNode> filter;
    std::shared_ptr<PeakCompNode> peakComp;

    virtual char const* const name() const override { return "Peak Compressor"; }

    explicit ex_peak_compressor(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        ContextRenderLock r(&ac, "ex_peak_compressor");
        kick_node = std::make_shared<SampledAudioNode>(ac);
        hihat_node = std::make_shared<SampledAudioNode>(ac);
        snare_node = std::make_shared<SampledAudioNode>(ac);
        kick_node->setBus(r, _demo->MakeBusFromSampleFile("samples/kick.wav", ac.sampleRate()));
        hihat_node->setBus(r, _demo->MakeBusFromSampleFile("samples/hihat.wav", ac.sampleRate()));
        snare_node->setBus(r, _demo->MakeBusFromSampleFile("samples/snare.wav", ac.sampleRate()));

        filter = std::make_shared<BiquadFilterNode>(ac);
        filter->setType(lab::FilterType::LOWPASS);
        filter->frequency()->setValue(1800.f);

        peakComp = std::make_shared<PeakCompNode>(ac);
        _root_node = peakComp;
        ac.connect(peakComp, filter, 0, 0);
        ac.connect(filter, kick_node, 0, 0);
        ac.connect(filter, hihat_node, 0, 0);
        //hihat_node->gain()->setValue(0.2f);

        ac.connect(filter, snare_node, 0, 0);
    }

    virtual void play() override final
    {
        connect();
        hihat_node->schedule(0);

        // Speed Metal
        float startTime = 0.1f;
        float bpm = 30.f;
        float bar_length = 60.f / bpm;
        float eighthNoteTime = bar_length / 8.0f;
        for (float bar = 0; bar < 8; bar += 1)
        {
            float time = startTime + bar * bar_length;

            kick_node->schedule(time);
            kick_node->schedule(time + 4 * eighthNoteTime);

            snare_node->schedule(time + 2 * eighthNoteTime);
            snare_node->schedule(time + 6 * eighthNoteTime);

            float hihat_beat = 8;
            for (float i = 0; i < hihat_beat; i += 1)
                hihat_node->schedule(time + bar_length * i / hihat_beat);

        }
    }
};



/////////////////////////////
//    ex_stereo_panning    //
/////////////////////////////

// This illustrates the use of equal-power stereo panning.
struct ex_stereo_panning : public labsound_example
{
    std::shared_ptr<SampledAudioNode> audioClipNode;
    std::shared_ptr<StereoPannerNode> stereoPanner;
    std::chrono::steady_clock::time_point prev;
    bool autopan;
    float pos;

    virtual char const* const name() const override { return "Stereo Panning"; }

    explicit ex_stereo_panning(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        std::shared_ptr<AudioBus> audioClip = _demo->MakeBusFromSampleFile("samples/trainrolling.wav", ac.sampleRate());
        audioClipNode = std::make_shared<SampledAudioNode>(ac);
        stereoPanner = std::make_shared<StereoPannerNode>(ac);
        _root_node = stereoPanner;
        autopan = true;
        pos = 0.f;

        {
            ContextRenderLock r(&ac, "ex_stereo_panning");

            audioClipNode->setBus(r, audioClip);
            ac.connect(stereoPanner, audioClipNode, 0, 0);
        }
    }

    virtual void play() override final
    {
        if (!audioClipNode)
            return;

        connect();

        audioClipNode->schedule(0.0, -1); // -1 to loop forever
        prev = std::chrono::steady_clock::now();
    }

    virtual void update() override
    {
        if (!autopan)
            return;

        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        const float seconds = 10.f;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - prev;
        float t = fmodf(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() * 0.001f, seconds);
        float halfTime = seconds * 0.5f;
        pos = (t - halfTime) / halfTime;

        // Put position a +up && +front, because if it goes right through the
        // listener at (0, 0, 0) it abruptly switches from left to right.
        stereoPanner->pan()->setValue(pos);
    }

    virtual void ui() override final
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        ImGui::BeginChild("###PANNING", ImVec2{ 0, 100 }, true);
        if (ImGui::SliderFloat("Pan", &pos, -1.f, 1.f, "%0.3f"))
        {
            autopan = false;
            stereoPanner->pan()->setValue(pos);
        }
        ImGui::EndChild();
    }
};



//////////////////////////////////
//    ex_hrtf_spatialization    //
//////////////////////////////////

// This illustrates 3d sound spatialization and doppler shift. Headphones are recommended for this sample.
struct ex_hrtf_spatialization : public labsound_example
{
    std::shared_ptr<AudioBus> audioClip;
    std::shared_ptr<SampledAudioNode> audioClipNode;
    std::shared_ptr<PannerNode> panner;
    std::chrono::steady_clock::time_point prev;
    ImVec4 pos;
    ImVec4 minPos;
    ImVec4 maxPos;
    bool autopan;

    virtual char const* const name() const override { return "HRTF Spatialization"; }

    explicit ex_hrtf_spatialization(Demo& demo) : labsound_example(demo)
    {
        autopan = true;
        pos = ImVec4{ 0, 0.1f, 0.1f, 0 };
        minPos = ImVec4{ -1, -1, -1, 0 };
        maxPos = ImVec4{ 1, 1, 1, 0 };

        auto& ac = *_demo->context.get();
        std::cout << "Sample Rate is: " << ac.sampleRate() << std::endl;
        audioClip = _demo->MakeBusFromSampleFile("samples/trainrolling.wav", ac.sampleRate());
        audioClipNode = std::make_shared<SampledAudioNode>(ac);

        std::string hrtf_path = asset_base;
        hrtf_path += "/hrtf";

        panner = std::make_shared<PannerNode>(ac, hrtf_path.c_str());  // note hrtf search path
        _root_node = panner;

        ContextRenderLock r(&ac, "ex_hrtf_spatialization");

        panner->setPanningModel(PanningMode::HRTF);

        audioClipNode->setBus(r, audioClip);
        ac.connect(panner, audioClipNode, 0, 0);
    }

    virtual void play() override final
    {
        connect();

        auto& ac = *_demo->context.get();
        ac.listener()->setPosition({ 0, 0, 0 });
        panner->setVelocity(4, 0, 0);
        prev = std::chrono::steady_clock::now();
        audioClipNode->schedule(0.0, -1); // -1 to loop forever
    }

    virtual void update() override
    {
        if (!autopan)
            return;

        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        const float seconds = 10.f;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - prev;
        float t = fmodf(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() * 0.001f, seconds);
        float halfTime = seconds * 0.5f;
        pos.x = (t - halfTime) / halfTime;

        panner->setPosition({ pos.x, pos.y, pos.z });
    }
    
    virtual void ui() override final
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        ImGui::BeginChild("###HRTF", ImVec2{ 0, 500 }, true);
        if (InputVec3("Pos", &pos, minPos, maxPos, 1.f))
        {
            autopan = false;
            panner->setPosition({ pos.x, pos.y, pos.z });
        }
        if (ImGui::Button("Stop"))
            disconnect();

        ImGui::EndChild();
    }
};



////////////////////////////////
//    ex_convolution_reverb    //
////////////////////////////////

// This shows the use of the `ConvolverNode` to produce reverb from an arbitrary impulse response.
struct ex_convolution_reverb : public labsound_example
{
    std::shared_ptr<ConvolverNode> convolve;
    std::shared_ptr<GainNode> wetGain;
    std::shared_ptr<GainNode> dryGain;
    std::shared_ptr<SampledAudioNode> voiceNode;
    std::shared_ptr<GainNode> masterGain;

    virtual char const* const name() const override { return "Convolution Reverb"; }

    explicit ex_convolution_reverb(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        std::shared_ptr<AudioBus> impulseResponseClip = _demo->MakeBusFromSampleFile("impulse/cardiod-rear-levelled.wav", ac.sampleRate());
        std::shared_ptr<AudioBus> voiceClip = _demo->MakeBusFromSampleFile("samples/voice.ogg", ac.sampleRate());

        if (!impulseResponseClip || !voiceClip)
        {
            std::cerr << "Could not open sample data\n";
            return;
        }

        ContextRenderLock r(&ac, "ex_convolution_reverb");
        masterGain = std::make_shared<GainNode>(ac);
        masterGain->gain()->setValue(0.5f);

        convolve = std::make_shared<ConvolverNode>(ac);
        convolve->setImpulse(impulseResponseClip);

        wetGain = std::make_shared<GainNode>(ac);
        wetGain->gain()->setValue(0.5f);
        dryGain = std::make_shared<GainNode>(ac);
        dryGain->gain()->setValue(0.1f);

        voiceNode = std::make_shared<SampledAudioNode>(ac);
        voiceNode->setBus(r, voiceClip);

        // voice --> dry --+----------------------+
        //                 |                      |
        //                 +-> convolve --> wet --+--> master --> 

        ac.connect(dryGain, voiceNode, 0, 0);
        ac.connect(convolve, dryGain, 0, 0);
        ac.connect(wetGain, convolve, 0, 0);
        ac.connect(masterGain, wetGain, 0, 0);
        ac.connect(masterGain, dryGain, 0, 0);
        _root_node = masterGain;// masterGain;
    }

    virtual void play() override final
    {
        if (!_root_node)
            return;

        connect();
        voiceNode->schedule(0.0);
    }

    virtual void ui() override final
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        ImGui::BeginChild("###CONVREVERB", ImVec2{ 0, 100 }, true);
        ImGui::TextUnformatted("Convolution reverb");
        static float dry = dryGain->gain()->value();
        if (ImGui::InputFloat("dry gain", &dry))
        {
            dryGain->gain()->setValue(dry);
        }
        static float wet = wetGain->gain()->value();
        if (ImGui::InputFloat("wet gain", &wet))
        {
            wetGain->gain()->setValue(wet);
        }
        static float master = masterGain->gain()->value();
        if (ImGui::InputFloat("master gain", &master))
        {
            masterGain->gain()->setValue(master);
        }
        if (ImGui::Button("Disconnect mic"))
        {
            disconnect();
        }
        ImGui::EndChild();
    }

    //ui - file chooser for impulse response and for voice clip
};

///////////////////
//    ex_misc    //
///////////////////

// An example with a several of nodes to verify api + functionality changes/improvements/regressions
struct ex_misc : public labsound_example
{
    std::array<int, 8> majorScale = { 0, 2, 4, 5, 7, 9, 11, 12 };
    std::array<int, 8> naturalMinorScale = { 0, 2, 3, 5, 7, 9, 11, 12 };
    std::array<int, 6> pentatonicMajor = { 0, 2, 4, 7, 9, 12 };
    std::array<int, 8> pentatonicMinor = { 0, 3, 5, 7, 10, 12 };
    std::array<int, 8> delayTimes = { 266, 533, 399 };

    std::shared_ptr<AudioBus> audioClip;
    std::shared_ptr<SampledAudioNode> audioClipNode;
    std::shared_ptr<PingPongDelayNode> pingping;

    virtual char const* const name() const override { return "PingPong Delay"; }

    explicit ex_misc(Demo& demo) : labsound_example(demo) 
    {
        auto& ac = *_demo->context.get();
        audioClip = _demo->MakeBusFromSampleFile("samples/cello_pluck/cello_pluck_As0.wav", ac.sampleRate());
        audioClipNode = std::make_shared<SampledAudioNode>(ac);
        pingping = std::make_shared<PingPongDelayNode>(ac, 240.0f);

        ContextRenderLock r(&ac, "ex_misc");

        pingping->BuildSubgraph(ac);
        pingping->SetFeedback(.75f);
        pingping->SetDelayIndex(lab::TempoSync::TS_16);

        _root_node = pingping->output;

        audioClipNode->setBus(r, audioClip);
        ac.connect(pingping->input, audioClipNode, 0, 0);
    }

    virtual void play() override
    {
        connect();
        audioClipNode->schedule(0.25);
    }
};

///////////////////////////
//    ex_dalek_filter    //
///////////////////////////

// Send live audio to a Dalek filter, constructed according to the recipe at http://webaudio.prototyping.bbc.co.uk/ring-modulator/.
// This is used as an example of a complex graph constructed using the LabSound API.
struct ex_dalek_filter : public labsound_example
{
    std::shared_ptr<AudioHardwareInputNode> input;

    std::shared_ptr<OscillatorNode> vIn;
    std::shared_ptr<GainNode> vInGain;
    std::shared_ptr<GainNode> vInInverter1;
    std::shared_ptr<GainNode> vInInverter2;
    std::shared_ptr<GainNode> vInInverter3;
    std::shared_ptr<DiodeNode> vInDiode1;
    std::shared_ptr<DiodeNode> vInDiode2;
    std::shared_ptr<GainNode> vcInverter1;
    std::shared_ptr<DiodeNode> vcDiode3;
    std::shared_ptr<DiodeNode> vcDiode4;
    std::shared_ptr<GainNode> outGain;
    std::shared_ptr<DynamicsCompressorNode> compressor;
    std::shared_ptr<SampledAudioNode> audioClipNode;

    virtual char const* const name() const override { return "Mic Dalek"; }

    explicit ex_dalek_filter(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();

        std::shared_ptr<lab::AudioBus> audioClip;
        if (!demo.use_live)
        {
            audioClip = _demo->MakeBusFromSampleFile("samples/voice.ogg", ac.sampleRate());
            if (!audioClip)
                return;
            
            audioClipNode = std::make_shared<SampledAudioNode>(ac);
        }

        ContextRenderLock r(&ac, "ex_dalek_filter");

        vIn = std::make_shared<OscillatorNode>(ac);
        vIn->frequency()->setValue(30.0f);
        vIn->start(0.f);

        vInGain = std::make_shared<GainNode>(ac);
        vInGain->gain()->setValue(0.5f);

        // GainNodes can take negative gain which represents phase inversion
        vInInverter1 = std::make_shared<GainNode>(ac);
        vInInverter1->gain()->setValue(-1.0f);
        vInInverter2 = std::make_shared<GainNode>(ac);
        vInInverter2->gain()->setValue(-1.0f);

        vInDiode1 = std::make_shared<DiodeNode>(ac);
        vInDiode2 = std::make_shared<DiodeNode>(ac);

        vInInverter3 = std::make_shared<GainNode>(ac);
        vInInverter3->gain()->setValue(-1.0f);

        // Now we create the objects on the Vc side of the graph
        vcInverter1 = std::make_shared<GainNode>(ac);
        vcInverter1->gain()->setValue(-1.0f);

        vcDiode3 = std::make_shared<DiodeNode>(ac);
        vcDiode4 = std::make_shared<DiodeNode>(ac);

        // A gain node to control master output levels
        outGain = std::make_shared<GainNode>(ac);
        outGain->gain()->setValue(1.0f);

        // A small addition to the graph given in Parker's paper is a compressor node
        // immediately before the output. This ensures that the user's volume remains
        // somewhat constant when the distortion is increased.
        compressor = std::make_shared<DynamicsCompressorNode>(ac);
        compressor->threshold()->setValue(-14.0f);

        // Now we connect up the graph following the block diagram above (on the web page).
        // When working on complex graphs it helps to have a pen and paper handy!

        if (demo.use_live)
        {
            input = lab::MakeAudioHardwareInputNode(r);
            ac.connect(vcInverter1, input, 0, 0);
            ac.connect(vcDiode4, input, 0, 0);
        }
        else
        {
            audioClipNode->setBus(r, audioClip);
            ac.connect(vcInverter1, audioClipNode, 0, 0);
            ac.connect(vcDiode4, audioClipNode, 0, 0);
        }

        ac.connect(vcDiode3, vcInverter1, 0, 0);

        // Then the Vin side
        ac.connect(vInGain, vIn, 0, 0);
        ac.connect(vInInverter1, vInGain, 0, 0);
        ac.connect(vcInverter1, vInGain, 0, 0);
        ac.connect(vcDiode4, vInGain, 0, 0);

        ac.connect(vInInverter2, vInInverter1, 0, 0);
        ac.connect(vInDiode2, vInInverter1, 0, 0);
        ac.connect(vInDiode1, vInInverter2, 0, 0);

        // Finally connect the four diodes to the destination via the output-stage compressor and master gain node
        ac.connect(vInInverter3, vInDiode1, 0, 0);
        ac.connect(vInInverter3, vInDiode2, 0, 0);

        ac.connect(compressor, vInInverter3, 0, 0);
        ac.connect(compressor, vcDiode3, 0, 0);
        ac.connect(compressor, vcDiode4, 0, 0);

        ac.connect(outGain, compressor, 0, 0);

        _root_node = vcDiode4;// outGain;
    }

    virtual void play() override
    {
        connect();
        if (!input)
            audioClipNode->schedule(0.f);
    }

    virtual void ui() override final
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        ImGui::BeginChild("###DALEK", ImVec2{ 0, 100 }, true);
        ImGui::TextUnformatted("Dalek voice changer active");
        if (ImGui::Button("Disconnect mic"))
        {
            disconnect();
        }
        ImGui::EndChild();
    }

};

/////////////////////////////////
//    ex_redalert_synthesis    //
/////////////////////////////////

// This is another example of a non-trival graph constructed with the LabSound API. Furthermore, it incorporates
// the use of several `FunctionNodes` that are base types used for implementing complex DSP without modifying
// LabSound internals directly.
struct ex_redalert_synthesis : public labsound_example
{
    std::shared_ptr<FunctionNode> sweep;
    std::shared_ptr<FunctionNode> outputGainFunction;

    std::shared_ptr<OscillatorNode> osc;
    std::shared_ptr<GainNode> oscGain;
    std::shared_ptr<OscillatorNode> resonator;
    std::shared_ptr<GainNode> resonatorGain;
    std::shared_ptr<GainNode> resonanceSum;

    std::shared_ptr<DelayNode> delay[5];

    std::shared_ptr<GainNode> delaySum;
    std::shared_ptr<GainNode> filterSum;

    std::shared_ptr<BiquadFilterNode> filter[5];

    virtual char const* const name() const override { return "Red Alert"; }

    explicit ex_redalert_synthesis(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        ContextRenderLock r(&ac, "ex_redalert_synthesis");

        sweep = std::make_shared<FunctionNode>(ac, 1);
        sweep->setFunction([](ContextRenderLock& r, FunctionNode* me, int channel, float* values, size_t framesToProcess) 
        {
            double dt = 1.0 / r.context()->sampleRate();
            double now = fmod(me->now(), 1.2f);

            for (size_t i = 0; i < framesToProcess; ++i)
            {
                //0 to 1 in 900 ms with a 1200ms gap in between
                if (now > 0.9)
                {
                    values[i] = 487.f + 360.f;
                }
                else
                {
                    values[i] = std::sqrt((float)now * 1.f / 0.9f) * 487.f + 360.f;
                }

                now += dt;
            }
        });

        outputGainFunction = std::make_shared<FunctionNode>(ac, 1);
        outputGainFunction->setFunction([](ContextRenderLock& r, FunctionNode* me, int channel, float* values, size_t framesToProcess) 
        {
            double dt = 1.0 / r.context()->sampleRate();
            double now = fmod(me->now(), 1.2f);

            for (size_t i = 0; i < framesToProcess; ++i)
            {
                //0 to 1 in 900 ms with a 1200ms gap in between
                if (now > 0.9)
                {
                    values[i] = 0;
                }
                else
                {
                    values[i] = 0.333f;
                }

                now += dt;
            }
        });

        osc = std::make_shared<OscillatorNode>(ac);
        osc->setType(OscillatorType::SAWTOOTH);
        osc->frequency()->setValue(220);
        oscGain = std::make_shared<GainNode>(ac);
        oscGain->gain()->setValue(0.5f);

        resonator = std::make_shared<OscillatorNode>(ac);
        resonator->setType(OscillatorType::SINE);
        resonator->frequency()->setValue(220);

        resonatorGain = std::make_shared<GainNode>(ac);
        resonatorGain->gain()->setValue(0.0f);

        resonanceSum = std::make_shared<GainNode>(ac);
        resonanceSum->gain()->setValue(0.5f);

        // sweep drives oscillator frequency
        ac.connectParam(osc->frequency(), sweep, 0);

        // oscillator drives resonator frequency
        ac.connectParam(resonator->frequency(), osc, 0);

        // osc --> oscGain -------------+
        // resonator -> resonatorGain --+--> resonanceSum
        ac.connect(oscGain, osc, 0, 0);
        ac.connect(resonanceSum, oscGain, 0, 0);
        ac.connect(resonatorGain, resonator, 0, 0);
        ac.connect(resonanceSum, resonatorGain, 0, 0);

        delaySum = std::make_shared<GainNode>(ac);
        delaySum->gain()->setValue(0.2f);

        // resonanceSum --+--> delay0 --+
        //                +--> delay1 --+
        //                + ...    .. --+
        //                +--> delay4 --+---> delaySum
        float delays[5] = { 0.015f, 0.022f, 0.035f, 0.024f, 0.011f };
        for (int i = 0; i < 5; ++i)
        {
            delay[i] = std::make_shared<DelayNode>(ac, 0.04f);
            delay[i]->delayTime()->setFloat(delays[i]);
            ac.connect(delay[i], resonanceSum, 0, 0);
            ac.connect(delaySum, delay[i], 0, 0);
        }

        filterSum = std::make_shared<GainNode>(ac);
        filterSum->gain()->setValue(0.2f);

        // delaySum --+--> filter0 --+
        //            +--> filter1 --+
        //            +--> filter2 --+
        //            +--> filter3 --+
        //            +--------------+----> filterSum
        //
        ac.connect(filterSum, delaySum, 0, 0);

        float centerFrequencies[4] = { 740.f, 1400.f, 1500.f, 1600.f };
        for (int i = 0; i < 4; ++i)
        {
            filter[i] = std::make_shared<BiquadFilterNode>(ac);
            filter[i]->frequency()->setValue(centerFrequencies[i]);
            filter[i]->q()->setValue(12.f);
            ac.connect(filter[i], delaySum, 0, 0);
            ac.connect(filterSum, filter[i], 0, 0);
        }

        // filterSum --> destination
        ac.connectParam(filterSum->gain(), outputGainFunction, 0);

        _root_node = filterSum;
    }

    virtual void play() override
    {
        connect();
        sweep->start(0);
        outputGainFunction->start(0);
        osc->start(0);
        resonator->start(0);
    }

    virtual void ui() override final
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        ImGui::BeginChild("###ALERT", ImVec2{ 0, 100 }, true);
        ImGui::TextUnformatted("Red Alert");
        if (ImGui::Button("Stop"))
        {
            disconnect();
        }
        ImGui::EndChild();
    }

};

//////////////////////////
//    ex_wavepot_dsp    //
//////////////////////////

// "Unexpected Token" from Wavepot. Original by Stagas: http://wavepot.com/stagas/unexpected-token (MIT License)
// Wavepot is effectively ShaderToy but for the WebAudio API. 
// This sample shows the utility of LabSound as an experimental playground for DSP (synthesis + processing) using the `FunctionNode`. 
struct ex_wavepot_dsp : public labsound_example
{
    float note(int n, int octave = 0)
    {
        return std::pow(2.0f, (n - 33.f + (12.f * octave)) / 12.0f) * 440.f;
    }

    std::vector<std::vector<int>> bassline = {
        {7, 7, 7, 12, 10, 10, 10, 15},
        {7, 7, 7, 15, 15, 17, 10, 29},
        {7, 7, 7, 24, 10, 10, 10, 19},
        {7, 7, 7, 15, 29, 24, 15, 10} };

    std::vector<int> melody = {
        7, 15, 7, 15,
        7, 15, 10, 15,
        10, 12, 24, 19,
        7, 12, 10, 19 };

    std::vector<std::vector<int>> chords = { {7, 12, 17, 10}, {10, 15, 19, 24} };

    float quickSin(float x, float t)
    {
        return std::sin(2.0f * float(static_cast<float>(LAB_PI)) * t * x);
    }

    float quickSaw(float x, float t)
    {
        return 1.0f - 2.0f * fmod(t, (1.f / x)) * x;
    }

    float quickSqr(float x, float t)
    {
        return quickSin(x, t) > 0 ? 1.f : -1.f;
    }

    // perc family of functions implement a simple attack/decay, creating a short & percussive envelope for the signal
    float perc(float wave, float decay, float o, float t)
    {
        float env = std::max(0.f, 0.889f - (o * decay) / ((o * decay) + 1.f));
        auto ret = wave * env;
        return ret;
    }

    float perc_b(float wave, float decay, float o, float t)
    {
        float env = std::min(0.f, 0.950f - (o * decay) / ((o * decay) + 1.f));
        auto ret = wave * env;
        return ret;
    }

    float hardClip(float n, float x)
    {
        return x > n ? n : x < -n ? -n : x;
    }

    struct FastLowpass
    {
        float v = 0;
        float operator()(float n, float input)
        {
            return v += (input - v) / n;
        }
    };

    struct FastHighpass
    {
        float v = 0;
        float operator()(float n, float input)
        {
            return v += input - v * n;
        }
    };

    // http://www.musicdsp.org/showone.php?id=24
    // A Moog-style 24db resonant lowpass
    struct MoogFilter
    {
        float y1 = 0;
        float y2 = 0;
        float y3 = 0;
        float y4 = 0;

        float oldx = 0;
        float oldy1 = 0;
        float oldy2 = 0;
        float oldy3 = 0;

        float p, k, t1, t2, r, x;

        float process(float cutoff_, float resonance_, float sample_, float sampleRate)
        {
            float cutoff = 2.0f * cutoff_ / sampleRate;
            float resonance = static_cast<float>(resonance_);
            float sample = static_cast<float>(sample_);

            p = cutoff * (1.8f - 0.8f * cutoff);
            k = 2.f * std::sin(cutoff * static_cast<float>(M_PI) * 0.5f) - 1.0f;
            t1 = (1.0f - p) * 1.386249f;
            t2 = 12.0f + t1 * t1;
            r = resonance * (t2 + 6.0f * t1) / (t2 - 6.0f * t1);

            x = sample - r * y4;

            // Four cascaded one-pole filters (bilinear transform)
            y1 = x * p + oldx * p - k * y1;
            y2 = y1 * p + oldy1 * p - k * y2;
            y3 = y2 * p + oldy2 * p - k * y3;
            y4 = y3 * p + oldy3 * p - k * y4;

            // Clipping band-limited sigmoid
            y4 -= (y4 * y4 * y4) / 6.f;

            oldx = x;
            oldy1 = y1;
            oldy2 = y2;
            oldy3 = y3;

            return y4;
        }
    };

    MoogFilter lp_a[2];
    MoogFilter lp_b[2];
    MoogFilter lp_c[2];

    FastLowpass fastlp_a[2];
    FastHighpass fasthp_c[2];

    std::shared_ptr<FunctionNode> grooveBox;
    std::shared_ptr<ADSRNode> envelope;

    double elapsedTime;
    float songLenSeconds;

    virtual char const* const name() const override { return "Wavepot DSP"; }

    explicit ex_wavepot_dsp(Demo& demo) : labsound_example(demo)
    {
        elapsedTime = 0.;
        songLenSeconds = 12.0f;

        auto& ac = *_demo->context.get();
        envelope = std::make_shared<ADSRNode>(ac);
        envelope->set(6.0f, 0.75f, 0.125, 14.0f, 0.0f, songLenSeconds);
        envelope->gate()->setValue(1.f);
        grooveBox = std::make_shared<FunctionNode>(ac, 2);

        grooveBox->setFunction([this](ContextRenderLock& r, FunctionNode* self, int channel, float* samples, size_t framesToProcess)
        {
            float lfo_a, lfo_b, lfo_c;
            float bassWaveform, percussiveWaveform, bassSample;
            float padWaveform, padSample;
            float kickWaveform, kickSample;
            float synthWaveform, synthPercussive, synthDegradedWaveform, synthSample;
                
            float dt = 1.f / r.context()->sampleRate();  // time duration of one sample
            float now = static_cast<float>(self->now());

            int nextMeasure = int((now / 2)) % bassline.size();
            auto bm = bassline[nextMeasure];

            int nextNote = int((now * 4.f)) % bm.size();
            float bn = note(bm[nextNote], 0);

            auto p = chords[int(now / 4) % chords.size()];

            auto mn = note(melody[int(now * 3.f) % melody.size()], int(2 - (now * 3)) % 4);

            for (size_t i = 0; i < framesToProcess; ++i)
            {
                lfo_a = quickSin(2.0f, now);
                lfo_b = quickSin(1.0f / 32.0f, now);
                lfo_c = quickSin(1.0f / 128.0f, now);

                // Bass
                bassWaveform = quickSaw(bn, now) * 1.9f + quickSqr(bn / 2.f, now) * 1.0f + quickSin(bn / 2.f, now) * 2.2f + quickSqr(bn * 3.f, now) * 3.f;
                percussiveWaveform = perc(bassWaveform / 3.f, 48.0f, fmod(now, 0.125f), now) * 1.0f;
                bassSample = lp_a[channel].process(1000.f + (lfo_b * 140.f), quickSin(0.5f, now + 0.75f) * 0.2f, percussiveWaveform, r.context()->sampleRate());

                // Pad
                padWaveform = 5.1f * quickSaw(note(p[0], 1), now) + 3.9f * quickSaw(note(p[1], 2), now) + 4.0f * quickSaw(note(p[2], 1), now) + 3.0f * quickSqr(note(p[3], 0), now);
                padSample = 1.0f - ((quickSin(2.0f, now) * 0.28f) + 0.5f) * fasthp_c[channel](0.5f, lp_c[channel].process(1100.f + (lfo_a * 150.f), 0.05f, padWaveform * 0.03f, r.context()->sampleRate()));

                // Kick
                kickWaveform = hardClip(0.37f, quickSin(note(7, -1), now)) * 2.0f + hardClip(0.07f, quickSaw(note(7, -1), now * 0.2f)) * 4.00f;
                kickSample = quickSaw(2.f, now) * 0.054f + fastlp_a[channel](240.0f, perc(hardClip(0.6f, kickWaveform), 54.f, fmod(now, 0.5f), now)) * 2.f;

                // Synth
                synthWaveform = quickSaw(mn, now + 1.0f) + quickSqr(mn * 2.02f, now) * 0.4f + quickSqr(mn * 3.f, now + 2.f);
                synthPercussive = lp_b[channel].process(3200.0f + (lfo_a * 400.f), 0.1f, perc(synthWaveform, 1.6f, fmod(now, 4.f), now) * 1.7f, r.context()->sampleRate()) * 1.8f;
                synthDegradedWaveform = synthPercussive * quickSin(note(5, 2), now);
                synthSample = 0.4f * synthPercussive + 0.05f * synthDegradedWaveform;

                // Mixer
                samples[i] = (0.66f * hardClip(0.65f, bassSample)) + (0.50f * padSample) + (0.66f * synthSample) + (2.75f * kickSample);

                now += dt;
            }

            elapsedTime += now;
        });

        ac.connect(envelope, grooveBox, 0, 0);
        _root_node = envelope;
    }

    virtual void play() override final
    {
        if (_root_node && _root_node->output(0)->isConnected())
            return;

        grooveBox->start(0);
        connect();
    }

    virtual void ui() override final
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        ImGui::BeginChild("###WAVEPOT", ImVec2{ 0, 100 }, true);
        ImGui::TextUnformatted("Wavepot DSP");
        if (ImGui::Button("Stop"))
        {
            disconnect();
        }
        ImGui::EndChild();
    }
};

///////////////////////////////
//    ex_granulation_node    //
///////////////////////////////

struct ex_granulation_node : public labsound_example
{
    std::shared_ptr<AudioBus> grain_source;
    std::shared_ptr<GranulationNode> granulation_node;
    std::shared_ptr<GainNode> gain;

    virtual char const* const name() const override { return "Granulation"; }

    explicit ex_granulation_node(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        grain_source = _demo->MakeBusFromSampleFile("samples/cello_pluck/cello_pluck_As0.wav", ac.sampleRate());
        if (!grain_source) 
            return;

        granulation_node = std::make_shared<GranulationNode>(ac);
        gain = std::make_shared<GainNode>(ac);
        //std::shared_ptr<RecorderNode> recorder;
        gain->gain()->setValue(0.75f);

        {
            ContextRenderLock r(&ac, "ex_granulation_node");
            granulation_node->setGrainSource(r, grain_source);

            //AudioStreamConfig outputConfig = { -1, 2, ac.sampleRate() };
            //recorder = std::make_shared<RecorderNode>(ac, outputConfig);
            //ac.addAutomaticPullNode(recorder);
            //recorder->startRecording();
        }

        ac.connect(gain, granulation_node, 0, 0);
        _root_node = gain;
        //ac.connect(recorder, gain, 0, 0);
        //recorder->stopRecording();
        //ac.removeAutomaticPullNode(recorder);
        //recorder->writeRecordingToWav("ex_granulation_node.wav", false);
    }

    virtual void play() override final
    {
        connect();
        granulation_node->start(0.0f);
    }
};

////////////////////////
//    ex_poly_blep    //
////////////////////////

struct ex_poly_blep : public labsound_example
{
    std::shared_ptr<PolyBLEPNode> polyBlep;
    std::shared_ptr<GainNode> gain;
    std::vector<PolyBLEPType> blepWaveforms =
    {
        PolyBLEPType::TRIANGLE,
        PolyBLEPType::SQUARE,
        PolyBLEPType::RECTANGLE,
        PolyBLEPType::SAWTOOTH,
        PolyBLEPType::RAMP,
        PolyBLEPType::MODIFIED_TRIANGLE,
        PolyBLEPType::MODIFIED_SQUARE,
        PolyBLEPType::HALF_WAVE_RECTIFIED_SINE,
        PolyBLEPType::FULL_WAVE_RECTIFIED_SINE,
        PolyBLEPType::TRIANGULAR_PULSE,
        PolyBLEPType::TRAPEZOID_FIXED,
        PolyBLEPType::TRAPEZOID_VARIABLE
    };

    std::chrono::steady_clock::time_point prev;
    int waveformIndex = 0;

    virtual char const* const name() const override { return "Poly BLEP"; }

    explicit ex_poly_blep(Demo& demo) : labsound_example(demo)
    {
        auto& ac = *_demo->context.get();
        polyBlep = std::make_shared<PolyBLEPNode>(ac);
        gain = std::make_shared<GainNode>(ac);

        gain->gain()->setValue(1.0f);
        ac.connect(gain, polyBlep, 0, 0);
        _root_node = gain;

        polyBlep->frequency()->setValue(220.f);
        polyBlep->setType(PolyBLEPType::TRIANGLE);
        polyBlep->start(0.0f);
    }

    virtual void play() override final
    {
        connect();
        prev = std::chrono::steady_clock::now();
    }

    virtual void update() override
    {
        if (!_root_node || !_root_node->output(0)->isConnected())
            return;

        const uint32_t delay_time_ms = 500;
        auto now = std::chrono::steady_clock::now();
        if (now - prev < std::chrono::milliseconds(delay_time_ms))
            return;

        prev = now;

        auto waveform = blepWaveforms[waveformIndex % blepWaveforms.size()];
        polyBlep->setType(waveform);
        waveformIndex++;
    }
};


std::shared_ptr<ex_simple> simple;
std::shared_ptr<ex_sfxr> sfxr;
std::shared_ptr<ex_osc_pop> osc_pop;
std::shared_ptr<ex_playback_events> playback_events;
std::shared_ptr<ex_offline_rendering> offline_rendering;
std::shared_ptr<ex_tremolo> tremolo;
std::shared_ptr<ex_frequency_modulation> frequency_mod;
std::shared_ptr<ex_runtime_graph_update> runtime_graph_update;
std::shared_ptr<ex_microphone_loopback> microphone_loopback;
std::shared_ptr<ex_microphone_reverb> microphone_reverb;
std::shared_ptr<ex_peak_compressor> peak_compressor;
std::shared_ptr<ex_stereo_panning> stereo_panning;
std::shared_ptr<ex_hrtf_spatialization> hrtf_spatialization;
std::shared_ptr<ex_convolution_reverb> convolution_reverb;
std::shared_ptr<ex_misc> misc;
std::shared_ptr<ex_dalek_filter> dalek_filter;
std::shared_ptr<ex_redalert_synthesis> redalert_synthesis;
std::shared_ptr<ex_wavepot_dsp> wavepot_dsp;
std::shared_ptr<ex_granulation_node> granulation;
std::shared_ptr<ex_poly_blep> poly_blep;

std::vector<std::shared_ptr<labsound_example>> examples;

std::shared_ptr<labsound_example> example_ui;



void instantiate_demos(Demo& demo)
{
    simple = std::make_shared<ex_simple>(demo);
    sfxr = std::make_shared<ex_sfxr>(demo);
    osc_pop = std::make_shared<ex_osc_pop>(demo);
    playback_events = std::make_shared<ex_playback_events>(demo);
    offline_rendering = std::make_shared<ex_offline_rendering>(demo);
    tremolo = std::make_shared<ex_tremolo>(demo);
    frequency_mod = std::make_shared<ex_frequency_modulation>(demo);
    runtime_graph_update = std::make_shared<ex_runtime_graph_update>(demo);
    microphone_loopback = std::make_shared<ex_microphone_loopback>(demo);
    microphone_reverb = std::make_shared<ex_microphone_reverb>(demo);
    peak_compressor = std::make_shared<ex_peak_compressor>(demo);
    stereo_panning = std::make_shared<ex_stereo_panning>(demo);
    hrtf_spatialization = std::make_shared<ex_hrtf_spatialization>(demo);
    convolution_reverb = std::make_shared<ex_convolution_reverb>(demo);
    misc = std::make_shared<ex_misc>(demo);
    dalek_filter = std::make_shared<ex_dalek_filter>(demo);
    redalert_synthesis = std::make_shared<ex_redalert_synthesis>(demo);
    wavepot_dsp = std::make_shared<ex_wavepot_dsp>(demo);
    granulation = std::make_shared<ex_granulation_node>(demo);
    poly_blep = std::make_shared<ex_poly_blep>(demo);

    examples.push_back(simple);
    examples.push_back(sfxr);
    examples.push_back(osc_pop);
    examples.push_back(playback_events);
    examples.push_back(offline_rendering);
    examples.push_back(tremolo);
    examples.push_back(frequency_mod);
    examples.push_back(runtime_graph_update);
    examples.push_back(microphone_loopback);
    examples.push_back(microphone_reverb);
    examples.push_back(peak_compressor);
    examples.push_back(stereo_panning);
    examples.push_back(hrtf_spatialization);
    examples.push_back(convolution_reverb);
    examples.push_back(misc);
    examples.push_back(dalek_filter);
    examples.push_back(redalert_synthesis);
    examples.push_back(wavepot_dsp);
    examples.push_back(granulation);
    examples.push_back(poly_blep);
}

void run_demo_ui(Demo& demo)
{
    auto c = demo.context.get();
    for (auto& i : examples)
    {
        i->update();
    }

    ImGui::Columns(2);

    for (auto& i : examples)
    {
        if (ImGui::Button(i->name()))
        {
            if (example_ui)
                example_ui->disconnect();

            example_ui = i;
            i->play();
            traverse_ui(*c);
        }
    }

    ImGui::NextColumn();

    if (example_ui)
        example_ui->ui();

    ImGui::Separator();

    if (ImGui::Button("Flush debug data"))
    {
        c->flushDebugBuffer("C:\\Projects\\foo.wav");
    }

    if (example_ui && ImGui::Button("Disconnect demo"))
    {
        example_ui.reset();
        for (auto& i : examples)
        {
            i->disconnect();
        }

        c->synchronizeConnections();
        traverse_ui(*c);
    }

    ImVec2 pos = ImGui::GetCursorPos();
    float y = 0;
    for (auto& i : displayNodes)
    {
        ImVec2 p = pos;
        p.x += i.x * 5;
        p.y += y;
        y += 15;
        ImGui::SetCursorPos(p);
        ImGui::Button(i.name.c_str());
    }
}

void run_context_ui(Demo& demo)
{
    static std::vector<std::string> inputs;
    static std::vector<std::string> outputs;
    static std::vector<int> input_reindex;
    static std::vector<int> output_reindex;

    static int input = 0;
    static int output = 0;
    static bool* input_checks;  // nb: std::vector<bool> is a ... contraption. Not a vector of bools per se
    static bool* output_checks;

    static std::vector<AudioDeviceInfo> info;
    if (info.size() == 0)
    {
        info = lab::MakeAudioDeviceList();
        int reindex = 0;
        for (auto& i : info)
        {
            if (i.num_input_channels > 0)
            {
                inputs.push_back(i.identifier);
                input_reindex.push_back(reindex);
            }
            if (i.num_output_channels > 0)
            {
                outputs.push_back(i.identifier);
                output_reindex.push_back(reindex);
            }
            ++reindex;
        }

        input_checks = reinterpret_cast<bool*>(malloc(sizeof(bool) * inputs.size()));
        output_checks = reinterpret_cast<bool*>(malloc(sizeof(bool) * outputs.size()));

        for (auto& i : info)
        {
            if (i.num_input_channels > 0)
            {
                input_checks[inputs.size()] = i.is_default_input;
            }
            if (i.num_output_channels > 0)
            {
                output_checks[outputs.size()] = i.is_default_output;
            }
        }
    }

    ImGui::BeginChild("Devices", ImVec2{ 0, 100 });
    ImGui::Columns(2);
    ImGui::TextUnformatted("Inputs");
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
    for (int j = 0; j < inputs.size(); ++j)
        if (ImGui::Checkbox(inputs[j].c_str(), &input_checks[j]))
        {
            for (int i = 0; i < inputs.size(); ++i)
                if (i != j)
                    input_checks[i] = false;
        }

    ImGui::NextColumn();
    ImGui::TextUnformatted("Outputs");
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
    for (int j = 0; j < outputs.size(); ++j)
        if (ImGui::Checkbox(outputs[j].c_str(), &output_checks[j]))
        {
            for (int i = 0; i < outputs.size(); ++i)
                if (i != j)
                    output_checks[i] = false;
        }
    ImGui::EndChild();
    if (ImGui::Button("Create Context"))
    {
        AudioStreamConfig inputConfig;
        for (int i = 0; i < inputs.size(); ++i)
            if (input_checks[i])
            {
                int r = input_reindex[i];
                inputConfig.device_index = r;
                inputConfig.desired_channels = info[r].num_input_channels;
                inputConfig.desired_samplerate = info[r].nominal_samplerate;
                break;
            }
        AudioStreamConfig outputConfig;
        for (int i = 0; i < outputs.size(); ++i)
            if (output_checks[i])
            {
                int r = output_reindex[i];
                outputConfig.device_index = r;
                outputConfig.desired_channels = info[r].num_output_channels;
                outputConfig.desired_samplerate = info[r].nominal_samplerate;
                break;
            }

        if (outputConfig.device_index >= 0)
        {
            demo.use_live = inputConfig.device_index >= 0;
            demo.context = lab::MakeRealtimeAudioContext(outputConfig, inputConfig);
            auto& ac = *demo.context.get();
            demo.recorder = std::make_shared<RecorderNode>(ac, outputConfig);
            demo.context->connect(ac.device(), demo.recorder);
            demo.context->synchronizeConnections();
            instantiate_demos(demo);
        }
    }
}

Demo* demo = nullptr;

void frame() 
{
    ImGuiIO& io = ImGui::GetIO();
    static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    static bool open = true;
    ImGui::Begin("LabSound Demo", &open, flags);

    if (demo->context)
        run_demo_ui(*demo);
    else
        run_context_ui(*demo);

    ImGui::End();
}


int main(int, char **) 
{
    Demo _demo;
    demo = &_demo;

    // when ready start the UI (this will not return until the app finishes)
    imgui_app(frame);    

    _demo.shutdown();
    return 0;
}
