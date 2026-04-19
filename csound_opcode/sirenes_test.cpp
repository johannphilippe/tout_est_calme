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


template<double cutoff, double hysteresis, double ratio, double threshold> 
struct old_lady : csnd::Plugin<2, 1> 
{
    int init()
    {
        _pitch = new pitch_detector(frequency(10.0), frequency(cutoff), float(csound->sr())  , lin_to_db(db_to_linear(hysteresis)) );
        lpf.set_coeffs(csound->sr(), cutoff);
        control_lpf.set_coeffs(csound->sr(), 3.0);
        tracked = 10.0f;

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


    double tracked = 20.0f; // currently tracked frequency 
    double output = 20.0f; // scaled output 
    cascade_lpf<8> lpf;
    pitch_detector* _pitch;
    rms<4096> _rms;

    butterworth_lpf control_lpf;

};

using celeste = old_lady<400.0, -40.0, 8.0, -40.0>;
using gisele = old_lady<500.0, -40.0, 15.0, -40.0>;
using gabrielle = old_lady<500.0, -60.0, 14.5, -60.0>;


/*
    PLL pitch tracker — Lazzarini I/Q + atan2 design.

    Mixes input with quadrature VCO (cos/sin), low-pass filters both channels,
    then extracts phase error via atan2.  This is amplitude-independent and bounded
    in [-π, π], eliminating the integral-windup and out-of-phase problems of a
    simple multiplier PLL.

    Gain compensation (RMS normalisation) is applied before mixing so that the
    loop filter time constant is independent of pickup level, which matters here
    because low-frequency sirens produce much weaker signals than high-frequency ones.

    Args: asig, ilpf_cutoff, krms_thr_db, kloop_pole, kKphi
      ilpf_cutoff  : 8-stage Butterworth prefilter cutoff in Hz (init-rate)
      krms_thr_db  : noise gate — signal below this RMS level (dB) is silenced
      kloop_pole   : one-pole loop-filter coefficient (0–1); higher = slower/smoother
                     pull-in range ≈ sr·(1–pole)/(2π) Hz; start with 0.995 then increase
      kKphi        : VCO gain, Hz per radian of phase error per sample (start: 0.0001)
    Output: kfreq (VCO frequency in Hz; equals input frequency when locked)
*/
struct pll_tracker : csnd::Plugin<1, 5>
{
    int init()
    {
        prefilter.set_coeffs(csound->sr(), inargs[1]);
        _rms_buf.allocate(csound, RMS_WIN);
        _rms.init(&_rms_buf[0], RMS_WIN);
        _vco_freq  = 20.0;
        _vco_phase = 0.0;
        _I_filt    = 0.0;
        _Q_filt    = 0.0;
        return OK;
    }

    int aperf()
    {
        const double sr        = csound->sr();
        const double rms_thr   = db_to_linear(inargs[2]);
        const double pole      = inargs[3];
        const double inv_pole  = 1.0 - pole;
        const double Kphi      = inargs[4];

        for (size_t n = 0; n < ksmps(); ++n)
        {
            double x   = prefilter.process(inargs(0)[n]);
            double rv  = _rms(x);
            double sig = (rv > rms_thr) ? x / (rv * 2.0) : 0.0;

            // I/Q mixing — cos for in-phase, sin for quadrature
            _I_filt = pole * _I_filt + inv_pole * sig * std::cos(_vco_phase);
            _Q_filt = pole * _Q_filt + inv_pole * sig * std::sin(_vco_phase);

            // Phase error: bounded [-π, π], amplitude-independent via atan2
            const double phase_err = std::atan2(_Q_filt, _I_filt);

            // Frequency integrator: drives freq toward zero phase error
            _vco_freq = clip(_vco_freq + Kphi * phase_err, 5.0, 1000.0);

            _vco_phase += 2.0 * std::numbers::pi * _vco_freq / sr;
            if (_vco_phase >= 2.0 * std::numbers::pi)
                _vco_phase -= 2.0 * std::numbers::pi;
        }

        outargs[0] = _vco_freq;
        return OK;
    }

    static constexpr size_t RMS_WIN = 4096;

    double                _vco_freq  = 20.0;
    double                _vco_phase = 0.0;
    double                _I_filt    = 0.0;
    double                _Q_filt    = 0.0;
    cascade_lpf<8>        prefilter;
    csnd::AuxMem<double>  _rms_buf;
    rms_ext               _rms;
};


/*
    Multi-period zero-crossing pitch tracker.
    Accumulates inum_periods consecutive upward threshold crossings, then estimates
    frequency from the total elapsed time:  f = N / (elapsed_samples / sr)

    Gain compensation (RMS normalisation) is applied before threshold testing so
    that kzc_threshold is consistent regardless of pickup level — important when
    low-frequency sirens produce much weaker signals than high-frequency ones.

    Output is smoothed with a k-rate 2nd-order Butterworth lowpass.

    Args: asig, ilpf_cutoff, krms_thr_db, kzc_thr, inum_periods, ksmoo_hz
      ilpf_cutoff  : 8-stage Butterworth prefilter cutoff in Hz (init-rate)
      krms_thr_db  : noise gate in dB — signal below this level is silenced
      kzc_thr      : crossing threshold on the gain-compensated signal (linear, e.g. 0.1)
      inum_periods : number of periods to average (init-rate, ≥ 1)
      ksmoo_hz     : output smoother cutoff in Hz (k-rate, e.g. 2.0)
    Output: kfreq
*/
struct mpzc_tracker : csnd::Plugin<1, 6>
{
    int init()
    {
        const double sr = csound->sr();
        prefilter.set_coeffs(sr, inargs[1]);
        _n_periods = std::max(1, (int)inargs[4]);
        // Circular buffer: needs n_periods+1 timestamps to span n_periods intervals
        _crossings.allocate(csound, _n_periods + 1);
        _rms_buf.allocate(csound, RMS_WIN);
        _rms.init(&_rms_buf[0], RMS_WIN);
        _smoo.set_coeffs(sr / ksmps(), inargs[5]);
        _head         = 0;
        _stored       = 0;
        _last         = 0.0;
        _sample_count = 0;
        _freq         = 0.0;
        return OK;
    }

    int aperf()
    {
        const double sr      = csound->sr();
        const double rms_thr = db_to_linear(inargs[2]);
        const double zc_thr  = inargs[3];
        const int    cap     = _n_periods + 1;

        // Allow k-rate update of smoother cutoff
        _smoo.set_coeffs(sr / ksmps(), inargs[5]);

        for (size_t n = 0; n < ksmps(); ++n)
        {
            double x    = prefilter.process(inargs(0)[n]);
            double rv   = _rms(x);
            double comp = (rv > rms_thr) ? x / (rv * 2.0) : 0.0;

            if (_last <= zc_thr && comp > zc_thr)
            {
                _crossings[_head] = _sample_count;
                _head = (_head + 1) % cap;
                if (_stored < cap) ++_stored;

                if (_stored == cap)
                {
                    // After writing and advancing _head, oldest entry is now at _head
                    const size_t oldest  = _crossings[_head];
                    const size_t newest  = _crossings[(_head - 1 + cap) % cap];
                    const size_t elapsed = newest - oldest;
                    if (elapsed > 0)
                        _freq = double(_n_periods) * sr / double(elapsed);
                }
            }
            _last = comp;
            ++_sample_count;
        }

        outargs[0] = _smoo.process(_freq);
        return OK;
    }

    static constexpr size_t RMS_WIN = 4096;

    int                  _n_periods    = 4;
    int                  _head         = 0;
    int                  _stored       = 0;
    size_t               _sample_count = 0;
    double               _last         = 0.0;
    double               _freq         = 0.0;
    csnd::AuxMem<size_t> _crossings;
    csnd::AuxMem<double> _rms_buf;
    cascade_lpf<8>       prefilter;
    butterworth_lpf      _smoo;
    rms_ext              _rms;
};


void csnd::on_load(Csound *csound)
{
    csnd::plugin<periodic_pitch_tracker>(csound, "pptrack",   "k",  "ai",    csnd::thread::ia);
    csnd::plugin<zc_tracker>            (csound, "zctrack",   "k",  "aiii",  csnd::thread::ia);

    csnd::plugin<celeste>               (csound, "celeste",   "ka", "a",     csnd::thread::ia);
    csnd::plugin<gisele>                (csound, "gisele",    "ka", "a",     csnd::thread::ia);
    csnd::plugin<gabrielle>             (csound, "gabrielle", "ka", "a",     csnd::thread::ia);

    csnd::plugin<pll_tracker>           (csound, "pll_track",  "k",  "aikkk",  csnd::thread::ia);
    csnd::plugin<mpzc_tracker>          (csound, "mpzc_track", "k",  "aikkik", csnd::thread::ia);
}