#include <iostream>
#include<cmath>
#include<plugin.h>
#include<modload.h>
#include<OpcodeBase.hpp>
#include<cmath>
#include"dsp.hpp"

#include "fx/envelope.hpp"
#include "fx/peak.hpp"
#include "pitch/pitch_detector.hpp"

using namespace cycfi::q; 

/*
    TODO before end of week :
    - Validate pitch tracking pipeline for each siren 
    - Create a dedicated opcode for each siren 
    - OSC send it 
    - Create ESP32 code to : receive OSC, control siren, adjust voltage 
*/



/*
    Args : 
    - asig
    - khysteresis (linear, db = 20 * log10(khysteresis))
*/

struct periodic_pitch_tracker : csnd::Plugin<1, 2>
{
    int init()
    {
        float hysteresis = inargs[1];
        _pitch = new pitch_detector(frequency(20.0), frequency(1000.0), 48000.0, lin_to_db(hysteresis) );
        smoo = new one_pole_lowpass(frequency(1.0) , csound->sr() / ksmps());
        return OK;
    }

    int aperf() 
    {
        
        for(size_t n = 0; n < ksmps(); ++n)
        {
            (*_pitch)(float(inargs(0)[n]));
            freq = (*smoo)(double(_pitch->get_frequency()));
        }

        outargs[0] = freq; //double(_pitch->get_frequency());

        return OK;
    }
    double freq = 0.0f;
    pitch_detector *_pitch;
    one_pole_lowpass *smoo;
};


struct zc_tracker : csnd::Plugin<1, 4> 
{
    int init()
    {
        smoo = new one_pole_lowpass(frequency(20.0) , csound->sr() / ksmps());
        negative = inargs[1] > 0.0 ? true : false; 
        threshold = inargs[2]; 
        max_step = inargs[3];
        return OK;
    }


    int aperf() 
    {
        for(size_t i = 0; i < ksmps(); ++i)
        {
            double smp = negative ? inargs(0)[i] : -inargs(0)[i]; 
            if(last <= threshold && smp > threshold) 
            {
                //Crossed 0, Calculate freq, then zero counter
                double period = double(count) / double(csound->sr()); 
                double new_freq = 1.0 / period; 
                if(abs(new_freq - freq) < max_step /*25.0*/)  
                {
                    freq = (*smoo)(new_freq);
                } 
                count = 0; 
            }
            last = smp;
            ++count;
        }

        outargs[0] = freq; 
     
        return OK;
    }

    double last = 1.0f; // replace with buffer to check if ascending (or descending )
    bool negative = false;  // Check negative part of audio (useful for some magnets sirens where low peak is stronger)
    double threshold = 0.01f;  // -40 dB by default
    double max_step = 100.0f;
    size_t count = 0;
    double freq = 0.0;


    one_pole_lowpass *smoo;

};


template<double cutoff, double hysteresis, double ratio, double threshold> 
struct old_lady : csnd::Plugin<2, 1> 
{
    int init()
    {
        _pitch = new pitch_detector(frequency(10.0), frequency(1000.0), 48000.0, lin_to_db(db_to_linear(hysteresis)) );
        lpf.set_coeffs(csound->sr(), cutoff);
        control_lpf.set_coeffs(csound->sr(), 2.0);
        tracked = 10.0f;

        zc_pitch.init(0.01f, false);
        return OK;
    }

    int aperf() 
    {
        double filt = 0.0f;
        for(size_t n = 0; n < ksmps(); ++n)
        {
            // First LPF 
            double filtered = lpf.process(inargs(0)[n]);
            // Then gain compensation 
            double comp = compensate(filtered, _rms(filtered), threshold);
            // Output (signal visualisation purpose)
            outargs(1)[n] = comp;
            // Process pitch tracking 
            double zero_cross = zc_pitch(comp);
            (*_pitch)(float(comp));
            // Then check, distance with previously tracked freq : if too high, filter last value 
            // Also, if the new frequency is below minimum, ignore it 
            double cur_tracked = double(_pitch->get_frequency());
            if( (std::abs(cur_tracked - tracked) > (tracked * 1.2f)) || (cur_tracked < 10.0f) )
            {
                // No change
                std::cout << "Frequency jump detected : " << cur_tracked << " vs " << tracked << " && zc : " << zero_cross << std::endl;
            } 
            else 
            {
                std::cout << "Frequency tracked : " << cur_tracked  << " & Zero cross : " << zero_cross << std::endl;
                tracked = cur_tracked;
            }
            // Finally smooth tracked freq for output
            filt = control_lpf.process(tracked);
        }

        /*
            Celeste freq ratio : estimation * 8
        */
        output =  clip( filt * ratio, 10, 1000.0);
        outargs[0] = output; 
        return OK;
    }

    double tracked = 20.0f; // currently tracked frequency 
    double output = 20.0f; // scaled output 
    cascade_lpf<8> lpf;
    pitch_detector* _pitch;
    rms<1024> _rms;

    butterworth_lpf control_lpf;

    zc_pitch_detector zc_pitch;
};

using celeste = old_lady<200.0, -40.0, 8.0, -60.0>;
using gisele = old_lady<500.0, -40.0, 15.0, -40.0>; 
using gabrielle = old_lady<500.0, -40.0, 15.0, -40.0>; 

void csnd::on_load(Csound *csound) 
{
    csnd::plugin<periodic_pitch_tracker>(csound, "pptrack", "k", "ai", csnd::thread::ia); 
    csnd::plugin<zc_tracker>(csound, "zctrack", "k", "aiii", csnd::thread::ia);

    csnd::plugin<celeste>(csound, "celeste", "ka", "a", csnd::thread::ia);
    csnd::plugin<gisele>(csound, "gisele", "ka", "a", csnd::thread::ia);
    csnd::plugin<gabrielle>(csound, "gabrielle", "ka", "a", csnd::thread::ia);
}