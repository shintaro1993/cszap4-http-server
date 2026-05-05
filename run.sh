#!/bin/bash

run() {
    echo
    echo "----------------------------------------"
    request="$1"
    echo "$request"
    echo
    ./client "$request"
}

./main & echo $! > main.pid
sleep 1

run "GET / HTTP/1.1"
run "GET /not_found HTTP/1.1"
run "POST / HTTP/1.1"

run "GET /calc?query=1+1"
run "GET /calc?query=1+2+3"
run "GET /calc?query"

kill $(cat main.pid) && rm main.pid