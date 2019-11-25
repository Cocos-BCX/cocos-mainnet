Cocos 主网  
==============  

点击 [这里](https://github.com/Cocos-BCX/cocos_mainnet/blob/master/README_Chinese.md) 查看英文文档.    

Cocos-BCX是下一代游戏数字经济平台。该项目旨在为游戏开发人员提供易于使用的，全面的区块链游戏基础设施。为游戏玩家提供透明，公平和开放的游戏环境。底层链系统基于石墨烯，用于优化共识机制和添加智能合约系统。并提供许多游戏功能，例如：随机数，合约会话机制，计时器和心跳。  


# 入门  
## 前期准备  

我们建议在Ubuntu 16.04 LTS（64位）上构建  
```  
sudo apt-get update
sudo apt-get install autoconf cmake git vim libbz2-dev libdb++-dev libdb-dev
libssl-dev openssl libreadline-dev libtool libcurl4-openssl-dev libboost-all-dev
```  


## 构建源码   
```  
cmake -DBUILD_PROCESS_ENCRYPTION=NO .  
make  
```  

构建完成后，可以使用以下命令启动见证节点:    
```  
./programs/witness_node/witness_node  
```  

该节点将自动创建一个包含配置文件的数据目录。完全同步区块链可能需要几个小时。同步后，您可以使用Ctrl + C退出节点，并通过如下编辑COCOS_BCX_DATABASE / config.ini来设置命令行钱包:  

```  
rpc-endpoint = 127.0.0.1:8090  
```  

*注意*: 请注意，当前（2019-11-19）一个完整的节点将需要超过160GB的RAM才能运行，并且所需的内存正在快速增长。在运行节点之前，请将下表视为最低要求:

| 默认 | 充分 | 最低要求  | ElasticSearch 
| --- | --- | --- | ---
| 100G HDD, 16G RAM | 640G SSD, 64G RAM * | 80G HDD, 4G RAM | 500G SSD, 32G RAM

再次启动见证节点后，可以在单独的终端中运行:  

```  
./programs/cli_wallet/cli_wallet  
```  
设置您的初始密码:  

```  
>>> set_password <PASSWORD>  
>>> unlock <PASSWORD>  
```  

要导入您的初始余额:

```  
>>> import_balance <ACCOUNT NAME> [<WIF_KEY>] true  
```  

如果通过此连接发送私钥，则rpc-endpoint应该绑定到localhost以获得安全性. 使用帮助查看所有可用的钱包命令. 源代码定义和所有命令的列表在 [此处提供](https://cn-dev.cocosbcx.io/docs/22-cli_wallet).  

# API使用  

我们提供了几种不同的API.  每个API都有自己的ID.
运行时 `witness_node`, 最初有两个API可供使用:
API 0 提供对数据库的只读访问权限, 而API 1用于登录并获得对其他受限API的访问权限.  

这是一个使用`npm` 的 `wscat` 包来建立websockets连接:

    $ npm install -g wscat
    $ wscat -c ws://127.0.0.1:8090
    > {"id":1, "method":"call", "params":[0,"get_accounts",[["1.2.0"]]]}
    < {"id":1,"result":[{"id":"1.2.0","annotations":[],"membership_expiration_date":"1969-12-31T23:59:59","registrar":"1.2.0","referrer":"1.2.0","lifetime_referrer":"1.2.0","network_fee_percentage":2000,"lifetime_referrer_fee_percentage":8000,"referrer_rewards_percentage":0,"name":"committee-account","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[],"address_auths":[]},"active":{"weight_threshold":6,"account_auths":[["1.2.5",1],["1.2.6",1],["1.2.7",1],["1.2.8",1],["1.2.9",1],["1.2.10",1],["1.2.11",1],["1.2.12",1],["1.2.13",1],["1.2.14",1]],"key_auths":[],"address_auths":[]},"options":{"memo_key":"GPH1111111111111111111111111111111114T1Anm","voting_account":"1.2.0","num_witness":0,"num_committee":0,"votes":[],"extensions":[]},"statistics":"2.7.0","whitelisting_accounts":[],"blacklisting_accounts":[]}]}

我们可以使用HTTP客户端（例如 `curl` 用于API的客户端）执行相同的操作,而无需登录或其他会话状态:

    $ curl --data '{"jsonrpc": "2.0", "method": "call", "params": [0, "get_accounts", [["1.2.0"]]], "id": 1}' http://127.0.0.1:8090/rpc
    {"id":1,"result":[{"id":"1.2.0","annotations":[],"membership_expiration_date":"1969-12-31T23:59:59","registrar":"1.2.0","referrer":"1.2.0","lifetime_referrer":"1.2.0","network_fee_percentage":2000,"lifetime_referrer_fee_percentage":8000,"referrer_rewards_percentage":0,"name":"committee-account","owner":{"weight_threshold":1,"account_auths":[],"key_auths":[],"address_auths":[]},"active":{"weight_threshold":6,"account_auths":[["1.2.5",1],["1.2.6",1],["1.2.7",1],["1.2.8",1],["1.2.9",1],["1.2.10",1],["1.2.11",1],["1.2.12",1],["1.2.13",1],["1.2.14",1]],"key_auths":[],"address_auths":[]},"options":{"memo_key":"GPH1111111111111111111111111111111114T1Anm","voting_account":"1.2.0","num_witness":0,"num_committee":0,"votes":[],"extensions":[]},"statistics":"2.7.0","whitelisting_accounts":[],"blacklisting_accounts":[]}]}

可使用常规JSON-RPC访问API 0:

    $ curl --data '{"jsonrpc": "2.0", "method": "get_accounts", "params": [["1.2.0"]], "id": 1}' http://127.0.0.1:8090/rpc  


# 贡献    
感谢您考虑提供源代码帮助！我们欢迎互联网上的任何人做出贡献，甚至感谢最小的修复！如果您想为以太坊做贡献，请分叉，修复，提交并发送请求请求，以供维护者审查并合并到主要的代码库.  

# Resources  
* [Cocos-BCX 浏览器](https://www.cocosabc.com/): We can view the corresponding information on the chain in the blockchain browser.
* [SDK](https://cn-dev.cocosbcx.io/docs/711): We provide rich api connection support, including various sdk:js-sdk, ios-sdk, android-sdk, python-sdk, crytop API...  
* [DAPP 案例](https://cn-dev.cocosbcx.io/docs/81-%E6%8A%BD%E5%A5%96%E7%A4%BA%E4%BE%8B): We provide DAPP cases, for example [cocos-dice-sample](https://github.com/Cocos-BCX/cocos-dice-sample).   
* [白皮书](https://www.cocosbcx.io/static/Whitepaper_zh.pdf)  


# License  
Cocos-BCX主网已获得MIT许可. 有关更多信息，请参见COPYING或参见 https://opensource.org/licenses/MIT.  
