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
    Params : 
    - asig 
    - i Pre-filter cutoff 
    - i Analysis Hysteresis & Gain threshold 
    - i freq Multiplier ratio 

*/

struct celeste : csnd::Plugin<2, 4> 
{
    int init()
    {
        cutoff = inargs[1];
        hysteresis = inargs[2]; 
        ratio = inargs[3]; 

        _pitch = new pitch_detector(frequency(10.0), frequency(cutoff), float(csound->sr())  , lin_to_db(db_to_linear(hysteresis)) );
        lpf.set_coeffs(csound->sr(), cutoff);
        control_lpf.set_coeffs(csound->sr(), 3.0);
        tracked = 10.0f;
        _rms_mem.allocate(csound, 8192);
        _rms.init(_rms_mem.data(), 8192);
        _zc.init(hysteresis, false); 
        _cmp.init(csound->sr(), 1.0);
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
            double comp = _cmp(filtered, _rms(filtered), hysteresis);
            // Output (signal visualisation purpose)
            outargs(1)[n] = comp;
            // Process pitch tracking 
            (*_pitch)(float(comp));
            // Then check, distance with previously tracked freq : if too high, filter last value 
            // Also, if the new frequency is below minimum, ignore it 
            
            double cur_tracked = double(_pitch->get_frequency());
            filt = cur_tracked; // control_lpf.process(cur_tracked);
            /*
            double ratio_jump = cur_tracked / tracked;
            if(ratio_jump > 4 || ratio_jump < 0.25 || cur_tracked < 5.0f)
            {
                // No change
                //std::cout << "Frequency jump detected : " << cur_tracked << " vs " << tracked  << std::endl;
            } 
            else 
            {
                //std::cout << "Frequency tracked : " << cur_tracked   << std::endl;
                tracked = cur_tracked;
            }
            // Finally smooth tracked freq for output
            filt = control_lpf.process(tracked);
            */
        }

        /*
            Celeste freq ratio : estimation * 8
        */
        output =  clip( filt * ratio, 10.0, 1000.0);
        outargs[0] = output; 
        return OK;
    }


    double cutoff, hysteresis, ratio;  

    double tracked = 20.0f; // currently tracked frequency 
    double output = 20.0f; // scaled output 
    cascade_lpf<8> lpf;
    pitch_detector* _pitch;
    rms_ext _rms;
    csnd::AuxMem<double> _rms_mem;

    butterworth_lpf control_lpf;
    zc_pitch_detector _zc;
    compensator _cmp;
};


/*
    Params : 
    - asig 
    - i Pre-filter cutoff 
    - i Analysis Hysteresis & Gain threshold 
    - i freq Multiplier ratio 

*/

struct gisele : csnd::Plugin<2, 4> 
{
    int init()
    {
        cutoff = inargs[1];
        hysteresis = inargs[2]; 
        ratio = inargs[3]; 

        _pitch = new pitch_detector(frequency(10.0), frequency(cutoff), float(csound->sr())  , lin_to_db(db_to_linear(hysteresis)) );
        lpf.set_coeffs(csound->sr(), cutoff);
        control_lpf.set_coeffs(csound->sr(), 3.0);
        tracked = 10.0f;
        _rms_mem.allocate(csound, 8192);
        _rms.init(_rms_mem.data(), 8192);
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
            double comp = compensate(filtered, _rms(filtered), hysteresis);
            // Output (signal visualisation purpose)
            outargs(1)[n] = comp;
            // Process pitch tracking 
            (*_pitch)(float(comp));
            // Then check, distance with previously tracked freq : if too high, filter last value 
            // Also, if the new frequency is below minimum, ignore it 
            double cur_tracked = double(_pitch->get_frequency());
            double ratio_jump = cur_tracked / tracked;
            if(ratio_jump > 4 || ratio_jump < 0.25 || cur_tracked < 5.0f)
            {
                // No change
                //std::cout << "Frequency jump detected : " << cur_tracked << " vs " << tracked  << std::endl;
            } 
            else 
            {
                //std::cout << "Frequency tracked : " << cur_tracked   << std::endl;
                tracked = cur_tracked;
            }
            // Finally smooth tracked freq for output
            filt = control_lpf.process(tracked);
        }

        /*
            Celeste freq ratio : estimation * 8
        */
        output =  clip( filt * ratio, 20, 1000.0);
        outargs[0] = output; 
        return OK;
    }


    double cutoff, hysteresis, ratio;  

    double tracked = 20.0f; // currently tracked frequency 
    double output = 20.0f; // scaled output 
    cascade_lpf<8> lpf;
    pitch_detector* _pitch;
    rms_ext _rms;
    csnd::AuxMem<double> _rms_mem;

    butterworth_lpf control_lpf;
};

/*
    Params : 
    - asig 
    - i Pre-filter cutoff 
    - i Analysis Hysteresis & Gain threshold 
    - i freq Multiplier ratio 

*/

struct gabrielle : csnd::Plugin<2, 4> 
{
    int init()
    {
        cutoff = inargs[1];
        hysteresis = inargs[2]; 
        ratio = inargs[3]; 

        _pitch = new pitch_detector(frequency(10.0), frequency(cutoff), float(csound->sr())  , lin_to_db(db_to_linear(hysteresis)) );
        lpf.set_coeffs(csound->sr(), cutoff);
        control_lpf.set_coeffs(csound->sr(), 3.0);
        tracked = 10.0f;
        _rms_mem.allocate(csound, 8192);
        _rms.init(_rms_mem.data(), 8192);
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
            double comp = compensate(filtered, _rms(filtered), hysteresis);
            // Output (signal visualisation purpose)
            outargs(1)[n] = comp;
            // Process pitch tracking 
            (*_pitch)(float(comp));
            // Then check, distance with previously tracked freq : if too high, filter last value 
            // Also, if the new frequency is below minimum, ignore it 
            double cur_tracked = double(_pitch->get_frequency());
            double ratio_jump = cur_tracked / tracked;
            if(ratio_jump > 4 || ratio_jump < 0.25 || cur_tracked < 5.0f)
            {
                // No change
                //std::cout << "Frequency jump detected : " << cur_tracked << " vs " << tracked  << std::endl;
            } 
            else 
            {
                //std::cout << "Frequency tracked : " << cur_tracked   << std::endl;
                tracked = cur_tracked;
            }
            // Finally smooth tracked freq for output
            filt = control_lpf.process(tracked);
        }

        /*
            Celeste freq ratio : estimation * 8
        */
        output =  clip( filt * ratio, 20, 1000.0);
        outargs[0] = output; 
        return OK;
    }


    double cutoff, hysteresis, ratio;  

    double tracked = 20.0f; // currently tracked frequency 
    double output = 20.0f; // scaled output 
    cascade_lpf<8> lpf;
    pitch_detector* _pitch;
    rms_ext _rms;
    csnd::AuxMem<double> _rms_mem;

    butterworth_lpf control_lpf;
};






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
            double smp = negative ? -double(inargs(0)[i]) : double(inargs(0)[i]); 
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

/*


instr 2 
    ain = inch(1)
    icut = 1000
    asig = ain
    //asig = highcut(ain, icut, 3, 0)
    asig = butlp( butlp( butlp( butlp( ain, icut ), icut ), icut  ), icut )

    krms = rms(asig)
    kgain init 1.0
    kg init 1.0
    if(krms < 0.001) then 
        kg = 1.0
    else 
        kg = 1.0 / krms 
    endif

    kgain = lineto(kg, 0.01)

    asig *= kgain

    //asig_comp = balance2(asig, ain)
    iinv init 0
    ithreshold init 0.1
    ktrack = zctrack(asig, iinv, ithreshold, 100)

    kfreq init 100
    kfreq = limit:k(ktrack * (1.5)  , 10, icut)
    ao = oscili(0.02 , kfreq, 2 )  ;+ oscili(0.1, 500) 
    printf("\t\ttrack : freq : %f %f \n", changed:k(ktrack), ktrack, kfreq)
    outs(ao, a(0) )
    outch(3, asig)
endin
 
    params : 
    - asig 
    - cutoff 
    - hysteresis 
    - max step (e.g. 100Hz to avoid octave jumps)
*/

struct sonora_slim : csnd::Plugin<2, 5> 
{
    int init() 
    {
        cutoff = inargs[1];
        threshold = inargs[2];
        ratio = inargs[3];
        max_step = inargs[4];
        _lpf.set_coeffs(csound->sr(), cutoff);
        _rms.init(10, csound->sr()); 
        _cmp.init(csound->sr(), 10.0, 1.0);
        smoo = new one_pole_lowpass(frequency(20.0) , csound->sr() / ksmps());
        count = 0;
        last = 0.0f;
        freq = 0.0f;
        _tmp.allocate(csound, ksmps());
        return OK; 
    }

    int aperf() 
    {
        bool negative = false;
        double rms_val = 0.0f;
        for(size_t n = 0; n < ksmps(); ++n)
        {
            double filtered = _lpf.process(inargs(0)[n]);
            rms_val = _rms(filtered); 
            _tmp[n] = filtered;
        }

        for(size_t n = 0; n < ksmps(); ++n)
        {
            double comp = _cmp(_tmp[n], rms_val, threshold, 1.0);
            //double comp = _tmp[n] * 4.0;
            outargs(1)[n] = comp;
            double smp = negative ? -comp : comp; 
            if(last <= db_to_linear(threshold) && smp > db_to_linear(threshold) ) 
            {
                //Crossed 0, Calculate freq, then zero counter
                double period = double(count) / double(csound->sr()); 
                double new_freq = 1.0 / period; 
                if(abs(new_freq - freq) < max_step /*25.0*/)  
                {
                    freq = (*smoo)(new_freq );
                } 
                count = 0; 
            }
            last = smp;
            ++count;
        }

        outargs[0] = freq * ratio; 
        return OK;
    }

    csnd::AuxMem<double> _tmp; 
    double cutoff, threshold, ratio, max_step;
    double freq = 0.0f;

    size_t count = 0; 
    double last = 0.0f;

    cascade_lpf<8> _lpf; 
    compensator _cmp; 
    rms _rms; 
    one_pole_lowpass *smoo;

};

// sonora piaf


struct sonora_piaf : csnd::Plugin<2, 5> 
{
    int init() 
    {
        cutoff = inargs[1];
        threshold = inargs[2];
        ratio = inargs[3];
        max_step = inargs[4];
        _lpf.set_coeffs(csound->sr(), cutoff);
        _rms.init(10, csound->sr()); 
        _cmp.init(csound->sr(), 10.0, 1.0);
        smoo = new one_pole_lowpass(frequency(20.0) , csound->sr() / ksmps());
        count = 0;
        last = 0.0f;
        freq = 0.0f;
        _tmp.allocate(csound, ksmps());
        return OK; 
    }

    int aperf() 
    {
        bool negative = false;
        double rms_val = 0.0f;
        for(size_t n = 0; n < ksmps(); ++n)
        {
            double filtered = _lpf.process(inargs(0)[n]);
            rms_val = _rms(filtered); 
            _tmp[n] = filtered;
        }

        for(size_t n = 0; n < ksmps(); ++n)
        {
            double comp = _cmp(_tmp[n], rms_val, threshold, 1.0);
            //double comp = _tmp[n] * 4.0;
            outargs(1)[n] = comp;
            double smp = negative ? -comp : comp; 
            if(last <= db_to_linear(threshold) && smp > db_to_linear(threshold) ) 
            {
                //Crossed 0, Calculate freq, then zero counter
                double period = double(count) / double(csound->sr()); 
                double new_freq = 1.0 / period; 
                if(abs(new_freq - freq) < max_step /*25.0*/)  
                {
                    freq = (*smoo)(new_freq );
                } 
                count = 0; 
            }
            last = smp;
            ++count;
        }

        outargs[0] = freq * ratio; 
        return OK;
    }

    csnd::AuxMem<double> _tmp; 
    double cutoff, threshold, ratio, max_step;
    double freq = 0.0f;

    size_t count = 0; 
    double last = 0.0f;

    cascade_lpf<8> _lpf; 
    compensator _cmp; 
    rms _rms; 
    one_pole_lowpass *smoo;
};



void csnd::on_load(Csound *csound)
{
    csnd::plugin<celeste>               (csound, "celeste",   "ka", "aiii",     csnd::thread::ia);
    csnd::plugin<gisele>                (csound, "gisele",    "ka", "aiii",     csnd::thread::ia);
    csnd::plugin<gabrielle>             (csound, "gabrielle", "ka", "aiii",     csnd::thread::ia);

    csnd::plugin<sonora_slim>            (csound, "sonora_slim", "ka", "aiiii", csnd::thread::ia);
    csnd::plugin<sonora_piaf>            (csound, "sonora_piaf", "ka", "aiiii", csnd::thread::ia);

    csnd::plugin<periodic_pitch_tracker>(csound, "pptrack",   "k",  "ai",    csnd::thread::ia);
    csnd::plugin<zc_tracker>            (csound, "zctrack",   "k",  "aiii",  csnd::thread::ia);
}