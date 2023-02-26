
// SPDX-License-Identifier: BSD-2-Clause
// Copyright (C) 2020, The LabSound Authors. All rights reserved.

#pragma once
#ifndef AudioNode_h
#define AudioNode_h

#include "LabSound/core/AudioNodeDescriptor.h"
#include "LabSound/core/AudioNodeScheduler.h"
#include "LabSound/core/AudioParamDescriptor.h"
#include "LabSound/core/AudioSettingDescriptor.h"
#include "LabSound/core/Mixing.h"
#include "LabSound/core/Profiler.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lab
{

// clang-format off
enum PanningModel
{
    PANNING_NONE = 0,
    EQUALPOWER   = 1,
    HRTF         = 2,
    _PanningModeCount
};

enum FilterType
{
    FILTER_NONE = 0,
    LOWPASS     = 1,
    HIGHPASS    = 2,
    BANDPASS    = 3,
    LOWSHELF    = 4,
    HIGHSHELF   = 5,
    PEAKING     = 6,
    NOTCH       = 7,
    ALLPASS     = 8,
    _FilterTypeCount
};

enum OscillatorType
{
    OSCILLATOR_NONE  = 0,
    SINE             = 1,
    FAST_SINE        = 2,
    SQUARE           = 3,
    SAWTOOTH         = 4,
    FALLING_SAWTOOTH = 5,
    TRIANGLE         = 6,
    CUSTOM           = 7,
    _OscillatorTypeCount
};
// clang-format on

class AudioBus;
class AudioContext;
class AudioParam;
class AudioSetting;
class AudioSourceProvider;
class ContextGraphLock;
class ContextRenderLock;

// AudioNode is the basic building block for a signal processing graph.
// It may be an audio source, an intermediate processing module, or an audio 
// destination.
//

// low bit of current_sample_frame indexes time point 0 or 1
// (so that time and epoch are written atomically, after the alternative epoch has been filled in)
struct SamplingInfo
{
    uint64_t current_sample_frame{0};
    double current_time{0.0};
    float sampling_rate{0.f};
    std::chrono::high_resolution_clock::time_point epoch[2];
};


class AudioNode
{
protected:
    // all storage to the node is in this struct ~ the graph and
    // other usages can refer to the Internal data rather than the
    // node, so that a node can be deleted, but can expire from
    // the processing graph when no references are being held any more.

    struct Internal {
        explicit Internal(AudioContext& ac);
                
        AudioNodeScheduler scheduler;

        struct InputMapping {
            std::shared_ptr<AudioNode> node;
            int inputChannel = -1;
            int outputChannel = -1;
        };

        std::vector<InputMapping> inputs;
        std::shared_ptr<AudioBus> output;
        std::shared_ptr<AudioBus> summingBus;
        
        std::vector<std::shared_ptr<AudioNode>> receivers;

        std::vector<std::shared_ptr<AudioParam>>   params;
        std::vector<std::shared_ptr<AudioSetting>> settings;

        ProfileSample graphTime;    // how much time the node spend pulling inputs
        ProfileSample totalTime;    // total time spent by the node. total-graph is the self time.

        ChannelInterpretation m_channelInterpretation{ ChannelInterpretation::Speakers };
        int desiredChannelCount;
        int color = 0;
        bool isInitialized {false};
        
        uint64_t graphEpoch = 0;
    };
    std::shared_ptr<Internal> _self;
    
public :
    enum : int
    {
        ProcessingSizeInFrames = 128
    };

    AudioNode() = delete;
    virtual ~AudioNode();

    explicit AudioNode(AudioContext &, AudioNodeDescriptor const &);

    static void printGraph(const AudioNode* root, 
                        std::function<void(const char *)> prnln);
    
    ProfileSample graphTime() const { return _self->graphTime; }
    ProfileSample totalTime() const { return _self->totalTime; }

    SchedulingState schedulingState() const { return _self->scheduler.playbackState(); }

    // update the scheduler, and report whether the node is scheduled to play
    // during this quantum
    bool updateSchedule(ContextRenderLock& r, int bufferSize);

    //--------------------------------------------------
    // required interface
    //
    virtual const char* name() const = 0;

    // The input busses (if any) will already have their input data available
    // when process() is called. Subclasses will take this input data and 
    // render into this node's output buses.
    // Called from context's audio thread.
    virtual void process(ContextRenderLock &, int bufferSize);

    // Resets DSP processing state (clears delay lines, filter memory, etc.)
    // Called from context's audio thread.
    virtual void reset(ContextRenderLock &) = 0;

    // tailTime() is the length of time (not counting latency time) where 
    // non-zero output may occur after continuous silent input.
    virtual double tailTime(ContextRenderLock & r) const = 0;

    // latencyTime() is the length of time it takes for non-zero output to 
    // appear after non-zero input is provided. This only applies to processing
    // delay which is an artifact of the processing algorithm chosen and is 
    // *not* part of the intrinsic desired effect. For example, a "delay" 
    // effect is expected to delay the signal, and thus would not be considered
    // latency.
    virtual double latencyTime(ContextRenderLock & r) const = 0;

    //--------------------------------------------------
    // overridable interface
    //
    // If the final node class has ScheduledNode in its class hierarchy, this 
    // will return true. This is to save the cost of a dynamic_cast when
    // scheduling nodes.
    virtual bool isScheduledNode() const { return false; }

    // No significant resources should be allocated until initialize() is called.
    // Processing may not occur until a node is initialized.
    virtual void initialize();
    virtual void uninitialize();

    //--------------------------------------------------
    // rendering
    bool inputsAreSilent(ContextRenderLock &);
    void silenceOutputs(ContextRenderLock &);
    void unsilenceOutputs(ContextRenderLock &);

    // propagatesSilence() should return true if the node will generate silent output when given silent input. By default, AudioNode
    // will take tailTime() and latencyTime() into account when determining whether the node will propagate silence.
    virtual bool propagatesSilence(ContextRenderLock & r) const;

    // processIfNecessary() is called by our output(s) when the rendering graph needs this AudioNode to process.
    // This method ensures that the AudioNode will only process once per rendering time quantum even if it's called repeatedly.
    // This handles the case of "fanout" where an output is connected to multiple AudioNode inputs.
    // Called from context's audio thread.
    void processIfNecessary(ContextRenderLock & r, int bufferSize);

    //--------------------------------------------------
    // inputs and outputs
    bool isInitialized() const { return _self->isInitialized; }

    int numberOfInputs() const {
        return static_cast<int>(_self->inputs.size()); }
    
    bool isConnected(std::shared_ptr<AudioNode>) const;

    std::shared_ptr<AudioNode> input(int index);
    std::shared_ptr<AudioBus> output() { return _self->output; }
    std::shared_ptr<AudioBus> summedInput();
    
    std::shared_ptr<AudioBus> renderedOutputCurrentQuantum(ContextRenderLock& r);

    static void connect(std::shared_ptr<AudioNode> rcv,
                        std::shared_ptr<AudioNode> send,
                        int inputSrcChannel, int dstChannel);
    void disconnect(std::shared_ptr<AudioNode> inputSrc, int inputSrcChannel, int dstChannel);
    void disconnectReceivers();
    
    //--------------------------------------------------
    // channel management

    // Called when a new connection has been made to one of our inputs or the connection number of channels has changed.
    // This potentially gives us enough information to perform a lazy initialization or, if necessary, a re-initialization.
    // Called from main thread.
//    virtual void checkNumberOfChannelsForInput(ContextRenderLock &, AudioNodeInput *);

//    virtual void conformChannelCounts();

    int channelCount();
    void setChannelCount(int channelCount);

    //--------------------------------------------------
    // parameters and settings

    // returns a vector of parameter names
    std::vector<std::string> paramNames() const;
    std::vector<std::string> paramShortNames() const;
    std::shared_ptr<AudioParam> param(int index);
    int param_index(char const * const str);

    //  settings
    std::vector<std::string> settingNames() const;
    std::vector<std::string> settingShortNames() const;

    std::shared_ptr<AudioParam> param(char const * const str);
    std::shared_ptr<AudioSetting> setting(char const * const str);
    std::shared_ptr<AudioSetting> setting(int index);
    int setting_index(char const * const str);

    std::vector<std::shared_ptr<AudioParam>> params() const {
        return _self->params; }
    std::vector<std::shared_ptr<AudioSetting>> settings() const {
        return _self->settings; }

    void render(AudioContext* context,
                AudioSourceProvider* provider,
                AudioBus* inputBus, AudioBus* renderToThisBus,
                const SamplingInfo & info);

protected:

    virtual void runWorkBeforeScheduleUpdate(ContextRenderLock&) {}

    // Called by processIfNecessary() to cause all parts of the rendering graph connected to us to process.
    // Each rendering quantum, the audio data for each of the AudioNode's inputs will be available after this method is called.
    // Called from context's audio thread.
    void pullInputs(ContextRenderLock &, int bufferSize);

    friend class AudioContext;

    // starts an immediate ramp to zero in preparation for disconnection
    void scheduleDisconnect() { _self->scheduler.stop(0); }

    // returns true if the disconnection ramp has reached zero.
    // This is intended to allow the AudioContext to manage popping artifacts
    bool disconnectionReady() const {
        return _self->scheduler._playbackState != SchedulingState::PLAYING; }

    static void _printGraph(const AudioNode * root,
                        std::function<void(const char *)> prnln, int indent);

    static void gatherInputsAndUpdateSchedules(ContextRenderLock& renderLock, 
                             AudioNode* root,
                             std::vector<AudioNode*>& scheduled,
                             std::vector<AudioNode*>& unscheduled,
                             int frames,
                             uint64_t current_sample_frame);

};



}  // namespace lab

#endif  // AudioNode_h

