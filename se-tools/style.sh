#!/bin/sh
uncrustify -c contribute/style.cfg -f $1 | contribute/pp
