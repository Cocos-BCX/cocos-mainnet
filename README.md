2018-08-20:
    调整区块应用时序，优化执行框架(进行中)
    优化TX应用逻辑，减少TX执行次数　(进行中)
    规划opreation_result返回数据结构(完成)
    规划contract_result返回数据结构，规整账户同质代币影响, 非同质代币影响, 合约中留言等等(完成)
    优化websocket错误返回,解除异常详情(不再向客户段send错误详情)返回可能导致错误返回僵死的问题
2018-08-27:
    修复非同质交易过期,清理失败的问题
    修改区块生产后的确认过程，确认过程中允许移除确认失败的tx
    增加lua_cjson模块，支持在lua脚本中进行对象序列化与反序列化
    更新系统版本：ubuntu 18.04 ,boost库保持1.57.0 openssl库保持1.0.2 (apt-get install libssl1.0-dev)



2018-10-28:
    本版本以独立lua虚拟机实例运行合约，拥有合约ＩＯ注册表读写逻辑，拥有较高的稳定性

2018-11-20:
    合约ABI支持lua_table对象输入
    完善合约执行异常抛出逻辑
    使用write_list注册表中各个目标的值(true\false)描述是否清理指定对象
    修复合约递归调用时，出现的chainhelper被覆盖的问题；
    添加静态函数注册方式并实现get_account_contract_data静态函数用于在合约内部查看其他人的用户数据

2019-6-19:
    move project  on line
    url = ssh://nico.tang@39.104.66.114:8009/walker.git
