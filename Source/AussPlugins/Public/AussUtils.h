#pragma once

#include <aussruntime/Api.h>

namespace AussUtils
{
	void InitSDK();

	std::string ReadFieldFromAuss(const std::string &key);

	std::vector<std::string> ReadKeysFromAuss(const std::string& key);

	void WriteFieldToAuss(const std::string &key, const std::string &value);

	void DeleteFieldFromAuss(const std::string &key);

	void DeleteFieldsFromAuss(const std::vector<std::string> &keys);
}