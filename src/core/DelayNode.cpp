/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "LabSound/core/DelayNode.h"
#include "LabSound/core/AudioBasicProcessorNode.h"
#include "LabSound/core/AudioProcessor.h"

#include "internal/AudioDSPKernel.h"
#include "internal/DelayProcessor.h"

namespace WebCore
{

// WebAudio: 
// "The maxDelayTime parameter is optional and specifies the 
//  maximum delay time in seconds allowed for the delay line."
const double maximumAllowedDelayTime = 128;

DelayNode::DelayNode(float sampleRate, double maxDelayTime) : AudioBasicProcessorNode(sampleRate)
{
    if (maxDelayTime <= 0 || maxDelayTime >= maximumAllowedDelayTime)
	{
		 throw std::out_of_range("Delay time exceeds limit of 128 seconds");
	}
    m_processor.reset(new DelayProcessor(sampleRate, 1, maxDelayTime));
    setNodeType(NodeTypeDelay);
    initialize();
}

std::shared_ptr<AudioParam> DelayNode::delayTime()
{
    return delayProcessor()->delayTime();
}

DelayProcessor * DelayNode::delayProcessor() 
{ 
	return static_cast<DelayProcessor*>(processor()); 
}

} // WebCore
