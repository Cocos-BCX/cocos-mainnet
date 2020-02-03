#!/bin/bash

node="witness_node"
echo "=================== run ${node} begin ==================="

nohup ./${node} --genesis-json config/genesis.json >> /dev/null 2>&1 &
sleep 5

pkill ${node}
cp config/config.ini COCOS_BCX_DATABASE/config.ini
nohup ./${node} --genesis-json config/genesis.json --replay-blockchain  >> logs/${node}.log 2>&1 & 
tail -f logs/witness_node.log