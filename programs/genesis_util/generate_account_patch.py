#!/usr/bin/env python3

import argparse
import json
import subprocess
import sys

def dump_json(obj, out, pretty):
    if pretty:
        json.dump(obj, out, indent=2, sort_keys=True)
    else:
        json.dump(obj, out, separators=(",", ":"), sort_keys=True)
    return

def main():
    parser = argparse.ArgumentParser(description="Generate a patch file that adds init accounts")
    parser.add_argument("-o", "--output", metavar="OUT", default="-", help="output filename (default: stdout)")
    parser.add_argument("-a", "--accounts", metavar="ACCOUNTS", default="-", help="file containing name, balances to create")
    parser.add_argument("-p", "--pretty", action="store_true", default=False, help="pretty print output")
    parser.add_argument("-s", "--secret", metavar="SECRET", default=None, help="private key generation secret")
    opts = parser.parse_args()

    if opts.secret is None:
        sys.stderr.write("missing required parameter --secret\n")
        sys.stderr.flush()
        sys.exit(1)

    with open(opts.accounts, "r") as f:
        accounts = json.load(f)

    initial_accounts = []
    initial_balances = []
    for e in accounts:
        name = e["name"]
        owner_str = subprocess.check_output(["programs/genesis_util/get_dev_key", opts.secret, "owner-"+name]).decode("utf-8")
        active_str = subprocess.check_output(["programs/genesis_util/get_dev_key", opts.secret, "active-"+name]).decode("utf-8")
        owner = json.loads(owner_str)
        active = json.loads(active_str)
        initial_accounts.append({
            "name" : name,
            "owner_key" : owner[0]["public_key"],
            "active_key" : active[0]["public_key"],
            "is_lifetime_member" : True,
            })
        for bal in e.get("balances", []):
            bal = dict(bal)
            bal["owner"] = active[0]["address"]
            initial_balances.append(bal)
    result = {
       "append" : {
       "initial_accounts" : initial_accounts },
    }
    if len(initial_balances) > 0:
        result["append"]["initial_balances"] = initial_balances

    if opts.output == "-":
        dump_json( result, sys.stdout, opts.pretty )
        sys.stdout.flush()
    else:
        with open(opts.output, "w") as f:
            dump_json( result, f, opts.pretty )
    return

if __name__ == "__main__":
    main()
