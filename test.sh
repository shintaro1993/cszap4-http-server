#!/bin/bash

trap 'kill $(cat main.pid) && rm main.pid' EXIT

assert() {
    request="$1"
    expected="$2"

    actual_body=$(./client "$request" | sed -n '5p')

    if [ "$actual_body" = "$expected" ]; then
        echo "$request => $actual_body"
    else
        echo "$request => FAIL"
        echo "  expected: $expected"
        echo "  actual body: $actual_body"
        exit 1
    fi
}

./main & echo $! > main.pid
sleep 1

# レスポンスの5行目（ボディ）をチェック
# TODO: ボディの取り出しを改善する（現在は5行目決め打ち）

assert "GET / HTTP/1.1"             "Hello World!"
assert "GET /hoge HTTP/1.1"    "404 Not Found"
assert "POST / HTTP/1.1"            "Only GET method is supported"

assert "GET /calc?query=1 HTTP/1.1"       "1"
assert "GET /calc?query=1+1 HTTP/1.1"     "2"
assert "GET /calc?query=1-1+1 HTTP/1.1"   "1"

echo "OK"