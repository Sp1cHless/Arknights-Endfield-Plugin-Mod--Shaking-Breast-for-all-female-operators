#pragma once
// common/safe_unity.h — SEH-wrapped Unity invocation helpers.  All Unity API
// calls from animation callbacks go through here (baseline rule: SEH + no
// allocation).  Depends on the resolved method pointers in il2cpp_api.h.
#include "quat.h"
#include "../il2cpp/il2cpp_api.h"

static inline Quat SafeGetLocalRotation(void *transform) {
  if (!transform || !g_transform_get_localRotation) return QuatIdentity();
  __try {
    void *boxed = Invoke(g_transform_get_localRotation, transform);
    if (!boxed) return QuatIdentity();
    float *d = (float *)((char *)boxed + IL2CPP_BOXED_DATA);
    return {d[0], d[1], d[2], d[3]};
  } __except (1) {
    return QuatIdentity();
  }
}

static inline void SafeSetLocalRotation(void *transform, Quat q) {
  if (!transform || !g_transform_set_localRotation) return;
  __try {
    void *params[] = {&q};
    Invoke(g_transform_set_localRotation, transform, params);
  } __except (1) {
  }
}

static inline void *SafeGetComponentTransform(void *component) {
  if (!component || !g_component_get_transform) return nullptr;
  __try {
    return Invoke(g_component_get_transform, component);
  } __except (1) {
    return nullptr;
  }
}

static inline void *SafeGetComponentGameObject(void *component) {
  if (!component || !g_component_get_gameObject) return nullptr;
  __try {
    return Invoke(g_component_get_gameObject, component);
  } __except (1) {
    return nullptr;
  }
}

// Recursive child search by exact name (baseline: maxDepth 40).
static inline void *SafeFindChildRecursive(void *transform, const char *targetName,
                                           int maxDepth) {
  if (!transform || maxDepth <= 0) return nullptr;
  __try {
    char name[256] = "";
    if (g_object_get_name) {
      void *ns = Invoke(g_object_get_name, transform);
      if (ns) ReadStrUtf8(ns, name, sizeof(name));
    }
    if (strcmp(name, targetName) == 0) return transform;

    void *countBoxed = Invoke(g_transform_get_childCount, transform);
    int count = countBoxed ? *(int *)((char *)countBoxed + IL2CPP_BOXED_DATA) : 0;
    for (int i = 0; i < count; i++) {
      void *params[] = {&i};
      void *child = Invoke(g_transform_GetChild, transform, params);
      if (child) {
        void *found = SafeFindChildRecursive(child, targetName, maxDepth - 1);
        if (found) return found;
      }
    }
  } __except (1) {
  }
  return nullptr;
}

// Read an object's name into buf ("" on failure).
static inline void SafeGetObjectName(void *obj, char *buf, int sz) {
  buf[0] = 0;
  if (!obj || !g_object_get_name) return;
  __try {
    void *ns = Invoke(g_object_get_name, obj);
    if (ns) ReadStrUtf8(ns, buf, sz);
  } __except (1) {
    buf[0] = 0;
  }
}
