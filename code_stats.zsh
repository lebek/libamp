#!/usr/bin/env zsh

setopt EXTENDED_GLOB
lib_lines=$(sloccount *.c~test_*.c | grep ansic: | tr -s "[:space:]" | sed 's/^[ \t]*//' | cut -d " " -f 2)
test_lines=$(sloccount test_*.c | grep ansic: | tr -s "[:space:]" | sed 's/^[ \t]*//' | cut -d " " -f 2)
example_lines=$(sloccount examples | grep ansic: | tr -s "[:space:]" | sed 's/^[ \t]*//' | cut -d " " -f 2)

echo "Lines of Physical Source Code:"
echo
echo "  Library:  " $lib_lines
echo "  Tests:    " $test_lines
echo "  Examples: " $example_lines
echo
echo "  Total:    " $(( $lib_lines + $test_lines + $example_lines ))

