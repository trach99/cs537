#!/bin/bash

# This script is used to edit the Makefile to include the tester in the
# _mkdir rule. This is necessary because the tester is not included in the
# _mkdir rule by default. This script is called by the run-tests.sh script.

echo $1
echo $2


# This command will add the _tester\ before the _mkdir\ rule in the Makefile
gawk '($1 == "_mkdir\\") { printf("\t_tester\\\n"); } { print $0 }' $1 > $2
