#!/usr/bin/env python3

import argparse
import json
import sys

def dump_json(obj, out, pretty):
    if pretty:
        json.dump(obj, out, indent=2, sort_keys=True)
    else:
        json.dump(obj, out, separators=(",", ":"), sort_keys=True)
    return

def load_names(infile):
    names = set()
    for line in infile:
        if '#' in line:
            line = line[:line.index('#')]
        line = line.strip()
        if line == "":
            continue
        names.add(line)
    return names

def main():
    parser = argparse.ArgumentParser(description="Set is_prefixed=False for a list of names")
    parser.add_argument("-o", "--output", metavar="OUT", default="-", help="output filename (default: stdout)")
    parser.add_argument("-i", "--input", metavar="IN", default="-", help="input filename (default: stdin)")
    parser.add_argument("-n", "--names", metavar="NAMES", help="list of names to unprefix")
    parser.add_argument("-p", "--pretty", action="store_true", default=False, help="pretty print output")
    opts = parser.parse_args()

    if opts.input == "-":
        genesis = json.load(sys.stdin)
    else:
        with open(opts.input, "r") as f:
            genesis = json.load(f)

    if opts.names == "-":
        names = load_names(sys.stdin)
    else:
        with open(opts.names, "r") as f:
            names = load_names(f)

    for account in genesis["initial_accounts"]:
        if account["name"] in names:
            account["is_prefixed"] = False

    if opts.output == "-":
        dump_json( genesis, sys.stdout, opts.pretty )
        sys.stdout.flush()
    else:
        with open(opts.output, "w") as f:
            dump_json( genesis, f, opts.pretty )
    return

if __name__ == "__main__":
    main()
