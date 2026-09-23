// Minimal shim so the .ino can be syntax/type-checked with g++ on a desktop.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#define PROGMEM
#define INPUT_PULLUP 2
#define OUTPUT 1
#define HIGH 1
#define LOW 0
inline uint8_t pgm_read_byte(const uint8_t* p) { return *p; }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int  digitalRead(int) { return HIGH; }
inline void delay(int ms) { usleep(ms * 1000); }
struct SerialT {
  int  fakeInput = -1;
  void begin(long) {}
  int  available() { return fakeInput >= 0; }
  int  read() { int c = fakeInput; fakeInput = -1; return c; }
  void print(const char* s) { fputs(s, stdout); }
  void print(char c) { putchar(c); }
  void print(int v) { printf("%d", v); }
  void println(const char* s) { puts(s); }
  void println(char c) { printf("%c\n", c); }
  void println() { putchar('\n'); }
  template<typename... A> void printf(const char* f, A... a) { ::printf(f, a...); }
};
extern SerialT Serial;
