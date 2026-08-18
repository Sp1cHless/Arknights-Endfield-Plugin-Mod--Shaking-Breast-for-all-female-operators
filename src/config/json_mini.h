#pragma once
// config/json_mini.h — minimal JSON parser (startup only; never in animation
// callbacks).  Supports objects, arrays, strings, numbers, bool, null.
// No allocation after parse; values are copied into caller structs by the
// ConfigLoader.  ~200 lines, no external dependency.
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace jsonmini {

struct Value {
  enum Type { Null, Bool, Number, String, Array, Object } type = Null;
  bool b = false;
  double num = 0.0;
  std::string str;
  std::vector<Value> arr;
  std::vector<std::pair<std::string, Value>> obj;

  const Value *Find(const char *key) const {
    if (type != Object) return nullptr;
    for (auto &kv : obj)
      if (kv.first == key) return &kv.second;
    return nullptr;
  }
  const Value *FindIn(const char *key, const char *key2) const {
    const Value *v = Find(key);
    return v ? v->Find(key2) : nullptr;
  }
  bool GetBool(const char *key, bool def) const {
    const Value *v = Find(key);
    return (v && v->type == Bool) ? v->b : def;
  }
  double GetNumber(const char *key, double def) const {
    const Value *v = Find(key);
    return (v && v->type == Number) ? v->num : def;
  }
  std::string GetString(const char *key, const char *def) const {
    const Value *v = Find(key);
    return (v && v->type == String) ? v->str : std::string(def ? def : "");
  }
};

struct Parser {
  const char *p = nullptr;

  void SkipWs() {
    while (*p && isspace((unsigned char)*p)) p++;
  }

  bool ParseString(std::string &out) {
    if (*p != '"') return false;
    p++;
    out.clear();
    while (*p && *p != '"') {
      if (*p == '\\' && p[1]) {
        p++;
        switch (*p) {
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case 'r': out += '\r'; break;
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case '/': out += '/'; break;
          case 'u': {
            // minimal \uXXXX (ASCII-only conversion)
            unsigned code = 0;
            for (int i = 0; i < 4 && isxdigit((unsigned char)p[1]); i++) {
              p++;
              code = code * 16 + (isdigit((unsigned char)*p) ? *p - '0'
                                  : (tolower((unsigned char)*p) - 'a' + 10));
            }
            if (code < 128) out += (char)code;
            else out += '?';
            break;
          }
          default: out += *p; break;
        }
        p++;
      } else {
        out += *p;
        p++;
      }
    }
    if (*p != '"') return false;
    p++;
    return true;
  }

  bool ParseNumber(double &out) {
    const char *start = p;
    if (*p == '-') p++;
    while (isdigit((unsigned char)*p)) p++;
    if (*p == '.') {
      p++;
      while (isdigit((unsigned char)*p)) p++;
    }
    if (*p == 'e' || *p == 'E') {
      p++;
      if (*p == '+' || *p == '-') p++;
      while (isdigit((unsigned char)*p)) p++;
    }
    if (p == start) return false;
    char tmp[64];
    size_t n = (size_t)(p - start);
    if (n >= sizeof(tmp)) return false;
    memcpy(tmp, start, n);
    tmp[n] = 0;
    out = atof(tmp);
    return true;
  }

  bool ParseValue(Value &v) {
    SkipWs();
    if (!*p) return false;
    if (*p == '{') {
      v.type = Value::Object;
      p++;
      SkipWs();
      if (*p == '}') { p++; return true; }
      while (*p) {
        SkipWs();
        std::string key;
        if (!ParseString(key)) return false;
        SkipWs();
        if (*p != ':') return false;
        p++;
        Value child;
        if (!ParseValue(child)) return false;
        v.obj.emplace_back(key, child);
        SkipWs();
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; return true; }
        return false;
      }
      return false;
    }
    if (*p == '[') {
      v.type = Value::Array;
      p++;
      SkipWs();
      if (*p == ']') { p++; return true; }
      while (*p) {
        Value child;
        if (!ParseValue(child)) return false;
        v.arr.push_back(child);
        SkipWs();
        if (*p == ',') { p++; continue; }
        if (*p == ']') { p++; return true; }
        return false;
      }
      return false;
    }
    if (*p == '"') {
      v.type = Value::String;
      return ParseString(v.str);
    }
    if (strncmp(p, "true", 4) == 0) { v.type = Value::Bool; v.b = true; p += 4; return true; }
    if (strncmp(p, "false", 5) == 0) { v.type = Value::Bool; v.b = false; p += 5; return true; }
    if (strncmp(p, "null", 4) == 0) { v.type = Value::Null; p += 4; return true; }
    if (*p == '-' || isdigit((unsigned char)*p)) {
      v.type = Value::Number;
      return ParseNumber(v.num);
    }
    return false;
  }

  bool Parse(Value &root) {
    SkipWs();
    if (!ParseValue(root)) return false;
    SkipWs();
    return *p == 0;  // full consumption
  }
};

// Returns false on syntax error; root left untouched on failure.
static bool ParseJson(const char *text, Value &root) {
  Parser pr;
  pr.p = text;
  return pr.Parse(root);
}

static bool LoadJsonFile(const char *path, Value &root) {
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0 || sz > 1024 * 1024) { fclose(f); return false; }
  std::string buf((size_t)sz, '\0');
  size_t rd = fread(&buf[0], 1, (size_t)sz, f);
  fclose(f);
  if (rd != (size_t)sz) return false;
  return ParseJson(buf.c_str(), root);
}

}  // namespace jsonmini
