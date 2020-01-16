#Run in docker

#FN
##docker build
docker build -t fn_node .  

##docker run
docker run -itd --restart=always --name fn_node -v $PWD/config:/root/witness/config -v $PWD/COCOS_BCX_DATABASE:/root/witness/COCOS_BCX_DATABASE -v $PWD/logs:/root/witness/logs -p 8069:8049 -p 8060:8050  fn_node

##docker interact
docker exec -it fn_node bash  
