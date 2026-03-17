#!/usr/bin/env bash

N=$1
exe="flotilla"
exe_path="builddir/flotilla"
seconds=30

function verify_executable() {
  # if [ ! -f ${exe_path} ]; then
  #   echo "Executable not found. Building..."
    ./build.sh build
  # fi
}

function spawn_process() {
  echo "Spawning $N instances of $exe..."
  for (( i=0; i<N; i++)); do
    "${exe_path}" &
  done
}

function kill_process() {
  echo "Cleaning up processes..."
  kill -SIGKILL $(pgrep ${exe})
}

verify_executable

echo "Waiting for $seconds seconds..."
spawn_process
sleep $seconds

kill_process
