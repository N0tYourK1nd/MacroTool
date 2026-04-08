#!/bin/sh
gcc src/MacroTool.c src/cJSON.c -o MacroTool -I/usr/include -L/usr/lib -lX11 -lXtst -lXext
