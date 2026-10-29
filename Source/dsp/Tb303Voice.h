#pragma once

// TB-303 style monophonic voice. Plain C++17, no JUCE, so it can be tested offline.
//
// Signal flow (inside the voice, at 2x the host sample rate):
//   saw/square VCO -> diode ladder VCF -> VCA -> [acid stage] -> halfband decimator
// then at host rate: DC blocker -> volume.

#include <algorithm>
#include <array>
#include <cmath>

namespace acid
{
constexpr double kPi = 3.14159265358979323846;

struct VoiceParams
{
    // Classic 303 panel
    double tuneSemis = 0.0;   // -12..+12
    int    waveform  = 0;     // 0 = saw, 1 = square
    double cutoff    = 0.35;  // 0..1
    double resonance = 0.7;   // 0..1
    double envMod    = 0.55;  // 0..1
    double decay     = 0.35;  // 0..1
    double accent    = 0.7;   // 0..1
    double volumeDb  = -6.0;

    // Acid mode: Devil Fish style mods + output stage. Ignored while acidMode is false,
    // in which case the stock 303 values in `stock` below are used.
    bool   acidMode      = false;
    double overdrive     = 0.3;    // 0..1, drive into the filter
    double filterFM      = 0.0;    // 0..1, VCO -> cutoff at audio rate
    double tracking      = 0.0;    // 0..1, cutoff follows pitch (1 = full tracking)
    double sweepSpeed    = 0.5;    // 0..1, accent (C13) sweep speed, 0.5 = stock
    double slideMs       = 60.0;
    double ampDecayMs    = 3000.0; // VCA decay while the gate is held
    double accentDecayMs = 200.0;  // filter envelope decay on accented notes
    double softAttackMs  = 3.0;    // VCA attack on non-accented notes
    double muffler       = 0.0;    // 0..1, soft clip after the VCA
    double distortion    = 0.0;    // 0..1, asymmetric clipper
    double sub           = 0.0;    // 0..1, clean low end added back after the clipper
};

namespace stock
{
constexpr double slideMs        = 60.0;
constexpr double ampDecayMs     = 3000.0;
constexpr double accentDecayMs  = 200.0;
constexpr double softAttackMs   = 3.0;
constexpr double accentAttackMs = 0.5;   // accented notes always have a sharp attack
constexpr double releaseMs      = 12.0;  // VCA closing after gate off
} // namespace stock

namespace detail
{
// Large-signal conductance of a diode / transistor pair: tanh(x) / x
inline double secant (double x) noexcept
{
    return std::abs (x) < 1.0e-4 ? 1.0 - x * x / 3.0 : std::tanh (x) / x;
}

inline double polyBlep (double t, double dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

// Coefficient for an exponential that falls to 5% after timeMs
inline double fallCoef (double timeMs, double rate) noexcept
{
    return std::exp (-3.0 / (std::max (timeMs, 0.01) * 0.001 * rate));
}

// One-pole smoothing step that covers 5% of the remaining distance after timeMs
inline double approachCoef (double timeMs, double rate) noexcept
{
    return 1.0 - fallCoef (timeMs, rate);
}

inline double clamp01 (double x) noexcept { return std::clamp (x, 0.0, 1.0); }

// One-pole TPT (trapezoidal) filter
struct OnePole
{
    double s = 0.0, G = 0.0;

    void setCutoff (double hz, double rate) noexcept
    {
        const double g = std::tan (kPi * std::min (hz, 0.49 * rate) / rate);
        G = g / (1.0 + g);
    }
    double lp (double x) noexcept
    {
        const double v = G * (x - s);
        const double y = v + s;
        s = y + v;
        return y;
    }
    double hp (double x) noexcept { return x - lp (x); }
    void reset() noexcept { s = 0.0; }
};

struct Smoothed
{
    double value = 0.0, target = 0.0;
    double next (double coef) noexcept { value += (target - value) * coef; return value; }
    void snap() noexcept { value = target; }
};
} // namespace detail

//==============================================================================
// Diode ladder low-pass, as in the TB-303.
//
// Unlike the Moog transistor ladder (four buffered stages), the capacitors of a diode
// ladder are coupled to the stages on both sides. Linearised, that gives
//     H(s) = 1 / (s^4 + 7 s^3 + 15 s^2 + 10 s + 1)
// whose poles are spread out (0.12, 1, 2.35, 3.53 x wc) instead of stacked: a gentle
// slope near the cutoff, getting steeper further up. With feedback k the ladder
// self-oscillates at w = sqrt(10/7) wc when k = 901/49 (~18.4).
//
// Each diode pair passes a current tanh(dv). Per sample the tanh terms are replaced by
// their secant gain from the previous sample, which turns the ladder into a linear
// tridiagonal system that is solved exactly (zero-delay feedback, trapezoidal rule).
// The feedback path has a high-pass like the real circuit, so bass survives high
// resonance. Its phase lead raises the self-oscillation point at low cutoffs (measured:
// k_th ~ 18.39 + 27r + 9r^2, r = highpass / cutoff); that is compensated down to
// ~2x the highpass frequency, below which resonance fades like on the hardware.
// Measured tuning after compensation: within 40 cents at 600 Hz, 5 cents at 3 kHz.
class DiodeLadder
{
public:
    static constexpr double kResonanceRatio     = 1.1952286093343936; // sqrt(10/7)
    static constexpr double kSelfOscFeedback    = 901.0 / 49.0;
    static constexpr double kFeedbackHighpassHz = 150.0;

    // Feedback that puts the ladder exactly at self-oscillation for a given cutoff
    static double selfOscFeedback (double resonanceHz) noexcept
    {
        const double r = std::min (0.5, kFeedbackHighpassHz / std::max (resonanceHz, 1.0));
        return kSelfOscFeedback + 27.0 * r + 9.0 * r * r;
    }

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        fbHighpass.setCutoff (kFeedbackHighpassHz, fs);
        reset();
    }

    void reset() noexcept
    {
        s.fill (0.0);
        c.fill (1.0);
        fbHighpass.reset();
    }

    // resonanceHz: where the resonant peak sits.
    // feedback: 1.0 = self-oscillation threshold at this cutoff.
    double process (double x, double resonanceHz, double feedback) noexcept
    {
        // The highpass also pulls the peak sharp (measured ~ +0.68r); pre-correct for it
        const double hz = std::clamp (resonanceHz, 10.0, 0.45 * fs);
        const double fc = hz / (1.0 + 0.68 * std::min (0.5, kFeedbackHighpassHz / hz));
        const double g = std::tan (kPi * fc / fs) / kResonanceRatio;
        const double k = feedback * selfOscFeedback (fc);

        // M = I - g*A, symmetric tridiagonal with the current coupling conductances
        const double gc1 = g * c[1], gc2 = g * c[2], gc3 = g * c[3];
        const double m11 = 1.0 + gc1;
        const double m22 = 1.0 + gc1 + gc2;
        const double m33 = 1.0 + gc2 + gc3;
        const double m44 = 1.0 + gc3 + g * c[4];

        // Thomas algorithm, forward elimination shared by both right-hand sides
        const double cp1 = -gc1 / m11;
        const double den2 = m22 + gc1 * cp1;
        const double cp2 = -gc2 / den2;
        const double den3 = m33 + gc2 * cp2;
        const double cp3 = -gc3 / den3;
        const double den4 = m44 + gc3 * cp3;

        // Response to the integrator states alone (p) ...
        const double dp1 = s[0] / m11;
        const double dp2 = (s[1] + gc1 * dp1) / den2;
        const double dp3 = (s[2] + gc2 * dp2) / den3;
        const double p4 = (s[3] + gc3 * dp3) / den4;
        const double p3 = dp3 - cp3 * p4;
        const double p2 = dp2 - cp2 * p3;
        const double p1 = dp1 - cp1 * p2;

        // ... and per unit of input drive u (q)
        const double dq1 = g * c[0] / m11;
        const double dq2 = gc1 * dq1 / den2;
        const double dq3 = gc2 * dq2 / den3;
        const double q4 = gc3 * dq3 / den4;
        const double q3 = dq3 - cp3 * q4;
        const double q2 = dq2 - cp2 * q3;
        const double q1 = dq1 - cp1 * q2;

        // Solve the feedback loop u = x - k * HP(v4) for u in one step
        const double hpGain = 1.0 - fbHighpass.G;
        const double u = (x - k * hpGain * (p4 - fbHighpass.s)) / (1.0 + k * hpGain * q4);

        const double v1 = p1 + q1 * u;
        const double v2 = p2 + q2 * u;
        const double v3 = p3 + q3 * u;
        const double v4 = p4 + q4 * u;

        s[0] = 2.0 * v1 - s[0];
        s[1] = 2.0 * v2 - s[1];
        s[2] = 2.0 * v3 - s[2];
        s[3] = 2.0 * v4 - s[3];
        fbHighpass.lp (v4);

        c[0] = detail::secant (u);
        c[1] = detail::secant (v1 - v2);
        c[2] = detail::secant (v2 - v3);
        c[3] = detail::secant (v3 - v4);
        c[4] = detail::secant (v4);

        return v4;
    }

private:
    double fs = 88200.0;
    std::array<double, 4> s {};
    std::array<double, 5> c {};
    detail::OnePole fbHighpass;
};

//==============================================================================
// 2:1 decimator: 47-tap Kaiser-windowed halfband FIR (~70 dB stopband)
class HalfbandDecimator
{
public:
    static constexpr int kTaps = 47;

    void prepare() noexcept
    {
        constexpr int mid = kTaps / 2;
        constexpr double beta = 7.0;
        double sum = 0.0;

        for (int n = 0; n < kTaps; ++n)
        {
            const int m = n - mid;
            const double ideal = m == 0 ? 0.5 : std::sin (kPi * m * 0.5) / (kPi * m);
            const double r = (double) m / mid;
            const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
            taps[(std::size_t) n] = ideal * w;
            sum += taps[(std::size_t) n];
        }

        for (auto& t : taps)
            t /= sum;

        reset();
    }

    void reset() noexcept
    {
        history.fill (0.0);
        pos = 0;
    }

    double process (double a, double b) noexcept
    {
        push (a);
        push (b);
        const double* x = history.data() + pos;
        double acc = 0.0;
        for (int n = 0; n < kTaps; ++n)
            acc += taps[(std::size_t) n] * x[n];
        return acc;
    }

private:
    void push (double x) noexcept
    {
        pos = (pos == 0 ? kTaps : pos) - 1;
        history[(std::size_t) pos] = history[(std::size_t) (pos + kTaps)] = x;
    }

    static double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 32; ++k)
        {
            const double h = x / (2.0 * k);
            term *= h * h;
            sum += term;
        }
        return sum;
    }

    std::array<double, kTaps> taps {};
    std::array<double, 2 * kTaps> history {};
    int pos = 0;
};

//==============================================================================
class Tb303Voice
{
public:
    static constexpr int kOversampling = 2;

    // Knob ranges, chosen to feel like a 303 panel
    static constexpr double kCutoffLowHz        = 200.0; // resonant peak with cutoff and env mod at zero
    static constexpr double kCutoffRangeOct     = 4.5;   // cutoff knob span
    static constexpr double kEnvModDepthOct     = 4.5;   // filter envelope depth at full env mod
    static constexpr double kEnvModBaseShiftOct = 1.0;   // env mod also lowers the base cutoff (dual-gang pot)
    static constexpr double kAccentDepthOct     = 3.0;   // accent sweep depth
    static constexpr double kFilterFmDepthOct   = 3.0;
    static constexpr double kClassicMaxFeedback = 0.94;  // x self-oscillation: a stock 303 never quite whistles
    static constexpr double kAcidMaxFeedback    = 1.08;  // acid mode can self-oscillate
    static constexpr double kFilterInputLevel   = 0.5;
    static constexpr double kNormalLevel        = 0.6;   // VCA level of a non-accented note
    static constexpr double kAccentLevel        = 0.9;   // extra VCA level at full accent
    static constexpr double kSquareLevel        = 0.85;
    static constexpr double kFilterMakeup       = 10.0;  // post-filter buffer gain (the ladder loses a lot of level)

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        fsOS = sampleRate * kOversampling;

        ladder.prepare (fsOS);
        squareDroop.setCutoff (25.0, fsOS);
        subLp1.setCutoff (110.0, fsOS);
        subLp2.setCutoff (110.0, fsOS);
        distPreHp.setCutoff (90.0, fsOS);
        distTone.setCutoff (4200.0, fsOS);
        dcBlock.setCutoff (8.0, fs);
        decimator.prepare();

        smoothCoef = detail::approachCoef (30.0, fs);
        releaseCoef = detail::fallCoef (stock::releaseMs, fs);

        reset();
        setParams (params);
        for (auto* sm : smoothers())
            sm->snap();
    }

    void reset() noexcept
    {
        phase = 0.0;
        cv = targetCv = 36.0;
        meg = veg = c13 = 0.0;
        gate = attacking = sliding = accentOn = false;
        ladder.reset();
        squareDroop.reset();
        subLp1.reset();
        subLp2.reset();
        distPreHp.reset();
        distTone.reset();
        dcBlock.reset();
        decimator.reset();
    }

    void setParams (const VoiceParams& p) noexcept
    {
        params = p;
        const bool acid = p.acidMode;
        const double r = detail::clamp01 (p.resonance);

        smBaseOct.target = std::log2 (kCutoffLowHz) + detail::clamp01 (p.cutoff) * kCutoffRangeOct
                         - detail::clamp01 (p.envMod) * kEnvModBaseShiftOct;
        smEnvDepth.target = detail::clamp01 (p.envMod) * kEnvModDepthOct;
        smFeedback.target = (acid ? kAcidMaxFeedback : kClassicMaxFeedback) * (1.0 - std::pow (1.0 - r, 2.5));
        smInGain.target = kFilterInputLevel * (acid ? 1.0 + 7.0 * p.overdrive * p.overdrive : 1.0);
        smOutGain.target = std::pow (10.0, p.volumeDb / 20.0);
        smFm.target = acid ? detail::clamp01 (p.filterFM) * kFilterFmDepthOct : 0.0;
        smTracking.target = acid ? detail::clamp01 (p.tracking) : 0.0;
        smMuffler.target = acid ? detail::clamp01 (p.muffler) : 0.0;
        smDist.target = acid ? detail::clamp01 (p.distortion) : 0.0;
        smSub.target = acid ? detail::clamp01 (p.sub) : 0.0;

        slideCoef = detail::approachCoef (acid ? p.slideMs : stock::slideMs, fs);

        // 303 decay knob: roughly 200 ms .. 2 s. Accent forces a short decay.
        megNormalCoef = detail::fallCoef (200.0 * std::pow (10.0, detail::clamp01 (p.decay)), fs);
        megAccentCoef = detail::fallCoef (acid ? p.accentDecayMs : stock::accentDecayMs, fs);
        vegDecayCoef = detail::fallCoef (acid ? p.ampDecayMs : stock::ampDecayMs, fs);
        softAttackMs = acid ? p.softAttackMs : stock::softAttackMs;

        // Accent sweep (C13): the resonance pot sits in its charge path, so at high
        // resonance the sweep is slower and consecutive accents stack up ("wow").
        const double sweepScale = acid ? std::pow (4.0, 1.0 - 2.0 * detail::clamp01 (p.sweepSpeed)) : 1.0;
        c13ChargeCoef = 1.0 - std::exp (-1.0 / ((0.0005 + 0.025 * r) * sweepScale * fs));
        c13DischargeCoef = 1.0 - std::exp (-1.0 / ((0.0005 + 0.12 * r) * sweepScale * fs));
    }

    // slide = true: legato, glide to the new pitch without retriggering the envelopes
    void noteOn (int midiNote, bool accented, bool slide) noexcept
    {
        targetCv = (double) midiNote;
        accentOn = accented;

        if (slide && gate)
        {
            sliding = true;
            return;
        }

        sliding = false;
        cv = targetCv;
        meg = 1.0;
        gate = true;
        attacking = true;
        const double attackMs = accented ? stock::accentAttackMs : softAttackMs;
        attackInc = 1.0 / std::max (1.0, attackMs * 0.001 * fs);
    }

    // External cutoff modulation in octaves (e.g. the tempo-synced LFO)
    void setCutoffModOct (double octaves) noexcept { cutoffModOct = octaves; }

    void noteOff() noexcept
    {
        gate = false;
        attacking = false;
        sliding = false;
    }

    float process() noexcept
    {
        const double baseOct  = smBaseOct.next (smoothCoef);
        const double envDepth = smEnvDepth.next (smoothCoef);
        const double k        = smFeedback.next (smoothCoef);
        const double inGain   = smInGain.next (smoothCoef);
        const double outGain  = smOutGain.next (smoothCoef);
        const double fm       = smFm.next (smoothCoef);
        const double tracking = smTracking.next (smoothCoef);
        const double muffler  = smMuffler.next (smoothCoef);
        const double dist     = smDist.next (smoothCoef);
        const double sub      = smSub.next (smoothCoef);

        // Slide is an RC lag on the pitch CV, as in the 303's slide circuit
        cv += (targetCv - cv) * slideCoef;
        const double hz = 440.0 * std::exp2 ((cv + params.tuneSemis - 69.0) / 12.0);
        const double dt = std::min (hz / fsOS, 0.45);

        // Filter envelope (MEG): instant attack, exponential decay, ignores gate off
        meg *= accentOn ? megAccentCoef : megNormalCoef;

        // Amp envelope (VEG)
        if (gate)
        {
            if (attacking)
            {
                veg += attackInc;
                if (veg >= 1.0)
                {
                    veg = 1.0;
                    attacking = false;
                }
            }
            else
            {
                veg *= vegDecayCoef;
            }
        }
        else
        {
            veg *= releaseCoef;
        }

        // Accent: the MEG through the accent pot. Goes straight to the VCA, and to the
        // cutoff through the C13 sweep circuit.
        const double accentCv = accentOn ? meg * detail::clamp01 (params.accent) : 0.0;
        c13 += (accentCv - c13) * (accentCv > c13 ? c13ChargeCoef : c13DischargeCoef);

        flushDenormals();

        const double vca = veg * (kNormalLevel + kAccentLevel * accentCv);
        const double cutoffOct = baseOct + envDepth * meg + kAccentDepthOct * c13
                               + tracking * (cv - 48.0) / 12.0 + cutoffModOct;

        std::array<double, kOversampling> os {};
        for (auto& y : os)
        {
            const double osc = nextOsc (dt);
            const double resonanceHz = std::exp2 (cutoffOct + fm * osc);
            y = ladder.process (osc * inGain, resonanceHz, k) * kFilterMakeup * vca;

            if (params.acidMode)
                y = acidStage (y, muffler, dist, sub);
        }

        double out = decimator.process (os[0], os[1]);
        out = dcBlock.hp (out) * outGain;

        if (! std::isfinite (out))
        {
            reset();
            out = 0.0;
        }

        return (float) out;
    }

    bool isGateOn() const noexcept         { return gate; }
    bool isSliding() const noexcept        { return gate && sliding && std::abs (targetCv - cv) > 0.05; }
    double getAccentSweep() const noexcept { return c13; }

private:
    double nextOsc (double dt) noexcept
    {
        phase += dt;
        if (phase >= 1.0)
            phase -= 1.0;

        if (params.waveform == 0)
            return 2.0 * phase - 1.0 - detail::polyBlep (phase, dt);

        // The 303 square comes off the saw through a transistor shaper; the coupling
        // cap gives it a visible droop on low notes.
        double t2 = phase + 0.5;
        if (t2 >= 1.0)
            t2 -= 1.0;
        const double sq = (phase < 0.5 ? 1.0 : -1.0) + detail::polyBlep (phase, dt) - detail::polyBlep (t2, dt);
        return squareDroop.hp (sq) * kSquareLevel;
    }

    double acidStage (double x, double muffler, double dist, double sub) noexcept
    {
        // Devil Fish "muffler": soft clipper on the VCA output that squashes the loudest peaks
        if (muffler > 1.0e-4)
        {
            const double d = 1.0 + 5.0 * muffler;
            x = std::tanh (x * d) / d * (1.0 + 2.0 * muffler);
        }

        const double low = subLp2.lp (subLp1.lp (x));

        // Asymmetric clipper (mismatched diodes), bass removed first and added back clean via `sub`
        double d = distPreHp.hp (x) * (1.0 + 60.0 * dist * dist);
        d = d >= 0.0 ? std::tanh (1.3 * d) : 0.8 * std::tanh (0.9 * d);
        d = distTone.lp (d) * 0.7;
        x += (d - x) * std::min (1.0, 3.0 * dist);

        // Gentle soft limit so drive + sub can't run away
        return std::tanh (x + low * sub * 1.5);
    }

    void flushDenormals() noexcept
    {
        if (meg < 1.0e-12) meg = 0.0;
        if (veg < 1.0e-12) veg = 0.0;
        if (c13 < 1.0e-12) c13 = 0.0;
    }

    std::array<detail::Smoothed*, 10> smoothers() noexcept
    {
        return { &smBaseOct, &smEnvDepth, &smFeedback, &smInGain, &smOutGain,
                 &smFm, &smTracking, &smMuffler, &smDist, &smSub };
    }

    VoiceParams params;
    double fs = 44100.0, fsOS = 88200.0;

    // Oscillator and pitch
    double phase = 0.0, cv = 36.0, targetCv = 36.0, slideCoef = 0.0, cutoffModOct = 0.0;

    // Envelopes and accent
    double meg = 0.0, veg = 0.0, c13 = 0.0;
    double megNormalCoef = 0.0, megAccentCoef = 0.0, vegDecayCoef = 0.0, releaseCoef = 0.0;
    double attackInc = 0.0, softAttackMs = stock::softAttackMs;
    double c13ChargeCoef = 0.0, c13DischargeCoef = 0.0;
    bool gate = false, attacking = false, sliding = false, accentOn = false;

    // Smoothed controls
    double smoothCoef = 0.0;
    detail::Smoothed smBaseOct, smEnvDepth, smFeedback, smInGain, smOutGain,
                     smFm, smTracking, smMuffler, smDist, smSub;

    DiodeLadder ladder;
    HalfbandDecimator decimator;
    detail::OnePole squareDroop, subLp1, subLp2, distPreHp, distTone, dcBlock;
};
} // namespace acid
