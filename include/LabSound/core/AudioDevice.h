// License: BSD 3 Clause
// Copyright (C) 2015+, The LabSound Authors. All rights reserved.

#pragma once

#ifndef lab_audiodevice_h
#define lab_audiodevice_h

#include "LabSound/core/AudioNode.h"
#include "LabSound/core/AudioSourceProvider.h"
#include "LabSound/extended/AudioContextLock.h"
#include "LabSound/extended/Logging.h"

#include <chrono>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace lab
{
class AudioBus;
class AudioContext;
class AudioNodeInput;
class AudioHardwareInput;
class AudioSourceProvider;
struct AudioDeviceInfo;

//-------------------
//   AudioDevice
//-------------------

struct AudioDeviceIndex
{
    uint32_t index;
    bool valid;
};


// Input and Output
struct AudioStreamConfig
{
    int32_t device_index{-1};
    uint32_t desired_channels{0};
    float desired_samplerate{0};
};

//-------------------------------------------
//   Audio Device Configuration Settings
//-------------------------------------------

struct AudioDeviceInfo
{
    int32_t index{-1};
    std::string identifier;
    uint32_t num_output_channels{0};
    uint32_t num_input_channels{0};
    std::vector<float> supported_samplerates;
    float nominal_samplerate{0};
    bool is_default_output{false};
    bool is_default_input{false};
};


class AudioDevice
{
protected:
    AudioStreamConfig _outConfig = {};
    AudioStreamConfig _inConfig = {};
    AudioSourceProvider* _sourceProvider = nullptr;
    std::shared_ptr<AudioDestinationNode> _destinationNode;

public:
    AudioDevice(const AudioStreamConfig & inputConfig,
                const AudioStreamConfig & outputConfig)
    : _inConfig(inputConfig), _outConfig(outputConfig)
    {
        LOG_INFO("AudioHardwareDeviceNode() \n"
                 "\t* Sample Rate:     %f \n"
                 "\t* Input Channels:  %i \n"
                 "\t* Output Channels: %i   ",
            outputConfig.desired_samplerate, inputConfig.desired_channels, outputConfig.desired_channels);

        if (inputConfig.device_index != -1)
        {
            _sourceProvider = new AudioSourceProvider(inputConfig.desired_channels);
        }
    }

    virtual ~AudioDevice() {
        delete _sourceProvider;
    }

    void setDestinationNode(std::shared_ptr<AudioDestinationNode> callback) { 
        _destinationNode = callback; 
    }

    const AudioStreamConfig & getOutputConfig() const {
        return _outConfig; }
    const AudioStreamConfig & getInputConfig() const {
        return _inConfig; }

    AudioSourceProvider* sourceProvider() const { return _sourceProvider; }
    
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual void backendReinitialize() = 0;
};

class AudioDevice_Null : public AudioDevice {
    bool _isRunning = false;
public:
    AudioDevice_Null(const AudioStreamConfig & inputConfig,
                     const AudioStreamConfig & outputConfig)
    : AudioDevice(inputConfig, outputConfig) {}
    
    virtual ~AudioDevice_Null() = default;
    virtual void start() { _isRunning = true; }
    virtual void stop() { _isRunning = false; }
    virtual bool isRunning() const { return _isRunning; }
    virtual void backendReinitialize() { stop(); }
};

class AudioDestinationNode : public AudioNode {
protected:
    AudioContext * _context;

    virtual double tailTime(ContextRenderLock & r) const override { return 0; }
    virtual double latencyTime(ContextRenderLock & r) const override { return 0; }

    // Platform specific implementation
    std::shared_ptr<AudioDevice> _platformAudioDevice;
    
public:
    static const char* static_name() { return "AudioDestination"; }
    virtual const char* name() const override { return static_name(); }
    static AudioNodeDescriptor* desc();

    explicit AudioDestinationNode(
        AudioContext& ac,
        std::shared_ptr<AudioDevice> device);
    
    virtual ~AudioDestinationNode()
    {
        uninitialize();
    }
    
    AudioDevice* device() const { return _platformAudioDevice.get(); }

    virtual void initialize() override;
    virtual void uninitialize() override;
    virtual void reset(ContextRenderLock &) override;
};

}  // lab

#endif  // lab_audiodevice_h
