#!/usr/bin/env python3

import argparse
import hashlib
import json
import sys

def main():
    parser = argparse.ArgumentParser(description="Set is_prefixed=false for all asset owners")
    parser.add_argument("-o", "--output", metavar="OUT", default="-", help="output filename (default: stdout)")
    parser.add_argument("-i", "--input", metavar="IN", default="-", help="input filename (default: stdin)")
    parser.add_argument("-n", "--num", metavar="N", default=3, type=int, help="number of hashes per key (default: 3)")
    parser.add_argument("-s", "--size", metavar="BITS", default=8*1048576, type=int, help="number of bits in filter")
    parser.add_argument("-a", "--algorithm", metavar="NAME", default="sha256", type=str, help="hash algorithm (must exist in hashlib)")
    opts = parser.parse_args()

    if opts.input == "-":
        genesis = json.load(sys.stdin)
    else:
        with open(opts.input, "r") as f:
            genesis = json.load(f)

    keys = set()

    for account in genesis["initial_accounts"]:
        keys.add(account["owner_key"])
        keys.add(account["active_key"])

    for asset in genesis["initial_assets"]:
        for cr in asset.get("collateral_records", []):
            keys.add(cr["owner"])

    for balance in genesis["initial_balances"]:
        keys.add(balance["owner"])

    for vb in genesis["initial_vesting_balances"]:
        keys.add(vb["owner"])

    for witness in genesis["initial_witness_candidates"]:
        keys.add(witness["block_signing_key"])

    sys.stderr.write("got {n} distinct keys\n".format(n=len(keys)))

    keys = [(str(i) + ":" + k).encode("UTF-8") for k in sorted(keys) for i in range(opts.num)]

    data = bytearray((opts.size + 7) >> 3)

    h = getattr(hashlib, opts.algorithm)
    for k in keys:
        address = int(h(k).hexdigest(), 16) % opts.size
        data[address >> 3] |= (1 << (address & 7))

    popcount = [bin(i).count("1") for i in range(256)]
    w = sum(popcount[x] for x in data)
    sys.stderr.write("""w={w}   o={o:.3%}   p={p:.3%}
w: Hamming weight    o: Occupancy    p: False positive probability
""".format(
w=w,
o=float(w) / float(opts.size),
p=(float(w) / float(opts.size))**opts.num,
))

    if opts.output == "-":
        sys.stdout.write(data)
        sys.stdout.flush()
    else:
        with open(opts.output, "wb") as f:
            f.write(data)
    return

if __name__ == "__main__":
    main()
