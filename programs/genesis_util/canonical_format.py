#!/usr/bin/env python3

import argparse
import json
import sys

if len(sys.argv) < 3:
    print("syntax: "+sys.argv[0]+" INFILE OUTFILE")
    sys.exit(0)

with open(sys.argv[1], "r") as infile:
    genesis = json.load(infile)
with open(sys.argv[2], "w") as outfile:
    json.dump(genesis, outfile, separators=(',', ':'), sort_keys=True)
