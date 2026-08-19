#include "Snippet.h"
#include "Params.h"
#include "io.h"
#include "../math/kernels.h"

#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <set>

#include <lua.hpp>   // already wraps the C headers in extern "C"

namespace slope {

// a stable handle, hot reload swaps the chunk underneath it
struct Snippet::Call {
    std::string name;
    int  ref = LUA_NOREF;
    long failed_frame = -1;
    bool reported = false;
};

namespace {

// ── userdata, vec2 / vec3 / complex ─────────────────────────────────────────
// one payload, three metatables differing only in what * and / mean
struct SVec { int tag; double v[3]; };

constexpr int TAG_V2 = 2, TAG_V3 = 3, TAG_CPX = 4;

const char* MT_V2  = "slope.vec2";
const char* MT_V3  = "slope.vec3";
const char* MT_CPX = "slope.complex";

const char* mtName(int tag) {
    return tag == TAG_V2 ? MT_V2 : tag == TAG_V3 ? MT_V3 : MT_CPX;
}

SVec* pushSVec(lua_State* L, int tag, double x, double y, double z = 0) {
    auto* s = (SVec*)lua_newuserdata(L, sizeof(SVec));
    s->tag = tag; s->v[0] = x; s->v[1] = y; s->v[2] = z;
    luaL_getmetatable(L, mtName(tag));
    lua_setmetatable(L, -2);
    return s;
}

SVec* asSVec(lua_State* L, int i) {
    if (!lua_isuserdata(L, i)) return nullptr;
    return (SVec*)lua_touserdata(L, i);
}

int comps(int tag) { return tag == TAG_V3 ? 3 : 2; }

// ── the interpreter and its registries ──────────────────────────────────────

lua_State* L = nullptr;

struct Section {
    std::string name;
    std::string file;
    int  ref      = LUA_NOREF;   // the chunk
    int  call_ref = LUA_NOREF;   // its return value, when that is a function
    std::vector<std::string> keys;
    long last_frame = -1;
    bool running  = false;
    bool failed   = false;
    bool reported = false;
};

struct Var {
    Snippet::Value value, prev;
    long frame = -1;
    long gen   = 0;
    Section* sec = nullptr;      // null for a C++ derivation
};

struct SourceFile {
    path given;
    std::string resolved;
    std::filesystem::file_time_type mtime{};
    bool loaded = false;
};

std::vector<SourceFile> files;
std::map<std::string, std::unique_ptr<Section>> sections;
std::map<std::string, Var> vars;
std::map<std::string, Snippet::Derivation> derivations;
std::map<std::string, Snippet::CallPtr> calls;

long frame_counter = 0;
TimeObject current_time;
std::string last_error;

int builtins_ref = LUA_NOREF;
int time_ref     = LUA_NOREF;

bool evaluateSection(Section* s);
bool evaluateVar(const std::string& name);

// set while discovery only learns what each section publishes, where a failure
// is about a name that has not run yet rather than about the snippet
bool quiet_reports = false;

void reportOnce(Section* s, const std::string& what) {
    if (quiet_reports) return;
    last_error = what;
    if (s && s->reported) return;
    if (s) s->reported = true;
    spdlog::error("[snippet] {}", what);
}

// ── reading a Lua value into a Snippet::Value ───────────────────────────────
Snippet::Value readValue(lua_State* s, int idx) {
    Snippet::Value out;
    if (lua_isnumber(s, idx)) {
        out.v[0] = lua_tonumber(s, idx);
        out.n = 1;
    } else if (lua_isboolean(s, idx)) {
        out.v[0] = lua_toboolean(s, idx) ? 1 : 0;
        out.n = 1;
    } else if (auto* u = asSVec(s, idx)) {
        int c = comps(u->tag);
        for (int i = 0; i < c; i++) out.v[i] = u->v[i];
        out.n = c;
    } else if (lua_istable(s, idx)) {
        // a plain array of 1..4 numbers is accepted too
        int n = int(lua_objlen(s, idx));
        n = n > 4 ? 4 : n;
        for (int i = 0; i < n; i++) {
            lua_rawgeti(s, idx, i + 1);
            out.v[i] = lua_tonumber(s, -1);
            lua_pop(s, 1);
        }
        out.n = n;
    }
    return out;
}

void pushValue(lua_State* s, const Snippet::Value& v) {
    switch (v.n) {
    case 1: lua_pushnumber(s, v.v[0]); break;
    case 2: pushSVec(s, TAG_V2, v.v[0], v.v[1]); break;
    case 3: pushSVec(s, TAG_V3, v.v[0], v.v[1], v.v[2]); break;
    case 4:
        lua_createtable(s, 4, 0);
        for (int i = 0; i < 4; i++) {
            lua_pushnumber(s, v.v[i]);
            lua_rawseti(s, -2, i + 1);
        }
        break;
    default: lua_pushnil(s); break;
    }
}

// Params and snippets share one namespace, so a name missing here is looked
// for there before it is called unknown.
Snippet::Value fromParams(const std::string& name) {
    Snippet::Value v;
    v.n = Params::read(name, v.v.data());
    return v;
}

std::set<std::string> clash_reported;

void storeVar(const std::string& name, Section* sec, const Snippet::Value& val) {
    // one namespace, so a name held by both sides is a mistake, not a winner
    if (Params::components(name) > 0 && !clash_reported.count(name)) {
        clash_reported.insert(name);
        spdlog::error("[snippet] \"{}\" is published by a section and registered as a "
                      "parameter; they share one namespace, so rename one", name);
    }
    Var& var = vars[name];
    var.sec = sec;
    if (var.frame >= 0) {
        bool moved = var.value.n != val.n;
        for (int i = 0; !moved && i < val.n; i++)
            moved = var.value.v[i] != val.v[i];
        if (moved) var.gen++;
    }
    var.value = val;
    var.frame = frame_counter;
}

// ── name resolution, the section environment's __index ──────────────────────
int env_index(lua_State* s) {
    // upvalue 1 is the builtins table
    const char* key = lua_tostring(s, 2);
    if (!key) return 0;

    lua_pushvalue(s, 2);
    lua_rawget(s, lua_upvalueindex(1));
    if (!lua_isnil(s, -1)) return 1;
    lua_pop(s, 1);

    std::string name = key;
    if (vars.count(name) || derivations.count(name)) {
        if (evaluateVar(name)) {
            pushValue(s, vars[name].value);
            return 1;
        }
        // a section that just failed keeps the value it last published, so a
        // reader of it freezes with it instead of reading zero
        if (auto it = vars.find(name); it != vars.end() && it->second.value.valid()) {
            pushValue(s, it->second.value);
            return 1;
        }
    }
    // a callable section, so one snippet can build on another
    if (auto it = sections.find(name); it != sections.end()
        && evaluateSection(it->second.get()) && it->second->call_ref != LUA_NOREF) {
        lua_rawgeti(s, LUA_REGISTRYINDEX, it->second->call_ref);
        return 1;
    }
    if (auto v = fromParams(name); v.valid()) {
        pushValue(s, v);
        return 1;
    }
    return 0;
}

// ── evaluation ──────────────────────────────────────────────────────────────
bool evaluateSection(Section* s) {
    if (!L || !s) return false;
    // before the memo, which would answer for a section reading itself and hand
    // back the previous frame's value instead of reporting the cycle
    if (s->running) {
        reportOnce(s, "cycle through section '" + s->name + "'");
        s->failed = true;
        return false;
    }
    if (s->last_frame == frame_counter) return !s->failed;

    s->running = true;
    s->last_frame = frame_counter;

    lua_rawgeti(L, LUA_REGISTRYINDEX, s->ref);
    if (lua_pcall(L, 0, 1, 0) != 0) {
        reportOnce(s, std::string(lua_tostring(L, -1) ? lua_tostring(L, -1) : "error"));
        lua_pop(L, 1);
        s->failed = true;
        s->running = false;
        return false;
    }

    s->failed = false;
    if (lua_isfunction(L, -1)) {
        // a callable section, keep the closure, it reads the world at call time
        if (s->call_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, s->call_ref);
        s->call_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else if (lua_istable(L, -1) && lua_objlen(L, -1) == 0) {
        // a dictionary, one variable per key
        s->keys.clear();
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            // lua_isstring() also accepts a number, and reading such a key with
            // lua_tostring() converts it in place, which breaks the lua_next walk
            if (lua_type(L, -2) == LUA_TSTRING) {
                std::string k = lua_tostring(L, -2);
                storeVar(k, s, readValue(L, -1));
                s->keys.push_back(k);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    } else {
        storeVar(s->name, s, readValue(L, -1));
        lua_pop(L, 1);
    }

    s->running = false;
    return true;
}

bool evaluateVar(const std::string& name) {
    auto d = derivations.find(name);
    if (d != derivations.end()) {
        Var& var = vars[name];
        if (var.frame == frame_counter) return true;
        var.frame = frame_counter;   // set first, a derivation reading itself stops here
        storeVar(name, nullptr, d->second(current_time));
        return true;
    }
    auto it = vars.find(name);
    if (it == vars.end() || !it->second.sec) return false;
    if (it->second.frame == frame_counter) return true;
    return evaluateSection(it->second.sec);
}

// ── built-in functions ──────────────────────────────────────────────────────
int l_vec2(lua_State* s) {
    pushSVec(s, TAG_V2, luaL_optnumber(s, 1, 0), luaL_optnumber(s, 2, 0));
    return 1;
}
int l_vec3(lua_State* s) {
    pushSVec(s, TAG_V3, luaL_optnumber(s, 1, 0), luaL_optnumber(s, 2, 0),
             luaL_optnumber(s, 3, 0));
    return 1;
}
int l_complex(lua_State* s) {
    pushSVec(s, TAG_CPX, luaL_optnumber(s, 1, 0), luaL_optnumber(s, 2, 0));
    return 1;
}
int l_cis(lua_State* s) {
    double a = luaL_checknumber(s, 1);
    pushSVec(s, TAG_CPX, std::cos(a), std::sin(a));
    return 1;
}
int l_smoothstep(lua_State* s) {
    lua_pushnumber(s, smoothstep(luaL_checknumber(s, 1)));
    return 1;
}

// operand helper, a number counts as a scalar and an SVec as itself
bool operands(lua_State* s, SVec& a, SVec& b, bool& a_num, bool& b_num) {
    SVec* pa = asSVec(s, 1);
    SVec* pb = asSVec(s, 2);
    a_num = !pa; b_num = !pb;
    if (pa) a = *pa; else { a.tag = 0; a.v[0] = lua_tonumber(s, 1); }
    if (pb) b = *pb; else { b.tag = 0; b.v[0] = lua_tonumber(s, 2); }
    return pa || pb;
}

int tagOf(const SVec& a, bool a_num, const SVec& b, bool b_num) {
    return a_num ? b.tag : (b_num ? a.tag : (a.tag == TAG_CPX || b.tag == TAG_CPX ? TAG_CPX : a.tag));
}

int l_add(lua_State* s) {
    SVec a, b; bool an, bn;
    if (!operands(s, a, b, an, bn)) return 0;
    int tag = tagOf(a, an, b, bn), c = comps(tag);
    double r[3] = {0,0,0};
    for (int i = 0; i < c; i++) r[i] = (an ? a.v[0] : a.v[i]) + (bn ? b.v[0] : b.v[i]);
    pushSVec(s, tag, r[0], r[1], r[2]);
    return 1;
}
int l_sub(lua_State* s) {
    SVec a, b; bool an, bn;
    if (!operands(s, a, b, an, bn)) return 0;
    int tag = tagOf(a, an, b, bn), c = comps(tag);
    double r[3] = {0,0,0};
    for (int i = 0; i < c; i++) r[i] = (an ? a.v[0] : a.v[i]) - (bn ? b.v[0] : b.v[i]);
    pushSVec(s, tag, r[0], r[1], r[2]);
    return 1;
}
int l_unm(lua_State* s) {
    SVec* a = asSVec(s, 1);
    if (!a) return 0;
    pushSVec(s, a->tag, -a->v[0], -a->v[1], -a->v[2]);
    return 1;
}
int l_mul(lua_State* s) {
    SVec a, b; bool an, bn;
    if (!operands(s, a, b, an, bn)) return 0;
    int tag = tagOf(a, an, b, bn);
    if (!an && !bn && tag == TAG_CPX) {
        pushSVec(s, TAG_CPX, a.v[0]*b.v[0] - a.v[1]*b.v[1],
                             a.v[0]*b.v[1] + a.v[1]*b.v[0]);
        return 1;
    }
    // scalar * vector, or componentwise for two vectors
    int c = comps(tag);
    double r[3] = {0,0,0};
    for (int i = 0; i < c; i++) r[i] = (an ? a.v[0] : a.v[i]) * (bn ? b.v[0] : b.v[i]);
    pushSVec(s, tag, r[0], r[1], r[2]);
    return 1;
}
int l_div(lua_State* s) {
    SVec a, b; bool an, bn;
    if (!operands(s, a, b, an, bn)) return 0;
    int tag = tagOf(a, an, b, bn);
    if (!bn && tag == TAG_CPX) {
        double d = b.v[0]*b.v[0] + b.v[1]*b.v[1];
        if (d == 0) return luaL_error(s, "division by zero complex");
        double ar = an ? a.v[0] : a.v[0], ai = an ? 0 : a.v[1];
        pushSVec(s, TAG_CPX, (ar*b.v[0] + ai*b.v[1]) / d,
                             (ai*b.v[0] - ar*b.v[1]) / d);
        return 1;
    }
    int c = comps(tag);
    double r[3] = {0,0,0};
    for (int i = 0; i < c; i++) {
        double den = bn ? b.v[0] : b.v[i];
        r[i] = den == 0 ? 0 : (an ? a.v[0] : a.v[i]) / den;
    }
    pushSVec(s, tag, r[0], r[1], r[2]);
    return 1;
}
int l_tostring(lua_State* s) {
    SVec* a = asSVec(s, 1);
    if (!a) return 0;
    char buf[96];
    if (a->tag == TAG_CPX) snprintf(buf, sizeof buf, "%g%+gi", a->v[0], a->v[1]);
    else if (a->tag == TAG_V3) snprintf(buf, sizeof buf, "(%g, %g, %g)", a->v[0], a->v[1], a->v[2]);
    else snprintf(buf, sizeof buf, "(%g, %g)", a->v[0], a->v[1]);
    lua_pushstring(s, buf);
    return 1;
}

int l_norm(lua_State* s) {
    SVec* a = asSVec(s, 1);
    if (!a) return 0;
    int c = comps(a->tag);
    double n = 0;
    for (int i = 0; i < c; i++) n += a->v[i]*a->v[i];
    lua_pushnumber(s, std::sqrt(n));
    return 1;
}
int l_dot(lua_State* s) {
    SVec* a = asSVec(s, 1); SVec* b = asSVec(s, 2);
    if (!a || !b) return 0;
    int c = comps(a->tag);
    double d = 0;
    for (int i = 0; i < c; i++) d += a->v[i]*b->v[i];
    lua_pushnumber(s, d);
    return 1;
}
int l_cross(lua_State* s) {
    SVec* a = asSVec(s, 1); SVec* b = asSVec(s, 2);
    if (!a || !b) return 0;
    pushSVec(s, TAG_V3, a->v[1]*b->v[2] - a->v[2]*b->v[1],
                        a->v[2]*b->v[0] - a->v[0]*b->v[2],
                        a->v[0]*b->v[1] - a->v[1]*b->v[0]);
    return 1;
}
int l_arg(lua_State* s) {
    SVec* a = asSVec(s, 1);
    if (!a) return 0;
    lua_pushnumber(s, std::atan2(a->v[1], a->v[0]));
    return 1;
}
int l_conj(lua_State* s) {
    SVec* a = asSVec(s, 1);
    if (!a) return 0;
    pushSVec(s, TAG_CPX, a->v[0], -a->v[1]);
    return 1;
}

// fields (.x .y .z .re .im) first, then the methods table in upvalue 1
int l_index(lua_State* s) {
    SVec* a = asSVec(s, 1);
    const char* k = lua_tostring(s, 2);
    if (a && k && k[1] == '\0') {
        if (*k == 'x') { lua_pushnumber(s, a->v[0]); return 1; }
        if (*k == 'y') { lua_pushnumber(s, a->v[1]); return 1; }
        if (*k == 'z') { lua_pushnumber(s, a->v[2]); return 1; }
    }
    if (a && k && !std::strcmp(k, "re")) { lua_pushnumber(s, a->v[0]); return 1; }
    if (a && k && !std::strcmp(k, "im")) { lua_pushnumber(s, a->v[1]); return 1; }
    lua_pushvalue(s, 2);
    lua_rawget(s, lua_upvalueindex(1));
    return 1;
}

// ── the `t` table ───────────────────────────────────────────────────────────
// the TimeObject argument is ignored, `t` is the one the slideshow published
// this frame, so t:afterKeyframe and t.afterKeyframe both work
int l_afterKeyframe(lua_State* s) {
    const char* n = lua_tostring(s, lua_gettop(s));
    lua_pushboolean(s, n && current_time.afterKeyframe(n));
    return 1;
}
int l_beforeKeyframe(lua_State* s) {
    const char* n = lua_tostring(s, lua_gettop(s));
    lua_pushboolean(s, n && current_time.beforeKeyframe(n));
    return 1;
}
int l_atKeyframe(lua_State* s) {
    const char* n = lua_tostring(s, lua_gettop(s));
    lua_pushboolean(s, n && current_time.atKeyframe(n));
    return 1;
}
int l_secondsSince(lua_State* s) {
    const char* n = lua_tostring(s, lua_gettop(s));
    lua_pushnumber(s, n ? current_time.secondsSinceKeyframe(n) : 0);
    return 1;
}

// t:during("a"), t:during("a", "b"), and either with a trailing true for
// windows that must not overlap. Like its neighbours it takes the dotted call
// too, so the names are found from the first string argument rather than from
// a fixed index the `t` of a method call would shift.
int l_during(lua_State* s) {
    const int top = lua_gettop(s);
    int i = 1;
    while (i <= top && lua_type(s, i) != LUA_TSTRING) i++;
    if (i > top) { lua_pushnumber(s, 0); return 1; }
    const char* a = lua_tostring(s, i);
    const char* b = (i + 1 <= top && lua_type(s, i + 1) == LUA_TSTRING)
                        ? lua_tostring(s, i + 1) : nullptr;
    const int flag = b ? i + 2 : i + 1;
    const bool seq = lua_isboolean(s, flag) && lua_toboolean(s, flag);
    lua_pushnumber(s, b ? current_time.duringKeyframe(a, b, seq)
                        : current_time.duringKeyframe(a, seq));
    return 1;
}

int l_slidesSince(lua_State* s) {
    const char* n = lua_tostring(s, lua_gettop(s));
    lua_pushnumber(s, n ? current_time.slidesSinceKeyframe(n) : TimeObject::keyframe_unreached);
    return 1;
}

// ── Params, declared on first use ───────────────────────────────────────────
// declares a parameter with its default and its slider bounds, and returns the
// value. Reading one that already exists needs no call, the bare name works.
int l_param(lua_State* s) {
    const char* k = luaL_checkstring(s, 1);
    lua_pushnumber(s, Params::get(k, luaL_optnumber(s, 2, 0),
                                     luaL_optnumber(s, 3, 0),
                                     luaL_optnumber(s, 4, 0)));
    return 1;
}

void setField(lua_State* s, int tbl, const char* k, lua_CFunction f) {
    lua_pushstring(s, k);
    lua_pushcfunction(s, f);
    lua_settable(s, tbl < 0 ? tbl - 2 : tbl);
}

void buildMetatable(lua_State* s, const char* name, int tag) {
    luaL_newmetatable(s, name);
    int mt = lua_gettop(s);

    setField(s, mt, "__add", l_add);
    setField(s, mt, "__sub", l_sub);
    setField(s, mt, "__mul", l_mul);
    setField(s, mt, "__div", l_div);
    setField(s, mt, "__unm", l_unm);
    setField(s, mt, "__tostring", l_tostring);

    lua_newtable(s);                       // the methods table
    int methods = lua_gettop(s);
    setField(s, methods, "norm", l_norm);
    setField(s, methods, "dot", l_dot);
    if (tag == TAG_V3) setField(s, methods, "cross", l_cross);
    if (tag == TAG_CPX) {
        setField(s, methods, "abs", l_norm);
        setField(s, methods, "arg", l_arg);
        setField(s, methods, "conj", l_conj);
    }
    lua_pushstring(s, "__index");
    lua_pushvalue(s, methods);
    lua_pushcclosure(s, l_index, 1);
    lua_settable(s, mt);
    lua_pop(s, 1);                         // methods
    lua_pop(s, 1);                         // metatable
}

void copyGlobal(lua_State* s, int dst, const char* name) {
    lua_pushstring(s, name);
    lua_getglobal(s, name);
    lua_settable(s, dst);
}

void buildBuiltins() {
    lua_newtable(L);
    int b = lua_gettop(L);

    for (const char* g : {"math", "string", "table", "tostring", "tonumber",
                          "type", "ipairs", "pairs", "select", "error",
                          "assert", "pcall", "unpack", "print"})
        copyGlobal(L, b, g);

    setField(L, b, "vec2", l_vec2);
    setField(L, b, "vec3", l_vec3);
    setField(L, b, "complex", l_complex);
    setField(L, b, "cis", l_cis);
    setField(L, b, "smoothstep", l_smoothstep);
    setField(L, b, "param", l_param);

    // t, fields refreshed every frame, helpers built once
    lua_newtable(L);
    int t = lua_gettop(L);
    setField(L, t, "afterKeyframe", l_afterKeyframe);
    setField(L, t, "beforeKeyframe", l_beforeKeyframe);
    setField(L, t, "atKeyframe", l_atKeyframe);
    setField(L, t, "slidesSince", l_slidesSince);
    setField(L, t, "secondsSince", l_secondsSince);
    setField(L, t, "during", l_during);
    lua_pushvalue(L, t);
    time_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushstring(L, "t");
    lua_pushvalue(L, t);
    lua_settable(L, b);
    lua_pop(L, 1);

    builtins_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

void refreshTime() {
    if (!L || time_ref == LUA_NOREF) return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, time_ref);
    int t = lua_gettop(L);
    auto num = [&](const char* k, double v) {
        lua_pushstring(L, k); lua_pushnumber(L, v); lua_settable(L, t);
    };
    // Only the fields the slideshow actually fills. inner_time counts from a
    // primitive appearing and a snippet is not one, and relative_frame_number
    // is per primitive too, so exposing either would only ever lie.
    num("from_begin", current_time.from_begin);
    num("from_action", current_time.from_action);
    num("delta_time", current_time.delta_time);
    num("absolute_frame_number", current_time.absolute_frame_number);
    num("transition_parameter", current_time.transition_parameter);
    lua_pop(L, 1);
}

// ── loading ─────────────────────────────────────────────────────────────────
// "--- name" opens a section; the body keeps its line numbers so a Lua error
// points into the original file
bool sectionHeader(const std::string& line, std::string& name) {
    size_t i = line.find_first_not_of(" \t");
    if (i == std::string::npos || line.compare(i, 3, "---") != 0) return false;
    i = line.find_first_not_of(" \t", i + 3);
    if (i == std::string::npos) return false;
    size_t j = i;
    while (j < line.size() && (std::isalnum((unsigned char)line[j]) || line[j] == '_')) j++;
    if (j == i) return false;
    if (line.find_first_not_of(" \t\r", j) != std::string::npos) return false;
    name = line.substr(i, j - i);
    return true;
}

// every section name met while loading, compiled or not. One missing was
// deleted from the file, one present but not rebuilt is mid-edit, see rebuild()
std::set<std::string> seen_sections;

void addSection(const std::string& name, const std::string& body,
                const std::string& file, int first_line) {
    seen_sections.insert(name);
    std::string src(first_line > 1 ? size_t(first_line - 1) : 0, '\n');
    src += body;
    std::string chunkname = "@" + file + ":" + name;

    if (luaL_loadbuffer(L, src.c_str(), src.size(), chunkname.c_str()) != 0) {
        spdlog::error("[snippet] {}", lua_tostring(L, -1) ? lua_tostring(L, -1) : "load error");
        last_error = lua_tostring(L, -1) ? lua_tostring(L, -1) : "load error";
        lua_pop(L, 1);
        return;
    }

    // a private environment per section, falling back to builtins then variables
    lua_newtable(L);
    lua_newtable(L);
    lua_pushstring(L, "__index");
    lua_rawgeti(L, LUA_REGISTRYINDEX, builtins_ref);
    lua_pushcclosure(L, env_index, 1);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    lua_setfenv(L, -2);

    // the second of two same-named sections wins, say so and free the first
    if (auto it = sections.find(name); it != sections.end()) {
        spdlog::warn("[snippet] section '{}' is declared twice ({} then {}), "
                     "the last one wins", name, it->second->file, file);
        if (it->second->ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, it->second->ref);
        if (it->second->call_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, it->second->call_ref);
    }

    auto s = std::make_unique<Section>();
    s->name = name;
    s->file = file;
    s->ref  = luaL_ref(L, LUA_REGISTRYINDEX);
    sections[name] = std::move(s);
}

bool loadFile(SourceFile& f) {
    f.resolved = formatPath(f.given);
    std::ifstream in(f.resolved);
    if (!in.is_open()) {
        if (f.loaded) spdlog::error("[snippet] cannot open {}", f.resolved);
        return false;
    }
    std::error_code ec;
    f.mtime = std::filesystem::last_write_time(f.resolved, ec);
    f.loaded = true;

    std::string line, name, body, cur;
    int line_no = 0, start = 0;
    while (std::getline(in, line)) {
        line_no++;
        if (sectionHeader(line, name)) {
            if (!cur.empty()) addSection(cur, body, f.given.string(), start);
            cur = name; body.clear(); start = line_no + 1;
        } else if (!cur.empty()) {
            body += line;
            body += '\n';
        }
    }
    if (!cur.empty()) addSection(cur, body, f.given.string(), start);
    return true;
}

// runs every section once, to learn what each one publishes. Deferred to the
// first frame rather than at load, sections branch on keyframes and the deck
// only registers those once its slides are built.
bool needs_discovery = false;
// no section runs before the first setTime, see get()
bool time_published = false;

void discover() {
    needs_discovery = false;
    // quiet, a section reading a name that has not run yet sees nil and the
    // error that follows says nothing about the file
    quiet_reports = true;
    for (auto& [name, s] : sections) {
        evaluateSection(s.get());
        s->reported = false;
        s->last_frame = -1;
    }
    quiet_reports = false;
    // every name exists now, so whatever still fails is the snippet's own error
    for (auto& [name, s] : sections) {
        if (!s->failed) continue;
        evaluateSection(s.get());
        // a section that fails at discovery still owns a variable of its name,
        // so the real error is reported from the frame that reads it
        if (s->failed && !vars.count(name))
            vars[name].sec = s.get();
        s->reported = false;
        s->last_frame = -1;
    }
    for (auto& [name, c] : calls)
        c->ref = LUA_NOREF;   // re-bound lazily against the new chunks
}

void rebuild() {
    if (!L) return;
    std::map<std::string, std::unique_ptr<Section>> previous;
    previous.swap(sections);
    seen_sections.clear();
    last_error.clear();

    for (auto& f : files) {
        if (loadFile(f)) continue;
        // a save in flight looks like this, so hold what the file owned
        for (auto& [n, s] : previous)
            if (s->file == f.given.string()) seen_sections.insert(n);
    }

    // a section whose new body does not compile keeps the chunk that worked, so
    // saving mid-edit never empties the show; one deleted from the file goes
    for (auto& [n, s] : previous) {
        if (!sections.count(n) && seen_sections.count(n)) {
            sections[n] = std::move(s);
            continue;
        }
        if (s->ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, s->ref);
        if (s->call_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, s->call_ref);
    }

    for (auto it = vars.begin(); it != vars.end(); )
        it = it->second.sec ? vars.erase(it) : std::next(it);
    needs_discovery = true;
}

void ensureDiscovered() {
    if (needs_discovery) discover();
}

} // namespace

// ── public API ──────────────────────────────────────────────────────────────

void Snippet::ensureState() {
    if (L) return;
    L = luaL_newstate();
    luaL_openlibs(L);
    buildMetatable(L, MT_V2, TAG_V2);
    buildMetatable(L, MT_V3, TAG_V3);
    buildMetatable(L, MT_CPX, TAG_CPX);
    buildBuiltins();
}

void Snippet::load(const path& file) {
    ensureState();
    for (auto& f : files)
        if (f.given == file) return;
    files.push_back(SourceFile{file, "", {}, false});
    rebuild();
    spdlog::info("[snippet] loaded {} ({} sections)", file.string(), sections.size());
}

void Snippet::HotReloadIfModified() {
    if (!L || files.empty()) return;
    bool changed = false;
    for (auto& f : files) {
        std::error_code ec;
        std::string resolved = formatPath(f.given);
        auto t = std::filesystem::last_write_time(resolved, ec);
        if (ec) continue;
        if (!f.loaded || t != f.mtime) changed = true;
    }
    if (!changed) return;
    spdlog::info("[snippet] reloading");
    rebuild();
}

void Snippet::setTime(const TimeObject& t) {
    // stamped even with no state, since it marks that a frame has run
    time_published = true;
    if (!L) return;
    current_time = t;
    frame_counter++;
    refreshTime();
    ensureDiscovered();
}

bool Snippet::ready() { return time_published; }
bool Snippet::ok() { return last_error.empty(); }
std::string Snippet::lastError() { return last_error; }

std::vector<std::string> Snippet::names() {
    std::vector<std::string> out;
    out.reserve(vars.size());
    for (auto& [n, v] : vars) out.push_back(n);
    return out;
}

Snippet::Value Snippet::get(const std::string& name) {
    // Sections are not evaluated before the first frame. Building the slides
    // reads positions, and a section run then would see no time and no
    // keyframes. Parameters have no such constraint, so they answer either way.
    if (L && time_published) {
        ensureDiscovered();
        if (evaluateVar(name))
            return vars[name].value;
        // the section failed this frame, hold its last value rather than zero
        if (auto it = vars.find(name); it != vars.end() && it->second.value.valid())
            return it->second.value;
    }
    return fromParams(name);
}

long Snippet::changed(const std::string& name) {
    if (!L || !time_published) return 0;
    ensureDiscovered();
    evaluateVar(name);
    auto it = vars.find(name);
    return it == vars.end() ? 0 : it->second.gen;
}

namespace {

// one entry per dirty() call site. The key is built from pointers the compiler
// already made unique (the file-name literal and any tag), so asking costs no
// allocation.
using WatchKey = std::tuple<const void*, unsigned, const void*>;
std::map<WatchKey, std::vector<long>> watchers;

// true on the first visit, then whenever one of the generations has moved
bool watch(std::vector<long>& seen, const char* const* names, std::size_t n) {
    bool moved = seen.size() != n;
    if (moved) seen.assign(n, -1);
    for (std::size_t i = 0; i < n; i++) {
        long g = Snippet::changed(names[i]);
        if (seen[i] != g) {
            seen[i] = g;
            moved = true;
        }
    }
    return moved;
}

}

bool Snippet::dirty(std::initializer_list<const char*> names, const char* tag,
                    std::source_location where) {
    if (!L) return true;
    ensureDiscovered();
    return watch(watchers[WatchKey{where.file_name(), where.line(), tag}],
                 names.begin(), names.size());
}

bool Snippet::dirty(const char* name, const char* tag, std::source_location where) {
    if (!L) return true;
    ensureDiscovered();
    return watch(watchers[WatchKey{where.file_name(), where.line(), tag}], &name, 1);
}

void Snippet::derive(const std::string& name, std::initializer_list<const char*> deps,
                     const Derivation& f) {
    // the initializer_list does not outlive this call, and the cache has to
    // outlive every frame, so both move into the closure
    auto owned = std::make_shared<std::vector<std::string>>(deps.begin(), deps.end());
    auto seen  = std::make_shared<std::vector<long>>();
    auto cache = std::make_shared<Value>();

    derive(name, Derivation([owned, seen, cache, f](const TimeObject& t) {
        std::vector<const char*> ptrs;
        ptrs.reserve(owned->size());
        for (const auto& d : *owned) ptrs.push_back(d.c_str());
        if (watch(*seen, ptrs.data(), ptrs.size()))
            *cache = f(t);
        return *cache;
    }));
}

void Snippet::derive(const std::string& name, std::initializer_list<const char*> deps,
                     const std::function<scalar(const TimeObject&)>& f) {
    derive(name, deps, Derivation([f](const TimeObject& t) {
        Value v;
        v.v[0] = f(t);
        v.n = 1;
        return v;
    }));
}

void Snippet::derive(const std::string& name, const Derivation& f) {
    ensureState();
    derivations[name] = f;
    vars[name].sec = nullptr;
}
void Snippet::derive(const std::string& name, const std::function<scalar(const TimeObject&)>& f) {
    derive(name, Derivation([f](const TimeObject& t) {
        Value v; v.v[0] = f(t); v.n = 1; return v;
    }));
}
void Snippet::derive(const std::string& name, const std::function<vec2(const TimeObject&)>& f) {
    derive(name, Derivation([f](const TimeObject& t) {
        vec2 r = f(t); Value v; v.v[0] = r(0); v.v[1] = r(1); v.n = 2; return v;
    }));
}
void Snippet::derive(const std::string& name, const std::function<vec(const TimeObject&)>& f) {
    derive(name, Derivation([f](const TimeObject& t) {
        vec r = f(t); Value v; v.v[0] = r(0); v.v[1] = r(1); v.v[2] = r(2); v.n = 3; return v;
    }));
}

Snippet::CallPtr Snippet::resolve(const std::string& name) {
    ensureState();
    auto it = calls.find(name);
    if (it != calls.end()) return it->second;
    auto c = std::make_shared<Call>();
    c->name = name;
    calls[name] = c;
    return c;
}

bool Snippet::invoke(const CallPtr& c, const scalar* in, const int* sizes,
                     int nargs, scalar* out, int nout) {
    if (!L || !c) return false;
    ensureDiscovered();
    if (c->failed_frame == frame_counter) return false;   // latched, broken is fast

    if (c->ref == LUA_NOREF) {
        auto it = sections.find(c->name);
        if (it == sections.end() || !evaluateSection(it->second.get())
            || it->second->call_ref == LUA_NOREF) {
            c->failed_frame = frame_counter;
            if (!c->reported) {
                c->reported = true;
                spdlog::error("[snippet] '{}' is not a callable section", c->name);
            }
            return false;
        }
        c->ref = it->second->call_ref;
        c->reported = false;
    }

    int base = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, c->ref);
    int k = 0;
    for (int i = 0; i < nargs; i++) {
        if (sizes[i] == 1)      lua_pushnumber(L, in[k]);
        else if (sizes[i] == 2) pushSVec(L, TAG_V2, in[k], in[k+1]);
        else                    pushSVec(L, TAG_V3, in[k], in[k+1], in[k+2]);
        k += sizes[i];
    }

    if (lua_pcall(L, nargs, LUA_MULTRET, 0) != 0) {
        last_error = lua_tostring(L, -1) ? lua_tostring(L, -1) : "error";
        if (!c->reported) {
            c->reported = true;
            spdlog::error("[snippet] {}", last_error);
        }
        lua_settop(L, base);
        c->failed_frame = frame_counter;
        return false;
    }

    // several plain numbers cost no allocation, which a hot snippet may prefer
    // over building a vector, "return x, y, z" and "return vec3(x,y,z)" are
    // the same thing here
    int nres = lua_gettop(L) - base;
    bool ok = nres > 0;
    if (nres > 1) {
        for (int i = 0; i < nout; i++)
            out[i] = i < nres ? lua_tonumber(L, base + 1 + i) : 0;
    } else if (nres == 1) {
        Value v = readValue(L, -1);
        ok = v.valid();
        for (int i = 0; i < nout; i++) out[i] = i < v.n ? v.v[i] : 0;
    }
    lua_settop(L, base);
    if (!ok) c->failed_frame = frame_counter;
    return ok;
}

}

