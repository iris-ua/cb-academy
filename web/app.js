/* SPDX-License-Identifier: GPL-2.0-or-later */
/* CiberRato Jr web page: CodeMirror editor, console, API panel and the bridge
 * to the WASM sim (src/main.c web_* exports, events via Module.onEvent). */
(function () {
  'use strict';
  var $ = function (id) { return document.getElementById(id); };

  /* ---- robot API: one table feeds autocomplete and the cheat-sheet --------- */
  var API = [
    { group: 'Move', items: [
      ['forward(n)', 'forward(${n})', 'Drive n robot diameters straight ahead (negative: backwards).'],
      ['turn(deg)', 'turn(${deg})', 'Turn in place by deg degrees. Positive turns left.'],
      ['turn_to(deg)', 'turn_to(${deg})', 'Turn to face a compass heading (degrees).'],
      ['motors(l, r)', 'motors(${l}, ${r})', 'Set left/right wheel power, -0.15 to 0.15. Waits one tick.'],
      ['wait(n)', 'wait(${n})', 'Do nothing for n ticks (1 tick = 50 ms). Default 1.'],
      ['finish()', 'finish()', 'End the run: the robot stops and gets scored.'],
    ] },
    { group: 'Sense', items: [
      ['ir(i)', 'ir(${0})', 'IR sensor: 0 front, 1 left, 2 right, 3 back. Bigger = closer (1 / distance).'],
      ['compass()', 'compass()', 'Heading in degrees, -180 to 180. 0 points right, 90 up.'],
      ['beacon()', 'beacon()', 'Angle to the beacon from where the robot faces (degrees), or nil if a wall hides it.'],
      ['ground()', 'ground()', 'Index of the target under the robot (0, 1, …), or -1 if none.'],
      ['bump()', 'bump()', 'true if the robot hit something on the last tick.'],
      ['time()', 'time()', 'Ticks since the start (1 tick = 50 ms).'],
    ] },
    { group: 'Show', items: [
      ['say(msg)', 'say(${msg})', 'Print a message in the console. print() does the same.'],
      ['led(name, on)', 'led("${visiting}", ${true})', 'Turn an LED on or off: "visiting" or "returning".'],
    ] },
  ];
  var LUA_WORDS = ('and break do else elseif end false for function if in local nil not or repeat return ' +
                   'then true until while').split(' ');
  var MATH = ['abs', 'floor', 'ceil', 'min', 'max', 'sqrt', 'random', 'pi', 'huge', 'sin', 'cos', 'atan'];

  var STARTER = [
    '-- Welcome! Write your robot program here, then press Run.',
    '-- Open the API panel for every command.',
    '',
    'say("Hello, I am a mouse!")',
    '',
    '-- drive until something is close in front',
    'while ir(0) < 2 do',
    '  motors(0.1, 0.1)',
    'end',
    '',
    'turn(90)',
    'forward(2)',
    'say("Done at tick " .. time())',
    'finish()',
    ''
  ].join('\n');

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
                                                info: it[2], type: 'function', boost: 2 }));
      });
    });
    LUA_WORDS.forEach(function (w) { opts.push({ label: w, type: 'keyword' }); });
    MATH.forEach(function (m) { opts.push({ label: 'math.' + m, type: m === 'pi' || m === 'huge' ? 'constant' : 'function' }); });
    return opts;
  }

  function makeEditor() {
    var opts = apiCompletions();
    function complete(ctx) {
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
    view = new CM.EditorView({ parent: $('editor'), state: CM.EditorState.create({ doc: load(codeKey, STARTER), extensions: extensions }) });
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
      a.title = 'Show in the editor';
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
    log('▶ Run', 'sys');
    call('web_run', [view.state.doc.toString()]);
  }

  /* Run button: Run (fresh start) -> Pause -> Resume -> Pause ... until the script ends or Reset. */
  var runState = 'idle', paused = false;
  function updateRunButton() {
    var b = $('run'), live = runState === 'running';
    b.textContent = !live ? '▶ Run' : paused ? '▶ Resume' : '❚❚ Pause';
    b.title = !live ? 'Run from the start (Ctrl+Enter)' : paused ? 'Continue the script' : 'Pause the script';
    b.classList.toggle('primary', !live || paused);
    var pill = $('state'), shown = live && paused ? 'paused' : runState;
    pill.textContent = shown;
    pill.className = 'pill ' + shown;
  }
  function runButton() {
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
    case 'state': setState(text); if (text === 'done') log('Script finished.', 'sys'); break;
    case 'speed': setSpeed(parseFloat(text)); break;
    case 'paused': paused = !!n; updateRunButton(); break;
    case 'level': if (ready) log('Level: ' + text, 'sys'); break;
    case 'finished': if (!current || !current.ch) log('Run over. Score: ' + n, 'win'); break;
    case 'result': showResult(JSON.parse(text)); break;
    case 'ready': /* main() is set up: window, level, robot */
      ready = true;
      initStages();
      log('Ready. Write a program and press Run.', 'sys');
      break;
    }
  };

  /* ---- stages: challenges (/challenges, in file order) and free-play levels
   * (/levels), both preloaded in the WASM file system. Progress and code per
   * challenge live in localStorage. ----------------------------------------- */
  var challenges = [], levels = [], current = null;
  var PROGRESS = 'ciberrato.progress';

  function readText(path) { return Module.FS.readFile(path, { encoding: 'utf8' }); }
  function jsonFiles(dir) {
    try { return Module.FS.readdir(dir).filter(function (f) { return /\.json$/.test(f); }).sort(); }
    catch (e) { return []; }
  }
  function lines(v) { return Array.isArray(v) ? v.join('\n') : v == null ? '' : String(v); }
  function secs(ticks) { return (ticks * 0.05).toFixed(1) + ' s'; }

  var progress = (function () { try { return JSON.parse(load(PROGRESS, '{}')) || {}; } catch (e) { return {}; } })();
  function saveProgress() { save(PROGRESS, JSON.stringify(progress)); }
  function unlocked(c) { return c.index === 0 || !!progress[challenges[c.index - 1].id]; }
  function codeKeyOf(st) { return st.ch ? 'ciberrato.code.' + st.id : 'ciberrato.code'; }

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

  function initStages() {
    jsonFiles('/challenges').forEach(function (f) {
      try {
        var src = readText('/challenges/' + f), d = JSON.parse(src), starter = '';
        if (d.starter) {
          try { starter = readText('/challenges/' + d.starter); } catch (e) { log('Challenge ' + f + ': missing ' + d.starter, 'err'); }
        }
        challenges.push({ ch: true, id: f.replace(/\.json$/, ''), index: challenges.length, src: src,
                          title: d.title || d.name || f, goal: lines(d.goal), hints: [].concat(d.hints || []), starter: starter });
      } catch (e) { log('Challenge ' + f + ': ' + e.message, 'err'); }
    });
    jsonFiles('/levels').forEach(function (f) {
      var src = readText('/levels/' + f), name = f;
      try { name = JSON.parse(src).name || f; } catch (e) { /* the sim reports it */ }
      levels.push({ ch: false, id: f, src: src, title: name });
    });
    var want = load('ciberrato.current', '');
    var st = challenges.concat(levels).filter(function (s) { return (s.ch ? 'ch:' : 'lv:') + s.id === want; })[0];
    if (!st || (st.ch && !unlocked(st))) st = challenges[0] || levels[0];
    if (st) openStage(st);
  }

  function openStage(st) {
    current = st;
    save('ciberrato.current', (st.ch ? 'ch:' : 'lv:') + st.id);
    var key = codeKeyOf(st);
    setDoc(key, load(key, st.ch ? st.starter : STARTER));
    call('web_load_level', [st.src]);
    call('web_reset'); /* the old script must not carry over */
    setErr(0);
    hideResult();
    $('pick-name').textContent = (st.ch ? (st.index + 1) + '. ' : '') + st.title;
    renderGoal();
  }

  function renderGoal() {
    var st = current, g = $('goal');
    g.hidden = !st || !st.ch;
    if (g.hidden) return;
    var best = progress[st.id];
    $('goal-num').textContent = 'Challenge ' + (st.index + 1);
    $('goal-title').textContent = st.title;
    $('goal-best').innerHTML = best ? stars(best.stars) : '';
    $('goal-best').title = best ? 'Best: ' + best.stars + ' stars in ' + secs(best.ticks) : '';
    $('goal-text').innerHTML = md(st.goal);
    $('hints').hidden = !st.hints.length;
    $('hints').open = false;
    $('hint-list').innerHTML = st.hints.map(function (h) { return '<li>' + inline(String(h)) + '</li>'; }).join('');
    window.dispatchEvent(new Event('resize'));
  }

  /* ---- result (event from the sim when a challenge run is judged) ---------- */
  function hideResult() { $('result').hidden = true; }

  function showResult(r) {
    var st = current;
    if (!st || !st.ch) return;
    var box = $('result'), next = challenges[st.index + 1];
    box.className = r.win ? 'ui' : 'ui fail';
    if (r.win) {
      var best = progress[st.id];
      if (!best || r.stars > best.stars || (r.stars === best.stars && r.ticks < best.ticks)) {
        progress[st.id] = { stars: r.stars, ticks: r.ticks, score: r.score };
        saveProgress();
        renderGoal();
      }
      log('Challenge complete! ' + '★★★☆☆☆'.slice(3 - r.stars, 6 - r.stars) + ' in ' + secs(r.ticks), 'win');
      $('res-stars').innerHTML = stars(r.stars);
      $('res-title').textContent = 'Challenge complete!';
      $('res-text').textContent = 'Time ' + secs(r.ticks) + (best && best.ticks <= r.ticks && best.stars >= r.stars ? ' · best ' + secs(best.ticks) : '');
      var fast = !r.star_time || r.ticks <= r.star_time;
      $('res-detail').innerHTML = '<li>Reached the goal</li>' +
        '<li class="' + (fast ? '' : 'off') + '">' + (r.star_time ? 'Done within ' + secs(r.star_time) : 'Done') + '</li>' +
        '<li class="' + (r.collisions ? 'off' : '') + '">No bumps' + (r.collisions ? ' (' + r.collisions + ' this time)' : '') + '</li>';
    } else {
      log('Not yet: ' + r.why, 'err');
      $('res-stars').innerHTML = '';
      $('res-title').textContent = 'Not yet';
      $('res-text').textContent = r.why;
      $('res-detail').innerHTML = '';
    }
    $('res-next').hidden = !r.win || !next;
    $('res-next').onclick = function () { if (next) openStage(next); };
    box.hidden = false;
  }

  /* ---- map ------------------------------------------------------------------ */
  function tile(st, line, locked) {
    var b = document.createElement('button');
    b.className = 'tile' + (locked ? ' locked' : '') + (st.ch && progress[st.id] ? ' done' : '') + (st === current ? ' current' : '');
    b.disabled = locked;
    b.innerHTML = (st.ch ? '<span class="n">Challenge ' + (st.index + 1) + '</span>' : '') + '<b></b><span class="stars"></span>';
    b.querySelector('b').textContent = st.title;
    b.lastChild.innerHTML = line;
    b.onclick = function () { showMap(false); openStage(st); };
    return b;
  }

  function renderMap() {
    var list = $('map-list'), free = $('map-free'), got = 0;
    list.textContent = free.textContent = '';
    challenges.forEach(function (c) {
      var p = progress[c.id], locked = !unlocked(c);
      if (p) got += p.stars;
      list.appendChild(tile(c, p ? stars(p.stars) + ' <span class="dim">' + secs(p.ticks) + '</span>'
                                 : '<span class="dim">' + (locked ? 'Finish the one before' : 'Not done yet') + '</span>', locked));
    });
    levels.forEach(function (l) { free.appendChild(tile(l, '', false)); });
    $('map-total').textContent = got + ' / ' + challenges.length * 3 + ' stars';
  }

  function showMap(on) {
    if (on) renderMap();
    $('map').hidden = !on;
    if (on) ($('map').querySelector('.tile.current') || $('map-close')).focus();
  }

  /* ---- export / import (progress + code per challenge) --------------------- */
  function exportProgress() {
    saveCode();
    var code = {};
    challenges.forEach(function (c) { var v = load(codeKeyOf(c), null); if (v != null) code[c.id] = v; });
    var data = { app: 'ciberrato-jr', version: 1, progress: progress, code: code, free: load('ciberrato.code', null) };
    var a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' }));
    a.download = 'ciberrato-progress.json';
    a.click();
    setTimeout(function () { URL.revokeObjectURL(a.href); }, 1000);
  }

  function importProgress(file) {
    file.text().then(function (t) {
      var d = JSON.parse(t);
      if (!d || d.app !== 'ciberrato-jr' || typeof d.progress !== 'object') throw new Error('not a CiberRato Jr progress file');
      if (!confirm('Replace your progress and saved code with this file?')) return;
      progress = {};
      Object.keys(d.progress || {}).forEach(function (id) {
        var p = d.progress[id];
        if (p && p.stars >= 1 && p.stars <= 3) progress[id] = { stars: p.stars | 0, ticks: +p.ticks || 0, score: +p.score || 0 };
      });
      saveProgress();
      Object.keys(d.code || {}).forEach(function (id) { if (typeof d.code[id] === 'string') save('ciberrato.code.' + id, d.code[id]); });
      if (typeof d.free === 'string') save('ciberrato.code', d.free);
      setDoc(codeKeyOf(current), load(codeKeyOf(current), current.ch ? current.starter : STARTER), true);
      renderGoal();
      renderMap();
      log('Progress imported.', 'sys');
    }).catch(function (e) { alert('Import failed: ' + e.message); });
  }

  function initMap() {
    $('pick').onclick = function () { showMap(true); };
    $('map-close').onclick = function () { showMap(false); };
    $('map').onclick = function (e) { if (e.target === $('map')) showMap(false); };
    $('map').addEventListener('keydown', function (e) { if (e.key === 'Escape') showMap(false); });
    $('export').onclick = exportProgress;
    $('import').onclick = function () { $('import-file').click(); };
    $('import-file').onchange = function () { var f = this.files[0]; this.value = ''; if (f) importProgress(f); };
    $('res-retry').onclick = function () { hideResult(); call('web_reset'); setErr(0); view.focus(); }; /* fix the code, then Run */
    $('res-map').onclick = function () { hideResult(); showMap(true); };
    $('restore').onclick = function () {
      if (!current || !current.ch || !confirm('Replace your code with the starter code? (Ctrl+Z undoes it.)')) return;
      view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: current.starter } });
      view.focus();
    };
  }

  /* ---- cheat-sheet --------------------------------------------------------- */
  function initCheat() {
    var body = $('cheat-body');
    API.forEach(function (g) {
      var h = document.createElement('h3');
      h.textContent = g.group;
      body.appendChild(h);
      g.items.forEach(function (it) {
        var d = document.createElement('div');
        d.className = 'fn';
        d.title = 'Click to insert';
        d.innerHTML = '<code></code><p></p>';
        d.firstChild.textContent = it[0];
        d.lastChild.textContent = it[2];
        d.onclick = function () { insertAtCursor(it[1].replace(/\$\{([^}]*)\}/g, '$1')); };
        body.appendChild(d);
      });
    });
    var tip = document.createElement('p');
    tip.className = 'tip';
    tip.innerHTML = 'Tips: press <b>Ctrl+Space</b> in the editor for suggestions, <b>Ctrl+Enter</b> to run.';
    body.appendChild(tip);

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

  /* ---- wiring ------------------------------------------------------------- */
  makeEditor();
  initCheat();
  initMap();
  setState('idle');
  setSpeed(1);
  $('run').onclick = runButton;
  $('reset').onclick = function () { call('web_reset'); setErr(0); hideResult(); };
  $('clear').onclick = function () { $('console').textContent = ''; };
  document.querySelectorAll('#speed button').forEach(function (b) {
    b.onclick = function () { call('web_set_speed', [+b.dataset.speed]); };
  });
  /* raylib sizes the canvas to its parent on window resize events */
  new ResizeObserver(function () { window.dispatchEvent(new Event('resize')); }).observe($('stage'));

})();
