Cocos mainnet    
==============  

Click [here](https://github.com/Cocos-BCX/cocos_mainnet/blob/master/README_CN.md) to see the Chinese document.  

Cocos-BCX is the next generation gaming digital economy platform. The project aims to provide game developers with an easy-to-use, comprehensive blockchain gaming infrastructure. Provide gamers with a transparent, fair and open game environment. The underlying chain system is based on graphene for optimization of consensus mechanisms and the addition of intelligent contract systems. And provide a lot of game features, such as: random number, contract session mechanism, timer and heartbeat.  


# Getting Started  
## Preparation  

We recommend building on Ubuntu 16.04 LTS (64-bit)
```  
sudo apt-get update
sudo apt-get install autoconf cmake git vim libbz2-dev libdb++-dev libdb-dev
libssl-dev openssl libreadline-dev libtool libcurl4-openssl-dev libboost-all-dev
```  


## Building the source  
```  
cmake -DBUILD_PROCESS_ENCRYPTION=NO .  
make  
```  

After Building, the witness_node can be launched with:
```  
./programs/witness_node/witness_node  
```  

The node will automatically create a data directory including a config file. It may take several hours to fully synchronize the blockchain. After syncing, you can exit the node using Ctrl+C and setup the command-line wallet by editing witness_node_data_dir/config.ini as follows:

```  
rpc-endpoint = 127.0.0.1:8090  
```  

*notice*：Please note that currently(2019-11-19) a full node will need more than 160GB of RAM to operate and required memory is growing fast. Consider the following table as minimal requirements before running a node:

| Default | Full | Minimal  | ElasticSearch 
| --- | --- | --- | ---
| 100G HDD, 16G RAM | 640G SSD, 64G RAM * | 80G HDD, 4G RAM | 500G SSD, 32G RAM

After starting the witness node again, in a separate terminal you can run:  

```  
./programs/cli_wallet/cli_wallet  
```  
Set your inital password:  

```  
>>> set_password <PASSWORD>  
>>> unlock <PASSWORD>  
```  

To import your initial balance:

```  
>>> import_balance <ACCOUNT NAME> [<WIF_KEY>] true  
```  

If you send private keys over this connection, rpc-endpoint should be bound to localhost for security.  
Use help to see all available wallet commands. Source definition and listing of all commands is available [here](https://cn-dev.cocosbcx.io/docs/22-cli_wallet)  

# Using the API  

We provide several different API's.  Each API has its own ID.
When running `witness_node`, initially two API's are available:
API 0 provides read-only access to the database, while API 1 is
used to login and gain access to additional, restricted API's.

Here is an example using `wscat` package from `npm` for websockets:

    $ npm install -g wscat
    $ wscat -c ws://127.0.0.1:8090
    > {"id":1, "method":"call", "params":[0,"get_accounts",[["1.2.0"]]]}
    < {"id":1,"result":[{"id":"1.2.0","annotations":[],"membership_expiration_date":"1969-12-31T23:59:59","registrar":"1.2.0","referrer":"1.2.0","lifetime_referrer":"1.2.0","network_fee_percentage":2000,"lifetime_referrer_fee_percentage":8000,"referrer_rewards_percentage":0,"name":"committee-account","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[],"address_auths":[]},"active":{"weight_threshold":6,"account_auths":[["1.2.5",1],["1.2.6",1],["1.2.7",1],["1.2.8",1],["1.2.9",1],["1.2.10",1],["1.2.11",1],["1.2.12",1],["1.2.13",1],["1.2.14",1]],"key_auths":[],"address_auths":[]},"options":{"memo_key":"GPH1111111111111111111111111111111114T1Anm","voting_account":"1.2.0","num_witness":0,"num_committee":0,"votes":[],"extensions":[]},"statistics":"2.7.0","whitelisting_accounts":[],"blacklisting_accounts":[]}]}

We can do the same thing using an HTTP client such as `curl` for API's which do not require login or other session state:

    $ curl --data '{"jsonrpc": "2.0", "method": "call", "params": [0, "get_accounts", [["1.2.0"]]], "id": 1}' http://127.0.0.1:8090/rpc
    {"id":1,"result":[{"id":"1.2.0","annotations":[],"membership_expiration_date":"1969-12-31T23:59:59","registrar":"1.2.0","referrer":"1.2.0","lifetime_referrer":"1.2.0","network_fee_percentage":2000,"lifetime_referrer_fee_percentage":8000,"referrer_rewards_percentage":0,"name":"committee-account","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[],"address_auths":[]},"active":{"weight_threshold":6,"account_auths":[["1.2.5",1],["1.2.6",1],["1.2.7",1],["1.2.8",1],["1.2.9",1],["1.2.10",1],["1.2.11",1],["1.2.12",1],["1.2.13",1],["1.2.14",1]],"key_auths":[],"address_auths":[]},"options":{"memo_key":"GPH1111111111111111111111111111111114T1Anm","voting_account":"1.2.0","num_witness":0,"num_committee":0,"votes":[],"extensions":[]},"statistics":"2.7.0","whitelisting_accounts":[],"blacklisting_accounts":[]}]}

API 0 is accessible using regular JSON-RPC:

    $ curl --data '{"jsonrpc": "2.0", "method": "get_accounts", "params": [["1.2.0"]], "id": 1}' http://127.0.0.1:8090/rpc  


# Contribution    
Thank you for considering to help out with the source code! We welcome contributions from anyone on the internet, and are grateful for even the smallest of fixes!If you'd like to contribute to go-ethereum, please fork, fix, commit and send a pull request for the maintainers to review and merge into the main code base.  

# Resources  
* [Cocos-BCX scan](https://www.cocosabc.com/): We can view the corresponding information on the chain in the blockchain browser.
* [SDK](https://cn-dev.cocosbcx.io/docs/711): We provide rich api connection support, including various sdk:js-sdk, ios-sdk, android-sdk, python-sdk, crytop API...  
* [DAPP Sample](https://cn-dev.cocosbcx.io/docs/81-%E6%8A%BD%E5%A5%96%E7%A4%BA%E4%BE%8B): We provide DAPP cases, for example [cocos-dice-sample](https://github.com/Cocos-BCX/cocos-dice-sample).   
* [White Paper](https://www.cocosbcx.io/static/Whitepaper_zh.pdf)  


# License  
Cocos-BCX mainnet is under the MIT license. See COPYING for more information or see https://opensource.org/licenses/MIT.  
   

