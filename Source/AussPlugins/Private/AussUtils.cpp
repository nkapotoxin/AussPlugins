#include "AussUtils.h"

using namespace Auss::Runtime;

namespace AussUtils
{
	void InitSDK()
	{
		Api::InitSDK();
	}

	std::string ReadFieldFromAuss(const std::string &key)
	{
		return Api::GetValue(key);
	}

	std::vector<std::string> ReadKeysFromAuss(const std::string& key)
	{
		return Api::GetKeys(key);
	}

	void WriteFieldToAuss(const std::string &key, const std::string &value)
	{
		return Api::SetValue(key, value);
	}

	void DeleteFieldFromAuss(const std::string &key)
	{
		return Api::DelKey(key);
	}

	void DeleteFieldsFromAuss(const std::vector<std::string> &keys)
	{
		return Api::DelKeys(keys);
	}
}