#!/bin/bash

./game_server_tests

if [ $? -ne 0 ]; then
    echo "Tests failed, terminating work"
    exit 1
fi

exec ./game_server --t 16 --c ./data/config.json --w ./static --state-file ./saves/save_state --save-state-period 3500