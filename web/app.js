/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CiberRato Academy web page: CodeMirror editor, console, API panel and the bridge
 * to the WASM sim (src/main.c web_* exports, events via Module.onEvent). */
(function () {
  'use strict';
  var $ = function (id) { return document.getElementById(id); };
  var t = I18N.t;

  /* ---- robot API: one table feeds autocomplete and the cheat-sheet --------- */
  var API = [
    { group: 'Move', items: [
      ['forward(n)', 'forward(${n})'],
      ['turn(deg)', 'turn(${deg})'],
      ['turn_to(deg)', 'turn_to(${deg})'],
      ['motors(l, r)', 'motors(${l}, ${r})'],
      ['wait(n)', 'wait(${n})'],
      ['finish()', 'finish()'],
    ] },
    { group: 'Sense', items: [
      ['ir(i)', 'ir(${0})'],
      ['compass()', 'compass()'],
      ['beacon()', 'beacon()'],
      ['ground()', 'ground()'],
      ['bump()', 'bump()'],
      ['time()', 'time()'],
    ] },
    { group: 'Show', items: [
      ['say(msg)', 'say(${msg})'],
      ['led(name, on)', 'led("${visiting}", ${true})'],
    ] },
  ];
  var LUA_WORDS = ('and break do else elseif end false for function if in local nil not or repeat return ' +
                   'then true until while').split(' ');
  var MATH = ['abs', 'floor', 'ceil', 'min', 'max', 'sqrt', 'random', 'pi', 'huge', 'sin', 'cos', 'atan'];

  function starter() { return t('starter'); } /* free play */
  function apiDesc(it) { return t('api.' + it[0].replace(/\(.*/, '')); }

  /* ---- per-viewer conveniences (may be unavailable) ------------------------ */
  function load(key, def) { try { var v = localStorage.getItem(key); return v == null ? def : v; } catch (e) { return def; } }
  function save(key, v) { try { localStorage.setItem(key, v); } catch (e) { /* private mode */ } }

  /* ---- editor -------------------------------------------------------------- */
  var view, setErr, setDoc, saveCode;
  var codeKey = 'ciberrato.code'; /* where the editor saves: free play or one challenge */

  function apiCompletions() {
    var opts = [];
    API.forEach(function (g) {
      g.items.forEach(function (it) {
        opts.push(CM.snippetCompletion(it[1], { label: it[0].replace(/\(.*/, ''), detail: it[0].replace(/^\w+/, ''),
                                                info: apiDesc(it), type: 'function', boost: 2 }));
      });
    });
    LUA_WORDS.forEach(function (w) { opts.push({ label: w, type: 'keyword' }); });
    MATH.forEach(function (m) { opts.push({ label: 'math.' + m, type: m === 'pi' || m === 'huge' ? 'constant' : 'function' }); });
    return opts;
  }

  function makeEditor() {
    var opts, optsLang;
    function complete(ctx) {
      if (optsLang !== I18N.lang) { opts = apiCompletions(); optsLang = I18N.lang; } /* descriptions */
      var w = ctx.matchBefore(/[\w.]+/);
      if (!w || (w.from === w.to && !ctx.explicit)) return null;
      return { from: w.from, options: opts, validFor: /^[\w.]*$/ };
    }
    var errEffect = CM.StateEffect.define();
    var errField = CM.StateField.define({
      create: function () { return CM.Decoration.none; },
      update: function (deco, tr) {
        if (tr.docChanged) deco = CM.Decoration.none;
        tr.effects.forEach(function (e) {
          if (!e.is(errEffect)) return;
          var n = e.value;
          deco = n > 0 && n <= tr.state.doc.lines
            ? CM.Decoration.set([CM.Decoration.line({ class: 'cm-err-line' }).range(tr.state.doc.line(n).from)])
            : CM.Decoration.none;
        });
        return deco;
      },
      provide: function (f) { return CM.EditorView.decorations.from(f); }
    });
    setErr = function (line, focus) {
      var fx = [errEffect.of(line)];
      if (line > 0 && line <= view.state.doc.lines) {
        var pos = view.state.doc.line(line).from;
        if (focus) view.dispatch({ effects: fx.concat(CM.EditorView.scrollIntoView(pos, { y: 'center' })), selection: { anchor: pos } });
        else view.dispatch({ effects: fx });
        if (focus) view.focus();
      } else view.dispatch({ effects: fx });
    };

    var saveTimer;
    saveCode = function () { clearTimeout(saveTimer); save(codeKey, view.state.doc.toString()); };
    var extensions = [
      CM.keymap.of([{ key: 'Mod-Enter', run: function () { run(); return true; } }, CM.indentWithTab]),
      CM.basicSetup,
      CM.StreamLanguage.define(CM.lua),
      CM.EditorState.languageData.of(function () { return [{ autocomplete: complete }]; }),
      CM.EditorState.tabSize.of(2),
      CM.oneDark,
      CM.EditorView.theme({ '&': { backgroundColor: '#13161e' }, '.cm-gutters': { backgroundColor: '#13161e' } }, { dark: true }),
      errField,
      CM.EditorView.updateListener.of(function (u) {
        if (!u.docChanged) return;
        clearTimeout(saveTimer);
        saveTimer = setTimeout(saveCode, 400);
      })
    ];
    view = new CM.EditorView({ parent: $('editor'), state: CM.EditorState.create({ doc: load(codeKey, starter()), extensions: extensions }) });
    /* show another program (its own storage slot, fresh undo history); keep: don't save the old one */
    setDoc = function (key, code, keep) {
      if (!keep) saveCode();
      clearTimeout(saveTimer);
      codeKey = key;
      view.setState(CM.EditorState.create({ doc: code, extensions: extensions }));
    };
  }

  function insertAtCursor(text) {
    var sel = view.state.selection.main;
    view.dispatch({ changes: { from: sel.from, to: sel.to, insert: text }, selection: { anchor: sel.from + text.length } });
    view.focus();
  }

  /* ---- console ------------------------------------------------------------- */
  var MAX_LINES = 500;
  function log(text, cls, line) {
    var box = $('console');
    var stick = box.scrollTop + box.clientHeight >= box.scrollHeight - 4;
    var el = document.createElement('div');
    if (cls) el.className = cls;
    var m = line > 0 && /^Line \d+/.exec(text);
    if (m) {
      var a = document.createElement('a');
      a.textContent = m[0];
      a.title = t('line.link');
      a.onclick = function () { setErr(line, true); };
      el.appendChild(a);
      el.appendChild(document.createTextNode(text.slice(m[0].length)));
    } else el.textContent = text;
    box.appendChild(el);
    while (box.childNodes.length > MAX_LINES) box.removeChild(box.firstChild);
    if (stick) box.scrollTop = box.scrollHeight;
  }

  /* ---- sim bridge ---------------------------------------------------------- */
  var ready = false;
  function call(fn, args) {
    if (!ready) return;
    var types = (args || []).map(function (a) { return typeof a === 'number' ? 'number' : 'string'; });
    return Module.ccall(fn, 'number', types, args || []);
  }

  function run() {
    if (!ready) return;
    setErr(0);
    hideResult();
    log(t('log.run'), 'sys');
    call('web_run', [view.state.doc.toString()]);
  }

  /* Run button: Run (fresh start) -> Pause -> Resume -> Pause ... until the script ends or Reset. */
  var runState = 'idle', paused = false, lastResult = null;
  function updateRunButton() {
    var b = $('run'), live = runState === 'running';
    var k = !live ? 'run' : paused ? 'resume' : 'pause';
    b.textContent = t(k);
    b.title = t(k + '.title');
    b.classList.toggle('primary', !live || paused);
    var pill = $('state'), shown = live && paused ? 'paused' : runState;
    pill.textContent = t('state.' + shown);
    pill.className = 'pill ' + shown;
  }
  function runButton() {
    Sfx.unlock();
    if (runState !== 'running') run();
    else call('web_pause', [paused ? 0 : 1]);
  }

  function setState(st) {
    runState = st;
    updateRunButton();
  }

  function setSpeed(n) {
    document.querySelectorAll('#speed button').forEach(function (b) { b.classList.toggle('on', +b.dataset.speed === n); });
  }

  Module.onEvent = function (kind, text, n) {
    switch (kind) {
    case 'print': log(text); break;
    case 'error': log(text, 'err', n); if (n > 0) setErr(n); break;
    case 'state': setState(text); if (text === 'done') log(t('log.finished'), 'sys'); break;
    case 'speed': setSpeed(parseFloat(text)); break;
    case 'paused': paused = !!n; updateRunButton(); break;
    case 'level': if (ready) log(t('log.level', text), 'sys'); break;
    case 'finished': if (!current || !current.ch) log(t('log.over', n), 'win'); break;
    case 'result': lastResult = JSON.parse(text); showResult(lastResult); Sfx.play(lastResult.win ? 'win' : 'fail'); break;
    case 'sound': Sfx.play(text); break;
    case 'ready': /* main() is set up: window, level, robot */
      ready = true;
      call('web_set_lang', [I18N.lang]);
      initStages();
      log(t('log.ready'), 'sys');
      break;
    }
  };

  /* ---- stages: challenges (/challenges, in file order), free-play levels
   * (/levels), both preloaded in the WASM file system, and the user's own
   * levels (made in the editor or loaded in teacher mode, kept in localStorage).
   * Progress and code per challenge live in localStorage. ------------------ */
  var challenges = [], levels = [], mine = [], current = null;
  var PROGRESS = 'ciberrato.progress', MINE = 'ciberrato.mylevels', TEACHER = 'ciberrato.teacher';
  var teacher = load(TEACHER, '') === '1';

  function readText(path) { return Module.FS.readFile(path, { encoding: 'utf8' }); }
  function jsonFiles(dir) {
    try { return Module.FS.readdir(dir).filter(function (f) { return /\.json$/.test(f); }).sort(); }
    catch (e) { return []; }
  }
  function lines(v) { return Array.isArray(v) ? v.join('\n') : v == null ? '' : String(v); }
  function secs(ticks) { return (ticks * 0.05).toFixed(1) + ' s'; }

  var progress = (function () { try { return JSON.parse(load(PROGRESS, '{}')) || {}; } catch (e) { return {}; } })();
  function saveProgress() { save(PROGRESS, JSON.stringify(progress)); }
  function unlocked(c) { return teacher || c.index === 0 || !!progress[challenges[c.index - 1].id]; }
  function codeKeyOf(st) { return st.ch ? 'ciberrato.code.' + st.id : st.custom ? 'ciberrato.code.my.' + st.id : 'ciberrato.code'; }
  function stageKey(st) { return (st.ch ? 'ch:' : st.custom ? 'cu:' : 'lv:') + st.id; }
  function starterOf(st) { return st.ch ? (st.starters[I18N.lang] || st.starters.en) : starter(); }

  /* text shown for a stage, in the page language */
  function localize(st) {
    var d = st.d;
    st.title = I18N.pick(d, 'title') || I18N.pick(d, 'name') || (st.custom ? t('custom') : st.id);
    st.goal = lines(I18N.pick(d, 'goal'));
    st.hints = [].concat(I18N.pick(d, 'hints') || []);
  }
  /* level JSON for the sim, with the shown name */
  function simSource(st) { return Object.keys(st.d).length ? JSON.stringify(Object.assign({}, st.d, { name: st.title })) : st.src; }

  /* tiny markdown: paragraphs, "- " lists, `code`, **bold**, *italic* */
  function esc(t) { return t.replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); }
  function inline(t) {
    return esc(t).replace(/`([^`]+)`/g, '<code>$1</code>').replace(/\*\*([^*]+)\*\*/g, '<b>$1</b>').replace(/\*([^*]+)\*/g, '<i>$1</i>');
  }
  function md(src) {
    var out = [], items = null;
    src.split('\n').forEach(function (ln) {
      var m = /^\s*[-*]\s+(.*)/.exec(ln);
      if (m) { (items = items || []).push('<li>' + inline(m[1]) + '</li>'); return; }
      if (items) { out.push('<ul>' + items.join('') + '</ul>'); items = null; }
      if (ln.trim()) out.push('<p>' + inline(ln) + '</p>');
    });
    if (items) out.push('<ul>' + items.join('') + '</ul>');
    return out.join('');
  }
  function stars(n) { return '★★★'.slice(0, n) + '<span class="off">' + '★★★'.slice(n) + '</span>'; }

  function makeCustom(src, id) {
    var st = { ch: false, custom: true, id: id, src: src, d: JSON.parse(src) };
    localize(st);
    return st;
  }
  function saveMine() { save(MINE, JSON.stringify(mine.map(function (st) { return { id: st.id, src: st.src }; }))); }
  function mineById(id) { return mine.filter(function (st) { return st.id === id; })[0]; }
  function loadMine(list) {
    mine = [];
    (Array.isArray(list) ? list : []).forEach(function (e) {
      try { if (e && typeof e.id === 'string' && typeof e.src === 'string') mine.push(makeCustom(e.src, e.id)); } catch (x) { /* skip broken */ }
    });
  }

  function initStages() {
    jsonFiles('/challenges').forEach(function (f) {
      try {
        var src = readText('/challenges/' + f), d = JSON.parse(src), starters = {};
        [['en', d.starter], ['pt', d.pt && d.pt.starter]].forEach(function (s) {
          if (!s[1]) return;
          try { starters[s[0]] = readText('/challenges/' + s[1]); } catch (e) { log(t('log.missing', f, s[1]), 'err'); }
        });
        if (starters.en == null) starters.en = '';
        var st = { ch: true, id: f.replace(/\.json$/, ''), index: challenges.length, src: src, d: d, starters: starters };
        localize(st);
        challenges.push(st);
      } catch (e) { log(t('log.missing', f, e.message), 'err'); }
    });
    jsonFiles('/levels').forEach(function (f) {
      var src = readText('/levels/' + f), d = {};
      try { d = JSON.parse(src); } catch (e) { /* the sim reports it */ }
      var st = { ch: false, id: f, src: src, d: d };
      localize(st);
      if (!d.name) st.title = f;
      levels.push(st);
    });
    try { loadMine(JSON.parse(load(MINE, '[]'))); } catch (e) { mine = []; }
    var want = load('ciberrato.current', '');
    var st = challenges.concat(levels, mine).filter(function (s) { return stageKey(s) === want; })[0];
    if (!st || (st.ch && !unlocked(st))) st = challenges[0] || levels[0];
    if (st) openStage(st);
  }

  function openStage(st) {
    current = st;
    save('ciberrato.current', stageKey(st));
    var key = codeKeyOf(st);
    setDoc(key, load(key, starterOf(st)));
    lastResult = null;
    call('web_load_level', [simSource(st)]);
    call('web_reset'); /* the old script must not carry over */
    setErr(0);
    hideResult();
    $('hints').open = false;
    renderGoal();
  }

  function renderGoal() {
    var st = current, g = $('goal');
    $('pick-name').textContent = st ? (st.ch ? (st.index + 1) + '. ' : '') + st.title : '…';
    g.hidden = !st || !(st.ch || (st.custom && st.goal));
    if (g.hidden) { window.dispatchEvent(new Event('resize')); return; }
    var best = st.ch && progress[st.id];
    $('goal-num').textContent = st.ch ? t('challenge', st.index + 1) : t('custom');
    $('goal-title').textContent = st.title;
    $('goal-best').innerHTML = best ? stars(best.stars) : '';
    $('goal-best').title = best ? t('best', best.stars, secs(best.ticks)) : '';
    $('restore').hidden = !st.ch;
    $('goal-text').innerHTML = md(st.goal);
    $('hints').hidden = !st.hints.length;
    $('hint-list').innerHTML = st.hints.map(function (h) { return '<li>' + inline(String(h)) + '</li>'; }).join('');
    window.dispatchEvent(new Event('resize'));
  }

  /* ---- result (event from the sim when a challenge run is judged) ---------- */
  function hideResult() { $('result').hidden = true; }

  /* record: false when re-shown after a language change */
  function showResult(r, again) {
    var st = current;
    if (!st || !(st.ch || st.custom)) return;
    var box = $('result'), next = st.ch && challenges[st.index + 1];
    box.className = r.win ? 'ui' : 'ui fail';
    if (r.win) {
      var best = st.ch && progress[st.id];
      if (st.ch && !again && (!best || r.stars > best.stars || (r.stars === best.stars && r.ticks < best.ticks))) {
        progress[st.id] = { stars: r.stars, ticks: r.ticks, score: r.score };
        saveProgress();
        renderGoal();
      }
      if (!again) log(t('log.complete', '★★★☆☆☆'.slice(3 - r.stars, 6 - r.stars), secs(r.ticks)), 'win');
      $('res-stars').innerHTML = stars(r.stars);
      $('res-title').textContent = t('res.complete');
      $('res-text').textContent = t('res.time', secs(r.ticks)) +
        (best && best.ticks <= r.ticks && best.stars >= r.stars ? t('res.best', secs(best.ticks)) : '');
      var fast = !r.star_time || r.ticks <= r.star_time;
      $('res-detail').innerHTML = '<li>' + esc(t('res.reached')) + '</li>' +
        '<li class="' + (fast ? '' : 'off') + '">' + esc(r.star_time ? t('res.within', secs(r.star_time)) : t('res.done')) + '</li>' +
        '<li class="' + (r.collisions ? 'off' : '') + '">' + esc(t('res.nobumps') + (r.collisions ? t('res.bumps', r.collisions) : '')) + '</li>';
    } else {
      if (!again) log(t('log.notyet', r.why), 'err');
      $('res-stars').innerHTML = '';
      $('res-title').textContent = t('res.notyet');
      $('res-text').textContent = r.why;
      $('res-detail').innerHTML = '';
    }
    $('res-next').hidden = !r.win || !next;
    $('res-next').onclick = function () { if (next) openStage(next); };
    box.hidden = false;
  }

  /* ---- map ------------------------------------------------------------------ */
  function tile(title, kicker, line, locked, isCurrent, done, onclick) {
    var b = document.createElement('button');
    b.className = 'tile' + (locked ? ' locked' : '') + (done ? ' done' : '') + (isCurrent ? ' current' : '');
    b.disabled = locked;
    b.innerHTML = (kicker ? '<span class="n"></span>' : '') + '<b></b><span class="stars"></span>';
    if (kicker) b.firstChild.textContent = kicker;
    b.querySelector('b').textContent = title;
    b.lastChild.innerHTML = line;
    b.onclick = function () { showMap(false); onclick(); };
    return b;
  }
  function stageTile(st, line, locked) {
    var kicker = st.ch ? t('challenge', st.index + 1) : st.custom ? t('custom') : '';
    return tile(st.title, kicker, line, locked, st === current, st.ch && progress[st.id], function () { openStage(st); });
  }
  /* own level: the tile plus edit / delete buttons */
  function mineTile(st) {
    var box = document.createElement('div'), acts = document.createElement('span');
    box.className = 'tile-box';
    acts.className = 'acts';
    box.appendChild(stageTile(st, '', false));
    [st.d.maze ? null : ['✎', 'mine.edit', function () { showMap(false); openEditor(st); }], /* the editor can't do maze rows */
     ['✕', 'mine.delete', function () { deleteMine(st); }]].forEach(function (a) {
      if (!a) return;
      var b = document.createElement('button');
      b.className = 'ghost';
      b.textContent = a[0];
      b.title = t(a[1]);
      b.onclick = a[2];
      acts.appendChild(b);
    });
    box.appendChild(acts);
    return box;
  }

  function renderMap() {
    var list = $('map-list'), free = $('map-free'), got = 0;
    list.textContent = free.textContent = '';
    challenges.forEach(function (c) {
      var p = progress[c.id], locked = !unlocked(c);
      if (p) got += p.stars;
      list.appendChild(stageTile(c, p ? stars(p.stars) + ' <span class="dim">' + secs(p.ticks) + '</span>'
                                      : '<span class="dim">' + esc(t(locked ? 'map.locked' : 'map.todo')) + '</span>', locked));
    });
    levels.forEach(function (l) { free.appendChild(stageTile(l, '', false)); });
    mine.forEach(function (st) { free.appendChild(mineTile(st)); });
    var mk = tile(t('make'), '', '<span class="dim">' + esc(t('make.sub')) + '</span>', false, false, false, function () { openEditor(null); });
    mk.classList.add('make');
    free.appendChild(mk);
    $('map-total').textContent = t('map.total', got, challenges.length * 3);
    $('teacher-box').hidden = !(teacher || /[?&#]teacher\b/.test(location.search + location.hash));
    $('teacher').checked = teacher;
    $('load-level').hidden = !teacher;
  }

  function showMap(on) {
    if (on) renderMap();
    $('map').hidden = !on;
    if (on) ($('map').querySelector('.tile.current') || $('map-close')).focus();
  }

  /* ---- own levels: made in the level editor or a teacher's file ------------ */
  /* id: replace that level, else add a new one; returns its id */
  function storeMine(src, id) {
    var st = makeCustom(src, id || 'u' + Date.now().toString(36)), old = mineById(st.id);
    if (old) mine[mine.indexOf(old)] = st;
    else mine.push(st);
    saveMine();
    if (current === old) current = st; /* the arena still shows the old version */
    return st;
  }

  function playMine(src, id) {
    var st;
    try { st = storeMine(src, id); } catch (e) { alert(t('loadlevel.fail', e.message)); return null; }
    log(t('log.custom', st.title), 'sys');
    openStage(st);
    return st.id;
  }

  function deleteMine(st) {
    if (!confirm(t('mine.confirm', st.title))) return;
    mine.splice(mine.indexOf(st), 1);
    saveMine();
    try { localStorage.removeItem(codeKeyOf(st)); } catch (e) { /* private mode */ }
    if (st === current) openStage(challenges[0] || levels[0]);
    renderMap();
  }

  /* edit: one of the user's levels, or null for a new one */
  function openEditor(edit) {
    var stale = false;
    LevelEdit.open({
      current: current ? simSource(current) : null,
      currentId: current && current.custom ? current.id : null,
      edit: edit ? { id: edit.id, src: edit.src } : null,
      exists: function (id) { return !!mineById(id); },
      onPlay: playMine,
      onSave: function (src, id) {
        try { stale = storeMine(src, id) === current || stale; } catch (e) { /* not valid yet */ }
      },
      onClose: function () { if (stale && current && current.custom) openStage(current); stale = false; }
    });
  }

  /* ---- export / import (progress + code per challenge) --------------------- */
  function exportProgress() {
    saveCode();
    var code = {};
    challenges.forEach(function (c) { var v = load(codeKeyOf(c), null); if (v != null) code[c.id] = v; });
    mine.forEach(function (st) { var v = load(codeKeyOf(st), null); if (v != null) code['my.' + st.id] = v; });
    var data = { app: 'ciberrato-academy', version: 1, progress: progress, code: code, free: load('ciberrato.code', null),
                 levels: mine.map(function (st) { return { id: st.id, src: st.src }; }) };
    var a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' }));
    a.download = 'ciberrato-progress.json';
    a.click();
    setTimeout(function () { URL.revokeObjectURL(a.href); }, 1000);
  }

  function importProgress(file) {
    file.text().then(function (txt) {
      var d = JSON.parse(txt);
      if (!d || (d.app !== 'ciberrato-academy' && d.app !== 'ciberrato-jr') || typeof d.progress !== 'object') throw new Error(t('import.bad'));
      if (!confirm(t('import.confirm'))) return;
      progress = {};
      Object.keys(d.progress || {}).forEach(function (id) {
        var p = d.progress[id];
        if (p && p.stars >= 1 && p.stars <= 3) progress[id] = { stars: p.stars | 0, ticks: +p.ticks || 0, score: +p.score || 0 };
      });
      saveProgress();
      Object.keys(d.code || {}).forEach(function (id) { if (typeof d.code[id] === 'string') save('ciberrato.code.' + id, d.code[id]); });
      if (typeof d.free === 'string') save('ciberrato.code', d.free);
      if (Array.isArray(d.levels)) {
        loadMine(d.levels);
        saveMine();
        if (current.custom) current = mineById(current.id) || current;
      }
      setDoc(codeKeyOf(current), load(codeKeyOf(current), starterOf(current)), true);
      renderGoal();
      renderMap();
      log(t('log.imported'), 'sys');
    }).catch(function (e) { alert(t('import.fail', e.message)); });
  }

  function initMap() {
    $('pick').onclick = function () { showMap(true); };
    $('map-close').onclick = function () { showMap(false); };
    $('map').onclick = function (e) { if (e.target === $('map')) showMap(false); };
    $('map').addEventListener('keydown', function (e) { if (e.key === 'Escape') showMap(false); });
    $('export').onclick = exportProgress;
    $('import').onclick = function () { $('import-file').click(); };
    $('import-file').onchange = function () { var f = this.files[0]; this.value = ''; if (f) importProgress(f); };
    $('teacher').onchange = function () {
      teacher = this.checked;
      save(TEACHER, teacher ? '1' : '');
      renderMap();
    };
    $('load-level').onclick = function () { $('level-file').click(); };
    $('level-file').onchange = function () {
      var f = this.files[0];
      this.value = '';
      if (f) f.text().then(function (src) { showMap(false); playMine(src, null); });
    };
    $('res-retry').onclick = function () { hideResult(); call('web_reset'); setErr(0); view.focus(); }; /* fix the code, then Run */
    $('res-map').onclick = function () { hideResult(); showMap(true); };
    $('restore').onclick = function () {
      if (!current || !current.ch || !confirm(t('restore.confirm'))) return;
      view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: starterOf(current) } });
      view.focus();
    };
  }

  /* ---- cheat-sheet --------------------------------------------------------- */
  function renderCheat() {
    var body = $('cheat-body');
    body.textContent = '';
    API.forEach(function (g) {
      var h = document.createElement('h3');
      h.textContent = t('g.' + g.group);
      body.appendChild(h);
      g.items.forEach(function (it) {
        var d = document.createElement('div');
        d.className = 'fn';
        d.title = t('cheat.insert');
        d.innerHTML = '<code></code><p></p>';
        d.firstChild.textContent = it[0];
        d.lastChild.textContent = apiDesc(it);
        d.onclick = function () { insertAtCursor(it[1].replace(/\$\{([^}]*)\}/g, '$1')); };
        body.appendChild(d);
      });
    });
    var tip = document.createElement('p');
    tip.className = 'tip';
    tip.innerHTML = t('cheat.tip');
    body.appendChild(tip);
  }

  function initCheat() {
    renderCheat();
    function show(on) {
      $('cheat').hidden = !on;
      $('help').setAttribute('aria-pressed', on);
      save('ciberrato.cheat', on ? '1' : '');
      window.dispatchEvent(new Event('resize')); /* canvas pane changed size */
    }
    $('help').onclick = function () { show($('cheat').hidden); };
    $('cheat-close').onclick = function () { show(false); };
    show(!!load('ciberrato.cheat', ''));
  }

  /* ---- language and sound -------------------------------------------------- */
  function showLang() {
    document.querySelectorAll('#lang button').forEach(function (b) { b.classList.toggle('on', b.dataset.lang === I18N.lang); });
  }

  function setLang(l) {
    if (l === I18N.lang) return;
    var oldStarter = current && starterOf(current);
    I18N.set(l);
    showLang();
    call('web_set_lang', [l]);
    challenges.concat(levels, mine).forEach(localize);
    if (current) {
      call('web_set_level_name', [current.title]);
      /* untouched starter code follows the language */
      if (view.state.doc.toString() === oldStarter) view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: starterOf(current) } });
    }
    updateRunButton();
    renderGoal();
    renderCheat();
    if (!$('map').hidden) renderMap();
    if (!$('result').hidden && lastResult) showResult(lastResult, true);
  }

  function showMute() {
    $('mute').textContent = Sfx.muted ? '🔇' : '🔊';
    $('mute').setAttribute('aria-pressed', Sfx.muted);
  }

  /* ---- wiring ------------------------------------------------------------- */
  I18N.apply();
  makeEditor();
  initCheat();
  initMap();
  setState('idle');
  setSpeed(1);
  showLang();
  showMute();
  $('run').onclick = runButton;
  $('reset').onclick = function () { call('web_reset'); setErr(0); hideResult(); };
  $('clear').onclick = function () { $('console').textContent = ''; };
  $('mute').onclick = function () { Sfx.setMuted(!Sfx.muted); Sfx.unlock(); showMute(); };
  document.querySelectorAll('#lang button').forEach(function (b) { b.onclick = function () { setLang(b.dataset.lang); }; });
  document.querySelectorAll('#speed button').forEach(function (b) {
    b.onclick = function () { call('web_set_speed', [+b.dataset.speed]); };
  });
  /* audio may only start after a user gesture; Ctrl+Enter runs from the keyboard */
  document.addEventListener('pointerdown', Sfx.unlock, true);
  document.addEventListener('keydown', Sfx.unlock, true);
  /* raylib sizes the canvas to its parent on window resize events */
  new ResizeObserver(function () { window.dispatchEvent(new Event('resize')); }).observe($('stage'));

})();
