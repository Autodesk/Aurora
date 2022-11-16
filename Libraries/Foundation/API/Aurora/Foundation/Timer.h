// Copyright 2022 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Aurora
{
namespace Foundation
{

/// A simple timer to measure time spent on the CPU.
class CPUTimer
{
public:
    using clock = std::chrono::high_resolution_clock;

    /// Constructor.
    CPUTimer() { reset(); };

    /// Resets the CPU timer.
    ///
    /// /param shouldSuspend Whether to reset the timer in a suspended state.
    void reset(bool shouldSuspend = false)
    {
        _startTime   = clock::now();
        _isSuspended = false;
        if (shouldSuspend)
        {
            suspend();
        }
    }

    /// Suspends the timer, so that any time spent suspended is not included in the elapsed time.
    void suspend()
    {
        if (!_isSuspended)
        {
            _suspendTime = clock::now();
            _isSuspended = true;
        }
    }

    /// Resumes the timer from a suspended state.
    void resume()
    {
        if (_isSuspended)
        {
            _startTime += clock::now() - _suspendTime;
            _isSuspended = false;
        }
    }

    /// Gets the time since the CPU timer was reset, in milliseconds.
    ///
    /// This does not include the time spent while the timer is suspended.
    float elapsed() const
    {
        auto duration   = (_isSuspended ? _suspendTime : clock::now()) - _startTime;
        auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        return static_cast<float>(durationMs.count());
    }

private:
    clock::time_point _startTime;
    clock::time_point _suspendTime;
    bool _isSuspended;
};

/// A regulator for progressive rendering.
///
/// This maintains a running average of time spent rendering previous samples to estimate the number
/// of samples (iterations) that can be performed in a target time.
///
/// There are two target times: "active" and "idle" which correspond to the counter being restarted
/// recently or not, respectively. If the counter is being restarted frequently, this usually
/// corresponds to user activity (e.g. moving the camera) and a lower target time is desirable. When
/// the counter (and user activity) is idle, more time can be allocated for additional samples,
/// which reduces the *total* rendering time due to fewer per-frame updates; such updates have
/// overhead.
class SampleCounter
{
public:
    /// Constructor.
    ///
    /// /param activeFrameTime The desired amount of time to render, in milliseconds, when the
    /// counter is actively being restarted.
    /// /param idleFrameTime The desired amount of time to render, in milliseconds, when the
    /// counter hasn't been restarted recently.
    /// /param maxSamples The maximum number of samples, at which point rendering is complete.
    SampleCounter(
        float activeFrameTime = 33.3f, float idleFrameTime = 200.0f, uint32_t maxSamples = 1000) :
        _activeFrameTime(activeFrameTime), _idleFrameTime(idleFrameTime), _maxSamples(maxSamples)
    {
        assert(activeFrameTime > 0.0f && idleFrameTime > 0.0f && maxSamples > 0);

        // Prepare the record array and reset the counter.
        _records.resize(TIME_COUNT);
        reset();
    }

    /// Resets the counter.
    ///
    /// This is useful after significant changes to the data being rendered, e.g. loading a new
    /// scene with possibly different complexity.
    void reset()
    {
        _timer.reset();
        _idleTimer.reset();
        _records[0].samples = 1;
        _isFull             = false;
        _currentTimeIndex   = 0;
        _sampleStart        = 0;
        _storedSamples      = 0;
        _storedTime         = 0;
    }

    /// Gets the current number of samples that have been accumulated, up to the maximum number
    /// of samples.
    uint32_t currentSamples() const { return _sampleStart; }

    /// Gets whether rendering is complete, i.e. the maximum number of samples has been reached.
    bool isComplete() const { return _sampleStart == _maxSamples; }

    /// Sets the maximum number of samples, at which point rendering is complete.
    ///
    /// This will not reset the counter, as the client may want rendering to continue starting from
    /// the current accumulated result, using additional samples.
    void setMaxSamples(uint32_t maxSamples)
    {
        assert(maxSamples > 0);

        _maxSamples = maxSamples;
    }

    /// Computes and returns the target number of samples for the next frame, based on previously
    /// collected frame render times and sample counts.
    ///
    /// This returns zero if rendering is complete. This also updates the current number of samples.
    ///
    /// /param sampleStart The sample index to use for rendering; the returned sample count refers
    /// to the following samples.
    /// /param restart When set to true, restarts sample tracking, so that sampleStart is zero. This
    /// should be set when the scene is changed, e.g. moving the camera.
    uint32_t update(uint32_t& sampleStart, bool restart = false)
    {
        uint32_t samples = 1;

        // Record whether rendering is already complete, as that state may change below.
        bool isDone = isComplete();

        // Handle a request to restart the counter.
        if (restart)
        {
            // Set the start sample to zero and reset the idle timer.
            _sampleStart = 0;
            _idleTimer.reset();

            // If rendering was previously complete, reset the timer and return a sample count to
            // fill the active frame time. This is necessary because a long idle time has likely
            // occurred since the last update, after rendering was completed.
            if (isDone)
            {
                _timer.reset();
                samples = samples = computeSampleCount(_activeFrameTime);
                sampleStart       = 0;
                _sampleStart += samples;

                return samples;
            }
        }

        // If rendering is already complete, return zero samples (nothing to do).
        if (isDone)
        {
            sampleStart = _maxSamples;

            return 0;
        }

        // Get the elapsed time since the last update and reset the timer. This usually corresponds
        // to the amount of time required to render the previous samples.
        float elapsedTime = _timer.elapsed();
        _timer.reset();

        // Store the elapsed time in the last record, and increment the total time. If the record
        // array is already full (primed), also subtract the previous time from the running total.
        Record& prevRecord = _records[_currentTimeIndex];
        if (_isFull)
        {
            _storedTime -= prevRecord.time;
        }
        prevRecord.time = elapsedTime;
        _storedTime += elapsedTime;

        // If the record array is full (primed), compute the target number of samples. If the record
        // array is not full yet, just use one sample.
        if (_isFull)
        {
            // Determine the target frame time: using the idle frame time if the idle duration has
            // passed since the counter was reset, and otherwise use the active frame time.
            const float kIdleDuration = 1000.0f;
            float targetFrameTime =
                _idleTimer.elapsed() > kIdleDuration ? _idleFrameTime : _activeFrameTime;

            // Determine the number of samples for the target frame time.
            samples = computeSampleCount(targetFrameTime);
        }

        // Advance to the next record. Since the records are stored in an array, loop back to the
        // beginning if needed. When this (first) happens, it also means the array is full, i.e.
        // primed with a complete set of data.
        _currentTimeIndex++;
        if (_currentTimeIndex == TIME_COUNT)
        {
            _isFull           = true;
            _currentTimeIndex = 0;
        }

        // Store the sample count in the next record, and increment the running total. If the record
        // array is already full (primed), also subtract the previous sample count from the total.
        Record& nextRecord = _records[_currentTimeIndex];
        if (_isFull)
        {
            _storedSamples -= nextRecord.samples;
        }
        nextRecord.samples = samples;
        _storedSamples += samples;

        // Set the output sample start, and increment the member value.
        sampleStart = _sampleStart;
        _sampleStart += samples;

        return samples;
    }

private:
    static const uint32_t TIME_COUNT = 20;

    struct Record
    {
        float time       = 0;
        uint32_t samples = 0;
    };

    uint32_t computeSampleCount(float targetFrameTime)
    {
        // Compute the average time per sample (iteration), and then the number of samples that
        // could be rendered in the target frame time, with at least one sample. If rendering would
        // be complete with these samples (i.e. up to the maximum number of samples), use that
        // smaller remaining number.
        float averageTime    = _storedTime / _storedSamples;
        uint32_t sampleCount = std::max(static_cast<uint32_t>(targetFrameTime / averageTime), 1u);
        sampleCount          = std::min(_maxSamples - _sampleStart, sampleCount);

        return sampleCount;
    }

    CPUTimer _timer;
    CPUTimer _idleTimer;
    std::vector<Record> _records;
    float _activeFrameTime   = 33.3f;
    float _idleFrameTime     = 250.0f;
    uint32_t _maxSamples     = 0;
    bool _isFull             = false;
    size_t _currentTimeIndex = 0;
    uint32_t _sampleStart    = 0;
    uint32_t _storedSamples  = 0;
    float _storedTime        = 0;
};

} // namespace Foundation
} // namespace Aurora
