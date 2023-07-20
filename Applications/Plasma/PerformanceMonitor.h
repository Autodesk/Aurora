// Copyright 2023 Autodesk, Inc.
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

// A class to track rendering performance statistics and provide periodic status updates.
class PerformanceMonitor
{
public:
    // Constructor, with a parameter to specify the status update interval.
    PerformanceMonitor(float statusInterval = 500.0f) : _statusInterval(statusInterval)
    {
        assert(statusInterval > 0.0f);
    }

    // Sets a callback function that may be called with a status message when a frame is completed,
    // through endFrame().
    using StatusFunction = function<void(const string& message)>;
    void setStatusFunction(StatusFunction statusFunction) { _statusFunction = statusFunction; }

    // Set the rendering dimensions.
    void setDimensions(const ivec2& dimensions)
    {
        assert(dimensions.x > 0 && dimensions.y > 0);

        _dimensions = dimensions;
    }

    // Notifies the performance monitor that rendering of a frame is going to begin. The parameter
    // indicates whether rendering will be restarting from the first sample with this frame.
    void beginFrame(bool isRestarting)
    {
        // If rendering is restarting, reset the variables tracking total work.
        if (isRestarting)
        {
            _totalTimer.reset();
            _totalFrames  = 0;
            _totalSamples = 0;
        }
    }

    // Notifies the performance monitor that rendering of a frame is done. The parameters indicate
    // whether rendering was completed (converged) with this frame and the number of samples
    // rendered in the frame.
    // NOTE: It is possible (though unlikely) for rendering to be restarted _and_ completed in a
    // single frame. The order of the operations in this function accounts for that.
    void endFrame(bool isComplete, int sampleCount)
    {
        // If rendering was *previously* complete, this frame is likely to be happening long after
        // the previous status update, i.e. the application was idle. In that case, reset the
        // variables tracking status updates.
        if (_isComplete)
        {
            _statusTimer.reset();
            _statusFrames   = 0;
            _statusSamples  = 0;
            _statusLastTime = 0.0f;
        }
        _isComplete = isComplete;

        // Increment the frame and sample counters.
        _totalFrames++;
        _totalSamples += sampleCount;
        _statusFrames++;
        _statusSamples += sampleCount;

        // Record the elapsed time since the last status update. Return if the status update
        // interval has not yet elapsed *and* rendering has not been completed.
        float statusTime     = _statusTimer.elapsed();
        float statusDuration = statusTime - _statusLastTime;
        if (statusDuration < _statusInterval && !_isComplete)
        {
            return;
        }

        // Start building a status report string, starting with the current completed samples.
        stringstream report;
        report.imbue(locale(""));          // thousands separator for integers
        report << setprecision(1) << fixed // single decimal place, no scientific notation
               << "  |  " << _totalSamples << " SPP";

        // Append to the report string, with information about either:
        // - The total work for converged rendering (if complete).
        // - Work done since the last status update (otherwise).
        float megaraysPerSample = _dimensions.x * _dimensions.y / 1.0e6f;
        if (_isComplete)
        {
            // Collect performance stats for the total work performed to complete rendering.
            float durationInSeconds = _totalTimer.elapsed() / 1000.0f;

            // Add them to the report string.
            report << "  |  " << durationInSeconds << " s";
        }
        else
        {
            // Collect performance stats since the last status update.
            float durationInSeconds = statusDuration / 1000.0f;
            float timePerFrame      = statusDuration / _statusFrames;
            float timePerSample     = statusDuration / _statusSamples;
            float framesPerSecond   = _statusFrames / durationInSeconds;
            float megaraysPerSecond = megaraysPerSample * _statusSamples / durationInSeconds;

            // Add them to the report string.
            report << "  |  " << timePerFrame << " ms/frame"
                   << " (" << framesPerSecond << " FPS)"
                   << "  |  " << timePerSample << " ms/sample"
                   << " (" << megaraysPerSecond << " MRPS)";
        }

        // Call the status update function.
        if (_statusFunction)
        {
            _statusFunction(report.str());
        }
        // A status update was performed, so update the last status update time and frame count.
        _statusFrames   = 0;
        _statusSamples  = 0;
        _statusLastTime = statusTime;
    }

private:
    ivec2 _dimensions = ivec2(1280, 720);
    bool _isComplete  = true;

    // Variables that track total work performed from the first sample to the last sample.
    Foundation::CPUTimer _totalTimer;
    int _totalFrames  = 0;
    int _totalSamples = 0;

    // Variables that track work performed during a single status update interval.
    StatusFunction _statusFunction;
    Foundation::CPUTimer _statusTimer;
    float _statusInterval = 500.0f;
    int _statusFrames     = 0;
    int _statusSamples    = 0;
    float _statusLastTime = 0.0f;
};