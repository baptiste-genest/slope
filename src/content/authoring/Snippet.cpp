#include "content/authoring/Snippet.h"
#include "content/config/ReloadErrors.h"
#include "content/authoring/Params.h"
#include "content/config/io.h"
#include "math/kernels.h"

#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdarg>
#include <set>

#include <lua.hpp>   // already wraps the C headers in extern "C"

namespace slope {

namespace { struct Section; }

// a stable handle, hot reload swaps the chunk underneath it
struct Snippet::Call {
    std::string name;
    int  ref = LUA_NOREF;
    Section* sec = nullptr;   // bound with ref
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

// what a recorded block of calls read, see Snippet::beginRecord
bool recording = false;
Snippet::Deps record;
long reload_counter = 0;

long frame_counter = 0;
TimeObject current_time;
std::string last_error;

int builtins_ref = LUA_NOREF;
int time_ref     = LUA_NOREF;

bool evaluateSection(Section* s);
bool evaluateVar(const std::string& name);

// lets a built-in say which section misused it
Section* running = nullptr;

// set while discovery only learns what each section publishes, where a failure
// is about a name that has not run yet rather than about the snippet
bool quiet_reports = false;

// per file then per subject, so the editor lists them all
std::map<std::string, std::map<std::string, std::string>> problems;
std::set<std::string> warned;

void publishProblem(const std::string& file, const std::string& subject, const std::string& what) {
    auto& m = problems[file];
    m[subject] = what;
    std::string joined;
    for (auto& [k, msg] : m) joined += (joined.empty() ? "" : "\n") + msg;
    ReloadErrors::report(formatPath(file), "lua", joined);
}

void reportOnce(Section* s, const std::string& what) {
    if (quiet_reports) return;
    last_error = what;
    if (s && s->reported) return;
    if (s) s->reported = true;
    if (s) publishProblem(s->file, s->name, what);
    spdlog::error("[snippet] {}", what);
}

// once per reload, in the section's file or else the first snippet file
void warnOnce(const Section* s, const std::string& key, const std::string& what) {
    if (quiet_reports || !warned.insert(key).second) return;
    if (s) publishProblem(s->file, key, what);
    else if (!files.empty()) publishProblem(files.front().given.string(), key, what);
    spdlog::warn("[snippet] {}", what);
}

// where the Lua code calling a built-in is, as "file:section:line: "
std::string whereCalled(lua_State* s) {
    for (int level = 1; level <= 2; level++) {
        luaL_where(s, level);
        std::string w = lua_tostring(s, -1) ? lua_tostring(s, -1) : "";
        lua_pop(s, 1);
        if (!w.empty()) return w;
    }
    // "return f(x)" is a tail call, which drops the caller's line
    return running ? running->file + ":" + running->name + ": " : "";
}

// like luaL_error, but located at the caller
int raise(lua_State* s, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    lua_pushstring(s, whereCalled(s).c_str());
    lua_pushvfstring(s, fmt, ap);
    va_end(ap);
    lua_concat(s, 2);
    return lua_error(s);
}

void warnHere(lua_State* s, const std::string& key, const std::string& what) {
    warnOnce(running, key, whereCalled(s) + what);
}

bool finite(const Snippet::Value& v) {
    for (int i = 0; i < v.n; i++)
        if (!std::isfinite(v.v[i])) return false;
    return true;
}

void checkFinite(const Section* s, const std::string& name, const Snippet::Value& v) {
    if (!finite(v))
        warnOnce(s, name + "#finite", "'" + name + "' is nan or inf, maybe from a division by zero");
}

// ── reading what Lua returned into a Snippet::Value ─────────────────────────
const char* kShapes = " instead of at most 4 numbers, vectors or arrays";

std::string shape(int n) {
    switch (n) {
    case 0: return "nothing";
    case 1: return "a number";
    case 2: return "a vec2";
    case 3: return "a vec3";
    default: return std::to_string(n) + " numbers";
    }
}

// only a vec2 may widen to a vec3, in the z = 0 plane
bool fits(int n, int want, bool exact) {
    return n == want || (n == 2 && want == 3) || (!exact && n > want);
}

// numbers, booleans, vectors and flat arrays of those, in order
bool flatten(lua_State* s, int idx, Snippet::Value& out, int& total, std::string& err, int depth) {
    auto put = [&](double x) { if (total < 4) out.v[total] = x; total++; };
    switch (lua_type(s, idx)) {
    case LUA_TNUMBER:  put(lua_tonumber(s, idx)); return true;
    case LUA_TBOOLEAN: put(lua_toboolean(s, idx) ? 1 : 0); return true;
    case LUA_TUSERDATA: {
        SVec* u = asSVec(s, idx);
        if (u->tag != TAG_V2 && u->tag != TAG_V3 && u->tag != TAG_CPX) break;
        for (int i = 0; i < comps(u->tag); i++) put(u->v[i]);
        return true;
    }
    case LUA_TTABLE: {
        const int n = int(lua_objlen(s, idx));
        if (n == 0) { err = "a table with named keys only"; return false; }
        if (depth > 0) { err = "an array nested in an array"; return false; }
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(s, idx, i);
            const bool ok = flatten(s, lua_gettop(s), out, total, err, depth + 1);
            lua_pop(s, 1);
            if (!ok) return false;
        }
        return true;
    }
    default: break;
    }
    err = lua_isnil(s, idx) ? "nil" : std::string("a ") + lua_typename(s, lua_type(s, idx));
    return false;
}

// the `count` values from stack index `first` (absolute), as one value
bool readReturn(lua_State* s, int first, int count, Snippet::Value& out, std::string& err) {
    out = Snippet::Value();
    int total = 0;
    for (int i = 0; i < count; i++)
        if (!flatten(s, first + i, out, total, err, 0)) return false;
    if (total > 4) { err = std::to_string(total) + " numbers"; return false; }
    out.n = total;
    return true;
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
        const std::string msg = "'" + name + "' is both published by a section and registered as "
                                "a parameter, which share one namespace, so rename one";
        if (sec) publishProblem(sec->file, name + "#param", msg);
        spdlog::error("[snippet] {}", msg);
    }
    Var& var = vars[name];
    if (sec && var.sec && var.sec != sec)
        warnOnce(sec, name + "#twice", "'" + name + "' is published by both section '"
                 + var.sec->name + "' and section '" + sec->name + "', so the last one run wins");
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

    if (recording) {
        // t is a builtin and returns just below, so this is the only place it
        // can be seen, and reading it at all makes the caller time dependent
        if (std::strcmp(key, "t") == 0) record.time = true;
        else record.names.insert(key);
    }

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
    // a section not run yet, either a callable or a value its run just published
    if (auto it = sections.find(name); it != sections.end() && evaluateSection(it->second.get())) {
        if (it->second->call_ref != LUA_NOREF) {
            lua_rawgeti(s, LUA_REGISTRYINDEX, it->second->call_ref);
            return 1;
        }
        if (auto v = vars.find(name); v != vars.end() && v->second.value.valid()) {
            pushValue(s, v->second.value);
            return 1;
        }
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

    const int base = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, s->ref);
    Section* outer = running;
    running = s;
    const int rc = lua_pcall(L, 0, LUA_MULTRET, 0);
    running = outer;
    if (rc != 0) {
        reportOnce(s, std::string(lua_tostring(L, -1) ? lua_tostring(L, -1) : "error"));
        lua_settop(L, base);
        s->failed = true;
        s->running = false;
        return false;
    }

    s->failed = false;
    const int nres = lua_gettop(L) - base;
    if (nres == 1 && lua_isfunction(L, -1)) {
        // a callable section, keep the closure, it reads the world at call time
        if (s->call_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, s->call_ref);
        s->call_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else if (nres == 1 && lua_istable(L, -1) && lua_objlen(L, -1) == 0) {
        // a dictionary, one variable per key
        s->keys.clear();
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            // lua_isstring() also accepts a number, and reading such a key with
            // lua_tostring() converts it in place, which breaks the lua_next walk
            if (lua_type(L, -2) == LUA_TSTRING) {
                std::string k = lua_tostring(L, -2);
                Snippet::Value v;
                std::string err;
                if (lua_isfunction(L, -1))
                    reportOnce(s, "section '" + s->name + "', key '" + k + "' holds a function,"
                                  " which only works as a section of its own");
                else if (readReturn(L, lua_gettop(L), 1, v, err)) {
                    checkFinite(s, k, v);
                    storeVar(k, s, v);
                }
                else
                    reportOnce(s, "section '" + s->name + "', key '" + k + "' holds " + err + kShapes);
                s->keys.push_back(k);
            }
            lua_pop(L, 1);
        }
    } else if (nres == 0 || (nres == 1 && lua_isnil(L, -1))) {
        storeVar(s->name, s, Snippet::Value());
    } else {
        Snippet::Value v;
        std::string err;
        if (readReturn(L, base + 1, nres, v, err)) {
            checkFinite(s, s->name, v);
            storeVar(s->name, s, v);
        }
        else {
            // not stored, so readers keep the last value that made sense
            reportOnce(s, "section '" + s->name + "' returns " + err + kShapes);
            s->failed = true;
        }
    }
    lua_settop(L, base);

    s->running = false;
    return !s->failed;
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
    if (lua_gettop(s) > 2) return raise(s, "vec2 takes 2 numbers, got %d arguments", lua_gettop(s));
    pushSVec(s, TAG_V2, luaL_optnumber(s, 1, 0), luaL_optnumber(s, 2, 0));
    return 1;
}
int l_vec3(lua_State* s) {
    if (lua_gettop(s) > 3) return raise(s, "vec3 takes 3 numbers, got %d arguments", lua_gettop(s));
    pushSVec(s, TAG_V3, luaL_optnumber(s, 1, 0), luaL_optnumber(s, 2, 0),
             luaL_optnumber(s, 3, 0));
    return 1;
}
int l_complex(lua_State* s) {
    if (lua_gettop(s) > 2) return raise(s, "complex takes 2 numbers, got %d arguments", lua_gettop(s));
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

const char* kindOf(lua_State* s, int i) {
    if (SVec* u = asSVec(s, i))
        return u->tag == TAG_V2 ? "vec2" : u->tag == TAG_V3 ? "vec3" : "complex";
    return lua_typename(s, lua_type(s, i));
}

// a vector meets a number or a vector of its size, anything else is an error
bool operands(lua_State* s, SVec& a, SVec& b, bool& a_num, bool& b_num, const char* op) {
    SVec* pa = asSVec(s, 1);
    SVec* pb = asSVec(s, 2);
    a_num = !pa; b_num = !pb;
    if ((!pa && lua_type(s, 1) != LUA_TNUMBER) || (!pb && lua_type(s, 2) != LUA_TNUMBER)
        || (pa && pb && comps(pa->tag) != comps(pb->tag)))
        raise(s, "cannot %s a %s and a %s", op, kindOf(s, 1), kindOf(s, 2));
    if (pa) a = *pa; else { a.tag = 0; a.v[0] = lua_tonumber(s, 1); }
    if (pb) b = *pb; else { b.tag = 0; b.v[0] = lua_tonumber(s, 2); }
    return pa || pb;
}

// the other argument of a method, which must match the receiver
SVec* sameKind(lua_State* s, SVec* a, const char* method) {
    SVec* b = asSVec(s, 2);
    if (!b || comps(b->tag) != comps(a->tag))
        raise(s, "%s:%s() wants a %s but got a %s", kindOf(s, 1), method, kindOf(s, 1), kindOf(s, 2));
    return b;
}

int l_eq(lua_State* s) {
    SVec* a = asSVec(s, 1); SVec* b = asSVec(s, 2);
    bool eq = a && b && comps(a->tag) == comps(b->tag);
    for (int i = 0; eq && i < comps(a->tag); i++) eq = a->v[i] == b->v[i];
    lua_pushboolean(s, eq);
    return 1;
}

int tagOf(const SVec& a, bool a_num, const SVec& b, bool b_num) {
    return a_num ? b.tag : (b_num ? a.tag : (a.tag == TAG_CPX || b.tag == TAG_CPX ? TAG_CPX : a.tag));
}

int l_add(lua_State* s) {
    SVec a, b; bool an, bn;
    if (!operands(s, a, b, an, bn, "add")) return 0;
    int tag = tagOf(a, an, b, bn), c = comps(tag);
    double r[3] = {0,0,0};
    for (int i = 0; i < c; i++) r[i] = (an ? a.v[0] : a.v[i]) + (bn ? b.v[0] : b.v[i]);
    pushSVec(s, tag, r[0], r[1], r[2]);
    return 1;
}
int l_sub(lua_State* s) {
    SVec a, b; bool an, bn;
    if (!operands(s, a, b, an, bn, "subtract")) return 0;
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
    if (!operands(s, a, b, an, bn, "multiply")) return 0;
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
    if (!operands(s, a, b, an, bn, "divide")) return 0;
    int tag = tagOf(a, an, b, bn);
    if (!bn && tag == TAG_CPX) {
        double d = b.v[0]*b.v[0] + b.v[1]*b.v[1];
        if (d == 0) return raise(s, "division by zero complex");
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
    if (!a) return raise(s, "norm is a method, called as v:norm()");
    int c = comps(a->tag);
    double n = 0;
    for (int i = 0; i < c; i++) n += a->v[i]*a->v[i];
    lua_pushnumber(s, std::sqrt(n));
    return 1;
}
int l_dot(lua_State* s) {
    SVec* a = asSVec(s, 1);
    if (!a) return raise(s, "dot is a method, called as v:dot(w)");
    SVec* b = sameKind(s, a, "dot");
    int c = comps(a->tag);
    double d = 0;
    for (int i = 0; i < c; i++) d += a->v[i]*b->v[i];
    lua_pushnumber(s, d);
    return 1;
}
int l_cross(lua_State* s) {
    SVec* a = asSVec(s, 1);
    if (!a) return raise(s, "cross is a method, called as v:cross(w)");
    SVec* b = sameKind(s, a, "cross");
    pushSVec(s, TAG_V3, a->v[1]*b->v[2] - a->v[2]*b->v[1],
                        a->v[2]*b->v[0] - a->v[0]*b->v[2],
                        a->v[0]*b->v[1] - a->v[1]*b->v[0]);
    return 1;
}
int l_arg(lua_State* s) {
    SVec* a = asSVec(s, 1);
    if (!a) return raise(s, "arg is a method, called as v:arg()");
    lua_pushnumber(s, std::atan2(a->v[1], a->v[0]));
    return 1;
}
int l_conj(lua_State* s) {
    SVec* a = asSVec(s, 1);
    if (!a) return raise(s, "conj is a method, called as v:conj()");
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
    if (lua_isnil(s, -1))
        return raise(s, "a %s has no field '%s'", kindOf(s, 1), k ? k : "?");
    return 1;
}

// ── the `t` table ───────────────────────────────────────────────────────────
// an unknown name answers false like in C++, and is said with its line
void checkKeyframe(lua_State* s, const char* n) {
    if (TimeObject::keyframes && !TimeObject::keyframes->count(n))
        warnHere(s, std::string("keyframe#") + n, "unknown keyframe \"" + std::string(n) + "\"");
}

// the name ends the arguments, whether called t:f("a") or t.f("a")
const char* keyframeArg(lua_State* s) {
    if (lua_type(s, lua_gettop(s)) != LUA_TSTRING)
        raise(s, "a keyframe name is expected, as in t:afterKeyframe(\"name\")");
    const char* n = lua_tostring(s, lua_gettop(s));
    checkKeyframe(s, n);
    return n;
}

// the TimeObject argument is ignored, `t` is the one the slideshow published
// this frame, so t:afterKeyframe and t.afterKeyframe both work
int l_afterKeyframe(lua_State* s) {
    const char* n = keyframeArg(s);
    lua_pushboolean(s, n && current_time.afterKeyframe(n));
    return 1;
}
int l_beforeKeyframe(lua_State* s) {
    const char* n = keyframeArg(s);
    lua_pushboolean(s, n && current_time.beforeKeyframe(n));
    return 1;
}
int l_atKeyframe(lua_State* s) {
    const char* n = keyframeArg(s);
    lua_pushboolean(s, n && current_time.atKeyframe(n));
    return 1;
}
int l_secondsSinceKeyframe(lua_State* s) {
    const char* n = keyframeArg(s);
    lua_pushnumber(s, n ? current_time.secondsSinceKeyframe(n) : 0);
    return 1;
}

// t:duringKeyframe("a"), the same with ("a", "b"), and either with a trailing
// true for windows that must not overlap. Like its neighbours it takes the
// dotted call too, so the names are found from the first string argument
// rather than from a fixed index the `t` of a method call would shift.
int l_duringKeyframe(lua_State* s) {
    const int top = lua_gettop(s);
    int i = 1;
    while (i <= top && lua_type(s, i) != LUA_TSTRING) i++;
    if (i > top) return raise(s, "a keyframe name is expected, as in t:sinceKeyframe(\"name\")");
    const char* a = lua_tostring(s, i);
    const char* b = (i + 1 <= top && lua_type(s, i + 1) == LUA_TSTRING)
                        ? lua_tostring(s, i + 1) : nullptr;
    checkKeyframe(s, a);
    if (b) checkKeyframe(s, b);
    const int flag = b ? i + 2 : i + 1;
    const bool seq = lua_isboolean(s, flag) && lua_toboolean(s, flag);
    lua_pushnumber(s, b ? current_time.duringKeyframe(a, b, seq)
                        : current_time.duringKeyframe(a, seq));
    return 1;
}

// t:sinceKeyframe("a"), with an optional trailing true for a steeper rise.
// Same shape as t:duringKeyframe with one name, minus the fall.
int l_sinceKeyframe(lua_State* s) {
    const int top = lua_gettop(s);
    int i = 1;
    while (i <= top && lua_type(s, i) != LUA_TSTRING) i++;
    if (i > top) return raise(s, "a keyframe name is expected, as in t:sinceKeyframe(\"name\")");
    const char* a = lua_tostring(s, i);
    checkKeyframe(s, a);
    const bool seq = lua_isboolean(s, i + 1) && lua_toboolean(s, i + 1);
    lua_pushnumber(s, current_time.sinceKeyframe(a, seq));
    return 1;
}

int l_slidePosition(lua_State* s) {
    lua_pushnumber(s, current_time.slidePosition());
    return 1;
}

int l_slidesSinceKeyframe(lua_State* s) {
    const char* n = keyframeArg(s);
    lua_pushnumber(s, n ? current_time.slidesSinceKeyframe(n) : TimeObject::keyframe_unreached);
    return 1;
}

// ── Params, declared on first use ───────────────────────────────────────────
// declares a parameter with its default and its slider bounds, and returns the
// value. Reading one that already exists needs no call, the bare name works.
int l_param(lua_State* s) {
    const char* k = luaL_checkstring(s, 1);
    if (int c = Params::components(k); c > 1)
        return raise(s, "param(\"%s\") is a number, but \"%s\" is already a parameter of "
                             "%d components", k, k, c);
    const scalar lo = luaL_optnumber(s, 3, 0), hi = luaL_optnumber(s, 4, 0);
    if (lo > hi)
        warnHere(s, std::string("param#") + k, "param(\"" + std::string(k) + "\") "
                 + (lua_gettop(s) < 4 ? "gives a min without a max" : "has its min above its max")
                 + ", so its slider has no bounds (the order is param(name, default, min, max))");
    lua_pushnumber(s, Params::get(k, luaL_optnumber(s, 2, 0), lo, hi));
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
    setField(s, mt, "__eq", l_eq);
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
    setField(L, t, "slidesSinceKeyframe", l_slidesSinceKeyframe);
    setField(L, t, "secondsSinceKeyframe", l_secondsSinceKeyframe);
    setField(L, t, "duringKeyframe", l_duringKeyframe);
    setField(L, t, "sinceKeyframe", l_sinceKeyframe);
    setField(L, t, "slidePosition", l_slidePosition);
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
    num("slide_progress", current_time.slide_progress);
    lua_pop(L, 1);
}

// ── loading ─────────────────────────────────────────────────────────────────
// "--- name" opens a section; the body keeps its line numbers so a Lua error
// points into the original file. A name may be grouped with "/", so a section
// can own "fig/xrange" outright.
bool sectionHeader(const std::string& line, std::string& name) {
    size_t i = line.find_first_not_of(" \t");
    if (i == std::string::npos || line.compare(i, 3, "---") != 0) return false;
    i = line.find_first_not_of(" \t", i + 3);
    if (i == std::string::npos) return false;
    size_t j = i;
    while (j < line.size() && (std::isalnum((unsigned char)line[j])
                               || line[j] == '_' || line[j] == '/')) j++;
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
        publishProblem(file, name, last_error);
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
        const std::string msg = "section '" + name + "' is declared twice ("
                                + it->second->file + " then " + file + "), so the last one wins";
        publishProblem(file, name + "#twice", msg);
        spdlog::warn("[snippet] {}", msg);
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
    bool said_orphan = false;
    auto warnLine = [&](const std::string& what) {
        const std::string msg = f.given.string() + ":" + std::to_string(line_no) + ": " + what;
        publishProblem(f.given.string(), "line " + std::to_string(line_no), msg);
        spdlog::warn("[snippet] {}", msg);
    };
    while (std::getline(in, line)) {
        line_no++;
        if (sectionHeader(line, name)) {
            if (!cur.empty()) addSection(cur, body, f.given.string(), start);
            cur = name; body.clear(); start = line_no + 1;
            continue;
        }
        // "--- my-name" is no header and would glue its body onto the section above
        const size_t i = line.find_first_not_of(" \t");
        if (i != std::string::npos && line.compare(i, 3, "---") == 0) {
            const size_t a = line.find_first_not_of(" \t-", i);
            const size_t b = a == std::string::npos ? a : line.find_first_of(" \t\r", a);
            if (a != std::string::npos && (b == std::string::npos
                                           || line.find_first_not_of(" \t\r", b) == std::string::npos))
                warnLine("'" + line.substr(i) + "' is not a section header, because a name only "
                         "has letters, digits, _ and /");
        }
        if (!cur.empty()) {
            body += line;
            body += '\n';
        } else if (!said_orphan && i != std::string::npos && line.compare(i, 2, "--") != 0) {
            said_orphan = true;
            warnLine("code before the first \"--- name\" line belongs to no section and never runs");
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
    // all run again unmuted, since a warning from a working section is real too
    for (auto& [name, s] : sections) {
        evaluateSection(s.get());
        // a section that fails at discovery still owns a variable of its name,
        // so the real error is reported from the frame that reads it
        if (s->failed && !vars.count(name))
            vars[name].sec = s.get();
        // already reported in this pass, so later frames stay silent
        s->last_frame = -1;
    }
    for (auto& [name, c] : calls) {
        c->ref = LUA_NOREF;   // re-bound lazily against the new chunks
        c->sec = nullptr;
    }
    reload_counter++;
}

void rebuild() {
    if (!L) return;
    std::map<std::string, std::unique_ptr<Section>> previous;
    previous.swap(sections);
    seen_sections.clear();
    last_error.clear();
    problems.clear();
    warned.clear();
    for (auto& f : files)
        ReloadErrors::clear(formatPath(f.given), "lua");

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
    if (!std::filesystem::exists(formatPath(file)))
        throw std::runtime_error("snippets: cannot open \"" + file.string() + "\"");
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

std::vector<path> Snippet::WatchedFiles() {
    std::vector<path> out;
    for (auto& f : files) {
        std::error_code ec;
        path p = f.resolved.empty() ? path(formatPath(f.given)) : path(f.resolved);
        auto v = std::filesystem::weakly_canonical(p, ec);
        if (ec) v = p;
        bool seen = false;
        for (auto& o : out) if (o == v) { seen = true; break; }
        if (!seen) out.push_back(v);
    }
    return out;
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

bool Snippet::hasSection(const std::string& name) {
    ensureDiscovered();
    return sections.count(name);
}

bool Snippet::loadedAny() { return !files.empty(); }

bool Snippet::provides(const std::string& name) {
    ensureDiscovered();
    return vars.count(name) || derivations.count(name) || sections.count(name);
}
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

Snippet::Value Snippet::get(const std::string& name, int want) {
    const Value v = get(name);
    // an RGB read as a color takes alpha 1, see Value::operator RGBA
    if (!time_published || fits(v.n, want, true) || (v.n == 3 && want == 4)) return v;
    auto it = vars.find(name);
    const Section* s = it == vars.end() ? nullptr : it->second.sec;
    const std::string key = name + "@" + std::to_string(want);
    if (!v.valid()) {
        if (it == vars.end())
            warnOnce(nullptr, key, "no section, value or parameter called '" + name + "'");
        else if (s && !s->failed)
            warnOnce(s, key, "section '" + s->name + "' gives '" + name + "' no value where "
                             + shape(want) + " is read");
        return v;
    }
    const std::string who = s ? "'" + name + "' (section '" + s->name + "')"
                              : "'" + name + "'";
    warnOnce(s, key, who + " holds " + shape(v.n) + " where " + shape(want) + " is read, "
                     + (v.n < want ? "so the missing numbers are zeros" : "so the extra ones are dropped"));
    return v;
}

void Snippet::beginRecord() {
    record = Deps();
    recording = true;
}

Snippet::Deps Snippet::endRecord() {
    recording = false;
    return record;
}

long Snippet::reloads() {return reload_counter;}

long Snippet::stateOf(const Deps& d) {
    long h = reload_counter*1000003L;
    for (const auto& n : d.names) {
        h = h*31 + changed(n);
        // a parameter is not a section, so its generation never moves. Fold in
        // what it currently reads instead
        scalar v[4];
        if (int n_comp = Params::read(n, v); n_comp > 0)
            for (int i = 0; i < n_comp; i++)
                h = h*31 + (long)std::llround(v[i]*1e6);
    }
    return h;
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

namespace {

Section* sectionOf(const std::string& name) {
    auto it = sections.find(name);
    return it == sections.end() ? nullptr : it->second.get();
}

// once per handle and reload, in the section's file when there is one
void reportCall(Snippet::Call& c, const std::string& what) {
    if (c.reported) return;
    c.reported = true;
    last_error = what;
    if (Section* s = sectionOf(c.name)) publishProblem(s->file, c.name + "#call", what);
    else if (!files.empty()) publishProblem(files.front().given.string(), c.name + "#call", what);
    spdlog::error("[snippet] {}", what);
}

}

bool Snippet::invoke(const CallPtr& c, const scalar* in, const int* sizes,
                     int nargs, scalar* out, int nout, bool exact) {
    if (!L || !c) return false;
    ensureDiscovered();
    if (c->failed_frame == frame_counter) return false;   // latched, broken is fast

    if (c->ref == LUA_NOREF) {
        Section* s = sectionOf(c->name);
        if (!s || !evaluateSection(s) || s->call_ref == LUA_NOREF) {
            c->failed_frame = frame_counter;
            if (!s)
                reportCall(*c, "no section called '" + c->name + "'");
            else if (!s->failed)
                reportCall(*c, "section '" + c->name + "' returns a value but is called as a "
                               "function, which needs \"return function(...) ... end\"");
            return false;
        }
        c->ref = s->call_ref;
        c->sec = s;
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

    Section* outer = running;
    running = c->sec;
    const int rc = lua_pcall(L, nargs, LUA_MULTRET, 0);
    running = outer;
    if (rc != 0) {
        reportCall(*c, lua_tostring(L, -1) ? lua_tostring(L, -1) : "error");
        lua_settop(L, base);
        c->failed_frame = frame_counter;
        return false;
    }

    // plain numbers cost no allocation, unlike a vector
    Value v;
    std::string err;
    const bool ok = readReturn(L, base + 1, lua_gettop(L) - base, v, err);
    lua_settop(L, base);
    if (!ok || !v.valid()) {
        reportCall(*c, "the function of section '" + c->name + "' returns "
                       + (ok ? std::string("nothing") : err) + kShapes);
        c->failed_frame = frame_counter;
        return false;
    }
    if (!finite(v))
        warnOnce(c->sec, c->name + "#finite", "the function of section '" + c->name
                 + "' returns nan or inf, maybe from a division by zero");
    if (!fits(v.n, nout, exact))
        warnOnce(c->sec, c->name + "#" + std::to_string(nout),
                 "the function of section '" + c->name + "' returns " + shape(v.n)
                 + " where " + shape(nout) + " is read, "
                 + (v.n < nout ? "so the missing numbers are zeros" : "so the extra ones are dropped"));
    for (int i = 0; i < nout; i++) out[i] = i < v.n ? v.v[i] : 0;
    return true;
}

}

namespace slope {

vec LiveVec::value() const
{
    return live() ? Snippet::get(snippet, 3).v3() : fixed;
}

scalar LiveScalar::value() const
{
    return live() ? Snippet::get(snippet, 1).num() : fixed;
}

std::string LiveVec::key() const
{
    if (live())
        return snippet;
    return std::to_string(fixed(0)) + "," + std::to_string(fixed(1))
         + "," + std::to_string(fixed(2));
}

void SnippetTexture::configure(const Spec &spec)
{
    const bool same = sp.fn == spec.fn && sp.res_u == spec.res_u && sp.res_v == spec.res_v
        && sp.u == spec.u && sp.v == spec.v && sp.components == spec.components
        && sp.when == spec.when;
    sp = spec;
    sp.components = std::clamp(sp.components,1,4);
    sp.res_u = std::max(1,sp.res_u);
    sp.res_v = std::max(1,sp.res_v);
    if (!same)
        sampled = false;
}

void SnippetTexture::sample()
{
    const int w = sp.res_u, h = std::max(1,sp.res_v), c = std::clamp(sp.components,1,4);
    const bool flat = h == 1;
    samples.assign(std::size_t(w)*h*c,0.f);

    auto call = Snippet::resolve(sp.fn);
    const int sizes[1] = {flat ? 1 : 2};
    scalar out[4];

    // the read set of this pass decides whether it is ever run again
    Snippet::beginRecord();
    for (int j = 0; j < h; j++){
        // texel centres, so the domain ends sit half a texel inside the edges
        const scalar y = h > 1 ? sp.v(0) + (sp.v(1)-sp.v(0))*(j+0.5)/h : 0;
        for (int i = 0; i < w; i++){
            const scalar x = sp.u(0) + (sp.u(1)-sp.u(0))*(i+0.5)/w;
            const scalar in[2] = {x,y};
            // keeping fewer numbers than returned is what `components` is for
            if (!Snippet::invoke(call,in,sizes,1,out,c,false))
                continue;   // a failing section leaves zeros rather than nothing
            float* q = samples.data() + (std::size_t(j)*w + i)*c;
            for (int k = 0; k < c; k++)
                q[k] = float(out[k]);
        }
    }
    deps = Snippet::endRecord();
    state = Snippet::stateOf(deps);
    sampled = true;
}

bool SnippetTexture::update()
{
    if (!Snippet::ready())
        return false;
    if (!sampled){
        sample();
        return true;
    }
    if (sp.when == Spec::When::Once)
        return false;
    if (sp.when == Spec::When::Always || deps.time){
        sample();
        return true;
    }
    // nothing it was built from has moved, so the samples still stand
    if (Snippet::stateOf(deps) == state)
        return false;
    sample();
    return true;
}

}
