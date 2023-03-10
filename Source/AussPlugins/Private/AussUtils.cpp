#include "AussUtils.h"

using namespace Aws::Runtime;

namespace AussUtils
{
	void InitSDK()
	{
		Api::InitSDK();
	}

	std::string ReadFieldFromAuss(const std::string &key)
	{
		return Api::Get(key);
	}

	void WriteFieldToAuss(const std::string &key, const std::string &value)
	{
		return Api::Set(key, value);
	}

	void DeleteFieldFromAuss(const std::string &key)
	{
		return Api::Del(key);
	}

	void DeleteFieldsFromAuss(const std::vector<std::string> &keys)
	{
		return Api::Del(keys);
	}

}