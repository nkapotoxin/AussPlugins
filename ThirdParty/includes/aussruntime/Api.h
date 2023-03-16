#pragma once
/**
 * The public Auss Runtime SDK API
 */
#include <aussruntime/Exports.h>
#include <iostream>
#include <vector>

namespace Auss
{
namespace Runtime
{
namespace Api
{

AUSS_RUNTIME_API std::string GetSdkVersion() noexcept;

AUSS_RUNTIME_API void InitSDK() noexcept;

AUSS_RUNTIME_API std::string GetValue(const std::string &key) noexcept;

AUSS_RUNTIME_API void SetValue(const std::string &key, const std::string &value) noexcept;

AUSS_RUNTIME_API void SyncSetValue(const std::string &key, const std::string &value) noexcept;

AUSS_RUNTIME_API void DelKey(const std::string &key) noexcept;

AUSS_RUNTIME_API void DelKeys(const std::vector<std::string> &keys) noexcept;

AUSS_RUNTIME_API std::vector<std::string> GetKeys(const std::string &key) noexcept;

} // namespace Api
} // namespace Runtime
} // namespace Auss