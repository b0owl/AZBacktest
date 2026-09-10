#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <cctype>

namespace formula {

/// @brief a named scalar the expression can reference, e.g. x, i, mean
struct Var {
    std::string name;
    double value = 0.0;
};

/// @brief hook for functions the host supplies, like p(k) over a data column
/// set handled to true if you recognise the name, and err if the call was wrong
/// @param ud whatever was handed to eval()
typedef double (*ExtraFn)(void* ud, const std::string& name,
                          const std::vector<double>& args,
                          bool& handled, std::string& err);

/// @brief result of one evaluation, ok() means value is meaningful
struct Result {
    double value = 0.0;
    std::string error;   ///< empty on success
    bool ok() const { return error.empty(); }
};

struct Parser {
    const char* p = nullptr;
    const std::vector<Var>* vars = nullptr;
    std::string err;

    ExtraFn extra = nullptr;   ///< host functions, tried after the builtins
    void*   extraUd = nullptr;

    void skip() { while (*p == ' ' || *p == '\t') p++; }

    /// @brief consume `tok` if it is next, after whitespace
    bool eat(const char* tok) {
        skip();
        size_t n = 0;
        while (tok[n]) n++;
        for (size_t i = 0; i < n; i++)
            if (p[i] != tok[i]) return false;
        // don't let "<" swallow the first half of "<="
        if ((tok[0] == '<' || tok[0] == '>' || tok[0] == '=' || tok[0] == '!')
            && n == 1 && p[1] == '=') return false;
        p += n;
        return true;
    }

    void fail(const std::string& msg) { if (err.empty()) err = msg; }

    double parseExpr() { return parseTernary(); }

    double parseTernary() {
        double c = parseOr();
        skip();
        if (*p == '?') {
            p++;
            double a = parseExpr();
            skip();
            if (*p != ':') { fail("expected ':' in ?: expression"); return 0.0; }
            p++;
            double b = parseExpr();
            return c != 0.0 ? a : b;
        }
        return c;
    }

    double parseOr() {
        double l = parseAnd();
        for (;;) {
            if (err.size()) return 0.0;
            if (eat("||")) { double r = parseAnd(); l = (l != 0.0 || r != 0.0) ? 1.0 : 0.0; }
            else return l;
        }
    }

    double parseAnd() {
        double l = parseCmp();
        for (;;) {
            if (err.size()) return 0.0;
            if (eat("&&")) { double r = parseCmp(); l = (l != 0.0 && r != 0.0) ? 1.0 : 0.0; }
            else return l;
        }
    }

    double parseCmp() {
        double l = parseAdd();
        for (;;) {
            if (err.size()) return 0.0;
            if      (eat("<=")) { double r = parseAdd(); l = l <= r ? 1.0 : 0.0; }
            else if (eat(">=")) { double r = parseAdd(); l = l >= r ? 1.0 : 0.0; }
            else if (eat("==")) { double r = parseAdd(); l = l == r ? 1.0 : 0.0; }
            else if (eat("!=")) { double r = parseAdd(); l = l != r ? 1.0 : 0.0; }
            else if (eat("<"))  { double r = parseAdd(); l = l <  r ? 1.0 : 0.0; }
            else if (eat(">"))  { double r = parseAdd(); l = l >  r ? 1.0 : 0.0; }
            else return l;
        }
    }

    double parseAdd() {
        double l = parseMul();
        for (;;) {
            if (err.size()) return 0.0;
            skip();
            if      (*p == '+') { p++; l += parseMul(); }
            else if (*p == '-') { p++; l -= parseMul(); }
            else return l;
        }
    }

    double parseMul() {
        double l = parseUnary();
        for (;;) {
            if (err.size()) return 0.0;
            skip();
            if (*p == '*') { p++; l *= parseUnary(); }
            else if (*p == '/') {
                p++;
                double r = parseUnary();
                if (r == 0.0) { fail("division by zero"); return 0.0; }
                l /= r;
            } else if (*p == '%') {
                p++;
                double r = parseUnary();
                if (r == 0.0) { fail("modulo by zero"); return 0.0; }
                l = std::fmod(l, r);
            } else return l;
        }
    }

    double parseUnary() {
        skip();
        if (*p == '-') { p++; return -parseUnary(); }
        if (*p == '+') { p++; return  parseUnary(); }
        if (*p == '!') { p++; return parseUnary() == 0.0 ? 1.0 : 0.0; }
        return parsePower();
    }

    // right associative so 2^3^2 is 2^(3^2), the usual maths reading
    double parsePower() {
        double base = parsePrimary();
        skip();
        if (*p == '^') { p++; return std::pow(base, parseUnary()); }
        return base;
    }

    double parsePrimary() {
        skip();
        if (err.size()) return 0.0;

        if (*p == '(') {
            p++;
            double v = parseExpr();
            skip();
            if (*p != ')') { fail("missing ')'"); return 0.0; }
            p++;
            return v;
        }

        if (std::isdigit((unsigned char)*p) || (*p == '.' && std::isdigit((unsigned char)p[1]))) {
            char* endp = nullptr;
            double v = std::strtod(p, &endp);
            p = endp;
            return v;
        }

        if (std::isalpha((unsigned char)*p) || *p == '_') {
            const char* start = p;
            while (std::isalnum((unsigned char)*p) || *p == '_') p++;
            std::string name(start, (size_t)(p - start));
            skip();
            if (*p == '(') { p++; return parseCall(name); }
            return lookup(name);
        }

        fail(*p ? std::string("unexpected '") + *p + "'" : std::string("unexpected end of expression"));
        return 0.0;
    }

    double lookup(const std::string& name) {
        if (vars)
            for (const Var& v : *vars)
                if (v.name == name) return v.value;
        if (name == "pi") return 3.14159265358979323846;
        if (name == "e")  return 2.71828182845904523536;
        fail("unknown name '" + name + "'");
        return 0.0;
    }

    /// @brief read the args after an already eaten '(' and run `name`
    double parseCall(const std::string& name) {
        std::vector<double> a;
        skip();
        if (*p != ')') {
            for (;;) {
                a.push_back(parseExpr());
                if (err.size()) return 0.0;
                skip();
                if (*p == ',') { p++; continue; }
                break;
            }
        }
        skip();
        if (*p != ')') { fail("missing ')' after " + name + "()"); return 0.0; }
        p++;
        return apply(name, a);
    }

    double need(const std::string& name, const std::vector<double>& a, size_t n) {
        if (a.size() != n) {
            fail(name + "() takes " + std::to_string(n) + " argument"
                 + (n == 1 ? "" : "s") + ", got " + std::to_string(a.size()));
            return 0.0;
        }
        return 1.0;
    }

    double apply(const std::string& n, const std::vector<double>& a) {
        if (n == "abs")   { if (!need(n,a,1)) return 0; return std::fabs(a[0]); }
        if (n == "sqrt")  { if (!need(n,a,1)) return 0;
                            if (a[0] < 0) { fail("sqrt() of a negative number"); return 0; }
                            return std::sqrt(a[0]); }
        if (n == "log")   { if (!need(n,a,1)) return 0;
                            if (a[0] <= 0) { fail("log() needs a positive number"); return 0; }
                            return std::log(a[0]); }
        if (n == "log10") { if (!need(n,a,1)) return 0;
                            if (a[0] <= 0) { fail("log10() needs a positive number"); return 0; }
                            return std::log10(a[0]); }
        if (n == "exp")   { if (!need(n,a,1)) return 0; return std::exp(a[0]); }
        if (n == "floor") { if (!need(n,a,1)) return 0; return std::floor(a[0]); }
        if (n == "ceil")  { if (!need(n,a,1)) return 0; return std::ceil(a[0]); }
        if (n == "round") { if (!need(n,a,1)) return 0; return std::floor(a[0] + 0.5); }
        if (n == "sign")  { if (!need(n,a,1)) return 0; return a[0] > 0 ? 1.0 : (a[0] < 0 ? -1.0 : 0.0); }
        if (n == "sin")   { if (!need(n,a,1)) return 0; return std::sin(a[0]); }
        if (n == "cos")   { if (!need(n,a,1)) return 0; return std::cos(a[0]); }
        if (n == "tan")   { if (!need(n,a,1)) return 0; return std::tan(a[0]); }
        if (n == "tanh")  { if (!need(n,a,1)) return 0; return std::tanh(a[0]); }
        if (n == "pow")   { if (!need(n,a,2)) return 0; return std::pow(a[0], a[1]); }
        if (n == "min")   { if (!need(n,a,2)) return 0; return a[0] < a[1] ? a[0] : a[1]; }
        if (n == "max")   { if (!need(n,a,2)) return 0; return a[0] > a[1] ? a[0] : a[1]; }
        if (n == "clamp") { if (!need(n,a,3)) return 0;
                            return a[0] < a[1] ? a[1] : (a[0] > a[2] ? a[2] : a[0]); }
        if (n == "if")    { if (!need(n,a,3)) return 0; return a[0] != 0.0 ? a[1] : a[2]; }

        // not a builtin, give the host a shot before giving up
        if (extra) {
            bool handled = false;
            std::string e;
            const double v = extra(extraUd, n, a, handled, e);
            if (handled) {
                if (!e.empty()) fail(e);
                return v;
            }
        }
        fail("unknown function '" + n + "()'");
        return 0.0;
    }
};

/// @brief evaluate `expr` with `vars` in scope
/// @return the value, or a Result holding the error if it won't parse
inline Result eval(const std::string& expr, const std::vector<Var>& vars,
                   ExtraFn extra = nullptr, void* extraUd = nullptr) {
    Result r;
    Parser ps;
    ps.p = expr.c_str();
    ps.vars = &vars;
    ps.extra = extra;
    ps.extraUd = extraUd;

    r.value = ps.parseExpr();
    ps.skip();
    if (ps.err.empty() && *ps.p)
        ps.err = std::string("unexpected trailing '") + *ps.p + "'";

    r.error = ps.err;
    if (!r.error.empty()) r.value = 0.0;
    else if (std::isnan(r.value) || std::isinf(r.value)) r.error = "result is not a finite number";
    return r;
}

/// @brief just check it parses, for live feedback next to a text box
/// @return empty if it's fine
inline std::string check(const std::string& expr, const std::vector<Var>& vars) {
    return eval(expr, vars).error;
}

} // namespace formula
