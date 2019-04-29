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
    parser = argparse.ArgumentParser(description="Remove entities from snapshot")
    parser.add_argument("-o", "--output", metavar="OUT", default="-", help="output filename (default: stdout)")
    parser.add_argument("-i", "--input", metavar="IN", default="-", help="input filename (default: stdin)")
    parser.add_argument("-a", "--asset", metavar="ASSETS", nargs="+", help="list of asset(s) to delete")    
    parser.add_argument("-p", "--pretty", action="store_true", default=False, help="pretty print output")
    opts = parser.parse_args()

    if opts.input == "-":
        genesis = json.load(sys.stdin)        
    else:
        with open(opts.input, "r") as f:
            genesis = json.load(f)

    if opts.asset is None: 
        opts.asset = []
    rm_asset_set = set(opts.asset)

    removed_asset_entries = {aname : 0 for aname in opts.asset}
    new_initial_assets = []
    for asset in genesis["initial_assets"]:
        symbol = asset["symbol"]
        if symbol not in rm_asset_set:
            new_initial_assets.append(asset)
        else:
            removed_asset_entries[symbol] += 1
    genesis["initial_assets"] = new_initial_assets

    removed_balance_entries = {aname : [] for aname in opts.asset}
    new_initial_balances = []
    for balance in genesis["initial_balances"]:
        symbol = balance["asset_symbol"]
        if symbol not in rm_asset_set:
            new_initial_balances.append(balance)
        else:
            removed_balance_entries[symbol].append(balance)
    genesis["initial_balances"] = new_initial_balances
    # TODO:  Remove from initial_vesting_balances

    for aname in opts.asset:
        sys.stderr.write(
           "Asset {sym} removed {acount} initial_assets, {bcount} initial_balances totaling {btotal}\n".format(
              sym=aname,
              acount=removed_asset_entries[aname],
              bcount=len(removed_balance_entries[aname]),
              btotal=sum(int(e["amount"]) for e in removed_balance_entries[aname]),
              ))

    if opts.output == "-":
        dump_json( genesis, sys.stdout, opts.pretty )
        sys.stdout.flush()
    else:
        with open(opts.output, "w") as f:
            dump_json( genesis, f, opts.pretty )
    return

if __name__ == "__main__":
    main()
