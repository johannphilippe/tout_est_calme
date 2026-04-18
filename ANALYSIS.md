# `old_lady` DSP Analysis

## Pipeline overview

```
asig → cascade_lpf<8> → RMS compensate → zc_pitch_detector (unused!)
                                        → cycfi pitch_detector → jump gate → control_lpf(2Hz) → × ratio → out
```

---

## Low-frequency tracking problem

### Root cause

`cycfi::q::pitch_detector` needs at least **2 full periods** of signal to produce an estimate.
At 10 Hz that is 200 ms = **9 600 samples** at 48 kHz before any output stabilises.
The slowness you observe is intrinsic to any period-based algorithm at these frequencies — the algorithm is correct, just fundamentally limited by physics.

### Recommendation 1 — Decimate before the cycfi detector

Apply a steep anti-aliasing LPF (your `cascade_lpf<8>` already is one), then downsample by factor D before feeding `pitch_detector`.

| Original SR | Decimation | Effective SR | 10 Hz → equivalent | Latency per period |
|---|---|---|---|---|
| 48 000 | 10× | 4 800 | same 10 Hz but 10× fewer samples to wait | ~20 ms |
| 48 000 | 16× | 3 000 | same 10 Hz | ~13 ms |

At SR = 4 800 Hz, a 10 Hz tone is 480 samples/period — cycfi will handle it fast and without extra configuration. The detected frequency in Hz is unchanged because Hz is absolute, not relative to sample rate.

**Minimal implementation sketch:**

```cpp
// In old_lady members:
size_t _decim_phase = 0;
static constexpr size_t DECIM = 10;
pitch_detector* _pitch; // re-init with sr/DECIM

// In aperf loop, after lpf.process():
if (++_decim_phase >= DECIM) {
    _decim_phase = 0;
    (*_pitch)(float(filtered)); // feed decimated sample
}
double cur_tracked = double(_pitch->get_frequency());
```

The `cascade_lpf<8>` prefilter already provides enough attenuation above Nyquist/2 of the decimated rate, so no extra filter is needed for 10×.

### Recommendation 2 — Use the zero-crossing tracker you already have

`zc_pitch` is computed every sample in `aperf` but its result is **only printed — never incorporated** into the output. For sub-20 Hz quasi-sinusoidal signals (which is what exits your 8-stage LPF), ZC is actually more responsive than cycfi because it fires on every half-period rather than waiting for autocorrelation convergence.

Suggested fusion strategy:
- If `cycfi` frequency < 20 Hz **or** confidence is low → trust `zc_pitch` output
- If `cycfi` frequency ≥ 20 Hz → trust `cycfi` (more robust against noise)

### Recommendation 3 — Multi-period ZC averaging

Instead of measuring one period at a time, accumulate N=4 periods and divide:

```cpp
// accumulate crossings, then: freq = N / (total_samples / sr)
```

This halves the variance of the estimate without increasing latency compared to cycfi.

---

## Other DSP issues

### 1. Frequency jump gate uses absolute difference, not ratio

```cpp
// Current (sirenes.cpp:141)
if( (std::abs(cur_tracked - tracked) > (tracked * 1.2f)) ...
```

This rejects a jump if `|Δf| > 1.2 × tracked`. At 10 Hz that means any jump above 12 Hz is rejected — very strict. At 100 Hz, jumps up to 120 Hz are accepted — too loose. The condition should be a ratio:

```cpp
double ratio_jump = cur_tracked / tracked;
if (ratio_jump > 1.5 || ratio_jump < 0.67) { /* reject */ }
```

### 2. `zc_pitch_detector` hardcodes 48 000 Hz

```cpp
// dsp.hpp:235
double period = double(_count) / 48000.0;
```

This breaks at any other Csound SR. It should receive SR at construction or `init()`.

### 3. `lin_to_db(db_to_linear(hysteresis))` is a no-op

```cpp
// sirenes.cpp:115
_pitch = new pitch_detector(..., lin_to_db(db_to_linear(hysteresis)));
```

`db → linear → db` returns `hysteresis` unchanged. Drop the double conversion.

### 4. RMS window too short for low frequencies

`rms<1024>` at 48 kHz = **21 ms**. One period of a 10 Hz signal is 100 ms. The RMS estimate oscillates within a single cycle, making the gain compensation noisy and modulating the signal fed to the pitch detector. Options:

- Increase to `rms<8192>` (~170 ms) for the sub-20 Hz range
- Or separate the RMS computation from the per-sample loop and update it at k-rate

### 5. Per-sample `std::cout` in real-time audio thread

```cpp
// sirenes.cpp:144, 148
std::cout << "Frequency jump detected : " ...
std::cout << "Frequency tracked : " ...
```

These fire **once per sample** at 48 kHz — that is ~48 000 `std::cout` calls per second. This will lock the audio thread and cause xruns. Gate behind a k-rate counter or remove before deployment.

### 6. `control_lpf` applied per-sample, output taken once per ksmps

The `control_lpf.process(tracked)` call runs ksmps times but only the last value is used. This is functionally equivalent to running it once per ksmps block, just wasteful. Minor, but worth cleaning up.

### 7. `tracked` initialised inconsistently

Member default is `20.0f`, `init()` sets it to `10.0f`. Pick one.

---

## Summary table

| Issue | Severity | Fix complexity |
|---|---|---|
| cycfi slow below 20 Hz | High | Medium (decimation) |
| `zero_cross` result unused | High | Low (wire it in) |
| Jump gate uses absolute Δf | Medium | Trivial |
| `std::cout` in audio loop | High (xruns) | Trivial |
| ZC hardcodes 48 kHz | Medium | Low |
| RMS window too short | Medium | Trivial |
| `lin_to_db(db_to_linear(...))` no-op | Low | Trivial |
| `control_lpf` per-sample waste | Low | Trivial |
