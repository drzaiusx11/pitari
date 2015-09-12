#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <stdint.h>
#include <readline/readline.h>
#include <readline/history.h>

#define TRUE 1
#define FALSE 0
#define BOOL uint8_t

// time in microseconds to wait for data lines to stabilize
// most ROM datasheets I found specify min of 24uS
#define READ_DELAY 45

// wiring pi's gpio numbers don't match rpi's silkscreen
// so make some easier to use names
#define	GPIO_17 0
#define	GPIO_18 1
#define	GPIO_27 2
#define	GPIO_22 3
#define	GPIO_23 4
#define	GPIO_24 5
#define	GPIO_25 6
#define	GPIO_4 7
#define	GPIO_SDA 8
#define	GPIO_SCL 9
#define	GPIO_CE0 10
#define	GPIO_CE1 11
#define	GPIO_MOSI 12
#define	GPIO_MISO 13
#define	GPIO_SCLK 14
#define	GPIO_TX 15
#define	GPIO_RX 16
#define	GPIO_5 21
#define	GPIO_6 22
#define	GPIO_13 23
#define	GPIO_19 24
#define	GPIO_26 25
#define	GPIO_12 26
#define	GPIO_16 27
#define	GPIO_20 28
#define	GPIO_21 29

// data bus
#define	D0 GPIO_TX
#define	D1 GPIO_RX
#define	D2 GPIO_18
#define	D3 GPIO_22
#define	D4 GPIO_24
#define	D5 GPIO_25
#define	D6 GPIO_CE0
#define	D7 GPIO_CE1

// chip select (aka A12)
#define CS GPIO_23

// 74LVC245's buffer dir pin
#define	BUFDIR GPIO_27

// address bus
#define	A0 GPIO_26
#define	A1 GPIO_19
#define	A2 GPIO_13
#define	A3 GPIO_6
#define	A4 GPIO_5
#define	A5 GPIO_SCLK
#define	A6 GPIO_MISO
#define	A7 GPIO_MOSI
#define	A8 GPIO_21
#define	A9 GPIO_20
#define	A10 GPIO_16
#define	A11 GPIO_12

/////////////////CART PINOUT/////////////////
// GND.5V.A8.A9.A11.A10.EN.D7.D6.D5.D4.D3  //
// A7..A6.A5.A4.A3..A2..A1.A0.D0.D1.D2.GND //
/////////////////////////////////////////////

// number of bytes for bank compares
#define NCOMPARES 128

// bank switching/mapper types
#define MAPPER_E0 1
#define MAPPER_E7 2
#define MAPPER_F4 3
#define MAPPER_F6 4
#define MAPPER_F8 5
#define MAPPER_FE 6
#define MAPPER_2K 7
#define MAPPER_4K 8

/// function prototypes ///
BOOL arrayEq(uint8_t* arr1, uint8_t* arr2, int len);
void bankSwitch(uint16_t hotspot);
int detectMapper();
void dumpCart(FILE* file, uint8_t mapper);
BOOL isE7(void);
BOOL isE0(void);
BOOL isF4(void);
BOOL isF6(void);
BOOL isF8(void);
BOOL isFE(void);
BOOL is2K(void);
const char* printMapper(uint8_t mapper);
uint8_t readFromAddress(uint16_t addr);
uint8_t* readFromAddressRange(uint16_t addr, int len);
void setup(void);
void writeAddress(uint16_t addr);
void writeToFile(FILE* file, uint16_t from, uint16_t to);

void writeToFile(FILE* file, uint16_t from, uint16_t to) {
  for(int addr = from; addr <= to; addr++) {
    uint8_t b = readFromAddress(addr); 
    fwrite(&b, 1, 1, file);
  }
}

void bankSwitch(uint16_t hotspot) {
  digitalWrite(CS, LOW);
  delayMicroseconds(READ_DELAY);
  writeAddress(hotspot);
  digitalWrite(CS, HIGH);
  delayMicroseconds(READ_DELAY);
}

void setup(void) {
  pinMode (D0, INPUT);
  pinMode (D1, INPUT);
  pinMode (D2, INPUT);
  pinMode (D3, INPUT);
  pinMode (D4, INPUT);
  pinMode (D5, INPUT);
  pinMode (D6, INPUT);
  pinMode (D7, INPUT);

  pinMode (A0, OUTPUT);
  pinMode (A1, OUTPUT);
  pinMode (A2, OUTPUT);
  pinMode (A3, OUTPUT);
  pinMode (A4, OUTPUT);
  pinMode (A5, OUTPUT);
  pinMode (A6, OUTPUT);
  pinMode (A7, OUTPUT);
  pinMode (A8, OUTPUT);
  pinMode (A9, OUTPUT);
  pinMode (A10, OUTPUT);
  pinMode (A11, OUTPUT);

  pinMode (CS, OUTPUT);
  pinMode (BUFDIR, OUTPUT);
  digitalWrite(CS, HIGH);
  digitalWrite(BUFDIR, LOW);
}

void writeAddress(uint16_t addr){
  digitalWrite(A11, 0x1 & (addr >> 11));
  digitalWrite(A10, 0x1 & (addr >> 10));
  digitalWrite(A9, 0x1 & (addr >> 9));
  digitalWrite(A8, 0x1 & (addr >> 8));
  digitalWrite(A7, 0x1 & (addr >> 7));
  digitalWrite(A6, 0x1 & (addr >> 6));
  digitalWrite(A5, 0x1 & (addr >> 5));
  digitalWrite(A4, 0x1 & (addr >> 4));
  digitalWrite(A3, 0x1 & (addr >> 3));
  digitalWrite(A2, 0x1 & (addr >> 2));
  digitalWrite(A1, 0x1 & (addr >> 1));
  digitalWrite(A0, 0x1 & addr);
  delayMicroseconds(READ_DELAY);
}

uint8_t readFromAddress(uint16_t addr) {
  writeAddress(addr);

  uint8_t d0 = digitalRead(D0);
  uint8_t d1 = digitalRead(D1);
  uint8_t d2 = digitalRead(D2);
  uint8_t d3 = digitalRead(D3);
  uint8_t d4 = digitalRead(D4);
  uint8_t d5 = digitalRead(D5);
  uint8_t d6 = digitalRead(D6);
  uint8_t d7 = digitalRead(D7);

  uint8_t val = (d7 << 7) + (d6 << 6) + (d5 << 5) + (d4 << 4) + (d3 << 3) + (d2 << 2) + (d1 << 1) + d0;

  return val;
}

uint8_t* readFromAddressRange(uint16_t addr, int len) {
  uint8_t *buf = (uint8_t*)malloc(sizeof(uint8_t) * len);

  for(int i = 0; i < len; i++) {
    buf[i] = readFromAddress(addr + i); 
  }

  return buf;
}

BOOL arrayEq(uint8_t* arr1, uint8_t* arr2, int len) {
  BOOL same = TRUE;
  for (int i = 0; i < len; i++) {
    if (arr1[i] != arr2[i]) {
      same = FALSE;
      break;
    }
  }
  return same;
}

const char* printMapper(uint8_t mapper) {
  if (mapper == MAPPER_E0) {
    return "E0";
  }
  if (mapper == MAPPER_E7) {
    return "E7";
  }
  if (mapper == MAPPER_F4) {
    return "F4";
  }
  if (mapper == MAPPER_F6) {
    return "F6";
  }
  if (mapper == MAPPER_F8) {
    return "F8";
  }
  if (mapper == MAPPER_FE) {
    return "FE";
  }
  if (mapper == MAPPER_2K) {
    return "2K";
  }
  return "4K"; // default to 4k for unknowns
}

int detectMapper() {
  if (isE0()) {
    return MAPPER_E0;
  }
  if (isE7()) {
    return MAPPER_E7;
  }
  if (isF6()) {
    return MAPPER_F6;
  }
  if (isF8()) {
    return MAPPER_F8;
  }
  if (isFE()) {
    return MAPPER_FE;
  }
  if (is2K()) {
    return MAPPER_2K;
  }
  return MAPPER_4K; // default to 4k for unknowns
  //return "unknown mapper";
}

void dumpCart(FILE* romfile, uint8_t mapper) {
  switch(mapper) {
    case MAPPER_2K:
      writeToFile(romfile, 0, 0x800);
    break;
    case MAPPER_4K:
      writeToFile(romfile, 0, 0xFFF);
    break;
    case MAPPER_E0:
      // grab banks 0 - 7 from slot 0
      for (int hotspot = 0xFE0; hotspot <= 0xFE7; hotspot++) {
        bankSwitch(hotspot); // select bank
        writeToFile(romfile, 0, 0x3FF);
      }
    break;
    case MAPPER_E7:
      // grab banks 0 - 6 from slot 0
      for (int hotspot = 0xFE0; hotspot <= 0xFE6; hotspot++) {
        bankSwitch(hotspot); // select bank
        writeToFile(romfile, 0, 0x7FF);
      }
      // bank 7 on slot 1
      writeToFile(romfile, 0x800, 0x7FF);
    break;
    case MAPPER_F4:
      for (int hotspot = 0xFF4; hotspot <= 0xFFB; hotspot++) {
        bankSwitch(hotspot); // select bank
        writeToFile(romfile, 0, 0xFFF);
      }
    break;
    case MAPPER_F6:
      for (int hotspot = 0xFF6; hotspot <= 0xFF9; hotspot++) {
        bankSwitch(hotspot); // select bank
        writeToFile(romfile, 0, 0xFFF);
      }
    break;
    case MAPPER_F8:
      for (int hotspot = 0xFF8; hotspot <= 0xFF9; hotspot++) {
        bankSwitch(hotspot); // select bank
        writeToFile(romfile, 0, 0xFFF);
      }
    break;
    case MAPPER_FE:
      digitalWrite(CS, LOW);
      pinMode(D5, OUTPUT);
      digitalWrite(BUFDIR, HIGH);
      digitalWrite(D5, HIGH);
      writeAddress(0x1F1);
      delayMicroseconds(50);
      writeAddress(0x1FE);
      delayMicroseconds(50);
      writeAddress(0x1FF);
      pinMode(D5, INPUT);
      digitalWrite(CS, HIGH);
      digitalWrite(BUFDIR, LOW);
      writeToFile(romfile, 0, 0xFFF);
      digitalWrite(CS, LOW);
      pinMode(D5, OUTPUT);
      digitalWrite(BUFDIR, HIGH);
      digitalWrite(D5, LOW);
      writeAddress(0x1F1);
      delayMicroseconds(50);
      writeAddress(0x1FE);
      delayMicroseconds(50);
      writeAddress(0x1FF);
      pinMode(D5, INPUT);
      digitalWrite(CS, HIGH);
      digitalWrite(BUFDIR, LOW);
      writeToFile(romfile, 0, 0xFFF);
    break;
  }
}

// 8 1K banks selectable into 4 slots, 
// but last bank is always at the last slot (0xC00 - 0xFFF)
// +------------------+
// | slot 0 (000-3FF) | banks selectable via: FE0 - FE7
// +------------------+
// | slot 1 (400-7FF) | ": FE8 - FEF
// +------------------+
// | slot 2 (800-BFF) | ": FF0 - FF7 
// +------------------+
// | slot 3 (C00-FFF) | (always contains last 1k rom bank of the 8k)
// +------------------+
BOOL isE0() {  
  bankSwitch(0xFE8);

  uint8_t *slot1bank0 = readFromAddressRange(0x400, NCOMPARES);

  // in E0, the last 0xC00 - 0xFFF never changes
  uint8_t *last0 = readFromAddressRange(0xC00, NCOMPARES);

  bankSwitch(0xFEF);
  uint8_t *slot1bank7 = readFromAddressRange(0x400, NCOMPARES);

  // last slot should never change (always stuck at bank 7)
  uint8_t *last1 = readFromAddressRange(0xC00, NCOMPARES);

  // different banks on the same slot should differ, last should stay the same, other slots should stay the same
  BOOL isE0 = !arrayEq(slot1bank0, slot1bank7, NCOMPARES) && arrayEq(last0, last1, NCOMPARES);
  
  free(last0);
  free(last1);
  free(slot1bank0);
  free(slot1bank7);

  return isE0;
}

// similar to E0, but 2k banks instead of 1
// +------------------+
// | slot 0 (000-3FF) | 2k banks selectable via: FE0 - FE6 (FE7 is 2k RAM)
// +------------------+
// | slot 1 (800-FFF) | bank 7 (static, can't be swapped out)
// +------------------+
BOOL isE7() {
  bankSwitch(0xFE0); // select bank 0
  uint8_t *bank0 = readFromAddressRange(512, NCOMPARES);
  
  // in E0, the last 0xC00 - 0xFFF never changes
  uint8_t *last0 = readFromAddressRange(0x800, NCOMPARES);

  bankSwitch(0xFE1); // select bank 1
  uint8_t *bank1 = readFromAddressRange(512, NCOMPARES);
  
  // in E0, the last 0x800 - 0xFFF never changes
  uint8_t *last1 = readFromAddressRange(0x800, NCOMPARES);
  BOOL isE7 = !arrayEq(bank0, bank1, NCOMPARES) && arrayEq(last0, last1, NCOMPARES);  
  
  free(bank0);
  free(last0);
  free(bank1);
  free(last1);
  
  return isE7;
}

// untested, are there even any F4 commercial carts?
BOOL isF4() {
  bankSwitch(0xFF4); // select bank 0
  uint8_t *bank0 = readFromAddressRange(0, NCOMPARES);
  bankSwitch(0xFF5); // select bank 1
  uint8_t *bank1 = readFromAddressRange(0, NCOMPARES);
  BOOL isF4 = !arrayEq(bank0, bank1, NCOMPARES);
  free(bank0);
  free(bank1);
  return isF4;
}

// should be called after isF4, due to overlap
BOOL isF6() {
  bankSwitch(0xFF6); // select bank 0
  uint8_t *bank0 = readFromAddressRange(0, NCOMPARES);
  bankSwitch(0xFF7); // select bank 1
  uint8_t *bank1 = readFromAddressRange(0, NCOMPARES);
  BOOL isF6 = !arrayEq(bank0, bank1, NCOMPARES);
  free(bank0);
  free(bank1);
  return isF6;
}

// should be called after isF6, due to overlap
BOOL isF8() {
  bankSwitch(0xFF8); // select bank 0
  uint8_t *bank0 = readFromAddressRange(0, NCOMPARES);
  bankSwitch(0xFF9); // select bank 1
  uint8_t *bank1 = readFromAddressRange(0, NCOMPARES);
  BOOL isF8 = !arrayEq(bank0, bank1, NCOMPARES);
  free(bank0);
  free(bank1);
  return isF8;
}

BOOL is2K() {
  uint8_t *front = readFromAddressRange(0, NCOMPARES);
  uint8_t *mirror = readFromAddressRange(0x800, NCOMPARES);
  BOOL is2K = arrayEq(front, mirror, NCOMPARES);
  free(front);
  free(mirror);
  return is2K;
}

BOOL isFE() {
  // access stack and set D5 for bank in FE scheme
  digitalWrite(CS, LOW);
  pinMode(D5, OUTPUT);
  digitalWrite(BUFDIR, HIGH);
  digitalWrite(D5, HIGH);
  writeAddress(0x1F1);
  delayMicroseconds(READ_DELAY);
  writeAddress(0x1FE);
  delayMicroseconds(READ_DELAY);
  writeAddress(0x1FF);
  pinMode(D5, INPUT);
  digitalWrite(CS, HIGH);
  digitalWrite(BUFDIR, LOW);
  uint8_t *bank0 = readFromAddressRange(0, NCOMPARES);
  digitalWrite(CS, LOW);
  pinMode(D5, OUTPUT);
  digitalWrite(BUFDIR, HIGH);
  digitalWrite(D5, LOW);
  writeAddress(0x1F1);
  delayMicroseconds(READ_DELAY);
  writeAddress(0x1FE);
  delayMicroseconds(READ_DELAY);
  writeAddress(0x1FF);
  pinMode(D5, INPUT);
  digitalWrite(CS, HIGH);
  digitalWrite(BUFDIR, LOW);
  uint8_t *bank1 = readFromAddressRange(0, NCOMPARES);
  BOOL isFE = !arrayEq(bank0, bank1, NCOMPARES);
  free(bank0);
  free(bank1);
  return isFE;
}

int main (int argc, char *argv[]) {
  wiringPiSetup () ;
  setup();

  uint8_t mapper = detectMapper();
  printf("Found cart of mapper type: %s\n", printMapper(mapper));

  if (argc == 2) {
    FILE* romfile = fopen(argv[1], "wb");
    dumpCart(romfile, mapper);
  }
  else {
    // interactive mode, used to query particular addresses
    printf("Entering interactive mode:\n[type address to view]\n\n", printMapper(mapper));
    char *input;
    while(TRUE) {
      input = readline("> ");
      add_history(input);
      long int addr = strtol(input, NULL, 16);
      bankSwitch(addr);
      printf("Address %X stores value: %X\n", addr, readFromAddress(addr));
    }
  }

  return 0;
}
