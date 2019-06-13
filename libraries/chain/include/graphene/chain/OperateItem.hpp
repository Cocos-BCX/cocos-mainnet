#pragma once
#include <graphene/chain/protocol/types.hpp>
namespace graphene { namespace chain {
	
 
	enum ControlType
	{
		ADD=0,
		DELETE=1,
		NoOperation
	};
	typedef struct ChangeAccountOperationControlObject{
	public:
		ControlType control;
		string strAssetID;
		string strItemVER;
 		string strItemID;
		string strData;
	}ChangeAccountOperationControlObject;
	void OperateItem(vector<string>& vec,const ChangeAccountOperationControlObject Op);
	string GetAssetID(const string strItem);
	string GetItemVER(const string strItem);
	string GetItemID(const string strItem);
	string GetStrData(const string strItem);
	ChangeAccountOperationControlObject GetControl(string& strItem);
	}
}
