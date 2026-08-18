#pragma once
// common/time_utils.h — tick counters and a QPC second clock.
#include <windows.h>

static inline DWORD NowMs() { return GetTickCount(); }

struct SecondClock {
  double qpf = 0.0;
  double last = 0.0;

  void Init() {
    if (qpf == 0.0) {
      LARGE_INTEGER f;
      QueryPerformanceFrequency(&f);
      qpf = (double)f.QuadPart;
    }
  }

  double Now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / qpf;
  }
};

// Frame-once QPC gate: returns true only when > 0.5ms elapsed since last
// accept (used to dedupe same-frame multi-instance callbacks for state
// advance; writes remain unconditional per baseline).
struct FrameGate {
  double lastT = 0.0;
  double qpf = 0.0;

  // Returns dt (seconds) if a new frame is detected, 0 otherwise.
  double Tick() {
    if (qpf == 0.0) {
      LARGE_INTEGER f;
      QueryPerformanceFrequency(&f);
      qpf = (double)f.QuadPart;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double tNow = (double)now.QuadPart / qpf;
    if (lastT == 0.0) {
      lastT = tNow;
      return 0.0;
    }
    double dt = tNow - lastT;
    if (dt > 0.0005) {
      if (dt > 0.05) dt = 0.05;
      lastT = tNow;
      return dt;
    }
    return 0.0;
  }
};
