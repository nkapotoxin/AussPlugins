#pragma once
/**
 * The public Auss Runtime SDK API
 */
#include <aussruntime/Exports.h>

namespace Auss
{
namespace Runtime
{
namespace Api
{

AUSS_RUNTIME_API std::string GetSdkVersion() noexcept;

AUSS_RUNTIME_API void InitSDK() noexcept;

AUSS_RUNTIME_API std::string Get(const std::string &key) noexcept;

AUSS_RUNTIME_API void Set(const std::string &key, const std::string &value) noexcept;

AUSS_RUNTIME_API void SyncSet(const std::string &key, const std::string &value) noexcept;

AUSS_RUNTIME_API void Del(const std::string &key) noexcept;

AUSS_RUNTIME_API void Del(const std::vector<std::string> &keys) noexcept;

} // namespace Api
} // namespace Runtime
} // namespace Auss