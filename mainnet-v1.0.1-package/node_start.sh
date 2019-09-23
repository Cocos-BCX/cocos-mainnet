#!/bin/bash

# genesis.json witness_node config.ini 都在当前路径下
# genesis.json 创世区块文件
# config.ini witness_node 启动的加载配置

nohup ./witness_node --genesis-json genesis.json >> witness_node.log 2>&1 &

