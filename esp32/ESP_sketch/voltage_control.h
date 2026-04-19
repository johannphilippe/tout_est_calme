#pragma once
#include <stdint.h>
#include <cmath>

inline constexpr uint16_t pow2_u16(uint16_t exponent)
{
    uint16_t base = 2;
    for (size_t i = 1; i < exponent; ++i)
        base *= 2;
    return base;
}

inline float clip(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/*
    Velocity-form PI controller with peak-boost phase.

    Velocity form means the PI output is a *delta* added to the running pwm_ratio,
    so the controller naturally integrates over time without a separate integral state —
    pwm_ratio itself is the accumulated output.

    Peak phase: when the error is large (siren far from target), the controller ramps
    pwm_ratio toward a fixed peak voltage at `peak_ramp` per tick instead of using PI.
    This overcomes static friction / high inertia without a sudden voltage step.
    Once the error falls below `release_threshold`, control returns to the PI phase.
*/
struct voltage_control
{
    enum class inertia_t
    {
        very_slow = 0,
        slow      = 1,
        medium    = 2,
        fast      = 3,
    };

    struct profile_t
    {
        float ki;                // integral gain: pwm_delta per Hz of error per tick
        float kp;                // proportional gain: pwm_delta per Hz of error change per tick
        float peak_up;           // pwm ratio target when going toward higher frequency
        float peak_down;         // pwm ratio target when going toward lower frequency
        float peak_threshold;    // Hz error above which peak phase engages
        float release_threshold; // Hz error below which peak phase disengages
        float peak_ramp;         // max pwm ratio change per tick during peak phase
    };

    static profile_t make_profile(inertia_t t)
    {
        switch (t)
        {
            case inertia_t::very_slow:
                // Heavy motor: needs full voltage to start, very slow PI to avoid overshoot
                return {0.000001f, 0.0001f, 1.0f, 0.0f, 100.0f, 30.0f, 0.005f};
            case inertia_t::slow:
                return {0.000003f, 0.0002f, 1.0f, 0.0f,  80.0f, 20.0f, 0.010f};
            case inertia_t::medium:
                return {0.000006f, 0.0004f, 0.9f, 0.1f,  60.0f, 15.0f, 0.020f};
            case inertia_t::fast:
                // Light motor: less peak needed, faster PI response
                return {0.000010f, 0.0008f, 0.8f, 0.2f,  40.0f, 10.0f, 0.040f};
        }
        return make_profile(inertia_t::medium);
    }

    voltage_control(inertia_t inertia_setting)
    {
        set_inertia(inertia_setting);
    }

    void set_inertia(inertia_t n)
    {
        inertia = n;
        profile = make_profile(n);
        reset();
    }

    /*
        destination : target frequency (Hz)
        source      : measured frequency at the moment destination was set — used to
                      detect target changes and reset controller state
        current     : current measured frequency (Hz)
    */
    uint16_t operator()(float destination, float source, float current)
    {
        float error     = destination - current;
        float abs_error = fabsf(error);
        int8_t dir      = (error >= 0.0f) ? 1 : -1;

        // Reset integrator state when the target changes (source is updated by caller)
        if (source != prev_source)
        {
            prev_source = source;
            prev_error  = error; // prevent a kp spike on the first tick after change
            in_peak     = (abs_error >= profile.peak_threshold);
        }

        // Hysteretic peak phase transitions
        if (abs_error >= profile.peak_threshold)  in_peak = true;
        if (abs_error <= profile.release_threshold) in_peak = false;

        if (in_peak)
        {
            // Ramp toward peak voltage — never jumps instantly to avoid overwhelming the detector
            float target = (dir > 0) ? profile.peak_up : profile.peak_down;
            float delta  = target - pwm_ratio;
            pwm_ratio += clip(delta, -profile.peak_ramp, profile.peak_ramp);
        }
        else
        {
            // Velocity-form PI: delta = kp * d(error) + ki * error
            // Accumulated in pwm_ratio which acts as the integral of the PI output
            float delta = profile.kp * (error - prev_error) + profile.ki * error;
            pwm_ratio   = clip(pwm_ratio + delta, 0.0f, 1.0f);
        }

        prev_error = error;
        output     = static_cast<uint16_t>(pwm_ratio * range);
        return output;
    }

    static constexpr uint8_t  resolution = 12;
    static constexpr uint16_t range      = pow2_u16(resolution);

private:
    inertia_t inertia = inertia_t::very_slow;
    profile_t profile = {};

    float    pwm_ratio   = 0.0f;
    float    prev_error  = 0.0f;
    float    prev_source = -1.0f; // sentinel: triggers reset on first call
    bool     in_peak     = false;
    uint16_t output      = 0;

    void reset()
    {
        pwm_ratio  = 0.0f;
        prev_error = 0.0f;
        in_peak    = false;
    }
};
