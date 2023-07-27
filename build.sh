#!/bin/bash

script_path=$(dirname "$0")
current_dir=$(cd "$script_path" && pwd)

mkdir "$current_dir"/build
cd "$current_dir"/build
cmake "$current_dir"
make

sudo make install
