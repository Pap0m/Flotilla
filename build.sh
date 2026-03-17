#!/usr/bin/env bash

clean () {
  rm -rf builddir/
}

build() {
  meson setup builddir
  meson compile -C builddir/
}

run() {
  meson compile -C builddir/
  ./builddir/flotilla
}

if [ "$1" == "build" ]; then
  build
fi

if [ "$1" == "run" ]; then
  if [ -d "builddir/" ]; then
    run
  else
    build
    run
  fi
fi

if [ "$1" == "clean" ]; then
  clean
fi
