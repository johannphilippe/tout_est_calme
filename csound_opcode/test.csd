<CsoundSynthesizer>                                                                                           
<CsOptions>
-odac:null
-iadc:null
-+rtaudio=jack
-B 4096
</CsOptions>
; ==============================================
<CsInstruments>
 
sr      =       48000
ksmps   =       16
nchnls =       3
0dbfs   =       1

/*
    Céleste (virtuose) 
    {
        cutoff : 1000, 4 * butterlp (ordre 8)
        compensation RMS kgain = 1.0 / rms / 4 
        hysteresis : 0.01 
        freq = tracking * 8
    }
    Gisèle (la vieille aigrie) 
    {
        cutoff : 1500, 4 * butterlp (ordre 8)
        compensation RMS kgain = 1.0 / rms / 4 
        hysteresis : 0.01 
        freq = tracking * 8 * 2
    }
    La grosse Gabrielle (la vieille aigrie) 
    {
        cutoff : 1500, 4 * butterlp (ordre 8)
        compensation RMS kgain = 1.0 / rms / 4 
        hysteresis : 0.01 
        freq = tracking * 8 * 2
    }


*/
seed(0)

gkorigin init 0
gktarget init 0
gkfreq init 0
gkchanged init 0

instr note
    inote init p4
    print(inote)
    ifreq = mtof:i(inote)
    print(ifreq)
    //OSCsend(1, "10.42.0.21", 8765, "/siren/inertia", "i", 0)
    //OSCsend(1, "10.42.0.21", 8765, "/siren/target", "f", ifreq)
    gkorigin = gktarget
    gktarget = ifreq
    ao = oscili(0.1, ifreq, 2)
    outs(ao, ao)
endin

instr sched 
    kmet = metro:k(0.1) // Every 10 seconds 
    krnd = int(random:k(30, 90))
    if(changed:k(kmet) > 0 && kmet > 0) then 
        schedulek("note", 0, 10, krnd)
    endif
endin

instr celeste_i 
    ain = inch(1)
    kfreq, asig celeste ain
    gkfreq = kfreq
    printk2(kfreq, 50)
    ao = oscili(0.1, kfreq, 2)
    outs( ao, a(0))
    outch(3, asig)
    //oscsend(kfreq, "10.42.0.21", 8765, "/siren/freq", "f", kfreq) 
endin

instr gisele_i 
    ain = inch(1)
    kfreq, asig gisele ain
    gkfreq = kfreq
    printk2(kfreq, 50)
    ao = oscili(0.1, kfreq, 2)
    outs( ao, a(0))
    outch(3, asig)
    //oscsend(kfreq, "10.42.0.21", 8765, "/siren/freq", "f", kfreq) 
endin
instr gabrielle_i 
    ain = inch(1)
    kfreq, asig gabrielle ain
    gkfreq = kfreq
    printk2(kfreq, 50)
    ao = oscili(0.1, kfreq, 2)
    outs( ao, a(0))
    outch(3, asig)
    //oscsend(kfreq, "10.42.0.21", 8765, "/siren/freq", "f", kfreq) 
endin

instr cv
    if(gktarget == gkorigin) then 
        // Do nothing 
    endif
    

    if(changed:k(gkfreq) > 0) then 
    endif
endin

instr 1 
    ain = inch(1)
    icut = 1000
    asig = ain
    //asig = highcut(ain, icut, 3, 0)
    asig = butlp( butlp( butlp( butlp( ain, icut ), icut ), icut  ), icut )

    krms = rms(asig)
    kgain init 1.0
    if(krms = 0.0) then 
        kgain = 1.0
    else 
        kgain = 1.0 / krms / 4
    endif

    asig *= kgain

    //asig_comp = balance2(asig, ain)
    ktrack = pptrack(asig, 0.01)

    kfreq init 2 
    kfreq = limit:k(ktrack * 16  /* * (1 + 7/12)*/  , 20, icut)
    ao = oscili(0.1, kfreq, 2 )
    printf("track : freq : %f %f \n", changed:k(ktrack), ktrack, kfreq)
    outs(ao, a(0) )
endin


/*
{
    News : 
    Sonora 12V : 
    {
        4 LPF Butterworth 1500Hz
        Compensation : 1.0 / rms 
        inversion = 0
        threshold = 0.1
        freq = tracking * 1.5

    }
    Sonore 24V: 
    {
        4 LPF Butterworth 1500Hz
        Compensation : 1.0 / rms 
        inversion = 0
        threshold = 0.1
        freq = tracking * 0.5
    }

}
*/
instr 2 
    ain = inch(1)
    icut = 1500
    asig = ain
    //asig = highcut(ain, icut, 3, 0)
    asig = butlp( butlp( butlp( butlp( ain, icut ), icut ), icut  ), icut )

    krms = rms(asig)
    kgain init 1.0
    if(krms < 0.001) then 
        kgain = 1.0
    else 
        kgain = 1.0 / krms 
    endif

    asig *= kgain

    //asig_comp = balance2(asig, ain)
    iinv init 1
    ithreshold init 0.1
    ktrack = zctrack(asig, iinv, ithreshold, 100)

    kfreq init 100
    kfreq = limit:k(ktrack * (4) /* (0.5) */  , 10, icut)
    ao = oscili(0.1 , kfreq, 2 )  ;+ oscili(0.1, 500) 
    printf("\t\ttrack : freq : %f %f \n", changed:k(ktrack), ktrack, kfreq)
    outs(ao, a(0) )
    outch(3, asig)


endin
 
</CsInstruments>
; ==============================================
<CsScore>
f 0 z
f2 0 128 10 1 0.5 0.3 0.25 0.2 0.167 0.14 0.125 .111   ; Sawtooth with a small amount of data
;i 2 0 -1
;i "celeste_i" 0 -1
i "gisele_i" 0 -1
;i "gabrielle_i" 0 -1

 
 
 
</CsScore>
</CsoundSynthesizer>
