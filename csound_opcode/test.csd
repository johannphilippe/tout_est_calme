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
    ifreq = mtof:i(inote)
    OSCsend(1, "123.45.1.10", 8765, "/siren/inertia", "i", int(2) )
    OSCsend(1, "123.45.1.10", 8765, "/siren/target", "f", ifreq)
    ao = oscili(0.02, ifreq, 2)
    outs(ao, ao)
endin

instr celeste_i 
    ain = inch(1)
    icut init 200
    ihyst init -40 
    imult init 8
    asig init 0
    kfreq, asig celeste ain, icut, ihyst, imult
    ;gkfreq = kfreq
    kmet = metro:k(10)
    printf( "freq : %f \n", kmet, kfreq )
    ao = oscili(0.02, kfreq, 2)
    outs( ao, a(0))
    outch(3, asig)
    //oscsend(kfreq, "10.42.0.21", 8765, "/siren/freq", "f", kfreq) 
endin

instr gisele_i 
    ain = inch(1)
    icut init 500
    ihyst init -40 
    imult init 16
    kfreq, asig gisele ain, icut, ihyst, imult
    gkfreq = kfreq
    kmet = metro:k(10)
    printf( "freq : %f \n", kmet, kfreq )
    ao = oscili(0.1, kfreq, 2)
    outs( ao, a(0))
    outch(3, asig)
    //oscsend(kfreq, "10.42.0.21", 8765, "/siren/freq", "f", kfreq) 
endin

instr gabrielle_i 
    icut = 500
    ihyst = -40 
    imult = 16
    ain = inch(1)
    kfreq, asig gabrielle ain, icut, ihyst, imult
    gkfreq = kfreq
    kmet = metro:k(10)
    printf( "freq : %f \n", kmet, kfreq )
    ao = oscili(0.1, kfreq, 2)
    outs( ao, a(0))
    outch(3, asig)
    //oscsend(kfreq, "10.42.0.21", 8765, "/siren/freq", "f", kfreq) 
endin

instr sonora_slim 
    ain = inch(1)
    icut = 1000 
    ihyst = -60
    iratio = 1.5
    imax_step = 100
    kfreq, asig sonora_slim ain, icut, ihyst, iratio, imax_step
    gkfreq = kfreq
    kmet = metro:k(10)
    printf( "freq : %f \n", kmet, kfreq)
    ao = oscili(0.02, kfreq, 2)
    outs( ao, a(0))
    outch(3, asig)
    //oscsend(kfreq, "
endin

instr sonora_piaf
    ain = inch(1)
    icut = 1000 
    ihyst = -60
    iratio = 0.5
    imax_step = 100
    kfreq, asig sonora_slim ain, icut, ihyst, iratio, imax_step
    gkfreq = kfreq
    kmet = metro:k(10)
    printf( "freq : %f \n", kmet, kfreq)
    ao = oscili(0.02, kfreq, 2)
    ;outs( ao, a(0))
    outch(3, asig)
    OSCsend(kfreq, "123.45.1.10", 8765, "/siren/pitch", "f", kfreq)
endin

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
    kfreq = limit:k(ktrack * (1.5) /* (0.5) */  , 10, icut)
    ao = oscili(0.02 , kfreq, 2 )  ;+ oscili(0.1, 500) 
    ;printf("\t\ttrack : freq : %f %f \n", changed:k(ktrack), ktrack, kfreq)
    ;outs(ao, a(0) )
    outch(3, asig)
endin
 

 
</CsInstruments>
; ==============================================
<CsScore>
f 0 z
f2 0 128 10 1 0.5 0.3 0.25 0.2 0.167 0.14 0.125 .111   ; Sawtooth with a small amount of data
;i 2 0 -1
;i "celeste_i" 0 -1
;i "gisele_i" 0 -1
;i "gabrielle_i" 0 -1
;i "pll_pitch_tracker" 0 -1
;i "mpzc_pitch_tracker" 0 -1
;i "sonora_slim" 0 -1
i "sonora_piaf" 0 -1

i "note" 0 10 60

 
 
 
</CsScore>
</CsoundSynthesizer>
