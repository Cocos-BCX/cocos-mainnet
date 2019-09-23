#!/usr/bin/python
# -*- coding: utf-8 -*- 

import os
import sys

symbol = "COCOS"
chain_id = "7c9a7b0b1b8cbe56aa3b24da08aaaf6b3b19a293e7446c7f94f0768d6790cdab"
rpc_endpoint = "127.0.0.1:8041"

def cli_wallet_start():
    cmd = 'cd wallet; ./cli_wallet --chain-id ' + chain_id + ' -s ws://' + rpc_endpoint
    print('>>> ' + cmd)
    os.system(cmd)

def init_balance():
    text = '''
    **********************************************************************************************
    * init0 ~ init10 has init-balance 100000000000 COCOS in genesis.json
    *
    * import balance cli_wallet steps:
    *  1. set_password 123456
    *  2. unlock 123456
    *  3. import_key  nicotest  5KAUeN3Yv51FzpLGGf4S1ByKpMqVFNzXTJK7euqc3NnaaLz1GJm
    *  4. import_balance  nicotest  ["5KAUeN3Yv51FzpLGGf4S1ByKpMqVFNzXTJK7euqc3NnaaLz1GJm"] true
    *  5. list_account_balances nicotest
    *  6. upgrade_account nicotest true
    *  7. exit cli_wallet: ctrl+c 
    **********************************************************************************************
    '''
    print(text)
    cli_wallet_start()

# wallet_first_init: run once
def wallet_first_init():
    cmd = 'rm -rf wallet;  mkdir wallet; cp cli_wallet wallet/'
    print('>>> ' + cmd)
    os.system(cmd)
    init_balance()


if __name__ == '__main__':
    if len(sys.argv) >= 2:
        if sys.argv[1] == 'init':
            wallet_first_init()
        else:
            cli_wallet_start()
    else:
        cli_wallet_start()
