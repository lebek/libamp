#!/usr/bin/env zsh

setopt EXTENDED_GLOB

splint +posixlib -I.  **/(#l)*.([ch]|cpp|hpp) "$@"

