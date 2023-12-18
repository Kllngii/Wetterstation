#!/usr/bin/env bash
cd "$(dirname "$0")" || exit
probe-rs attach --chip=RP2040 ./target/thumbv6m-none-eabi/debug/slint-pico