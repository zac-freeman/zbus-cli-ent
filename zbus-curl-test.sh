#!/bin/bash

curl --include \
     --no-buffer \
     --header "Host: localhost:8180" \
     --header "Sec-WebSocket-Version: 13" \
     --header "Origin: http://localhost" \
     --header "Connection: keep-alive, Upgrade" \
     --header "Upgrade: websocket" \
     --header "Sec-WebSocket-Key: a" \
     "$1"
