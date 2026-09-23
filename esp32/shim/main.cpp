#include "Arduino.h"
SerialT Serial;
#include "../micromouse_esp32/micromouse_esp32.ino"
int main() { setup(); Serial.fakeInput = 'g'; loop(); Serial.fakeInput = 'g'; loop(); Serial.fakeInput = 'g'; loop(); Serial.fakeInput = 'g'; loop(); Serial.fakeInput = 'p'; loop(); return 0; }
