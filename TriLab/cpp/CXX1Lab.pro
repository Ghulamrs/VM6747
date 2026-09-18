{
  "name": "CXX1Lab",
  "toolchain": "cxx1",
  "arch": "arm64-darwin",
  "groups": {
    "Sources": {
      "files": [
        "demo.cpp",
        "shapes.cpp",
        "report.cpp"
      ],
      "toolchain": "cxx1"
    },
    "Headers": {
      "files": [
        "shapes.h",
        "report.h"
      ]
    }
  },
  "build": {
    "target": "cxx1lab",
    "groups": [
      "Sources"
    ]
  }
}
