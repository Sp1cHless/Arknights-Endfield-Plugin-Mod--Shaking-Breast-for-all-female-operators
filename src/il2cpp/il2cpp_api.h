#pragma once
// il2cpp/il2cpp_api.h — IL2CPP export resolution, class/method/field lookup,
// invocation helpers, string reads, and the global Unity method pointers the
// motion core depends on.  Migrated from the verified baseline il2cpp_api.h.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "MinHook.h"

#include "../common/logger.h"

// ---- IL2CPP exports (resolved at startup via GetProcAddress) ----
#define D(ret, name, ...)                                                      \
  typedef ret(*t_##name)(__VA_ARGS__);                                         \
  static t_##name name = nullptr
D(void *, il2cpp_domain_get);
D(void *, il2cpp_thread_attach, void *);
D(void, il2cpp_thread_detach, void *);
D(void **, il2cpp_domain_get_assemblies, void *, size_t *);
D(void *, il2cpp_assembly_get_image, void *);
D(const char *, il2cpp_image_get_name, void *);
D(size_t, il2cpp_image_get_class_count, void *);
D(void *, il2cpp_image_get_class, void *, size_t);
D(void *, il2cpp_class_get_methods, void *, void **);
D(const char *, il2cpp_method_get_name, void *);
D(uint32_t, il2cpp_method_get_param_count, void *);
D(const char *, il2cpp_class_get_name, void *);
D(const char *, il2cpp_class_get_namespace, void *);
D(void *, il2cpp_object_get_class, void *);
D(void *, il2cpp_class_from_name, void *, const char *, const char *);
D(void *, il2cpp_method_get_param, void *, uint32_t);
D(const char *, il2cpp_type_get_name, void *);
D(void *, il2cpp_class_get_fields, void *, void **);
D(const char *, il2cpp_field_get_name, void *);
D(size_t, il2cpp_field_get_offset, void *);
D(int, il2cpp_field_get_flags, void *);
D(void *, il2cpp_class_get_method_from_name, void *, const char *, int);
D(void *, il2cpp_runtime_invoke, void *, void *, void **, void **);
D(void *, il2cpp_class_get_parent, void *);
D(void *, il2cpp_class_get_nested_types, void *, void **);
D(void *, il2cpp_class_get_declaring_type, void *);
D(void, il2cpp_field_static_get_value, void *, void *);
D(void, il2cpp_field_get_value, void *, void *, void *);
D(void, il2cpp_field_set_value, void *, void *, void *);
D(void *, il2cpp_field_get_type, void *);
D(int, il2cpp_type_get_type, void *);
D(void *, il2cpp_method_get_return_type, void *);
D(void *, il2cpp_class_from_type, void *);
D(void *, il2cpp_resolve_icall, const char *);
D(void *, il2cpp_string_new, const char *);
D(void *, il2cpp_class_get_type, void *);
D(void *, il2cpp_type_get_object, void *);
D(void *, il2cpp_object_new, void *);
D(void *, il2cpp_array_new_specific, void *, size_t);
D(void *, il2cpp_array_new, void *, size_t);
D(uint32_t, il2cpp_gchandle_new, void *, bool);
D(void *, il2cpp_gchandle_get_target, uint32_t);
D(void, il2cpp_gchandle_free, uint32_t);
#undef D

// IL2CPP object layout constants (verified on this game build).
#define IL2CPP_STR_LEN     0x10
#define IL2CPP_STR_CHARS   0x14
#define IL2CPP_ARRAY_LEN   0x18
#define IL2CPP_ARRAY_DATA  0x20
#define IL2CPP_BOXED_DATA  16

static HMODULE g_gaMod = nullptr;

static bool ResolveIl2Cpp() {
  g_gaMod = GetModuleHandleW(L"GameAssembly.dll");
  if (!g_gaMod) return false;
#define R(n) n = (t_##n)GetProcAddress(g_gaMod, #n)
  R(il2cpp_domain_get);
  R(il2cpp_thread_attach);
  R(il2cpp_thread_detach);
  R(il2cpp_domain_get_assemblies);
  R(il2cpp_assembly_get_image);
  R(il2cpp_image_get_name);
  R(il2cpp_image_get_class_count);
  R(il2cpp_image_get_class);
  R(il2cpp_class_get_methods);
  R(il2cpp_method_get_name);
  R(il2cpp_method_get_param_count);
  R(il2cpp_class_get_name);
  R(il2cpp_class_get_namespace);
  R(il2cpp_object_get_class);
  R(il2cpp_class_from_name);
  R(il2cpp_method_get_param);
  R(il2cpp_type_get_name);
  R(il2cpp_class_get_fields);
  R(il2cpp_field_get_name);
  R(il2cpp_field_get_offset);
  R(il2cpp_field_get_flags);
  R(il2cpp_class_get_method_from_name);
  R(il2cpp_runtime_invoke);
  R(il2cpp_class_get_parent);
  R(il2cpp_class_get_nested_types);
  R(il2cpp_class_get_declaring_type);
  R(il2cpp_field_static_get_value);
  R(il2cpp_field_get_value);
  R(il2cpp_field_set_value);
  R(il2cpp_field_get_type);
  R(il2cpp_type_get_type);
  R(il2cpp_method_get_return_type);
  R(il2cpp_class_from_type);
  R(il2cpp_resolve_icall);
  R(il2cpp_string_new);
  R(il2cpp_class_get_type);
  R(il2cpp_type_get_object);
  R(il2cpp_object_new);
  R(il2cpp_array_new_specific);
  R(il2cpp_array_new);
  R(il2cpp_gchandle_new);
  R(il2cpp_gchandle_get_target);
  R(il2cpp_gchandle_free);
#undef R
  return il2cpp_domain_get && il2cpp_class_get_methods && il2cpp_method_get_name;
}

// ---- Lookup helpers ----
static void *FindMethod(void *k, const char *n, int pc) {
  if (!k) return nullptr;
  void *it = nullptr, *m;
  while ((m = il2cpp_class_get_methods(k, &it))) {
    const char *mn = il2cpp_method_get_name(m);
    if (mn && strcmp(mn, n) == 0 && (int)il2cpp_method_get_param_count(m) == pc)
      return m;
  }
  return nullptr;
}

static void *FindMethodInHierarchy(void *k, const char *n, int pc) {
  void *cur = k;
  int depth = 0;
  while (cur && depth < 10) {
    void *m = FindMethod(cur, n, pc);
    if (m) return m;
    cur = il2cpp_class_get_parent ? il2cpp_class_get_parent(cur) : nullptr;
    depth++;
  }
  return nullptr;
}

// FindClass requires the assembly list (nullptr fails on this game — SOT bug).
static void *FindClass(const char *ns, const char *n, void **a, size_t c) {
  for (size_t i = 0; i < c; i++) {
    void *img = il2cpp_assembly_get_image(a[i]);
    if (!img) continue;
    size_t cc = il2cpp_image_get_class_count(img);
    for (size_t j = 0; j < cc; j++) {
      void *k = il2cpp_image_get_class(img, j);
      if (!k) continue;
      const char *cn = il2cpp_class_get_name(k);
      if (!cn || strcmp(cn, n) != 0) continue;
      const char *kns = il2cpp_class_get_namespace ? il2cpp_class_get_namespace(k) : "";
      if (strcmp(kns ? kns : "", ns) == 0) return k;
    }
  }
  return nullptr;
}

// Find a field offset by name walking the class hierarchy (returns -1).
static int FindFieldOffsetInHierarchy(void *klass, const char *fieldName) {
  if (!klass || !il2cpp_class_get_parent) return -1;
  void *cur = klass;
  int depth = 0;
  while (cur && depth < 10) {
    void *it = nullptr, *f;
    while ((f = il2cpp_class_get_fields(cur, &it))) {
      const char *fn = il2cpp_field_get_name(f);
      if (fn && strcmp(fn, fieldName) == 0)
        return (int)il2cpp_field_get_offset(f);
    }
    cur = il2cpp_class_get_parent(cur);
    depth++;
  }
  return -1;
}

// Find a field offset by one of several candidate names (returns -1).
static int FindFieldOffsetInHierarchyNames(void *klass, const char **names,
                                           int nameCount) {
  if (!klass || !il2cpp_class_get_parent) return -1;
  void *cur = klass;
  int depth = 0;
  while (cur && depth < 10) {
    void *it = nullptr, *f;
    while ((f = il2cpp_class_get_fields(cur, &it))) {
      const char *fn = il2cpp_field_get_name(f);
      if (!fn) continue;
      for (int i = 0; i < nameCount; i++) {
        if (strcmp(fn, names[i]) == 0)
          return (int)il2cpp_field_get_offset(f);
      }
    }
    cur = il2cpp_class_get_parent(cur);
    depth++;
  }
  return -1;
}

// ---- Invocation / string reads ----
static void *Invoke(void *method, void *obj, void **params = nullptr) {
  if (!method) return nullptr;
  __try {
    void *exc = nullptr;
    return il2cpp_runtime_invoke(method, obj, params, &exc);
  } __except (1) {
    return nullptr;
  }
}

static int ReadStrUtf8(void *s, char *b, int sz) {
  __try {
    if (!s) { b[0] = 0; return -1; }
    int32_t l = *(int32_t *)((char *)s + IL2CPP_STR_LEN);
    if (l <= 0 || l > 2048) { b[0] = 0; return -1; }
    wchar_t *c = (wchar_t *)((char *)s + IL2CPP_STR_CHARS);
    int written = WideCharToMultiByte(CP_UTF8, 0, c, l, b, sz - 1, NULL, NULL);
    b[written] = 0;
    return written;
  } __except (1) {
    b[0] = 0;
    return -2;
  }
}

static int UnboxInt32(void *boxed) {
  __try {
    return boxed ? *(int32_t *)((char *)boxed + IL2CPP_BOXED_DATA) : 0;
  } __except (1) {
    return 0;
  }
}

// ---- Global Unity method pointers (resolved once at startup) ----
static void *g_transformClass = nullptr;
static void *g_animatorClass = nullptr;
static void *g_transform_get_localRotation = nullptr;
static void *g_transform_set_localRotation = nullptr;
static void *g_transform_get_childCount = nullptr;
static void *g_transform_GetChild = nullptr;
static void *g_transform_get_parent = nullptr;
static void *g_component_get_gameObject = nullptr;
static void *g_component_get_transform = nullptr;
static void *g_object_get_name = nullptr;

// Resolve the core Unity classes/methods the motion core needs.
// Returns false if a REQUIRED symbol is missing.
static bool ResolveCoreUnitySymbols() {
  void *domain = il2cpp_domain_get();
  if (!domain) return false;
  size_t ac = 0;
  void **asms = il2cpp_domain_get_assemblies(domain, &ac);
  if (!asms || !ac) return false;

  g_transformClass = FindClass("UnityEngine", "Transform", asms, ac);
  if (!g_transformClass) { Log("[REFLECT] Transform class NOT found"); return false; }
  g_transform_get_localRotation = FindMethod(g_transformClass, "get_localRotation", 0);
  g_transform_set_localRotation = FindMethod(g_transformClass, "set_localRotation", 1);
  g_transform_get_childCount = FindMethod(g_transformClass, "get_childCount", 0);
  g_transform_GetChild = FindMethod(g_transformClass, "GetChild", 1);
  g_transform_get_parent = FindMethod(g_transformClass, "get_parent", 0);

  void *objectClass = FindClass("UnityEngine", "Object", asms, ac);
  g_object_get_name = objectClass ? FindMethod(objectClass, "get_name", 0) : nullptr;

  void *componentClass = FindClass("UnityEngine", "Component", asms, ac);
  if (componentClass) {
    g_component_get_gameObject = FindMethod(componentClass, "get_gameObject", 0);
    g_component_get_transform = FindMethod(componentClass, "get_transform", 0);
  }
  g_animatorClass = FindClass("UnityEngine", "Animator", asms, ac);

  bool ok = g_transform_get_localRotation && g_transform_set_localRotation &&
            g_transform_get_childCount && g_transform_GetChild &&
            g_object_get_name && g_component_get_transform;
  Log("[REFLECT] core: transform=%p setLR=%p name=%p compTF=%p -> %s",
      (void *)g_transform_set_localRotation, (void *)g_transform_set_localRotation,
      (void *)g_object_get_name, (void *)g_component_get_transform,
      ok ? "PASS" : "FAIL");
  return ok;
}
