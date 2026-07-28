// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <cstdint>

namespace libobsensor {

// Estimates the DeviceTimestamp of an audio frame using a sliding-window least-squares
// linear regression between accumulated sample count and host system time.
//
// Audio hardware runs on an independent crystal whose actual rate differs from the nominal
// value by ±50–100 ppm.  A fixed 1/sampleRate divisor accumulates tens of milliseconds of
// error per hour.  This class fits the line
//
//     systemTime(us) = slope * samplesFromAnchor + intercept
//
// over the last kWindowSize data points (one point per second) and uses the fitted line
// to produce timestamps that stay aligned with the host clock over long runs.
//
// All arithmetic is done in double with the origin shifted to the anchor sample so that
// the x and y values remain small (~10^5 and ~10^7 respectively over a 30-point window),
// keeping floating-point error well below 1 µs.
class AudioTimestampFitter {
public:
    static constexpr int kWindowSize   = 30;  // data points kept in the regression window
    static constexpr int kMinFitPts    = 5;   // minimum points required before using the fit
    static constexpr int kFitIntervalS = 1;   // seconds between data points

    void reset(uint32_t sampleRate) {
        sampleRate_    = sampleRate ? sampleRate : 48000;
        fitInterval_   = static_cast<uint64_t>(sampleRate_) * kFitIntervalS;
        nextFitAt_     = 0;
        anchorSysUs_   = 0;
        anchorSamples_ = 0;
        slope_         = 1000000.0 / sampleRate_;
        intercept_     = 0.0;
        count_         = 0;
        head_          = 0;
    }

    // Call once per audio packet.  totalSamples is the running count of audio frames
    // (samples per channel) BEFORE this packet; nowUs is the host system time in
    // microseconds when the packet was received.  Returns the estimated DeviceTimestamp
    // of the first sample in this packet.
    uint64_t update(uint64_t totalSamples, uint64_t nowUs) {
        if(anchorSysUs_ == 0) {
            anchorSysUs_   = nowUs;
            anchorSamples_ = totalSamples;
            slope_         = 1000000.0 / sampleRate_;
            intercept_     = 0.0;
        }

        uint64_t fromAnchor = totalSamples - anchorSamples_;

        if(fromAnchor >= nextFitAt_) {
            double x = static_cast<double>(fromAnchor);
            double y = static_cast<double>(nowUs - anchorSysUs_);
            addPoint(x, y);
            nextFitAt_ += fitInterval_;
            refit();
        }

        double x  = static_cast<double>(fromAnchor);
        double ts = static_cast<double>(anchorSysUs_) + intercept_ + slope_ * x;
        return static_cast<uint64_t>(ts);
    }

private:
    void addPoint(double x, double y) {
        xs_[head_] = x;
        ys_[head_] = y;
        head_      = (head_ + 1) % kWindowSize;
        if(count_ < kWindowSize)
            ++count_;
    }

    void refit() {
        if(count_ < kMinFitPts)
            return;
        double n  = static_cast<double>(count_);
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for(int i = 0; i < count_; ++i) {
            sx += xs_[i];
            sy += ys_[i];
            sxx += xs_[i] * xs_[i];
            sxy += xs_[i] * ys_[i];
        }
        double denom = n * sxx - sx * sx;
        if(denom < 1.0)
            return;
        slope_     = (n * sxy - sx * sy) / denom;
        intercept_ = (sy - slope_ * sx) / n;
    }

    uint32_t sampleRate_    = 48000;
    uint64_t fitInterval_   = 48000;
    uint64_t nextFitAt_     = 0;
    uint64_t anchorSysUs_   = 0;
    uint64_t anchorSamples_ = 0;
    double   slope_         = 62.5;
    double   intercept_     = 0.0;
    int      count_         = 0;
    int      head_          = 0;
    double   xs_[kWindowSize]{};
    double   ys_[kWindowSize]{};
};

}  // namespace libobsensor
