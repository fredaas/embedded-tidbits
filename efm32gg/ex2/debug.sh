#!/usr/bin/env bash
#
# Debug wrapper for remote debugging against JLinkGDBServer
#
# USAGE
#
#     debug.sh <filename.elf>
#

arm-none-eabi-gdb $1 -tui
