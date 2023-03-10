#pragma once

#include <aussruntime/Api.h>

namespace AussUtils
{
	DECLARE_LOG_CATEGORY_EXTERN(LogAussUtils, Log, All);

	inline DEFINE_LOG_CATEGORY(LogAussUtils)

	void InitSDK();

	std::string ReadFieldFromAuss(const std::string &key);

	void WriteFieldToAuss(const std::string &key, const std::string &value);

	void DeleteFieldFromAuss(const std::string &key);

	void DeleteFieldsFromAuss(const std::vector<std::string> &keys);

}