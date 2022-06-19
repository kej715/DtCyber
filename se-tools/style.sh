#!/bin/sh
uncrustify -c se-tools/style.cfg -f $1 | se-tools/pp
