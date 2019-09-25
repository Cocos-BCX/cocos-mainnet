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


# 4.account key
## 说明：
* 本文件存储的是genesis.json中初始化的11个见证人账号秘钥和1个资金账号秘钥
* 本文件中的秘钥只用于测试环境
    

## 4.1 资金账号：nicotest
``` json
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "BETREAD SOLA ARGOSY NEATIFY TILT SAMISEN DURABLE BOWPIN DIP FLAMFEW GEOTIC PIAL MOLLY SCHANZ CATHIN OFFING",
  "wif_priv_key": "5KgiWEMJPYbLpMhX6jvS9yBehhr4mWZhxX7hfxZQEo3rs8iakUQ",
  "pub_key": "COCOS5X4bfMnAmeWhLoiHKUNrRu7D3LTXKBZQkZvWGj9YCTDBAYaSXU"
}
```

## 4.2 初始账号：init0 ~ init10
```json
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "ALHENNA AJHAR PAUCITY CEDRENE PASQUIN NAIVETE EGAD SNOTTY LAYLAND SMUDGED BESHOD ROTGUT DORTER SHANDRY CHELIDE KIEL",
  "wif_priv_key": "5JoQ6vc5D8izcsrnfvwnPTwGgb2TzkqTZ2KgsFcupnfzrSByK4b",
  "pub_key": "COCOS7oRQiuWdd5iSjxF9PysPhtsTtyLjNX9g4tCr1je3qaHwQhT6vU"
}
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "TRINOL YACHT ALUMNUS ROWDY SLATCH METOPIC WOLFRAM COXA NEWCOME CLINT SHAMED AIRLIKE RECUT CRINKLE JOCOSE RHYPTIC",
  "wif_priv_key": "5JreD8sH9ghkwC3JNzxrQG3aMUCMbW4VUua2uQrFGDuuZa2xKKj",
  "pub_key": "COCOS5JCAjEn76BC3CxLzqh23D7W4QwCaUPbPFxgyvSTwaWVtFfWn83"
}
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "EUTAXIC HEMOL INDOLE CARER MEADOW UNCA HAPTICS GLANCER VITIATE PUSHING RAPE CHOL HOTBOX SUGUARO UNBAIT SCHTOFF",
  "wif_priv_key": "5HvemE5UBgPL52vF3khpUxkCHR257anyKe2GZhrzEnF5jomwHu3",
  "pub_key": "COCOS7xAz6DainYDviGNcuyHToYJRFScrjgyLLjmZcchFoYFwhxnYBb"
}
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "LIMPY NEFAST LEPRA BLASH VITA TEREBRA HAIKWAN DHARNA SUCKING AMOVE STOLA DINICAL HODDER TRISUL BANI VIPERY",
  "wif_priv_key": "5KErV3nehXSF66uQJFVftoRgXcs68BwxB8J9fgEuGyX468JPtre",
  "pub_key": "COCOS8dSYzG2wzf4f8zm6cqnjartf9hRAGb7WkdU8vE3mqfkJqoUUY5"
}
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "HAGROPE LAPELER OVA GOLDIE JOSSER FOCAL COUE DINICAL MECATE WIND SOSO SEASICK SLOAN SCOG SHAWL THRONAL",
  "wif_priv_key": "5JTzGvcyrLw1sFbWfHVWVyRJ6ZZAHf5uHjUoiWzhz37oP4mMP4S",
  "pub_key": "COCOS7orCRQj9ZLPzh4k6ZHBVGoniRdJBZEW3Fu8hMfEdFmb5UUj28p"
}
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "DURENOL WINT DIMPS COELIAC MACULE LENS SLAGMAN RACHIS POIL ANNEXAL INHUME PEBRINE PYROPUS ANITHER PROFANE TRIEDLY",
  "wif_priv_key": "5JVME8DM3Y522EhZ5DX2q5j6jrkhPGMzMdXrnPsExYJMjomruz1",
  "pub_key": "COCOS79eVAWkzeoMtKQ8QZ5wViet2LHFAd8S9XW7kdvsQkR83Smtc1R"
}
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "KODA BATIKER BALDRIB GRIMME PRESHOW ADROP SPONGY THAPSIA EMANIUM GEMMER WHARP GHALVA THISHOW UNAMEND ANISOYL CADAVER",
  "wif_priv_key": "5KKhwoygthU2GE8cKepsxMuvLutnYaMmyHLFAvouqtNdqCRZG6G",
  "pub_key": "COCOS4xEjo4FgN78U6XLPEPVuCx3LVqdXApcx6RZeeh2brvJ7wjrfHU"
}
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "POPPA TAIRGE CONJECT PELAGIC SANIES KASTURA JETTY VIGA LIGATOR FIERCE GANSY CASSADY POMONAL BONALLY TROPISM SHIBAH",
  "wif_priv_key": "5KLuGNq5ybn6GknK99cjCkH3FvGHFoA9BbeczkKSVaZputzd7vN",
  "pub_key": "COCOS6xsPUg9X1Ej1BfBdi4bNSt3TFZePyQG4ft3TYSN98QUXwvkVqf"
}
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "MAILBOX KET YARDFUL UPLOCK TUMMALS HAREM WIZ GLOOMTH SLIPPY SCRANK OUTBLOT SERAW ARPEN ARIST SWELTER WAYWARD",
  "wif_priv_key": "5JxnwAos8A3tV4gAooQzmwZRdmfjLJFv4SmqNiLrvP9Pordi2aL",
  "pub_key": "COCOS5iAyn8ERVG7DvjoZ5FnJeYu78FJRykocr4rtHC5iwZbmVvAGdN"
}
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "KODRO SCENA TOYWORT SUCCIN TOMB CHACKER BORAL FALLOW DECIDUA COVID BOKOM WHIRTLE BEANERY UPDATE FADEDLY INSTALL",
  "wif_priv_key": "5HsjTp5ke1paFYLAfTAgez6xUriwpEKaKmB7QxvCbEF8Ps3oyoX",
  "pub_key": "COCOS685H9zQAbpQaXXbgu3xv6TiHd3GHfEf3ykfzrsZJ8tC9RD1oGU"
}
locked >>> suggest_brain_key
suggest_brain_key
{
  "brain_priv_key": "DHOON GRUBS SWELTH SHUSHER LANAS JIKUNGU BETONY BEARDIE TORT APACE ASIDEU WORST TRUCKLE VIGIL LEPORID COWITCH",
  "wif_priv_key": "5JQsb85HJirGmEaRLgxxAo9BgZApU55n3U2RaNqiZtk1CbvaW4x",
  "pub_key": "COCOS7exvdzF3hkHmuVHZ7pUjrSGfigLFtzo7uqv84n79y8dEfNfURr"
}
locked >>> 
```

