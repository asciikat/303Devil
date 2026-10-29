desc:TB-303 Acid Bassline + 16-Step Acid Sequencer + Devil Fish
// tags: instrument synthesizer bass acid 303 techno sequencer
// author: Custom JSFX DSP + Step Sequencer (Verified & Patched)

slider1:0<0,1,1{Sawtooth,Square (303 Hollow)}>-Waveform
slider2:0<-24,24,1>-Tuning (Semitones)
slider3:35<0,100,0.1>-Cutoff Frequency (%)
slider4:75<0,100,0.1>-Resonance (%)
slider5:65<0,100,0.1>-Env Mod (%)
slider6:45<0,100,0.1>-Decay (%)
slider7:70<0,100,0.1>-Accent Amount (%)
slider8:60<10,200,1>-Slide Time (ms)
slider9:100<1,127,1>-Accent Velocity Threshold
slider10:1<0,1,1{Authentic 303,FULL ACID TECHNO}>-Acid Mode
slider11:55<0,100,0.1>-Acid Drive / Distortion (%)
slider12:130<60,180,1>-Internal BPM (Fallback)
slider13:0<0,1,1{Internal Sequencer,External MIDI}>-Sequencer Source
slider14:-6<-24,12,0.1>-Output Level (dB)
slider15:1<0,1,1{Host DAW Sync,Internal Free-Run}>-Clock Sync Mode
slider16:38<24,60,1>-Root Note (MIDI)

@init
ext_noinit = 1; // Prevent resetting arrays on transport play

pi = $pi;
twopi = 2.0 * pi;

// Memory Layout (Non-overlapping EEL2 Heap Addresses)
scope_buf = 1000;   // 1000..1511 (512 samples)
scope_len = 512;
seq_note  = 4000;   // 4000..4015 (16 steps)
seq_gate  = 4020;   // 4020..4035 (16 steps)
seq_slide = 4040;   // 4040..4055 (16 steps)
seq_acc   = 4060;   // 4060..4075 (16 steps)

// Initialize Default Acid Pattern only once
!pattern_initialized ? (
  phase = 0;
  current_note = -1;
  gate = 0;
  target_freq = 110;
  curr_freq = 110;
  vel = 0;
  is_accent = 0;
  is_slide = 0;
  s1 = s2 = s3 = s4 = 0;
  dc_x1 = dc_y1 = 0;
  sub_lp = 0;
  hp_fb = 0;
  vca_env = 0;
  vcf_env = 0;
  acc_cap = 0;
  scope_idx = 0;
  current_step = 0;
  last_step = -1;
  seq_sample_clock = 0;

  seq_gate[0]=1;  seq_note[0]=0;   seq_acc[0]=1;  seq_slide[0]=0;
  seq_gate[1]=1;  seq_note[1]=12;  seq_acc[1]=0;  seq_slide[1]=1;
  seq_gate[2]=1;  seq_note[2]=3;   seq_acc[2]=0;  seq_slide[2]=0;
  seq_gate[3]=0;  seq_note[3]=7;   seq_acc[3]=0;  seq_slide[3]=0;
  seq_gate[4]=1;  seq_note[4]=-2;  seq_acc[4]=1;  seq_slide[4]=0;
  seq_gate[5]=1;  seq_note[5]=5;   seq_acc[5]=0;  seq_slide[5]=1;
  seq_gate[6]=1;  seq_note[6]=7;   seq_acc[6]=0;  seq_slide[6]=0;
  seq_gate[7]=0;  seq_note[7]=0;   seq_acc[7]=0;  seq_slide[7]=0;
  seq_gate[8]=1;  seq_note[8]=0;   seq_acc[8]=1;  seq_slide[8]=0;
  seq_gate[9]=1;  seq_note[9]=10;  seq_acc[9]=0;  seq_slide[9]=1;
  seq_gate[10]=1; seq_note[10]=3;  seq_acc[10]=0; seq_slide[10]=0;
  seq_gate[11]=0; seq_note[11]=5;  seq_acc[11]=0; seq_slide[11]=0;
  seq_gate[12]=1; seq_note[12]=12; seq_acc[12]=1; seq_slide[12]=0;
  seq_gate[13]=1; seq_note[13]=7;  seq_acc[13]=0; seq_slide[13]=1;
  seq_gate[14]=1; seq_note[14]=3;  seq_acc[14]=0; seq_slide[14]=0;
  seq_gate[15]=0; seq_note[15]=0;  seq_acc[15]=0; seq_slide[15]=0;

  pattern_initialized = 1;
);

// UI Interaction State
active_knob = -1;
drag_y = 0;
drag_val = 0;
last_click_time = 0;

function tanh_approx(x) local(x2) (
  x = max(-3.0, min(3.0, x));
  x2 = x * x;
  x * (27.0 + x2) / (27.0 + 9.0 * x2);
);

function poly_blep(t, dt) (
  t < dt ? (
    t /= dt;
    t + t - t * t - 1.0;
  ) : t > (1.0 - dt) ? (
    t = (t - 1.0) / dt;
    t * t + t + t + 1.0;
  ) : 0.0;
);

function randomize_acid_pattern() local(i, r) (
  i = 0;
  loop(16,
    seq_gate[i] = (rand(1.0) > 0.25) ? 1 : 0;
    r = rand(6.0);
    r < 1.5 ? seq_note[i] = 0 :
    r < 2.3 ? seq_note[i] = -2 :
    r < 3.1 ? seq_note[i] = 3 :
    r < 3.9 ? seq_note[i] = 5 :
    r < 4.7 ? seq_note[i] = 7 :
    r < 5.3 ? seq_note[i] = 10 : seq_note[i] = 12;
    
    seq_acc[i] = (rand(1.0) > 0.62) ? 1 : 0;
    seq_slide[i] = (rand(1.0) > 0.58) ? 1 : 0;
    i += 1;
  );
);

function update_dsp_params() (
  wave = slider1;
  tune_Multiplier = 2 ^ (slider2 / 12.0);
  cutoff_norm = slider3 * 0.01;
  base_cutoff = 28.0 * (160.0 ^ (cutoff_norm ^ 1.2));
  res_norm = slider4 * 0.01;
  env_mod = slider5 * 0.01;
  decay_norm = slider6 * 0.01;
  accent_amt = slider7 * 0.01;
  slide_ms = slider8;
  acc_thresh = slider9;
  acid_mode = slider10;
  drive_norm = slider11 * 0.01;
  bpm = slider12;
  seq_source = slider13;
  out_gain = 10 ^ (slider14 / 20.0);
  clock_mode = slider15;
  root_midi = slider16;

  slide_coeff = exp(-1000.0 / (slide_ms * srate));
  decay_ms = 200.0 + (decay_norm ^ 2.0) * 2300.0;
  vcf_decay_normal = exp(-1000.0 / (decay_ms * srate));
  vcf_decay_accent = exp(-1000.0 / (210.0 * srate));

  vca_att = exp(-1000.0 / (1.5 * srate));
  vca_rel = exp(-1000.0 / (18.0 * srate));
  vca_hold_decay = exp(-1000.0 / (3500.0 * srate));

  acc_cap_att = exp(-1000.0 / (12.0 * srate));
  acc_cap_rel = acid_mode ? exp(-1000.0 / (140.0 * srate)) : exp(-1000.0 / (85.0 * srate));
  
  eff_bpm = (clock_mode == 0 && tempo > 0) ? tempo : bpm;
  step_samples = max(100, (60.0 / eff_bpm) * 0.25 * srate);
);

// Persist 16-step pattern in REAPER project / preset saves
@serialize
file_mem(0, seq_note, 16);
file_mem(0, seq_gate, 16);
file_mem(0, seq_slide, 16);
file_mem(0, seq_acc, 16);

@slider
update_dsp_params();

@block
update_dsp_params();

seq_source == 1 ? (
  while (midirecv(offset, msg1, msg2, msg3)) (
    status = msg1 & 0xF0;
    status == 0x90 && msg3 > 0 ? (
      note = msg2; vel = msg3;
      gate > 0 ? (is_slide = 1;) : (is_slide = 0; vcf_env = 1.0;);
      current_note = note; gate = 1;
      target_freq = 440.0 * (2.0 ^ ((note - 69.0) / 12.0)) * tune_Multiplier;
      !is_slide ? curr_freq = target_freq;
      vel >= acc_thresh ? (
        is_accent = 1;
        acc_cap = min(1.8, acc_cap + (acid_mode ? 0.85 : 0.65));
      ) : (
        is_accent = 0;
      );
    ) : (status == 0x80 || (status == 0x90 && msg3 == 0)) ? (
      msg2 == current_note ? (gate = 0; is_slide = 0;);
    ) : (
      midisend(offset, msg1, msg2, msg3);
    );
  );
) : (
  // Pass through any MIDI while running Internal Sequencer
  while (midirecv(offset, msg1, msg2, msg3)) (
    midisend(offset, msg1, msg2, msg3);
  );
);

@sample
// 1. Sample-Accurate Internal 16-Step Sequencer
seq_source == 0 ? (
  is_running = (clock_mode == 1) || (play_state & 1);
  is_running ? (
    (clock_mode == 0 && (play_state & 1)) ? (
      // Host DAW Beat Position Sync (4 sixteenth notes per quarter-note beat)
      total_16ths = beat_position * 4.0;
      current_step = floor(total_16ths) & 15;
      step_frac = total_16ths - floor(total_16ths);
      // Advance beat_position across the audio block for sample accuracy
      beat_position += (eff_bpm / (60.0 * srate));
    ) : (
      // Internal Free-Run Clock (plays immediately even if REAPER transport is stopped)
      seq_sample_clock += 1;
      seq_sample_clock >= step_samples ? (
        seq_sample_clock -= step_samples;
        current_step = (current_step + 1) & 15;
      );
      step_frac = seq_sample_clock / step_samples;
    );

    // Detect Step Transition
    current_step != last_step ? (
      prev_step = (current_step + 15) & 15;
      prev_had_slide = seq_gate[prev_step] && seq_slide[prev_step];

      seq_gate[current_step] ? (
        note = root_midi + seq_note[current_step];
        target_freq = 440.0 * (2.0 ^ ((note - 69.0) / 12.0)) * tune_Multiplier;

        // Authentic 303: Slide glides FROM a slide-flagged step INTO the current step
        prev_had_slide || seq_slide[current_step] ? (
          is_slide = 1;
        ) : (
          is_slide = 0;
          curr_freq = target_freq;
          vcf_env = 1.0;
        );

        gate = 1;
        seq_acc[current_step] ? (
          is_accent = 1;
          acc_cap = min(1.8, acc_cap + (acid_mode ? 0.85 : 0.65));
        ) : (
          is_accent = 0;
        );
      ) : (
        gate = 0;
        is_slide = 0;
      );

      last_step = current_step;
    );

    // Authentic 303 55% Gate Length (unless sliding into next step!)
    !seq_slide[current_step] && (step_frac > 0.55) ? (
      gate = 0;
    );
  ) : (
    gate = 0;
    is_slide = 0;
    last_step = -1;
  );
);

// 2. Pitch Glide (RC Slide when legato)
is_slide ? (
  curr_freq = target_freq + slide_coeff * (curr_freq - target_freq);
) : (
  curr_freq = target_freq;
);

// 3. Envelopes
gate ? (
  vca_target = is_accent ? (1.0 + accent_amt * 0.45) : 0.75;
  vca_env = vca_target + vca_att * (vca_env - vca_target);
  vca_env *= vca_hold_decay;
) : (
  vca_env *= vca_rel;
);

current_vcf_decay = is_accent ? vcf_decay_accent : vcf_decay_normal;
vcf_env *= current_vcf_decay;
acc_cap *= acc_cap_rel;

// 4. Oscillator (PolyBLEP Saw & 303 Hollow Derived Square)
dt = curr_freq / srate;
phase += dt;
phase >= 1.0 ? phase -= 1.0;

saw = 2.0 * phase - 1.0;
saw -= poly_blep(phase, dt);

wave == 0 ? (
  osc = saw;
) : (
  pw = 0.54;
  sq = phase < pw ? 1.0 : -1.0;
  sq += poly_blep(phase, dt);
  sq -= poly_blep(phase - pw + (phase < pw ? 1.0 : 0.0), dt);
  osc = sq * 0.82 - saw * 0.18;
);

// 5. 4-Pole Diode Ladder Filter + C13 Accent Sweep
env_Factor = vcf_env * (1.0 + env_mod * (acid_mode ? 7.5 : 5.5));
acc_Sweep = acc_cap * accent_amt * (acid_mode ? 4.5 : 2.8);

mod_cutoff = base_cutoff * (1.0 + env_Factor + acc_Sweep);
mod_cutoff = min(srate * 0.44, max(25.0, mod_cutoff));

wc = twopi * mod_cutoff / srate;
g = wc / (1.0 + wc);

k_res = (res_norm ^ 0.85) * (acid_mode ? 4.15 : 3.85);
is_accent ? k_res = min(4.3, k_res + accent_amt * 0.25);

fb_signal = s4 - hp_fb;
hp_fb += 0.008 * fb_signal;

input_sig = osc - k_res * tanh_approx(fb_signal * (acid_mode ? 1.4 : 1.0));
input_sig = tanh_approx(input_sig);

s1 += g * (input_sig - s1);
s2 += g * (s1 - s2);
s3 += g * (s2 - s3);
s4 += g * (s3 - s4);

flt_out = s4;
sig = flt_out * vca_env;

// 6. FULL ACID TECHNO DSP (Devil Fish + Asymmetric RAT Clipper + Sub Exciter)
acid_mode ? (
  sub_lp += 0.015 * (sig - sub_lp);
  drive_gain = 1.0 + (drive_norm ^ 1.5) * 14.0;
  driven = sig * drive_gain;
  driven >= 0 ? (
    clipped = tanh_approx(driven * 1.35);
  ) : (
    clipped = tanh_approx(driven * 0.9) * 1.1;
  );
  sig = clipped * 0.85 + sub_lp * (0.6 + drive_norm * 0.8);
);

dc_out = sig - dc_x1 + 0.995 * dc_y1;
dc_x1 = sig;
dc_y1 = dc_out;

final_out = dc_out * out_gain;

scope_buf[scope_idx] = final_out;
scope_idx = (scope_idx + 1) % scope_len;

spl0 = final_out;
spl1 = final_out;

// ==========================================================
// CUSTOM HARDWARE GUI & 16-STEP SEQUENCER (@gfx)
// ==========================================================
@gfx 720 620

function draw_knob(id, x, y, r, val, min_v, max_v, def_v, step_v, label, unit_str)
  local(norm, ang, start_ang, end_ang, px, py, i, tick_ang, tx1, ty1, tx2, ty2, dist, sensitivity, tw, th)
(
  (mouse_cap & 1) ? (
    active_knob == -1 ? (
      dist = sqrt((mouse_x - x)^2 + (mouse_y - y)^2);
      dist <= r + 6 ? (
        time_precise() - last_click_time < 0.25 ? (
          val = def_v;
          active_knob = -2;
        ) : (
          active_knob = id;
          drag_y = mouse_y;
          drag_val = val;
        );
        last_click_time = time_precise();
      );
    ) : active_knob == id ? (
      sensitivity = (mouse_cap & 8) ? 600.0 : 140.0;
      val = drag_val + ((drag_y - mouse_y) / sensitivity) * (max_v - min_v);
      val = max(min_v, min(max_v, val));
      step_v >= 1 ? val = floor(val + 0.5);
      slider_automate(2^(id-1));
    );
  ) : (
    active_knob = -1;
  );

  norm = (val - min_v) / (max_v - min_v);
  start_ang = -0.75 * pi;
  end_ang = 0.75 * pi;
  ang = start_ang + norm * (end_ang - start_ang);

  i = 0;
  loop(11,
    tick_ang = start_ang + (i / 10.0) * (end_ang - start_ang) - pi*0.5;
    tx1 = x + cos(tick_ang) * (r + 4);
    ty1 = y + sin(tick_ang) * (r + 4);
    tx2 = x + cos(tick_ang) * (r + 9);
    ty2 = y + sin(tick_ang) * (r + 9);
    acid_mode ? gfx_set(0.95, 0.85, 0.1, 0.7) : gfx_set(0.15, 0.15, 0.18, 0.85);
    gfx_line(tx1, ty1, tx2, ty2);
    i += 1;
  );

  gfx_set(0.05, 0.05, 0.06, 0.6);
  gfx_circle(x + 2, y + 3, r, 1);
  gfx_set(0.14, 0.14, 0.16, 1.0);
  gfx_circle(x, y, r, 1);

  acid_mode ? gfx_set(0.22, 0.22, 0.25, 1.0) : gfx_set(0.28, 0.29, 0.32, 1.0);
  gfx_circle(x, y, r * 0.78, 1);

  px = x + cos(ang - pi*0.5) * (r * 0.82);
  py = y + sin(ang - pi*0.5) * (r * 0.82);
  acid_mode ? gfx_set(1.0, 0.9, 0.0, 1.0) : gfx_set(1.0, 0.25, 0.15, 1.0);
  gfx_line(x, y, px, py);

  gfx_setfont(1, "Arial", 11, 'b');
  acid_mode ? gfx_set(0.95, 0.9, 0.2, 1.0) : gfx_set(0.12, 0.12, 0.15, 1.0);
  gfx_measurestr(label, tw, th);
  gfx_x = x - tw * 0.5; gfx_y = y - r - 22;
  gfx_drawstr(label);

  gfx_setfont(1, "Courier New", 11, 'b');
  acid_mode ? gfx_set(0.4, 1.0, 0.3, 0.9) : gfx_set(0.2, 0.2, 0.25, 0.9);
  step_v >= 1 ? sprintf(#str_val, "%d%s", val, unit_str) : sprintf(#str_val, "%.1f%s", val, unit_str);
  gfx_measurestr(#str_val, tw, th);
  gfx_x = x - tw * 0.5; gfx_y = y + r + 8;
  gfx_drawstr(#str_val);

  val;
);

// 1. CHASSIS BACKGROUND
acid_mode = slider10;
acid_mode ? (
  gfx_set(0.09, 0.09, 0.11, 1.0);
  gfx_rect(0, 0, 720, 620);
  gfx_set(0.95, 0.82, 0.05, 1.0);
  gfx_rect(10, 10, 700, 600, 0);
) : (
  gfx_set(0.76, 0.78, 0.80, 1.0);
  gfx_rect(0, 0, 720, 620);
  gfx_set(0.25, 0.26, 0.28, 1.0);
  gfx_rect(10, 10, 700, 600, 0);
);

// 2. HEADER BAR
acid_mode ? gfx_set(0.95, 0.82, 0.05, 1.0) : gfx_set(0.18, 0.19, 0.22, 1.0);
gfx_rect(20, 20, 680, 40);

gfx_setfont(1, "Arial", 16, 'b');
acid_mode ? gfx_set(0.08, 0.08, 0.1, 1.0) : gfx_set(0.92, 0.92, 0.94, 1.0);
gfx_x = 28; gfx_y = 32;
gfx_drawstr("TB-303 // DEVIL ACID RIG + 16-STEP SEQUENCER");

// Clock Sync Mode Button (Host Sync vs Free-Run Preview)
cx = 395; cy = 25; cw = 132; ch = 30;
gfx_set(0.12, 0.12, 0.14, 1.0); gfx_rect(cx, cy, cw, ch);
gfx_setfont(1, "Arial", 11, 'b');
slider15 == 0 ? (
  gfx_set(0.4, 0.95, 0.4, 1.0); gfx_x = cx + 10; gfx_y = cy + 9; gfx_drawstr("CLK: DAW SYNC");
) : (
  gfx_set(1.0, 0.85, 0.2, 1.0); gfx_x = cx + 10; gfx_y = cy + 9; gfx_drawstr("CLK: FREE-RUN");
);
(mouse_cap & 1) && !(last_mouse_cap & 1) ? (
  mouse_x >= cx && mouse_x <= cx+cw && mouse_y >= cy && mouse_y <= cy+ch ? (
    slider15 = 1 - slider15; slider_automate(2^14); update_dsp_params();
  );
);

// Waveform Button
wx = 535; wy = 25; ww = 155; wh = 30;
gfx_set(0.12, 0.12, 0.14, 1.0);
gfx_rect(wx, wy, ww, wh);
gfx_setfont(1, "Arial", 11, 'b');
slider1 == 0 ? (
  gfx_set(1.0, 0.4, 0.2, 1.0); gfx_x = wx + 12; gfx_y = wy + 9; gfx_drawstr("WAVE: SAWTOOTH");
) : (
  gfx_set(0.3, 0.9, 1.0, 1.0); gfx_x = wx + 10; gfx_y = wy + 9; gfx_drawstr("WAVE: 303 SQUARE");
);

(mouse_cap & 1) && !(last_mouse_cap & 1) ? (
  mouse_x >= wx && mouse_x <= wx+ww && mouse_y >= wy && mouse_y <= wy+wh ? (
    slider1 = 1 - slider1; slider_automate(1); update_dsp_params();
  );
);

// 3. SYNTH KNOBS (TOP ROW)
slider2 = draw_knob(2, 72,  125, 24, slider2, -24, 24, 0,  1,   "TUNING",    " st");
slider3 = draw_knob(3, 180, 125, 24, slider3, 0,   100, 35, 0.1, "CUTOFF",    "%");
slider4 = draw_knob(4, 288, 125, 24, slider4, 0,   100, 75, 0.1, "RESONANCE", "%");
slider5 = draw_knob(5, 396, 125, 24, slider5, 0,   100, 65, 0.1, "ENV MOD",   "%");
slider6 = draw_knob(6, 504, 125, 24, slider6, 0,   100, 45, 0.1, "DECAY",     "%");
slider7 = draw_knob(7, 612, 125, 24, slider7, 0,   100, 70, 0.1, "ACCENT",    "%");

// 4. SECOND ROW CONTROLS
slider8  = draw_knob(8,  68,  218, 21, slider8,  10,  200, 60,  1,   "SLIDE TIME", "ms");
slider16 = draw_knob(16, 158, 218, 21, slider16, 24,  60,  38,  1,   "ROOT MIDI",  "");
slider11 = draw_knob(11, 248, 218, 21, slider11, 0,   100, 55,  0.1, "ACID DRIVE", "%");
slider12 = draw_knob(12, 338, 218, 21, slider12, 60,  180, 130, 1,   "FREE BPM",   "");
slider14 = draw_knob(14, 428, 218, 21, slider14, -24, 12,  -6,  0.1, "OUTPUT",     "dB");

// Acid Mode Toggle Button
bx = 485; by = 192; bw = 210; bh = 52;
acid_mode ? (
  gfx_set(0.95, 0.80, 0.05, 1.0); gfx_rect(bx, by, bw, bh);
  gfx_set(1.0, 0.2, 0.1, 1.0); gfx_rect(bx+2, by+2, bw-4, bh-4, 0);
  gfx_setfont(1, "Arial", 13, 'b'); gfx_set(0.08, 0.08, 0.08, 1.0);
  gfx_x = bx + 14; gfx_y = by + 18; gfx_drawstr("FULL ACID TECHNO: ON");
) : (
  gfx_set(0.20, 0.21, 0.24, 1.0); gfx_rect(bx, by, bw, bh);
  gfx_setfont(1, "Arial", 12, 'b'); gfx_set(0.85, 0.85, 0.88, 1.0);
  gfx_x = bx + 18; gfx_y = by + 18; gfx_drawstr("ENGAGE ACID TECHNO");
);

(mouse_cap & 1) && !(last_mouse_cap & 1) ? (
  mouse_x >= bx && mouse_x <= bx+bw && mouse_y >= by && mouse_y <= by+bh ? (
    slider10 = 1 - slider10; slider_automate(2^9); update_dsp_params();
  );
);

// 5. 16-STEP ROLAND-STYLE ACID SEQUENCER MATRIX
seq_y = 266;
gfx_set(0.12, 0.13, 0.15, 1.0);
gfx_rect(20, seq_y, 680, 232);
gfx_set(0.3, 0.35, 0.4, 1.0);
gfx_rect(20, seq_y, 680, 232, 0);

gfx_setfont(1, "Arial", 13, 'b');
acid_mode ? gfx_set(0.95, 0.85, 0.1, 1.0) : gfx_set(0.9, 0.9, 0.9, 1.0);
gfx_x = 32; gfx_y = seq_y + 12;
gfx_drawstr("ROLAND-STYLE 16-STEP ACID MATRIX (L-Click: + / R-Click: -)");

// Sequencer Source Button (Internal Seq vs External MIDI)
sbx = 385; sby = seq_y + 7; sbw = 128; sbh = 26;
gfx_set(0.22, 0.24, 0.28, 1.0); gfx_rect(sbx, sby, sbw, sbh);
gfx_setfont(1, "Arial", 11, 'b'); gfx_set(0.9, 0.9, 0.9, 1.0);
gfx_x = sbx + 10; gfx_y = sby + 7;
slider13 == 0 ? gfx_drawstr("SRC: INT SEQ") : gfx_drawstr("SRC: EXT MIDI");
(mouse_cap & 1) && !(last_mouse_cap & 1) ? (
  mouse_x >= sbx && mouse_x <= sbx+sbw && mouse_y >= sby && mouse_y <= sby+sbh ? (
    slider13 = 1 - slider13; slider_automate(2^12); update_dsp_params();
  );
);

// Randomize Acid Button
rbx = 522; rby = seq_y + 7; rbw = 166; rbh = 26;
gfx_set(0.8, 0.3, 0.1, 1.0);
gfx_rect(rbx, rby, rbw, rbh);
gfx_setfont(1, "Arial", 12, 'b');
gfx_set(1, 1, 1, 1);
gfx_x = rbx + 20; gfx_y = rby + 7;
gfx_drawstr("[RANDOM ACID LINE]");

(mouse_cap & 1) && !(last_mouse_cap & 1) ? (
  mouse_x >= rbx && mouse_x <= rbx+rbw && mouse_y >= rby && mouse_y <= rby+rbh ? (
    randomize_acid_pattern();
  );
);

// Draw 16 Steps
step_w = 41;
step_start_x = 32;
s_top = seq_y + 42;

i = 0;
loop(16,
  sx_pos = step_start_x + i * step_w;
  gw = step_w - 6;
  
  // Highlight currently playing step
  ((clock_mode == 1) || (play_state & 1)) && (slider13 == 0) && (current_step == i) ? (
    gfx_set(0.95, 0.85, 0.1, 0.28);
    gfx_rect(sx_pos - 2, s_top, gw + 4, 180, 1);
  );

  // Step Number Header
  gfx_setfont(1, "Arial", 11, 'b');
  acid_mode ? gfx_set(0.9, 0.85, 0.1, 1.0) : gfx_set(0.7, 0.7, 0.7, 1.0);
  sprintf(#s_num, "%d", i + 1);
  gfx_measurestr(#s_num, tw, th);
  gfx_x = sx_pos + (gw - tw) * 0.5; gfx_y = s_top + 2;
  gfx_drawstr(#s_num);

  // GATE ROW (On/Off) - Fixed centered text coordinates!
  gy = s_top + 22; gh = 28;
  seq_gate[i] ? (
    acid_mode ? gfx_set(0.2, 0.9, 0.3, 1.0) : gfx_set(0.2, 0.75, 0.95, 1.0);
  ) : (
    gfx_set(0.18, 0.19, 0.22, 1.0);
  );
  gfx_rect(sx_pos, gy, gw, gh);
  gfx_setfont(1, "Arial", 10, 'b');
  seq_gate[i] ? gfx_set(0, 0, 0, 1) : gfx_set(0.55, 0.55, 0.6, 1);
  gfx_x = sx_pos + 9; gfx_y = gy + 8;
  seq_gate[i] ? gfx_drawstr("ON") : gfx_drawstr("--");

  // NOTE TRANSPOSITION ROW (-12 to +12 semitones)
  ny = gy + 36; nh = 32;
  gfx_set(0.22, 0.23, 0.26, 1.0);
  gfx_rect(sx_pos, ny, gw, nh);
  gfx_setfont(1, "Courier New", 11, 'b');
  gfx_set(0.95, 0.95, 0.95, 1.0);
  sprintf(#n_str, "%+d", seq_note[i]);
  gfx_measurestr(#n_str, tw, th);
  gfx_x = sx_pos + (gw - tw) * 0.5; gfx_y = ny + 10;
  gfx_drawstr(#n_str);

  // ACCENT ROW
  acy = ny + 40; ach = 26;
  seq_acc[i] ? gfx_set(1.0, 0.3, 0.1, 1.0) : gfx_set(0.18, 0.19, 0.22, 1.0);
  gfx_rect(sx_pos, acy, gw, ach);
  gfx_setfont(1, "Arial", 10, 'b');
  seq_acc[i] ? gfx_set(1,1,1,1) : gfx_set(0.55,0.55,0.6,1);
  gfx_x = sx_pos + 6; gfx_y = acy + 7;
  gfx_drawstr("ACC");

  // SLIDE ROW
  sly = acy + 34; slh = 26;
  seq_slide[i] ? gfx_set(0.95, 0.82, 0.1, 1.0) : gfx_set(0.18, 0.19, 0.22, 1.0);
  gfx_rect(sx_pos, sly, gw, slh);
  gfx_setfont(1, "Arial", 10, 'b');
  seq_slide[i] ? gfx_set(0,0,0,1) : gfx_set(0.55,0.55,0.6,1);
  gfx_x = sx_pos + 6; gfx_y = sly + 7;
  gfx_drawstr("SLD");

  // Left-Click & Right-Click Handling on Sequencer Grid
  left_click = (mouse_cap & 1) && !(last_mouse_cap & 1);
  right_click = (mouse_cap & 2) && !(last_mouse_cap & 2);
  (left_click || right_click) ? (
    mx = mouse_x; my = mouse_y;
    mx >= sx_pos && mx <= sx_pos + gw ? (
      my >= gy && my <= gy + gh ? ( seq_gate[i] = 1 - seq_gate[i]; );
      my >= ny && my <= ny + nh ? ( 
        left_click ? (
          seq_note[i] += 1;
          seq_note[i] > 12 ? seq_note[i] = -12;
        ) : (
          seq_note[i] -= 1;
          seq_note[i] < -12 ? seq_note[i] = 12;
        );
      );
      my >= acy && my <= acy + ach ? ( seq_acc[i] = 1 - seq_acc[i]; );
      my >= sly && my <= sly + slh ? ( seq_slide[i] = 1 - seq_slide[i]; );
    );
  );

  i += 1;
);

// 6. BOTTOM STATUS LEDs & NON-OVERLAPPING OSCILLOSCOPE
bot_y = 512;
gfx_set(0.10, 0.11, 0.13, 1.0);
gfx_rect(20, bot_y, 680, 88);
gfx_set(0.3, 0.35, 0.4, 1.0);
gfx_rect(20, bot_y, 680, 88, 0);

ly = bot_y + 44;
gfx_setfont(1, "Arial", 12, 'b');

// Gate LED
gfx_set(0.2, 0.2, 0.2, 1.0); gfx_circle(48, ly, 8, 1);
gate ? gfx_set(0.2, 1.0, 0.3, 1.0) : gfx_set(0.08, 0.25, 0.08, 1.0);
gfx_circle(48, ly, 6, 1);
gfx_set(0.9, 0.9, 0.9, 1.0); gfx_x = 64; gfx_y = ly - 6; gfx_drawstr("GATE");

// Slide LED
gfx_set(0.2, 0.2, 0.2, 1.0); gfx_circle(135, ly, 8, 1);
is_slide ? gfx_set(1.0, 0.85, 0.1, 1.0) : gfx_set(0.25, 0.22, 0.05, 1.0);
gfx_circle(135, ly, 6, 1);
gfx_set(0.9, 0.9, 0.9, 1.0); gfx_x = 151; gfx_y = ly - 6; gfx_drawstr("SLIDE");

// C13 Accent LED
gfx_set(0.2, 0.2, 0.2, 1.0); gfx_circle(225, ly, 8, 1);
acc_glow = min(1.0, acc_cap);
gfx_set(0.25 + 0.75 * acc_glow, 0.08, 0.08, 1.0);
gfx_circle(225, ly, 6, 1);
gfx_set(0.9, 0.9, 0.9, 1.0); gfx_x = 241; gfx_y = ly - 6; gfx_drawstr("C13 ACCENT");

// Oscilloscope (Dedicated right-hand bay, zero overlap with steps!)
osc_x = 355; osc_y = bot_y + 8; osc_w = 335; osc_h = 72;
gfx_set(0.04, 0.05, 0.06, 1.0);
gfx_rect(osc_x, osc_y, osc_w, osc_h);
gfx_set(0.25, 0.3, 0.35, 1.0);
gfx_rect(osc_x, osc_y, osc_w, osc_h, 0);

acid_mode ? gfx_set(1.0, 0.9, 0.1, 1.0) : gfx_set(0.2, 0.95, 0.4, 1.0);
i = 0;
last_px = osc_x;
last_py = osc_y + osc_h * 0.5;
while (i < osc_w - 2) (
  buf_pos = (scope_idx + floor(i * (scope_len / osc_w))) % scope_len;
  samp = max(-1.0, min(1.0, scope_buf[buf_pos]));
  px = osc_x + i + 1;
  py = (osc_y + osc_h * 0.5) - (samp * (osc_h * 0.42));
  i > 0 ? gfx_line(last_px, last_py, px, py);
  last_px = px; last_py = py;
  i += 1;
);

last_mouse_cap = mouse_cap;
update_dsp_params();