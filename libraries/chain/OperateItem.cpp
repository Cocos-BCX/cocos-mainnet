#include <graphene/chain/OperateItem.hpp>
namespace graphene {

	 namespace chain {
		//nico 装备存储
		void OperateItem(vector<string>& vec,const ChangeAccountOperationControlObject Op)
		{
			vector<string>::iterator iter;
			if(ControlType::DELETE==Op.control)
			{
				if(Op.strItemVER=="*")vec.clear();
				else if(Op.strItemID=="*")
				{
					for(iter = vec.begin(); iter != vec.end(); )
					{
						string strItemVER = GetItemVER(*iter);
						if(strItemVER == Op.strItemVER)
							vec.erase(iter);
						else 
							++iter;
					} 	
				}
				else
				{
					for(iter = vec.begin(); iter != vec.end();++iter )
					{
						string strItemVER = GetItemVER(*iter);
						string strItemID = GetItemID(*iter);
						if(strItemVER == Op.strItemVER && strItemID == Op.strItemID)
						{
							vec.erase(iter);
							break;
						}					
					}
				}
			}
			if(ControlType::ADD == Op.control)
			{
				if("*" == Op.strItemVER|| Op.strItemID=="*")
					return;
				for(iter = vec.begin(); iter != vec.end();++iter )
				{
					string strItemVER = GetItemVER(*iter);
					string strItemID = GetItemID(*iter);
					if(strItemVER == Op.strItemVER && strItemID == Op.strItemID)
					{
							*iter=Op.strAssetID + "$" + Op.strItemVER + "@" + Op.strItemID+";"+Op.strData;
							break;
					}
				}
				if(iter==vec.end())
				{	
					string strItemTmp = Op.strAssetID + "$" + Op.strItemVER + "@" + Op.strItemID+";"+Op.strData;
					vec.push_back(strItemTmp);
				}
			} 
		}


		string GetAssetID(const string strItem)
		{
			std::size_t found = strItem.find("$");
			
			if(std::string::npos != found)
			{
				return string(strItem, 0, found);
			}
			
			return "";
		}
		string GetItemVER(const string strItem)
		{
			std::size_t found1 = strItem.find("$");
			
			if(std::string::npos != found1)
			{
				std::size_t found2 = strItem.find("@");
				
				if(std::string::npos != found2)
				{
					return string(strItem, found1 + 1 , found2 - found1 - 1);
				}
				else
					return "";
			}
			else
				return "";
		}
		string GetItemID(const string strItem)
		{
			std::size_t found1 = strItem.find("@");
			
			if(std::string::npos != found1)
			{
				std::size_t found2 = strItem.find(";");
				
				if(std::string::npos != found2)
				{
					return string(strItem, found1 + 1 , found2 - found1 - 1);
				}else
					return "";
			}
			else
				return "";
		}
		string GetStrData(const string strItem)
		{
			std::size_t found = strItem.find(";");
			if(std::string::npos != found)
			{
				return string(strItem,found+1);
			}
			return "";
		}

		ChangeAccountOperationControlObject GetControl(string& strItem)
		{
			ChangeAccountOperationControlObject object;
			std::size_t found= strItem.find(":");
			if(std::string::npos != found)
			{
				string Control=string(strItem, 0, found);
				strItem=string(strItem,found+1);
				if(Control=="append")
				{
					object.control=ControlType::ADD;
				}
				else if(Control=="erase")
				{
					object.control=ControlType::DELETE;
				}
				else{
					object.control=ControlType::NoOperation;
				}
				object.strAssetID=GetAssetID(strItem);
				object.strItemVER=GetItemVER(strItem);
				object.strItemID=GetItemID(strItem);
				object.strData=GetStrData(strItem);
				if(object.strAssetID==""
					||object.strItemVER==""
					||object.strItemID==""
				)
				object.control=ControlType::NoOperation;
			return object;
			}
			object.control=ControlType::NoOperation;
			return object;
		}
		
	 }
}