#!/bin/bash

clean () {
  rm -rf build/
}

build() {
  cmake -B build/
  cmake --build build/ --parallel
}

run() {
  ./build/Flotilla
}

if [ "$1" == "build" ]; then
  build
fi

if [ "$1" == "run" ]; then
  if [ -d "build/" ]; then
    run
  else
    build
    run
  fi
fi

if [ "$1" == "clean" ]; then
  clean
fi
