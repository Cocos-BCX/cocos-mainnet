# cocos_mainnet

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

*notice*：Please note that currently(2018-10-17) a full node will need more than 160GB of RAM to operate and required memory is growing fast. Consider the following table as minimal requirements before running a node:

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

# License  
Cocos-BCX mainnet is under the MIT license. See COPYING for more information or see https://opensource.org/licenses/MIT.
