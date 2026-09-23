// Minimal Arduino shim so the sketch (dry-run configuration) can be compiled
// and exercised with g++ on a desktop. Time is virtual: delay() advances a
// clock instead of sleeping, so a whole slot replays in milliseconds.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <deque>

#define PROGMEM
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0

inline uint8_t pgm_read_byte(const uint8_t* p) { return *p; }

// ---- virtual clock
extern uint64_t shimClockUs;
inline uint32_t millis() { return (uint32_t)(shimClockUs / 1000); }
inline uint32_t micros() { return (uint32_t)shimClockUs; }
inline void delay(uint32_t ms) { shimClockUs += (uint64_t)ms * 1000; }
inline void delayMicroseconds(uint32_t us) { shimClockUs += us; }
inline void yield() { shimClockUs += 100; }

// ---- pins: the button is scripted, everything else is inert
struct ShimPress { uint64_t fromUs, toUs; };
extern int       shimButtonPin;
extern ShimPress shimPresses[8];
extern int       shimPressCount;
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int  digitalRead(int pin) {
  if (pin == shimButtonPin)
    for (int i = 0; i < shimPressCount; ++i)
      if (shimClockUs >= shimPresses[i].fromUs && shimClockUs < shimPresses[i].toUs) return LOW;
  return HIGH;
}
// Schedule a press of the button `holdMs` long, starting `delayMs` from now on the virtual clock.
inline void shimPressButton(uint32_t holdMs, uint32_t delayMs = 0) {
  if (shimPressCount == 8) { for (int i = 1; i < 8; ++i) shimPresses[i - 1] = shimPresses[i]; shimPressCount = 7; }
  const uint64_t from = shimClockUs + (uint64_t)delayMs * 1000;
  shimPresses[shimPressCount++] = ShimPress{ from, from + (uint64_t)holdMs * 1000 };
}

// ---- LED
extern uint8_t shimLedR, shimLedG, shimLedB;
extern int shimLedWrites;
inline void shimLedWrite(uint8_t r, uint8_t g, uint8_t b) { shimLedR = r; shimLedG = g; shimLedB = b; ++shimLedWrites; }

// ---- Serial with a scripted input queue
struct SerialT {
  std::deque<char> input;
  void begin(long) {}
  int  available() { return (int)input.size(); }
  int  read() { if (input.empty()) return -1; const char c = input.front(); input.pop_front(); return c; }
  void type(const char* s) { while (*s) input.push_back(*s++); }
  void print(const char* s) { fputs(s, stdout); }
  void print(char c) { putchar(c); }
  void print(int v) { printf("%d", v); }
  void print(unsigned v) { printf("%u", v); }
  void print(long v) { printf("%ld", v); }
  void print(double v) { printf("%.2f", v); }
  void println(const char* s) { puts(s); }
  void println(char c) { printf("%c\n", c); }
  void println(int v) { printf("%d\n", v); }
  void println() { putchar('\n'); }
  template<typename... A> void printf(const char* f, A... a) { ::printf(f, a...); }
};
extern SerialT Serial;
