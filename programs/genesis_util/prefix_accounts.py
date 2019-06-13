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
    parser = argparse.ArgumentParser(description="Add a prefix to selected account names")
    parser.add_argument("-o", "--output", metavar="OUT", default="-", help="output filename (default: stdout)")
    parser.add_argument("-i", "--input", metavar="IN", default="-", help="input filename (default: stdin)")
    parser.add_argument("-b", "--begin", metavar="PREFIX", default="", help="prefix to add")
    parser.add_argument("-p", "--pretty", action="store_true", default=False, help="pretty print output")
    opts = parser.parse_args()

    if opts.input == "-":
        genesis = json.load(sys.stdin)
    else:
        with open(opts.input, "r") as f:
            genesis = json.load(f)

    taken_names = set()
    unsettled_names = []
    name_map = {}
    prefix = opts.begin

    for account in genesis["initial_accounts"]:
        is_prefixed = account["is_prefixed"]
        name = account["name"]
        if is_prefixed:
            unsettled_names.append(name)
            name_map[name] = name
        else:
            taken_names.add(name)

    pass_num = 0
    while len(unsettled_names) > 0:
        num_resolved = 0
        pass_num += 1
        sys.stderr.write("attempting to resolve {n} names\n".format(n=len(unsettled_names)))
        if pass_num > 1:
            sys.stderr.write("names: {}\n".format("\n".join(unsettled_names)))
        new_unsettled_names = []
        for name in unsettled_names:
            new_name = prefix+name_map[name]
            name_map[name] = new_name
            if new_name in taken_names:
                new_unsettled_names.append(name)
            else:
                taken_names.add(name)
                num_resolved += 1
        sys.stderr.write("resolved {n} names\n".format(n=num_resolved))
        unsettled_names = new_unsettled_names

    for account in genesis["initial_accounts"]:
        name = account["name"]
        account["name"] = name_map.get(name, name)
        del account["is_prefixed"]
    for asset in genesis["initial_assets"]:
        issuer_name = asset["issuer_name"]
        asset["issuer_name"] = name_map.get(issuer_name, issuer_name)
    for witness in genesis["initial_witness_candidates"]:
        owner_name = witness["owner_name"]
        witness["owner_name"] = name_map.get(owner_name, owner_name)
    for committee in genesis["initial_committee_candidates"]:
        owner_name = member["owner_name"]
        member["owner_name"] = name_map.get(owner_name, owner_name)
    for worker in genesis["initial_worker_candidates"]:
        owner_name = worker["owner_name"]
        worker["owner_name"] = name_map.get(owner_name, owner_name)

    if opts.output == "-":
        dump_json( genesis, sys.stdout, opts.pretty )
        sys.stdout.flush()
    else:
        with open(opts.output, "w") as f:
            dump_json( genesis, f, opts.pretty )
    return

if __name__ == "__main__":
    main()
