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
        // Pre-warp the analogue cutoff to compensate for bilinear transform warping.
        const double wc = 2.0 * std::tan(std::numbers::pi * cutoff_hz / sample_rate);
        const double wc2 = wc * wc;

        // Butterworth 2nd-order prototype denominator: s^2 + sqrt(2)*s + 1
        // Bilinear substitution s = (2/T)(z-1)/(z+1) with T=1 (normalised).
        const double norm = 1.0 / (wc2 + std::numbers::sqrt2 * wc + 4.0);

        b0 = wc2 * norm;
        b1 = 2.0 * b0;
        b2 = b0;

        a1 = 2.0 * (wc2 - 4.0) * norm;
        a2 = (wc2 - std::numbers::sqrt2 * wc + 4.0) * norm;
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
        const double wc = 2.0 * std::tan(std::numbers::pi * cutoff_hz / sample_rate);
        const double wc2 = wc * wc;

        const double norm = 1.0 / (wc2 + std::numbers::sqrt2 * wc + 4.0);

        b0 =  4.0 * norm;
        b1 = -8.0 * norm;
        b2 =  4.0 * norm;

        // Feedback coefficients are identical to the lowpass.
        a1 = 2.0 * (wc2 - 4.0) * norm;
        a2 = (wc2 - std::numbers::sqrt2 * wc + 4.0) * norm;
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

template<size_t N>
struct rms 
{
    rms() : _buffer{} {}

    double operator()(double x) 
    {
        _buffer[_index] = x * x; // Store squared value
        _index = (_index + 1) % N; // Circular buffer index
        double sum = 0.0f;
        for (size_t i = 0; i < N; ++i) 
        {
            sum += _buffer[i];
        }
        return std::sqrt(sum / N); // Return RMS value
    }

private:
    std::array<double, N> _buffer;
    size_t _index = 0;
};

inline double compensate(double input, double rms, double threshold) 
{
    double gain = 1.0;
    if(rms < db_to_linear(threshold)) // If RMS is very low, avoid huge gain from compensation
    {
        gain = 0.0; // Avoid division by zero, no gain applied
    } 
    else 
    {
        gain = 1.0 / rms / 2; // Inverse of RMS for compensation
    }

    return input * gain;
}

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