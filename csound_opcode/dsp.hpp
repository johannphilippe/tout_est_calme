#pragma once 

#include<iostream>
#include<cmath>
#include "fx/biquad.hpp"
#include "support/decibel.hpp"

#include <numbers>
#include <cmath>

using namespace cycfi;

#pragma once
#include <cmath>
#include <numbers>

constexpr double db_to_linear(double db) noexcept
{
    return std::pow(10.0, db / 20.0);
}

// 2nd-order Butterworth filter using the bilinear transform with frequency pre-warping.
// Direct Form II Transposed — numerically stable, two delay elements.
//
// Usage:
//   butterworth_lpf f;
//   f.set_coeffs(sample_rate, cutoff_hz);
//   float y = f.process(x);

struct butterworth_lpf
{
    double b0{}, b1{}, b2{};   // feed-forward (numerator)
    double a1{}, a2{};         // feed-back    (denominator, a0 normalised to 1)
    double w1{}, w2{};         // delay state

    // Compute coefficients for a 2nd-order Butterworth lowpass.
    // sample_rate : samples per second (e.g. 48000.0)
    // cutoff_hz   : -3 dB frequency in Hz
    void set_coeffs(double sample_rate, double cutoff_hz) noexcept
    {
        const double c  = 1.0 / std::tan(std::numbers::pi * cutoff_hz / sample_rate);
        const double c2 = c * c;
        const double norm = 1.0 / (1.0 + std::numbers::sqrt2 * c + c2);

        b0 = norm;
        b1 = 2.0 * norm;
        b2 = norm;

        a1 = 2.0 * (1.0 - c2) * norm;
        a2 = (1.0 - std::numbers::sqrt2 * c + c2) * norm;
    }

    // Process one sample.
    [[nodiscard]] double process(double x) noexcept
    {
        const double y = b0 * x + w1;
        w1 = b1 * x - a1 * y + w2;
        w2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { w1 = w2 = 0.0; }
};

template<size_t N>
struct cascade_lpf
{
    std::array<butterworth_lpf, N> stages;

    void set_coeffs(double sample_rate, double cutoff_hz) noexcept
    {
        for (auto& stage : stages)
        {
            stage.set_coeffs(sample_rate, cutoff_hz);
        }
    }

    [[nodiscard]] double process(double x) noexcept
    {
        double y = x;
        for (auto& stage : stages)
        {
            y = stage.process(y);
        }
        return y;
    }

    void reset() noexcept
    {
        for (auto& stage : stages)
        {
            stage.reset();
        }
    }
};


// 2nd-order Butterworth highpass.
// Derived by substituting s → 1/s in the prototype, which flips the role
// of wc: numerator becomes (1 - 2z^-1 + z^-2) scaled, feedback is identical.
struct butterworth_hpf
{
    double b0{}, b1{}, b2{};
    double a1{}, a2{};
    double w1{}, w2{};

    void set_coeffs(double sample_rate, double cutoff_hz) noexcept
    {
        const double c  = 1.0 / std::tan(std::numbers::pi * cutoff_hz / sample_rate);
        const double c2 = c * c;
        const double norm = 1.0 / (1.0 + std::numbers::sqrt2 * c + c2);

        b0 =  c2 * norm;
        b1 = -2.0 * c2 * norm;
        b2 =  c2 * norm;

        a1 = 2.0 * (1.0 - c2) * norm;
        a2 = (1.0 - std::numbers::sqrt2 * c + c2) * norm;
    }

    [[nodiscard]] double process(double x) noexcept
    {
        const double y = b0 * x + w1;
        w1 = b1 * x - a1 * y + w2;
        w2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { w1 = w2 = 0.0; }
};

template<size_t N>
struct cascade_hpf
{
    std::array<butterworth_hpf, N> stages;

    void set_coeffs(double sample_rate, double cutoff_hz) noexcept
    {
        for (auto& stage : stages)
        {
            stage.set_coeffs(sample_rate, cutoff_hz);
        }
    }

    [[nodiscard]] double process(double x) noexcept
    {
        double y = x;
        for (auto& stage : stages)
        {
            y = stage.process(y);
        }
        return y;
    }

    void reset() noexcept
    {
        for (auto& stage : stages)
        {
            stage.reset();
        }
    }
};

// From Csound source code, with some modifications (see comments in sirenes.cpp)
struct rms
{
    void init(double half_power_hz, double sample_rate) noexcept
    {
        double tpidsr = 2.0 * std::numbers::pi / sample_rate;
        double b = 2.0 - std::cos(half_power_hz * tpidsr);
        c2 = b - std::sqrt(b * b - 1.0);
        c1 = 1.0 - c2;
        q  = 0.0;
    }

    double operator()(double x) noexcept
    {
        q = c1 * x * x + c2 * q;
        return std::sqrt(q);
    }

    void reset() noexcept { q = 0.0; }

private:
    double c1{}, c2{}, q{};
};

// RMS with externally-owned buffer (use csnd::AuxMem<double> to avoid large inline arrays
// blowing out Csound's opcode struct size limit).
struct rms_ext
{
    void init(double* buf, size_t n) noexcept
    {
        _buffer = buf;
        _n      = n;
        _index  = 0;
        std::fill(_buffer, _buffer + _n, 0.0);
    }

    double operator()(double x) noexcept
    {
        _buffer[_index] = x * x;
        _index = (_index + 1) % _n;
        double sum = 0.0;
        for (size_t i = 0; i < _n; ++i)
            sum += _buffer[i];
        return std::sqrt(sum / double(_n));
    }

private:
    double* _buffer = nullptr;
    size_t  _n      = 0;
    size_t  _index  = 0;
};

inline double compensate(double input, double rms, double threshold)
{
    double gain = 1.0;
    if(rms < db_to_linear(threshold)) // If RMS is very low, avoid huge gain from compensation
    {
        gain = 1.0; // Avoid division by zero, no gain applied
    } 
    else 
    {
        gain = 1.0 / rms * 0.25; // Inverse of RMS for compensation
    }

    return input * gain;
}

struct lineto 
{
    void init(double slew_time, double sample_rate) 
    {
        _slew_time = slew_time;
        _sample_rate = sample_rate;
        _current = 0.0;
        _target = 0.0;
        _step = 0.0;
    }

    double operator()(double target) 
    {
        if (target != _target) 
        {
            _target = target;
            double total_samples = _slew_time * _sample_rate;
            _step = (_target - _current) / total_samples;
        }
        _current += _step;
        if ((_step > 0.0 && _current > _target) || (_step < 0.0 && _current < _target)) 
        {
            _current = _target; // Clamp to target to avoid overshooting
        }
        return _current;
    }
    double _slew_time;
    double _sample_rate;
    double _current;
    double _target;
    double _step;

};

struct compensator
{
    void init(double sr, double cutoff, double attenuation = 0.25) 
    {
        _attenuation = attenuation;
        lin.init(0.01, sr); 
    }

    double operator()(double input, double rms, double threshold, double fallback = 0.0)
    {
        if(rms < db_to_linear(threshold)) // If RMS is very low, avoid huge gain from compensation
        {
            gain = lin(fallback);
        } 
        else 
        {
            gain =  lin(1.0 / rms * _attenuation); // Inverse of RMS for compensation, filtered to avoid abrupt changes
        }

        return input * gain;
    }

    double _attenuation = 0.25;
    double gain = 1.0; 
    lineto lin; 
};


inline double clip(double input, double min , double max) 
{
    if(input < min) return min;
    if(input > max) return max;
    return input;
}

/*
    Zero-crossing-based pitch detector 
    With threshold and inversion modes 
*/
struct zc_pitch_detector
{

    zc_pitch_detector() = default; 
    void init(float threshold = 0.01f, bool negative = false) 
    {
        _threshold = threshold;
        _negative = negative;
    }


    float operator()(float sample)
    {
        float smp = _negative ? -sample : sample; 
        if(_last <= _threshold && smp > _threshold) 
        {
            // Crossed zero, calculate frequency
            double period = double(_count) / 48000.0; // Assuming 48kHz sample rate
            double freq = 1.0 / period; 
            _count = 0; // Reset counter
            _last = smp; // Update last sample
            output = freq; // Store output frequency
        }
        _last = smp; // Update last sample
        ++_count; // Increment counter
        return output; // No pitch detected
    }

    double output = 0.0f;
    double _threshold; 
    bool _negative; 
    double _last = 0.0f;
    size_t _count = 0;
};

/* 
    Algorithm to estimate stability of a tracking 
    Stability is estimated by its continuity 
    To do so, it must comparate the frequency estimated with a highly filtered version of the tracked frequency, and if the jump is too high,
     consider it as a tracking loss and reduce confidance in the tracking.
*/

struct stability_estimator 
{
    stability_estimator() = default; 

    void operator()(float freq)
    {
        // Compare with highly filtered version of tracked freq 
        double ratio_jump = freq / _last_freq;
        if(ratio_jump > 4 || ratio_jump < 0.25 || freq < 5.0f)
        {
            // Consider as tracking loss, reduce stability
            stability *= 0.9; // Arbitrary decay factor
        } 
        else 
        {
            // Consider as stable tracking, increase stability
            stability = std::min(stability * 1.1, 1.0); // Arbitrary growth factor, cap at 1.0
        }
        _last_freq = freq; // Update last frequency
    }

    float stability = 0.0f; // Higher value means more stable tracking
    float _last_freq = 0.0f; // Last detected frequency
};