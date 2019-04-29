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

def main():
    parser = argparse.ArgumentParser(description="Apply a patch file to a JSON object")
    parser.add_argument("-o", "--output", metavar="OUT", default="-", help="output filename (default: stdout)")
    parser.add_argument("-i", "--input", metavar="IN", default="-", help="input filename (default: stdin)")
    parser.add_argument("-d", "--delta", metavar="DELTA", nargs="+", help="list of delta file(s) to apply")
    parser.add_argument("-p", "--pretty", action="store_true", default=False, help="pretty print output")
    opts = parser.parse_args()

    if opts.input == "-":
        genesis = json.load(sys.stdin)        
    else:
        with open(opts.input, "r") as f:
            genesis = json.load(f)

    if opts.delta is None: 
        opts.delta = []
    for filename in opts.delta:
        with open(filename, "r") as f:
            patch = json.load(f)
        for k, v in patch.get("append", {}).items():
            if k not in genesis:
                genesis[k] = []
                sys.stderr.write("[WARN]  item {k} was created\n".format(k=k))
            genesis[k].extend(v)
            sys.stderr.write("appended {n} items to {k}\n".format(n=len(v), k=k))
        for k, v in patch.get("replace", {}).items():
            genesis[k] = v
            sys.stderr.write("replaced item {k}\n".format(k=k))

    if opts.output == "-":
        dump_json( genesis, sys.stdout, opts.pretty )
        sys.stdout.flush()
    else:
        with open(opts.output, "w") as f:
            dump_json( genesis, f, opts.pretty )
    return

if __name__ == "__main__":
    main()
