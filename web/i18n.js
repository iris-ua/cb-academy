/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Page strings in English and Portuguese. t(key, a, b...) fills {0}, {1}...
 * Static HTML uses data-i18n (text), data-i18n-title (tooltip). Challenge text
 * comes from the challenge JSON ("pt": { title, goal, hints, starter }). */
(function () {
  'use strict';
  var S = {
    en: {
      'pick.title': 'Choose a challenge or level',
      'help.title': 'API cheat-sheet',
      'lang.title': 'Language',
      'mute.title': 'Sound on/off',
      'run': '▶ Run', 'run.title': 'Run from the start (Ctrl+Enter)',
      'resume': '▶ Resume', 'resume.title': 'Continue the script',
      'pause': '❚❚ Pause', 'pause.title': 'Pause the script',
      'reset': '↺ Reset', 'reset.title': 'Put the robot back at the start',
      'speed.title': 'Simulation speed',
      'restore': '↺ Starter code', 'restore.title': 'Replace your code with the starter code',
      'restore.confirm': 'Replace your code with the starter code? (Ctrl+Z undoes it.)',
      'hints': 'Hints', 'console': 'Console', 'clear': 'Clear', 'close': 'Close',
      'api': 'Robot API', 'loading': 'Loading…',
      'state.idle': 'idle', 'state.running': 'running', 'state.paused': 'paused',
      'state.done': 'done', 'state.error': 'error', 'state.no code': 'no code',
      'log.run': '▶ Run', 'log.finished': 'Script finished.', 'log.level': 'Level: {0}',
      'log.over': 'Run over. Score: {0}', 'log.ready': 'Ready. Write a program and press Run.',
      'log.missing': 'Challenge {0}: missing {1}', 'log.complete': 'Challenge complete! {0} in {1}',
      'log.notyet': 'Not yet: {0}', 'log.imported': 'Progress imported.', 'log.custom': 'Custom level: {0}',
      'line.link': 'Show in the editor',
      'challenge': 'Challenge {0}', 'custom': 'Custom level',
      'best': 'Best: {0} stars in {1}',
      'res.complete': 'Challenge complete!', 'res.time': 'Time {0}', 'res.best': ' · best {0}',
      'res.reached': 'Reached the goal', 'res.within': 'Done within {0}', 'res.done': 'Done',
      'res.nobumps': 'No bumps', 'res.bumps': ' ({0} this time)', 'res.notyet': 'Not yet',
      'res.map': '☰ Map', 'res.retry': '↺ Try again', 'res.next': 'Next ▶',
      'map.title': 'Challenges', 'map.free': 'Free play', 'map.locked': 'Finish the one before',
      'map.todo': 'Not done yet', 'map.total': '{0} / {1} stars',
      'export': 'Export progress', 'export.title': 'Save your progress and code to a file',
      'import': 'Import…', 'import.title': 'Load progress from a file',
      'import.confirm': 'Replace your progress and saved code with this file?',
      'import.bad': 'not a CiberRato Academy progress file', 'import.fail': 'Import failed: {0}',
      'teacher': 'Teacher mode', 'teacher.title': 'Unlock every challenge and load your own levels',
      'loadlevel': 'Load level…', 'loadlevel.title': 'Play a level JSON file',
      'loadlevel.fail': 'Cannot load the level: {0}',
      'make': '✎ Make a level', 'make.sub': 'Draw walls, cheese and light',
      'cheat.tip': 'Tips: press <b>Ctrl+Space</b> in the editor for suggestions, <b>Ctrl+Enter</b> to run.',
      'cheat.insert': 'Click to insert',
      'g.Move': 'Move', 'g.Sense': 'Sense', 'g.Show': 'Show',
      'api.forward': 'Drive n robot diameters straight ahead (negative: backwards).',
      'api.turn': 'Turn in place by deg degrees. Positive turns left.',
      'api.turn_to': 'Turn to face a compass heading (degrees).',
      'api.motors': 'Set left/right wheel power, -0.15 to 0.15. Waits one tick.',
      'api.wait': 'Do nothing for n ticks (1 tick = 50 ms). Default 1.',
      'api.finish': 'End the run: the robot stops and gets scored.',
      'api.ir': 'IR sensor: 0 front, 1 left, 2 right, 3 back. Bigger = closer (1 / distance).',
      'api.compass': 'Heading in degrees, -180 to 180. 0 points right, 90 up.',
      'api.beacon': 'Angle to the beacon from where the robot faces (degrees), or nil if a wall hides it.',
      'api.ground': 'Index of the target under the robot (0, 1, …), or -1 if none.',
      'api.bump': 'true if the robot hit something on the last tick.',
      'api.time': 'Ticks since the start (1 tick = 50 ms).',
      'api.say': 'Print a message in the console. print() does the same.',
      'api.led': 'Turn an LED on or off: "visiting" or "returning".',
      'starter': [
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
      ].join('\n'),
      /* level editor */
      'ed.title': 'Level editor', 'ed.name': 'Name', 'ed.size': 'Size',
      'ed.wall': 'Wall', 'ed.wall.title': 'Drag to draw a wall',
      'ed.cheese': 'Cheese', 'ed.cheese.title': 'Click to place cheese',
      'ed.beacon': 'Light', 'ed.beacon.title': 'Click to place the light (beacon)',
      'ed.start': 'Start', 'ed.start.title': 'Click to place the mouse, drag to aim it',
      'ed.erase': 'Erase', 'ed.erase.title': 'Click a wall, cheese or light to remove it',
      'ed.tall': 'Tall walls', 'ed.tall.title': 'Walls taller than the light hide it from beacon()',
      'ed.new': 'New', 'ed.new.confirm': 'Start an empty level? Unsaved changes are lost.',
      'ed.current': 'Edit current', 'ed.current.title': 'Start from the level that is open now',
      'ed.current.maze': 'This level is made of maze rows and cannot be edited here.',
      'ed.open': 'Open…', 'ed.open.title': 'Open a level JSON file',
      'ed.export': 'Export JSON', 'ed.export.title': 'Save the level as a JSON file',
      'ed.play': '▶ Play', 'ed.play.title': 'Close the editor and play this level',
      'ed.nostart': 'Place the mouse first (Start tool).',
      'ed.help': 'Grid squares are 1 robot wide. Right-click removes. Ctrl+Z undoes.',
      'ed.default': 'My level'
    },
    pt: {
      'pick.title': 'Escolhe um desafio ou nível',
      'help.title': 'Resumo da API',
      'lang.title': 'Língua',
      'mute.title': 'Som ligado/desligado',
      'run': '▶ Executar', 'run.title': 'Executar desde o início (Ctrl+Enter)',
      'resume': '▶ Continuar', 'resume.title': 'Continuar o programa',
      'pause': '❚❚ Pausa', 'pause.title': 'Pôr o programa em pausa',
      'reset': '↺ Reiniciar', 'reset.title': 'Pôr o robô de volta na partida',
      'speed.title': 'Velocidade da simulação',
      'restore': '↺ Código inicial', 'restore.title': 'Substituir o teu código pelo código inicial',
      'restore.confirm': 'Substituir o teu código pelo código inicial? (Ctrl+Z desfaz.)',
      'hints': 'Dicas', 'console': 'Consola', 'clear': 'Limpar', 'close': 'Fechar',
      'api': 'API do robô', 'loading': 'A carregar…',
      'state.idle': 'parado', 'state.running': 'a correr', 'state.paused': 'em pausa',
      'state.done': 'feito', 'state.error': 'erro', 'state.no code': 'sem código',
      'log.run': '▶ Executar', 'log.finished': 'O programa terminou.', 'log.level': 'Nível: {0}',
      'log.over': 'Fim. Pontos: {0}', 'log.ready': 'Pronto. Escreve um programa e carrega em Executar.',
      'log.missing': 'Desafio {0}: falta {1}', 'log.complete': 'Desafio concluído! {0} em {1}',
      'log.notyet': 'Ainda não: {0}', 'log.imported': 'Progresso importado.', 'log.custom': 'Nível próprio: {0}',
      'line.link': 'Mostrar no editor',
      'challenge': 'Desafio {0}', 'custom': 'Nível próprio',
      'best': 'Melhor: {0} estrelas em {1}',
      'res.complete': 'Desafio concluído!', 'res.time': 'Tempo {0}', 'res.best': ' · melhor {0}',
      'res.reached': 'Chegaste ao objetivo', 'res.within': 'Feito em menos de {0}', 'res.done': 'Feito',
      'res.nobumps': 'Sem toques', 'res.bumps': ' ({0} desta vez)', 'res.notyet': 'Ainda não',
      'res.map': '☰ Mapa', 'res.retry': '↺ Tentar outra vez', 'res.next': 'Seguinte ▶',
      'map.title': 'Desafios', 'map.free': 'Jogo livre', 'map.locked': 'Acaba o anterior',
      'map.todo': 'Ainda por fazer', 'map.total': '{0} / {1} estrelas',
      'export': 'Exportar progresso', 'export.title': 'Guardar o teu progresso e código num ficheiro',
      'import': 'Importar…', 'import.title': 'Carregar o progresso de um ficheiro',
      'import.confirm': 'Substituir o teu progresso e código guardado por este ficheiro?',
      'import.bad': 'não é um ficheiro de progresso do CiberRato Academy', 'import.fail': 'A importação falhou: {0}',
      'teacher': 'Modo professor', 'teacher.title': 'Desbloquear todos os desafios e carregar os teus níveis',
      'loadlevel': 'Carregar nível…', 'loadlevel.title': 'Jogar um ficheiro JSON de nível',
      'loadlevel.fail': 'Não foi possível carregar o nível: {0}',
      'make': '✎ Criar um nível', 'make.sub': 'Desenha paredes, queijo e luz',
      'cheat.tip': 'Dicas: carrega em <b>Ctrl+Espaço</b> no editor para sugestões, <b>Ctrl+Enter</b> para executar.',
      'cheat.insert': 'Clica para inserir',
      'g.Move': 'Mover', 'g.Sense': 'Sentir', 'g.Show': 'Mostrar',
      'api.forward': 'Anda n diâmetros do robô em frente (negativo: para trás).',
      'api.turn': 'Roda no lugar deg graus. Positivo vira à esquerda.',
      'api.turn_to': 'Vira para uma direção da bússola (graus).',
      'api.motors': 'Define a potência das rodas esquerda/direita, de -0.15 a 0.15. Espera um tique.',
      'api.wait': 'Não faz nada durante n tiques (1 tique = 50 ms). Por omissão 1.',
      'api.finish': 'Termina: o robô para e recebe a pontuação.',
      'api.ir': 'Sensor IR: 0 frente, 1 esquerda, 2 direita, 3 trás. Maior = mais perto (1 / distância).',
      'api.compass': 'Direção em graus, de -180 a 180. 0 aponta para a direita, 90 para cima.',
      'api.beacon': 'Ângulo até ao farol a partir da frente do robô (graus), ou nil se uma parede o esconder.',
      'api.ground': 'Índice do alvo debaixo do robô (0, 1, …), ou -1 se nenhum.',
      'api.bump': 'true se o robô bateu em algo no último tique.',
      'api.time': 'Tiques desde o início (1 tique = 50 ms).',
      'api.say': 'Escreve uma mensagem na consola. print() faz o mesmo.',
      'api.led': 'Liga ou desliga um LED: "visiting" ou "returning".',
      'starter': [
        '-- Bem-vindo! Escreve aqui o programa do robô e carrega em Executar.',
        '-- Abre o painel API para veres todos os comandos.',
        '',
        'say("Olá, sou um rato!")',
        '',
        '-- anda até haver algo perto à frente',
        'while ir(0) < 2 do',
        '  motors(0.1, 0.1)',
        'end',
        '',
        'turn(90)',
        'forward(2)',
        'say("Terminei no tique " .. time())',
        'finish()',
        ''
      ].join('\n'),
      'ed.title': 'Editor de níveis', 'ed.name': 'Nome', 'ed.size': 'Tamanho',
      'ed.wall': 'Parede', 'ed.wall.title': 'Arrasta para desenhar uma parede',
      'ed.cheese': 'Queijo', 'ed.cheese.title': 'Clica para pôr queijo',
      'ed.beacon': 'Luz', 'ed.beacon.title': 'Clica para pôr a luz (farol)',
      'ed.start': 'Partida', 'ed.start.title': 'Clica para pôr o rato, arrasta para o apontar',
      'ed.erase': 'Apagar', 'ed.erase.title': 'Clica numa parede, queijo ou luz para a remover',
      'ed.tall': 'Paredes altas', 'ed.tall.title': 'Paredes mais altas do que a luz escondem-na de beacon()',
      'ed.new': 'Novo', 'ed.new.confirm': 'Começar um nível vazio? Perdes as alterações.',
      'ed.current': 'Editar atual', 'ed.current.title': 'Começar a partir do nível aberto agora',
      'ed.current.maze': 'Este nível é feito de linhas de labirinto e não pode ser editado aqui.',
      'ed.open': 'Abrir…', 'ed.open.title': 'Abrir um ficheiro JSON de nível',
      'ed.export': 'Exportar JSON', 'ed.export.title': 'Guardar o nível num ficheiro JSON',
      'ed.play': '▶ Jogar', 'ed.play.title': 'Fechar o editor e jogar este nível',
      'ed.nostart': 'Põe primeiro o rato (ferramenta Partida).',
      'ed.help': 'Cada quadrado da grelha tem 1 robô de largura. Botão direito apaga. Ctrl+Z desfaz.',
      'ed.default': 'O meu nível'
    }
  };

  var lang = 'en';
  try { lang = localStorage.getItem('ciberrato.lang') || ''; } catch (e) { /* private mode */ }
  if (!S[lang]) lang = /^pt\b/i.test(navigator.language || '') ? 'pt' : 'en';

  function t(key) {
    var s = S[lang][key];
    if (s == null) s = S.en[key];
    if (s == null) return key;
    var args = arguments;
    return s.replace(/\{(\d)\}/g, function (m, i) { return args[+i + 1]; });
  }

  /* static page text */
  function apply(root) {
    document.documentElement.lang = lang;
    (root || document).querySelectorAll('[data-i18n]').forEach(function (el) { el.textContent = t(el.dataset.i18n); });
    (root || document).querySelectorAll('[data-i18n-title]').forEach(function (el) { el.title = t(el.dataset.i18nTitle); });
  }

  function set(l) {
    if (!S[l]) return;
    lang = l;
    try { localStorage.setItem('ciberrato.lang', l); } catch (e) { /* private mode */ }
    apply();
  }

  /* pick a field from a challenge/level JSON: its "pt" block in Portuguese */
  function pick(d, key) {
    var loc = lang !== 'en' && d && d[lang];
    return loc && loc[key] != null ? loc[key] : d ? d[key] : undefined;
  }

  window.I18N = { t: t, apply: apply, set: set, pick: pick, get lang() { return lang; } };
})();
