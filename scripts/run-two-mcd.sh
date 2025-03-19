#!/bin/sh

PIDS=()

cleanup() {
  for PID in "${PIDS[@]}"; do
     kill -TERM "$PID" > /dev/null 2>&1
  done
}

trap cleanup SIGINT

./clither --server --log "server.txt" --netlog "server-net.txt" &
PIDS+=($!)

./clither --ip localhost --username "Client 1" --prefix "Client 1: " --log "client1.txt" --netlog "client1-net.txt" &
PIDS+=($!)
./clither --ip localhost --username "Client 2" --prefix "Client 2: " --log "client2.txt" --netlog "client2-net.txt" --mcd 150 10 10 10 &
PIDS+=($!)

wait
