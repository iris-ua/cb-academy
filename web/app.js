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
  var view, setErr;

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
    view = new CM.EditorView({
      parent: $('editor'),
      state: CM.EditorState.create({
        doc: load('ciberrato.code', STARTER),
        extensions: [
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
            saveTimer = setTimeout(function () { save('ciberrato.code', view.state.doc.toString()); }, 400);
          })
        ]
      })
    });
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
    case 'speed': setSpeed(n); break;
    case 'paused': paused = !!n; updateRunButton(); break;
    case 'level': if (ready) log('Level: ' + text, 'sys'); break;
    case 'finished': log('Run over. Score: ' + n, 'win'); break;
    case 'ready': /* main() is set up: window, level, robot */
      ready = true;
      initLevels();
      log('Ready. Write a program and press Run.', 'sys');
      break;
    }
  };

  /* ---- levels (preloaded under /levels in the WASM file system) ------------ */
  function initLevels() {
    var sel = $('level');
    var files = Module.FS.readdir('/levels').filter(function (f) { return /\.json$/.test(f); }).sort();
    files.forEach(function (f) {
      var o = document.createElement('option');
      o.value = '/levels/' + f;
      try { o.textContent = JSON.parse(Module.FS.readFile(o.value, { encoding: 'utf8' })).name || f; }
      catch (e) { o.textContent = f; }
      sel.appendChild(o);
    });
    var want = load('ciberrato.level', '/levels/sandbox.json');
    if (files.indexOf(want.replace('/levels/', '')) >= 0) sel.value = want;
    sel.onchange = function () {
      save('ciberrato.level', sel.value);
      call('web_load_level', [Module.FS.readFile(sel.value, { encoding: 'utf8' })]);
      setErr(0);
    };
    if (sel.value !== '/levels/default.json') sel.onchange(); /* main() loaded default.json */
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
    tip.innerHTML = 'Tips: press <b>Ctrl+Space</b> in the editor for suggestions, <b>Ctrl+Enter</b> to run. ' +
                    'Click the arena to drive with the arrow keys when no script runs.';
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
  setState('idle');
  setSpeed(1);
  $('run').onclick = runButton;
  $('reset').onclick = function () { call('web_reset'); setErr(0); };
  $('clear').onclick = function () { $('console').textContent = ''; };
  document.querySelectorAll('#speed button').forEach(function (b) {
    b.onclick = function () { call('web_set_speed', [+b.dataset.speed]); };
  });
  /* raylib sizes the canvas to its parent on window resize events */
  new ResizeObserver(function () { window.dispatchEvent(new Event('resize')); }).observe($('stage'));

})();
