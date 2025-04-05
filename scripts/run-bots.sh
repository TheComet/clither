#!/bin/bash

PIDS=()

cleanup() {
  for PID in "${PIDS[@]}"; do
    kill -TERM "$PID" > /dev/null 2>&1
  done
  exit 0
}

trap cleanup SIGINT

#./clither --server --log "server.txt" --netlog "server-net.txt" &
#PIDS+=($!)

for i in {1..8}; do
  ./clither \
      --addr localhost \
      --username "Bot $i" \
      --bot ../../lua/bot.lua --gfx &
  PIDS+=($!)
done

wait
