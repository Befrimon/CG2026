#!/bin/bash

build()
{
  cmake -S . -B debug
  cmake --build debug -j 12
}
run()
{
  if [[ -f "debug/HertraFramework" ]]; then
    cd debug; ./CG2026; cd -
  else
    build
    cd debug; ./CG2026; cd -
  fi
}

if [[ "$1" == "--build" ]]; then
  build
elif [[ "$1" == "--run" ]]; then
  run
elif [[ "$1" == "--rebuild_and_run" ]]; then
  build
  run
fi
