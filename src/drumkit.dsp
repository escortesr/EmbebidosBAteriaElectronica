declare options "[osc:on]";
import("stdfaust.lib");

// Gate inputs for each drum
kickGate = button("Kick");
bassGate = button("BassTom");
medGate = button("MedTom");
highGate = button("HighTom");
snareGate = button("Snare");
hihatGate = button("Hihat");
hihatOpenGate = button("OpenHihat");
crashGate = button("Crash");

// KICK DRUM (your original code)
k_freq = 40;
k_decay = 0.7;
k_click_decay = 0.05;
k_envelope = kickGate : en.ar(0.001, k_decay);
k_freq_sweep = k_freq * (1+k_envelope);
k_main_osc = os.osc(k_freq_sweep) * k_envelope;
k_click_env = kickGate : en.ar(0.0001, k_click_decay);
k_click_osc = no.noise * k_click_env * 0.5;
k_click_filtered = k_click_osc : fi.highpass(1, 1000);
k_main_filtered = k_main_osc : fi.lowpass(2, 150);
kickModel = (1.5*k_main_filtered + k_click_filtered);

// BASS TOM
bt_freq = 85;
bt_decay = 0.8;
bt_click_decay = 0.03;
bt_envelope = bassGate : en.ar(0.002, bt_decay);
bt_freq_sweep = bt_freq * (1 + 2 * bt_envelope);
bt_main_osc = os.osc(bt_freq_sweep) * bt_envelope;
bt_click_env = bassGate : en.ar(0.0001, bt_click_decay);
bt_click_osc = no.noise * bt_click_env * 0.3;
bt_click_filtered = bt_click_osc : fi.highpass(1, 800);
bt_main_filtered = bt_main_osc : fi.lowpass(2, 200);
bassTomModel = (bt_main_filtered + bt_click_filtered);

// MEDIUM TOM
mt_freq = 130;
mt_decay = 0.6;
mt_click_decay = 0.025;
mt_envelope = medGate : en.ar(0.002, mt_decay);
mt_freq_sweep = mt_freq * (1 + 1.5 * mt_envelope);
mt_main_osc = os.osc(mt_freq_sweep) * mt_envelope;
mt_click_env = medGate : en.ar(0.0001, mt_click_decay);
mt_click_osc = no.noise * mt_click_env * 0.25;
mt_click_filtered = mt_click_osc : fi.highpass(1, 1200);
mt_main_filtered = mt_main_osc : fi.lowpass(2, 300);
medTomModel = (mt_main_filtered + mt_click_filtered);

// HIGH TOM
ht_freq = 200;
ht_decay = 0.4;
ht_click_decay = 0.02;
ht_envelope = highGate : en.ar(0.001, ht_decay);
ht_freq_sweep = ht_freq * (1 + 1.2 * ht_envelope);
ht_main_osc = os.osc(ht_freq_sweep) * ht_envelope;
ht_click_env = highGate : en.ar(0.0001, ht_click_decay);
ht_click_osc = no.noise * ht_click_env * 0.2;
ht_click_filtered = ht_click_osc : fi.highpass(1, 1500);
ht_main_filtered = ht_main_osc : fi.lowpass(2, 400);
highTomModel = (ht_main_filtered + ht_click_filtered);

// SNARE DRUM
s_freq = 220;
s_decay = 0.15;
s_noise_decay = 0.12;
s_envelope = snareGate : en.ar(0.001, s_decay);
s_freq_sweep = s_freq * (1 + 0.5 * s_envelope);
s_tone_osc = os.osc(s_freq_sweep) * s_envelope * 0.3;
s_noise_env = snareGate : en.ar(0.001, s_noise_decay);
s_noise = no.noise * s_noise_env * 0.8;
s_noise_filtered = s_noise : fi.bandpass(2, 800, 3000);
s_tone_filtered = s_tone_osc : fi.lowpass(2, 500);
snareModel = (s_tone_filtered + s_noise_filtered);

// HIHAT
h_decay = 0.08;
h_envelope = hihatGate : en.ar(0.0001, h_decay);
h_noise = no.noise * h_envelope;
h_filtered = h_noise : fi.highpass(2, 8000) : fi.lowpass(2, 15000);
hihatModel = h_filtered;

hihat_open_decay = 0.5;
hihat_open_envelope = hihatOpenGate : en.ar(0.001, hihat_open_decay);
hihat_open_noise = no.noise * hihat_open_envelope;
hihat_open_filtered = hihat_open_noise : fi.highpass(2, 6000) : fi.lowpass(2, 15000);
hihatOpenModel = hihat_open_filtered;

// RIDE CYMBAL (based on the synthesis image)
r_decay = 2.5;
r_envelope = crashGate : en.ar(0.005, r_decay);
r_fast_env = crashGate : en.ar(0.001, 0.1);

// Multiple oscillators with harmonic relationships (like in the image)

crash_decay = 1;
crash_envelope = crashGate : en.ar(0.002, crash_decay);
crash_noise = no.noise * crash_envelope;
crash_filtered = crash_noise : fi.highpass(2, 3000) : fi.lowpass(1, 18000);
crash_sound = crash_filtered * 0.6;

// Combine components
crashModel = crash_sound;

// MIX ALL DRUMS
drumKit = kickModel + bassTomModel + medTomModel + highTomModel + snareModel + hihatModel + hihatOpenModel + crashModel;

// OUTPUT
process = drumKit <: _, _;