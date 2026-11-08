/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Simple level editor: draw walls, place cheese, the light and the start pose
 * on a grid, then export the level JSON (format: src/sim/level.h) or play it.
 * LevelEdit.open({ current: <level JSON text or null>, onPlay: fn(jsonText) }). */
(function () {
  'use strict';
  var t = function () { return I18N.t.apply(null, arguments); };
  var SNAP = 0.5, THIN = 0.2, WALL_H = 2, TALL_H = 4, BEACON_H = 3, MAX_TARGETS = 8;
  var DRAFT = 'ciberrato.editor';

  var root, cv, g, opts = {}, lv, tool = 'wall', tall = false, undo = [];
  var drag = null, hover = null, cam = { s: 1, ox: 0, oy: 0 };

  /* ---- model --------------------------------------------------------------- */
  function blank() {
    return { base: {}, name: t('ed.default'), width: 14, height: 8, walls: [], targets: [], beacons: [],
             start: [{ x: 2, y: 4, dir: 0 }] };
  }
  function fromJSON(d) {
    var m = blank(), keep = {};
    Object.keys(d).forEach(function (k) {
      if (['name', 'width', 'height', 'walls', 'targets', 'beacons', 'start'].indexOf(k) < 0) keep[k] = d[k];
    });
    m.base = keep;
    m.name = d.name || m.name;
    m.width = +d.width || m.width;
    m.height = +d.height || m.height;
    m.walls = (d.walls || []).map(function (w) { return { height: +w.height || WALL_H, corners: w.corners.map(function (p) { return [+p[0], +p[1]]; }) }; });
    m.targets = (d.targets || []).map(function (p) { return { x: +p.x, y: +p.y, radius: +p.radius || 1 }; });
    m.beacons = (d.beacons || []).map(function (p) { return { x: +p.x, y: +p.y, height: +p.height || BEACON_H }; });
    m.start = (d.start || []).map(function (p) { return { x: +p.x, y: +p.y, dir: +p.dir || 0 }; });
    return m;
  }
  function r2(v) { return Math.round(v * 100) / 100; }
  function toJSON(m) {
    var d = Object.assign({ name: m.name, width: m.width, height: m.height }, m.base, {
      name: m.name, width: m.width, height: m.height,
      beacons: m.beacons.map(function (b) { return { x: r2(b.x), y: r2(b.y), height: b.height }; }),
      targets: m.targets.map(function (p) { return { x: r2(p.x), y: r2(p.y), radius: p.radius }; }),
      start: m.start.map(function (p) { return { x: r2(p.x), y: r2(p.y), dir: Math.round(p.dir) }; }),
      walls: m.walls.map(function (w) { return { height: w.height, corners: w.corners.map(function (c) { return [r2(c[0]), r2(c[1])]; }) }; })
    });
    if (!d.time_limit && d.time_limit !== 0) d.time_limit = 0;
    /* one wall per line keeps the file readable */
    return JSON.stringify(d, null, 2).replace(/\[\s+(-?[\d.]+),\s+(-?[\d.]+)\s+\]/g, '[$1, $2]')
      .replace(/\{\s+("x"[^{}]*?)\s+\}/g, function (m, body) { return '{ ' + body.replace(/\s*\n\s*/g, ' ') + ' }'; });
  }
  function saveDraft() { try { localStorage.setItem(DRAFT, toJSON(lv)); } catch (e) { /* private mode */ } }
  function push() { undo.push(toJSON(lv)); if (undo.length > 100) undo.shift(); }
  function change(fn) { push(); fn(); saveDraft(); syncForm(); draw(); }

  /* ---- geometry ------------------------------------------------------------ */
  function snap(v) { return Math.round(v / SNAP) * SNAP; }
  function toWorld(e) {
    var r = cv.getBoundingClientRect();
    return { x: (e.clientX - r.left - cam.ox) / cam.s, y: lv.height - (e.clientY - r.top - cam.oy) / cam.s };
  }
  function clampPt(p) { return { x: Math.max(0, Math.min(lv.width, snap(p.x))), y: Math.max(0, Math.min(lv.height, snap(p.y))) }; }
  function inside(pts, p) {
    var c = false;
    for (var i = 0, j = pts.length - 1; i < pts.length; j = i++) {
      var a = pts[i], b = pts[j];
      if ((a[1] > p.y) !== (b[1] > p.y) && p.x < (b[0] - a[0]) * (p.y - a[1]) / (b[1] - a[1]) + a[0]) c = !c;
    }
    return c;
  }
  /* a drag from a to b: a rectangle, or a thin wall along one axis */
  function wallRect(a, b) {
    var x0 = Math.min(a.x, b.x), x1 = Math.max(a.x, b.x), y0 = Math.min(a.y, b.y), y1 = Math.max(a.y, b.y);
    if (x1 - x0 < 1e-6 && y1 - y0 < 1e-6) return null;
    if (x1 - x0 < 1e-6) { x0 -= THIN / 2; x1 += THIN / 2; }
    if (y1 - y0 < 1e-6) { y0 -= THIN / 2; y1 += THIN / 2; }
    return [[x0, y0], [x1, y0], [x1, y1], [x0, y1]];
  }
  function hit(p) {
    for (var i = lv.targets.length - 1; i >= 0; i--) {
      var q = lv.targets[i];
      if (Math.hypot(p.x - q.x, p.y - q.y) <= q.radius) return { list: lv.targets, i: i };
    }
    for (i = lv.beacons.length - 1; i >= 0; i--)
      if (Math.hypot(p.x - lv.beacons[i].x, p.y - lv.beacons[i].y) <= 0.5) return { list: lv.beacons, i: i };
    for (i = lv.walls.length - 1; i >= 0; i--) if (inside(lv.walls[i].corners, p)) return { list: lv.walls, i: i };
    return null;
  }

  /* ---- drawing ------------------------------------------------------------- */
  function sx(x) { return cam.ox + x * cam.s; }
  function sy(y) { return cam.oy + (lv.height - y) * cam.s; }

  function draw() {
    if (!cv || root.hidden) return;
    var dpr = window.devicePixelRatio || 1, w = cv.clientWidth, h = cv.clientHeight;
    if (cv.width !== Math.round(w * dpr) || cv.height !== Math.round(h * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, w, h);
    var pad = 24;
    cam.s = Math.max(4, Math.min((w - 2 * pad) / lv.width, (h - 2 * pad) / lv.height));
    cam.ox = (w - lv.width * cam.s) / 2;
    cam.oy = (h - lv.height * cam.s) / 2;

    g.fillStyle = '#1b1f2a';
    g.fillRect(sx(0), sy(lv.height), lv.width * cam.s, lv.height * cam.s);
    g.lineWidth = 1;
    for (var x = 0; x <= lv.width; x++) {
      g.strokeStyle = x % 2 ? '#232836' : '#2b3142';
      g.beginPath(); g.moveTo(sx(x) + 0.5, sy(0)); g.lineTo(sx(x) + 0.5, sy(lv.height)); g.stroke();
    }
    for (var y = 0; y <= lv.height; y++) {
      g.strokeStyle = y % 2 ? '#232836' : '#2b3142';
      g.beginPath(); g.moveTo(sx(0), sy(y) + 0.5); g.lineTo(sx(lv.width), sy(y) + 0.5); g.stroke();
    }
    g.strokeStyle = '#4a5170';
    g.lineWidth = 2;
    g.strokeRect(sx(0), sy(lv.height), lv.width * cam.s, lv.height * cam.s);

    var maxB = lv.beacons.reduce(function (m, b) { return Math.max(m, b.height); }, 0);
    lv.walls.forEach(function (wl) { poly(wl.corners, maxB && wl.height >= maxB ? '#8d93ad' : '#5f6684'); });
    lv.targets.forEach(function (p, i) {
      disc(p.x, p.y, p.radius, 'rgba(255,200,70,.22)', '#ffc846');
      label(String(i), p.x, p.y, '#ffc846');
    });
    lv.beacons.forEach(function (b) {
      disc(b.x, b.y, 0.35, '#fff3c4', '#ffc846');
      g.strokeStyle = 'rgba(255,200,70,.5)';
      g.lineWidth = 1.5;
      for (var k = 1; k <= 2; k++) { g.beginPath(); g.arc(sx(b.x), sy(b.y), cam.s * 0.35 * (1 + k * 0.7), 0, 7); g.stroke(); }
    });
    lv.start.forEach(function (p) { mouse(p.x, p.y, p.dir); });

    if (drag && drag.tool === 'wall' && drag.b) {
      var c = wallRect(drag.a, drag.b);
      if (c) poly(c, 'rgba(110,190,240,.55)');
    } else if (hover && !drag) {
      g.fillStyle = 'rgba(110,190,240,.9)';
      g.beginPath(); g.arc(sx(hover.x), sy(hover.y), 3, 0, 7); g.fill();
    }
  }
  function poly(pts, fill) {
    g.fillStyle = fill;
    g.beginPath();
    pts.forEach(function (p, i) { if (i) g.lineTo(sx(p[0]), sy(p[1])); else g.moveTo(sx(p[0]), sy(p[1])); });
    g.closePath();
    g.fill();
  }
  function disc(x, y, r, fill, stroke) {
    g.beginPath(); g.arc(sx(x), sy(y), r * cam.s, 0, 7);
    g.fillStyle = fill; g.fill();
    g.strokeStyle = stroke; g.lineWidth = 2; g.stroke();
  }
  function label(s, x, y, col) {
    g.fillStyle = col;
    g.font = '800 ' + Math.max(10, cam.s * 0.5) + 'px Nunito, sans-serif';
    g.textAlign = 'center'; g.textBaseline = 'middle';
    g.fillText(s, sx(x), sy(y));
  }
  function mouse(x, y, dir) {
    var a = -dir * Math.PI / 180, r = 0.5 * cam.s;
    g.save();
    g.translate(sx(x), sy(y));
    g.rotate(a);
    g.fillStyle = '#c9cede';
    g.beginPath(); g.arc(0, 0, r, 0, 7); g.fill();
    g.fillStyle = '#6ebef0';
    g.beginPath(); g.moveTo(r * 1.15, 0); g.lineTo(r * 0.2, -r * 0.55); g.lineTo(r * 0.2, r * 0.55); g.closePath(); g.fill();
    g.restore();
  }

  /* ---- input --------------------------------------------------------------- */
  function onDown(e) {
    if (e.button === 2) { erase(toWorld(e)); return; }
    if (e.button !== 0) return;
    var p = clampPt(toWorld(e));
    cv.setPointerCapture(e.pointerId);
    if (tool === 'wall') drag = { tool: 'wall', a: p, b: p };
    else if (tool === 'start') { push(); drag = { tool: 'start' }; lv.start = [{ x: p.x, y: p.y, dir: lv.start[0] ? lv.start[0].dir : 0 }]; draw(); }
    else if (tool === 'cheese') change(function () {
      if (lv.targets.length >= MAX_TARGETS) lv.targets.shift();
      lv.targets.push({ x: p.x, y: p.y, radius: 1 });
    });
    else if (tool === 'beacon') change(function () { lv.beacons = [{ x: p.x, y: p.y, height: BEACON_H }]; });
    else if (tool === 'erase') erase(toWorld(e));
  }
  function onMove(e) {
    var raw = toWorld(e), p = clampPt(raw);
    hover = p;
    if (drag && drag.tool === 'wall') drag.b = p;
    else if (drag && drag.tool === 'start' && lv.start[0]) {
      var s = lv.start[0], dx = raw.x - s.x, dy = raw.y - s.y;
      if (Math.hypot(dx, dy) > 0.3) s.dir = Math.round(Math.atan2(dy, dx) * 180 / Math.PI / 45) * 45;
      if (s.dir === -180) s.dir = 180;
    }
    draw();
  }
  function onUp() {
    if (drag && drag.tool === 'wall') {
      var c = wallRect(drag.a, drag.b);
      if (c) change(function () { lv.walls.push({ height: tall ? TALL_H : WALL_H, corners: c }); });
    } else if (drag && drag.tool === 'start') saveDraft();
    drag = null;
    draw();
  }
  function erase(p) {
    var h = hit(p);
    if (h) change(function () { h.list.splice(h.i, 1); });
  }
  function onKey(e) {
    if (e.key === 'Escape') { close(); return; }
    if (e.target.tagName === 'INPUT') return;
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z') {
      e.preventDefault();
      if (undo.length) { lv = fromJSON(JSON.parse(undo.pop())); saveDraft(); syncForm(); draw(); }
    }
  }

  /* ---- UI ------------------------------------------------------------------ */
  var TOOLS = ['wall', 'cheese', 'beacon', 'start', 'erase'];
  function build() {
    root = document.createElement('div');
    root.id = 'lvled';
    root.className = 'ui';
    root.hidden = true;
    root.innerHTML =
      '<div class="sheet">' +
      '<div class="bar"><span class="brand" data-i18n="ed.title"></span>' +
      '<label class="dim"><span data-i18n="ed.name"></span> <input id="ed-name" maxlength="60"></label>' +
      '<label class="dim"><span data-i18n="ed.size"></span> <input id="ed-w" type="number" min="4" max="40"> × <input id="ed-h" type="number" min="4" max="30"></label>' +
      '<span class="grow"></span><button id="ed-close" class="ghost" data-i18n-title="close">✕</button></div>' +
      '<div class="bar tools"><span class="seg" id="ed-tools">' +
      TOOLS.map(function (k) { return '<button data-tool="' + k + '" data-i18n="ed.' + k + '" data-i18n-title="ed.' + k + '.title"></button>'; }).join('') +
      '</span><label class="dim" data-i18n-title="ed.tall.title"><input id="ed-tall" type="checkbox"> <span data-i18n="ed.tall"></span></label>' +
      '<span class="grow"></span>' +
      '<button id="ed-new" class="ghost" data-i18n="ed.new"></button>' +
      '<button id="ed-current" class="ghost" data-i18n="ed.current" data-i18n-title="ed.current.title"></button>' +
      '<button id="ed-open" class="ghost" data-i18n="ed.open" data-i18n-title="ed.open.title"></button>' +
      '<input id="ed-file" type="file" accept=".json,application/json" hidden>' +
      '<button id="ed-export" data-i18n="ed.export" data-i18n-title="ed.export.title"></button>' +
      '<button id="ed-play" class="primary" data-i18n="ed.play" data-i18n-title="ed.play.title"></button></div>' +
      '<canvas id="ed-canvas"></canvas>' +
      '<div class="bar foot dim" data-i18n="ed.help"></div>' +
      '</div>';
    document.body.appendChild(root);
    var $ = function (id) { return root.querySelector('#' + id); };
    cv = $('ed-canvas');
    g = cv.getContext('2d');
    cv.addEventListener('pointerdown', onDown);
    cv.addEventListener('pointermove', onMove);
    cv.addEventListener('pointerup', onUp);
    cv.addEventListener('pointerleave', function () { hover = null; draw(); });
    cv.addEventListener('contextmenu', function (e) { e.preventDefault(); });
    root.addEventListener('keydown', onKey);
    root.querySelectorAll('[data-tool]').forEach(function (b) { b.onclick = function () { setTool(b.dataset.tool); }; });
    $('ed-tall').onchange = function () { tall = this.checked; };
    $('ed-name').oninput = function () { lv.name = this.value; saveDraft(); };
    $('ed-w').onchange = function () { var v = Math.max(4, Math.min(40, Math.round(+this.value) || lv.width)); change(function () { lv.width = v; }); };
    $('ed-h').onchange = function () { var v = Math.max(4, Math.min(30, Math.round(+this.value) || lv.height)); change(function () { lv.height = v; }); };
    $('ed-close').onclick = close;
    $('ed-new').onclick = function () { if (confirm(t('ed.new.confirm'))) change(function () { lv = blank(); }); };
    $('ed-current').onclick = function () { if (opts.current) loadText(opts.current); };
    $('ed-open').onclick = function () { $('ed-file').click(); };
    $('ed-file').onchange = function () {
      var f = this.files[0];
      this.value = '';
      if (f) f.text().then(loadText);
    };
    $('ed-export').onclick = function () {
      var a = document.createElement('a');
      a.href = URL.createObjectURL(new Blob([toJSON(lv) + '\n'], { type: 'application/json' }));
      a.download = (lv.name || 'level').toLowerCase().normalize('NFD').replace(/[^\w]+/g, '-').replace(/^-|-$/g, '') + '.json';
      a.click();
      setTimeout(function () { URL.revokeObjectURL(a.href); }, 1000);
    };
    $('ed-play').onclick = function () {
      if (!lv.start.length) { alert(t('ed.nostart')); setTool('start'); return; }
      close();
      if (opts.onPlay) opts.onPlay(toJSON(lv));
    };
    new ResizeObserver(draw).observe(cv);
  }
  function loadText(text) {
    try {
      var d = JSON.parse(text);
      if (d.maze) { alert(t('ed.current.maze')); return; }
      if (!(+d.width > 0 && +d.height > 0)) throw new Error('width/height');
      change(function () { lv = fromJSON(d); });
    } catch (e) { alert(t('loadlevel.fail', e.message)); }
  }
  function setTool(k) {
    tool = k;
    root.querySelectorAll('[data-tool]').forEach(function (b) { b.classList.toggle('on', b.dataset.tool === k); });
  }
  function syncForm() {
    root.querySelector('#ed-name').value = lv.name;
    root.querySelector('#ed-w').value = lv.width;
    root.querySelector('#ed-h').value = lv.height;
  }

  function open(o) {
    opts = o || {};
    if (!root) build();
    if (!lv) {
      var d = null;
      try { d = JSON.parse(localStorage.getItem(DRAFT) || 'null'); } catch (e) { /* none */ }
      lv = d ? fromJSON(d) : blank();
    }
    I18N.apply(root);
    var cur = null;
    try { cur = opts.current && JSON.parse(opts.current); } catch (e) { /* none */ }
    root.querySelector('#ed-current').hidden = !cur || !!cur.maze;
    setTool(tool);
    syncForm();
    root.hidden = false;
    draw();
    root.querySelector('[data-tool="' + tool + '"]').focus();
  }
  function close() { root.hidden = true; }

  window.LevelEdit = { open: open };
})();
