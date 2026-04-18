#pragma once 
#include <cstdlib>
#include <stdlib.h>
#include <stdint.h>
#include <algorithm>

inline constexpr uint16_t pow2_u16(uint16_t exponent)
{
  uint16_t base = 2;
  for(size_t i = 0; i < exponent; ++i)
    base *= 2; 
  return base; 
}

inline float clip(float value, float min, float max) 
{
    if(value < min) return min;
    if(value > max) return max;
    return value;
}

struct voltage_control
{
    enum class inertia_t 
    {
        very_slow = 0,
        slow = 1, 
        medium = 2,
        fast = 3, 
    };

    inertia_t inertia = inertia_t::very_slow;

    struct threshold_t
    {
        float peak; 
        float near; 
        float very_near;

        float near_step; 
        float very_near_step;

        static threshold_t fast() 
        {
            return {fast_peak, fast_near, fast_very_near, fast_near_step, fast_very_near_step};
        }
        static threshold_t slow()
        {
            return {slow_peak, slow_near, slow_very_near, slow_near_step, slow_very_near_step};
        }
        static threshold_t very_slow() 
        {
            return {very_slow_peak, very_slow_near, very_slow_very_near, very_slow_near_step, very_slow_very_near_step};
        }
        static threshold_t medium()
        {
            return {medium_peak, medium_near, medium_very_near, medium_near_step, medium_very_near_step};
        }

        static constexpr float fast_peak = 0.5f; 
        static constexpr float fast_near = 0.85f; 
        static constexpr float fast_very_near = 0.95f;
        static constexpr float fast_near_step = 0.001f;
        static constexpr float fast_very_near_step = 0.0005f;

        static constexpr float slow_peak = 0.1f;
        static constexpr float slow_near = 0.50f;
        static constexpr float slow_very_near = 0.75f;
        static constexpr float slow_near_step = 0.005f;
        static constexpr float slow_very_near_step = 0.001f;

        static constexpr float very_slow_peak = 0.001f; 
        static constexpr float very_slow_near = 0.95f; 
        static constexpr float very_slow_very_near = 0.99f; 
        static constexpr float very_slow_near_step = 0.001f; 
        static constexpr float very_slow_very_near_step = 0.0001f;

        static constexpr float medium_peak = 0.25f;
        static constexpr float medium_near = 0.75f;
        static constexpr float medium_very_near = 0.90f;
        static constexpr float medium_near_step = 0.002f;
        static constexpr float medium_very_near_step = 0.0005f;
    };

    threshold_t thresholds;

    voltage_control(inertia_t inertia_setting) : inertia(inertia_setting)
    {
        set_inertia(inertia_setting);
    }

    void set_inertia(inertia_t n)
    {
        inertia = n; 
        switch(inertia) 
        {
            case inertia_t::fast:
                thresholds = threshold_t::fast();
                break;
            case inertia_t::slow:
                thresholds = threshold_t::slow();
                break;
            case inertia_t::medium:
                thresholds = threshold_t::medium();
                break;
            case inertia_t::very_slow:
                thresholds = threshold_t::very_slow();
        }
    }

    /* 
        - destination is the target frequency 
        - source is the origin frequency (frequency when the target changed)
        - current is the last PWM value sent to the hardware
    */
    uint16_t operator() (float destination, float source, float current) 
    {
        // Check percentage of the distance to the target

        if(current == destination)
            return output;

        Serial.printf("Target= %f, Source = %f,  Current = %f \n", destination, source, current);
        float up = std::max(destination, source);
        float down = std::min(destination, source);
        float distance = up - down;
        const int8_t direction = (destination > current) ? 1 : -1;
        float percent_run = 0.0f; 


        if(distance != 0.0f)
        {        
            if(destination > source) 
            {
                percent_run = (current - source) / distance;
            } else 
            {
                percent_run = (source - current) / distance;
            }
        }


        Serial.printf("Distance : %f, percent_run : %f \n", distance, percent_run);
        if(percent_run < thresholds.peak) 
        {
            res = (direction == 1) ? 1.0f : 0.0f;
        } else if(percent_run < thresholds.near) 
        {
                res -= direction * thresholds.near_step; 
        } else if(percent_run < thresholds.very_near) 
        {
            res -= current + direction * thresholds.very_near_step;
        } else 
        {
            // Do nothing 
        }

        res = clip(res, 0.0f, 1.0f);
        output = static_cast<uint16_t>(res * range);
        Serial.printf("output : %d \n", output);
        return output;
    }

    static constexpr uint8_t resolution = 12; 
    static constexpr uint16_t range = pow2_u16(resolution); 

    float res = 0.0f;
    uint16_t output = 0;

};