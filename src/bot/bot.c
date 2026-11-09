/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "bot.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define HOOK_STEP 1000 /* instructions between budget checks */

struct Bot {
    lua_State *L, *co;
    BotState   state;
    char      *code;
    char       name[64], chunk[66];
    BotSayFn   say;
    void      *ud;
    size_t     mem;

    Sim       *sim;            /* valid during a resume */
    int        r;
    int        cycle;          /* last sim cycle seen, -1 = sync on next tick */
    double     est_l, est_r;   /* noise-free motor output model (for helpers) */
    long       ops;
    bool       over_budget, out_of_mem, finished;

    char       err[BOT_ERR_MAX];
    int        err_line;
};

/* ---- helpers written in Lua (run once per state, before the student code) -- */

static const char PRELUDE[] =
    "local P = ...\n"
    "local motors, compass, speed = motors, compass, P.speed\n"
    "local MAX, R = 0.15, 0.5\n"
    "local function need(v, fn, what, lvl)\n"
    "  if type(v) ~= 'number' then\n"
    "    error(fn .. ': ' .. what .. ' must be a number (got ' .. type(v) .. ')', lvl or 3)\n"
    "  end\n"
    "end\n"
    "local function opt_speed(s, fn)\n"
    "  if s == nil then return 0.1 end\n"
    "  need(s, fn, 'speed', 4)\n"
    "  if s == 0 then error(fn .. ': speed must not be 0', 3) end\n"
    "  return math.min(math.abs(s), MAX)\n"
    "end\n"
    /* Command p this tick and 0 after: output o' = (p+o)/2, then halves, so the
     * total motion is gain*(p+o). p = left/gain - o lands exactly on target. */
    "local function move(amount, spd, gain, axis, apply)\n"
    "  local sign = amount < 0 and -1 or 1\n"
    "  local left = math.abs(amount)\n"
    "  while true do\n"
    "    local o = sign * axis(speed())\n"
    "    if math.abs(left) < 0.005 and math.abs(o) * gain < 0.005 then return end\n"
    "    local p = math.max(-MAX, math.min(spd, left / gain - o))\n"
    "    apply(sign * p)\n"
    "    left = left - gain * sign * axis(speed())\n"
    "  end\n"
    "end\n"
    "local function fwd_axis(l, r) return (l + r) / 2 end\n"
    "local function rot_axis(l, r) return (r - l) / 2 end\n"
    "local function fwd_apply(p) motors(p, p) end\n"
    "local function rot_apply(p) motors(-p, p) end\n"
    "function forward(n, s)\n"
    "  need(n, 'forward', 'distance')\n"
    "  move(n, opt_speed(s, 'forward'), 1, fwd_axis, fwd_apply)\n"
    "end\n"
    "function turn(deg, s)\n"
    "  need(deg, 'turn', 'angle')\n"
    "  move(math.rad(deg), opt_speed(s, 'turn'), 1 / R, rot_axis, rot_apply)\n"
    "end\n"
    "function turn_to(deg, s)\n"
    "  need(deg, 'turn_to', 'heading')\n"
    "  turn((deg - compass() + 180) % 360 - 180, s)\n"
    "end\n";

/* names offered by "did you mean" */
static const char *KNOWN[] = {
    "motors", "wait", "ir", "compass", "gps", "beacon", "beacons", "ground", "bump", "time", "led", "finish",
    "say", "print", "forward", "turn", "turn_to", "math", "string", "table", "pairs", "ipairs",
    "tostring", "tonumber", "type", "error", "assert", "pcall", "select",
};

/* ---- memory, hook ------------------------------------------------------- */

static void *bot_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    Bot *b = ud;
    size_t old = ptr ? osize : 0;
    if (nsize == 0) {
        free(ptr);
        b->mem -= old;
        return NULL;
    }
    if (nsize > old && b->mem + (nsize - old) > BOT_MEM_LIMIT) {
        b->out_of_mem = true;
        return NULL;
    }
    void *p = realloc(ptr, nsize);
    if (p) b->mem = b->mem - old + nsize;
    return p;
}

static Bot *bot_of(lua_State *L) { return *(Bot **)lua_getextraspace(L); }

static void budget_hook(lua_State *L, lua_Debug *ar)
{
    (void)ar;
    Bot *b = bot_of(L);
    if (++b->ops * HOOK_STEP <= BOT_TICK_BUDGET) return;
    b->over_budget = true;
    lua_sethook(L, budget_hook, LUA_MASKCOUNT, 1); /* keep failing, even inside pcall */
    lua_pushliteral(L, "time budget exceeded");
    lua_error(L);
}

/* ---- API ---------------------------------------------------------------- */

static Robot *robot(lua_State *L)
{
    Bot *b = bot_of(L);
    return &b->sim->robots[b->r];
}

static int l_motors(lua_State *L)
{
    Bot *b = bot_of(L);
    double l = luaL_checknumber(L, 1), r = luaL_checknumber(L, 2);
    sim_set_motors(b->sim, b->r, l, r);
    return lua_yield(L, 0);
}

static int wait_k(lua_State *L, int status, lua_KContext n)
{
    (void)status;
    return n > 1 ? lua_yieldk(L, 0, n - 1, wait_k) : 0;
}

static int l_wait(lua_State *L)
{
    lua_Integer n = luaL_optinteger(L, 1, 1);
    return n > 0 ? lua_yieldk(L, 0, (lua_KContext)n, wait_k) : 0;
}

static int l_ir(lua_State *L)
{
    lua_Integer i = luaL_checkinteger(L, 1);
    luaL_argcheck(L, i >= 0 && i < SIM_NUM_IR, 1, "use 0 (front), 1 (left), 2 (right) or 3 (back)");
    lua_pushnumber(L, robot(L)->ir[i]);
    return 1;
}

static int l_compass(lua_State *L) { lua_pushnumber(L, robot(L)->compass); return 1; }
static int l_gps(lua_State *L)
{
    if (!bot_of(L)->sim->level.params.gps) { lua_pushnil(L); return 1; }
    const Robot *rb = robot(L);
    lua_pushnumber(L, rb->gps_x);
    lua_pushnumber(L, rb->gps_y);
    return 2;
}
static int l_ground(lua_State *L)  { lua_pushinteger(L, robot(L)->ground); return 1; }
static int l_bump(lua_State *L)    { lua_pushboolean(L, robot(L)->bumper); return 1; }
static int l_time(lua_State *L)    { lua_pushinteger(L, bot_of(L)->sim->cycle); return 1; }

static int l_beacons(lua_State *L) { lua_pushinteger(L, bot_of(L)->sim->level.lab.nbeacons); return 1; }

static int l_beacon(lua_State *L)
{
    lua_Integer i = luaL_optinteger(L, 1, 0);
    int n = bot_of(L)->sim->level.lab.nbeacons;
    const Robot *rb = robot(L);
    /* beacon() on a level without beacons is nil, not an error */
    if (i < 0 || (i > 0 && i >= n))
        return luaL_argerror(L, 1, n > 1 ? lua_pushfstring(L, "use 0 to %d (see beacons())", n - 1)
                                 : n ? "this level has one beacon: use 0" : "this level has no beacons");
    if (i < n && rb->beacon_visible[i])
        lua_pushnumber(L, rb->beacon_dir[i]);
    else
        lua_pushnil(L);
    return 1;
}

static int l_led(lua_State *L)
{
    static const char *names[] = { "visiting", "returning", NULL };
    int which = luaL_checkoption(L, 1, NULL, names);
    bool on = lua_isnoneornil(L, 2) ? true : lua_toboolean(L, 2);
    Bot *b = bot_of(L);
    const Robot *rb = robot(L);
    sim_set_led(b->sim, b->r, which == 0 ? on : rb->led_visiting, which == 1 ? on : rb->led_returning);
    return 0;
}

static int l_finish(lua_State *L)
{
    Bot *b = bot_of(L);
    sim_finish(b->sim, b->r);
    b->finished = true;
    return lua_yield(L, 0);
}

static int l_say(lua_State *L)
{
    Bot *b = bot_of(L);
    int n = lua_gettop(L);
    luaL_Buffer buf;
    luaL_buffinit(L, &buf);
    for (int i = 1; i <= n; i++) {
        if (i > 1) luaL_addchar(&buf, ' ');
        luaL_tolstring(L, i, NULL);
        luaL_addvalue(&buf);
    }
    luaL_pushresult(&buf);
    if (b->say) b->say(b->ud, lua_tostring(L, -1));
    return 0;
}

static int l_speed(lua_State *L)
{
    Bot *b = bot_of(L);
    lua_pushnumber(L, b->est_l);
    lua_pushnumber(L, b->est_r);
    return 2;
}

static const luaL_Reg API[] = {
    { "motors", l_motors }, { "wait", l_wait }, { "ir", l_ir }, { "compass", l_compass },
    { "gps", l_gps }, { "beacon", l_beacon }, { "beacons", l_beacons },
    { "ground", l_ground }, { "bump", l_bump }, { "time", l_time },
    { "led", l_led }, { "finish", l_finish }, { "say", l_say }, { "print", l_say },
    { NULL, NULL },
};

/* ---- friendly errors ---------------------------------------------------- */

static int edit_distance(const char *a, const char *b)
{
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la > 31 || lb > 31) return 99;
    int d[32][32];
    for (int i = 0; i <= la; i++) d[i][0] = i;
    for (int j = 0; j <= lb; j++) d[0][j] = j;
    for (int i = 1; i <= la; i++)
        for (int j = 1; j <= lb; j++) {
            int c = tolower((unsigned char)a[i - 1]) != tolower((unsigned char)b[j - 1]);
            int m = d[i - 1][j - 1] + c;
            if (d[i - 1][j] + 1 < m) m = d[i - 1][j] + 1;
            if (d[i][j - 1] + 1 < m) m = d[i][j - 1] + 1;
            d[i][j] = m;
        }
    return d[la][lb];
}

static const char *suggest(const char *name)
{
    const char *best = NULL;
    int bd = 3;
    for (size_t i = 0; i < sizeof KNOWN / sizeof KNOWN[0]; i++) {
        int d = edit_distance(name, KNOWN[i]);
        if (d < bd && d * 2 < (int)strlen(KNOWN[i]) + 1) { bd = d; best = KNOWN[i]; }
    }
    return best;
}

/* "src:N: rest" -> rest, *line = N when src is the student chunk */
static const char *strip_pos(const Bot *b, const char *msg, int *line)
{
    const char *c = strchr(msg, ':');
    if (!c || !isdigit((unsigned char)c[1])) return msg;
    const char *e = c + 1;
    while (isdigit((unsigned char)*e)) e++;
    if (e[0] != ':' || e[1] != ' ') return msg;
    size_t n = strlen(b->name);
    if ((size_t)(c - msg) == n && strncmp(msg, b->name, n) == 0) *line = atoi(c + 1);
    return e + 2;
}

static int stack_line(const Bot *b, lua_State *co)
{
    lua_Debug ar;
    for (int lv = 0; lua_getstack(co, lv, &ar); lv++) {
        lua_getinfo(co, "Sl", &ar);
        if (ar.currentline > 0 && strcmp(ar.source, b->chunk) == 0) return ar.currentline;
    }
    return 0;
}

static void set_error(Bot *b, lua_State *co, bool runtime)
{
    const char *raw = lua_type(co, -1) == LUA_TSTRING ? lua_tostring(co, -1) : NULL;
    char msg[BOT_ERR_MAX - 16];
    int line = 0;

    if (b->over_budget) {
        snprintf(msg, sizeof msg, "your code ran too long without letting the robot move. "
                                  "Loops that read sensors need a wait() or motors() inside.");
    } else if (b->out_of_mem) {
        snprintf(msg, sizeof msg, "your code used too much memory.");
    } else if (!raw) {
        snprintf(msg, sizeof msg, "error(...) was called with a %s instead of a message.", luaL_typename(co, -1));
    } else {
        const char *m = strip_pos(b, raw, &line);
        char var[64];
        const char *g = strstr(m, "attempt to call a nil value (global '");
        if (g && sscanf(g + 37, "%63[^']", var) == 1) {
            const char *s = suggest(var);
            if (s) snprintf(msg, sizeof msg, "'%s' is not defined. Did you mean '%s'?", var, s);
            else snprintf(msg, sizeof msg, "'%s' is not defined. Check the spelling, or define it before using it.", var);
        } else if ((g = strstr(m, "attempt to assign to const variable '")) && sscanf(g + 37, "%63[^']", var) == 1) {
            /* Lua 5.5: 'for' loop variables are read-only */
            snprintf(msg, sizeof msg, "'%s' is the counter of a 'for' loop and cannot be changed. Copy it into another variable first.", var);
        } else {
            snprintf(msg, sizeof msg, "%s", m);
            char *eof = strstr(msg, " near <eof>");
            if (eof) snprintf(eof, sizeof msg - (size_t)(eof - msg), " at the end of the code");
        }
    }
    if (!line && runtime) line = stack_line(b, co);
    b->err_line = line;
    if (line) snprintf(b->err, sizeof b->err, "Line %d: %s", line, msg);
    else snprintf(b->err, sizeof b->err, "%s", msg);
    b->state = BOT_ERROR;
}

/* ---- lifecycle ---------------------------------------------------------- */

static void close_state(Bot *b)
{
    if (b->L) lua_close(b->L);
    b->L = b->co = NULL;
}

Bot *bot_new(BotSayFn say, void *ud)
{
    Bot *b = calloc(1, sizeof *b);
    if (!b) return NULL;
    b->say = say;
    b->ud = ud;
    return b;
}

void bot_free(Bot *b)
{
    if (!b) return;
    close_state(b);
    free(b->code);
    free(b);
}

bool bot_load(Bot *b, const char *code, const char *name)
{
    size_t n = strlen(code);
    char *copy = malloc(n + 1);
    if (!copy) return false;
    memcpy(copy, code, n + 1);
    free(b->code);
    b->code = copy;
    snprintf(b->name, sizeof b->name, "%s", name && name[0] ? name : "robot.lua");
    return bot_restart(b);
}

static void open_lib(lua_State *L, const char *name, lua_CFunction f)
{
    luaL_requiref(L, name, f, 1);
    lua_pop(L, 1);
}

bool bot_restart(Bot *b)
{
    close_state(b);
    b->state = BOT_NONE;
    b->err[0] = '\0';
    b->err_line = 0;
    b->over_budget = b->out_of_mem = b->finished = false;
    b->cycle = -1;
    b->est_l = b->est_r = 0.0;
    b->mem = 0;
    if (!b->code) return false;

    snprintf(b->chunk, sizeof b->chunk, "=%s", b->name);
    lua_State *L = b->L = lua_newstate(bot_alloc, b, 0); /* fixed hash seed: deterministic runs */
    if (!L) {
        snprintf(b->err, sizeof b->err, "could not start Lua");
        b->state = BOT_ERROR;
        return false;
    }
    *(Bot **)lua_getextraspace(L) = b;

    open_lib(L, LUA_GNAME, luaopen_base);
    open_lib(L, LUA_TABLIBNAME, luaopen_table);
    open_lib(L, LUA_STRLIBNAME, luaopen_string);
    open_lib(L, LUA_MATHLIBNAME, luaopen_math);
    open_lib(L, LUA_UTF8LIBNAME, luaopen_utf8);
    static const char *drop[] = { "dofile", "loadfile", "load", "collectgarbage", NULL };
    for (int i = 0; drop[i]; i++) {
        lua_pushnil(L);
        lua_setglobal(L, drop[i]);
    }
    lua_getglobal(L, LUA_STRLIBNAME);
    lua_pushnil(L);
    lua_setfield(L, -2, "dump");
    lua_pop(L, 1);
    (void)luaL_dostring(L, "math.randomseed(42)"); /* reproducible runs */

    lua_pushglobaltable(L);
    luaL_setfuncs(L, API, 0);
    lua_pop(L, 1);

    if (luaL_loadbufferx(L, PRELUDE, sizeof PRELUDE - 1, "=api", "t") != LUA_OK) goto internal;
    lua_newtable(L);
    lua_pushcfunction(L, l_speed);
    lua_setfield(L, -2, "speed");
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) goto internal;

    b->co = lua_newthread(L); /* stays on L's stack, which anchors it */
    if (luaL_loadbufferx(b->co, b->code, strlen(b->code), b->chunk, "t") != LUA_OK) {
        set_error(b, b->co, false);
        return false;
    }
    b->state = BOT_RUNNING;
    return true;

internal:
    snprintf(b->err, sizeof b->err, "internal: %s", lua_tostring(L, -1));
    b->state = BOT_ERROR;
    return false;
}

void bot_tick(Bot *b, Sim *s, int r)
{
    if (b->state != BOT_RUNNING || s->over || r < 0 || r >= s->nrobots) return;
    const Robot *rb = &s->robots[r];
    if (b->cycle < 0 || b->cycle > s->cycle) b->cycle = s->cycle;
    for (; b->cycle < s->cycle; b->cycle++) { /* one model step per sim_step since last resume */
        b->est_l = 0.5 * rb->left.in + 0.5 * b->est_l;
        b->est_r = 0.5 * rb->right.in + 0.5 * b->est_r;
    }

    b->sim = s;
    b->r = r;
    b->ops = 0;
    lua_sethook(b->co, budget_hook, LUA_MASKCOUNT, HOOK_STEP);
    int nres = 0;
    int st = lua_resume(b->co, b->L, 0, &nres);
    if (st == LUA_YIELD) {
        lua_pop(b->co, nres);
        if (b->finished) b->state = BOT_DONE;
    } else {
        if (st == LUA_OK) b->state = BOT_DONE;
        else set_error(b, b->co, true);
        sim_set_motors(s, r, 0.0, 0.0); /* script over: don't drive off */
    }
    b->sim = NULL;
}

BotState    bot_state(const Bot *b)      { return b->state; }
const char *bot_error(const Bot *b)      { return b->err; }
const char *bot_code(const Bot *b)       { return b->code; }
int         bot_error_line(const Bot *b) { return b->err_line; }
