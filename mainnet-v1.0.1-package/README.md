# 1.单节点测试链配置文件
* genesis.json  
* config.ini  


# 2.节点启动脚本
## 2.1 链节点第一次启动，初始化脚本
* chain_first_init.sh    
> 和配置文件，witness_node在同一路径下，执行命令： ./chain_first_init.sh   
> 该方式是以nohup形式启动的，chain-id 在chainID.log中，运行log在 witness_node.log中。


## 2.2 节点非第一次启动脚本
* node_start.sh  


# 3.钱包启动脚本
* wallet_start.py  
> 和cli_wallet在同一路径下   
> 
> 执行命令:  
> 
> **python  wallet_start.py init** : 第一次启动包含import_balance, nicotest account upgrade等操作.  
> 
> **python wallet_start.py**  启动cli_wallet

