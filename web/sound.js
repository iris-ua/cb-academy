/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Sound effects, synthesized with WebAudio (no files): squeak (cheese),
 * bump (collision), win and fail (challenge result). Muting is remembered. */
(function () {
  'use strict';
  var ctx = null, out = null, last = {};
  var muted = false;
  try { muted = localStorage.getItem('ciberrato.mute') === '1'; } catch (e) { /* private mode */ }

  /* browsers only start audio after a user gesture: call from a click */
  function unlock() {
    if (!ctx) {
      var AC = window.AudioContext || window.webkitAudioContext;
      if (!AC) return;
      ctx = new AC();
      out = ctx.createGain();
      out.gain.value = 0.35;
      out.connect(ctx.destination);
    }
    if (ctx.state === 'suspended') ctx.resume();
  }

  /* one oscillator note: freq sweeps f0 -> f1 over dur seconds, starting at t */
  function tone(type, f0, f1, t, dur, vol) {
    var o = ctx.createOscillator(), g = ctx.createGain();
    o.type = type;
    o.frequency.setValueAtTime(f0, t);
    o.frequency.exponentialRampToValueAtTime(f1, t + dur);
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(vol, t + 0.01);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    o.connect(g);
    g.connect(out);
    o.start(t);
    o.stop(t + dur + 0.02);
  }

  function noise(t, dur, vol, cutoff) {
    var n = Math.floor(ctx.sampleRate * dur), buf = ctx.createBuffer(1, n, ctx.sampleRate), d = buf.getChannelData(0);
    for (var i = 0; i < n; i++) d[i] = (Math.random() * 2 - 1) * (1 - i / n);
    var src = ctx.createBufferSource(), f = ctx.createBiquadFilter(), g = ctx.createGain();
    src.buffer = buf;
    f.type = 'lowpass';
    f.frequency.value = cutoff;
    g.gain.value = vol;
    src.connect(f);
    f.connect(g);
    g.connect(out);
    src.start(t);
  }

  var SOUNDS = {
    squeak: function (t) {
      tone('sine', 1700, 2600, t, 0.07, 0.5);
      tone('sine', 1900, 3000, t + 0.09, 0.08, 0.45);
    },
    bump: function (t) {
      tone('sine', 150, 55, t, 0.14, 0.9);
      noise(t, 0.08, 0.5, 900);
    },
    win: function (t) {
      [523.25, 659.25, 783.99, 1046.5].forEach(function (f, i) { tone('triangle', f, f, t + i * 0.1, i === 3 ? 0.45 : 0.14, 0.5); });
    },
    fail: function (t) {
      tone('triangle', 392, 370, t, 0.18, 0.45);
      tone('triangle', 311, 262, t + 0.2, 0.35, 0.45);
    }
  };

  function play(name) {
    if (muted || !ctx || ctx.state !== 'running' || !SOUNDS[name]) return;
    var now = ctx.currentTime;
    if (last[name] && now - last[name] < 0.09) return; /* fast-forward floods */
    last[name] = now;
    SOUNDS[name](now + 0.005);
  }

  function setMuted(m) {
    muted = !!m;
    try { localStorage.setItem('ciberrato.mute', muted ? '1' : ''); } catch (e) { /* private mode */ }
  }

  window.Sfx = { unlock: unlock, play: play, setMuted: setMuted, get muted() { return muted; } };
})();
