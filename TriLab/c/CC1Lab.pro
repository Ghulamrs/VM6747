{
  "name": "CC1Lab",
  "toolchain": "cc1",
  "arch": "arm64-darwin",
  "groups": {
    "Sources": {
      "files": [
        "main.c",
        "ex01_arithmetic.c",
        "ex02_control.c",
        "ex03_pointers.c",
        "ex04_structs.c",
        "ex05_bitfields.c",
        "ex06_strings.c",
        "ex07_floats.c",
        "ex08_fnptr.c",
        "ex09_initialisers.c",
        "ex10_fileio.c",
        "heavy.c"
      ],
      "toolchain": "cc1"
    },
    "Headers": [
      "examples.h"
    ]
  },
  "open": "main.c",
  "build": {
    "target": "cc1lab",
    "groups": [
      "Sources"
    ]
  }
}
