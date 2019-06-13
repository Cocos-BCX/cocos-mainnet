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
    parser.add_argument("-n", "--num", metavar="N", default=11, type=int, help="number of init witnesses")
    parser.add_argument("-w", "--witness", metavar="N", default=1, type=int, help="starting witness ID")
    parser.add_argument("-p", "--pretty", action="store_true", default=False, help="pretty print output")
    parser.add_argument("-m", "--mname", metavar="HOSTNAME", default="", help="machine name of target machine")
    parser.add_argument("-s", "--secret", metavar="SECRET", default=None, help="private key generation secret")
    opts = parser.parse_args()

    if opts.secret is None:
        sys.stderr.write("missing required parameter --secret\n")
        sys.stderr.flush()
        sys.exit(1)

    out_wits = []
    out_keys = []

    for i in range(opts.num):
        if opts.mname != "":
           istr = "wit-block-signing-"+opts.mname+"-"+str(i)
        else:
           istr = "wit-block-signing-"+str(i)
        prod_str = subprocess.check_output(["programs/genesis_util/get_dev_key", opts.secret, istr]).decode("utf-8")
        prod = json.loads(prod_str)
        out_wits.append('witness-id = "1.6.'+str(opts.witness+i)+'"\n')
        out_keys.append("private-key = "+json.dumps([prod[0]["public_key"], prod[0]["private_key"]])+"\n")

    out_data = "".join(out_wits + ["\n"] + out_keys)

    if opts.output == "-":
        sys.stdout.write(out_data)
        sys.stdout.flush()
    else:
        with open(opts.output, "w") as f:
            f.write(out_data)
    return

if __name__ == "__main__":
    main()
