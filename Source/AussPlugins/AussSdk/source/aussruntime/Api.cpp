#include <aussruntime/Api.h>

using namespace Auss::Runtime;

static const std::string sdkVersion = "0.0.1";

std::string Api::GetSdkVersion() noexcept
{
	return sdkVersion;
}

void Api::InitSDK() noexcept
{
	Internal::ServerState::CreateInstance();
}

std::string Api::GetValue(const std::string &key) noexcept
{
	Internal::CommonState* commonState = Internal::CommonState::GetInstance(Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER);
	if (!commonState)
	{
		return "";
	}

	Internal::ServerState* serverState = static_cast<Internal::ServerState*>(commonState);
	return serverState->Get(key);
}

void SetValue(const std::string &key, const std::string &value) noexcept
{
	Internal::CommonState* commonState = Internal::CommonState::GetInstance(Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER);
	if (!commonState)
	{
		return;
	}

	Internal::ServerState* serverState = static_cast<Internal::ServerState*>(commonState);
	return serverState->Set(key, value);
}

void SyncSetValue(const std::string &key, const std::string &value) noexcept
{
	Internal::CommonState* commonState = Internal::CommonState::GetInstance(Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER);
	if (!commonState)
	{
		return;
	}

	Internal::ServerState* serverState = static_cast<Internal::ServerState*>(commonState);
	return serverState->SyncSet(key, value);
}

void DelKey(const std::string &key) noexcept
{
	Internal::CommonState* commonState = Internal::CommonState::GetInstance(Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER);
	if (!commonState)
	{
		return;
	}

	Internal::ServerState* serverState = static_cast<Internal::ServerState*>(commonState);
	return serverState->Del(key);
}

void DelKeys(const std::vector<std::string> &keys) noexcept
{
	Internal::CommonState* commonState = Internal::CommonState::GetInstance(Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER);
	if (!commonState)
	{
		return;
	}

	Internal::ServerState* serverState = static_cast<Internal::ServerState*>(commonState);
	return serverState->Del(keys);
}

AUSS_RUNTIME_API std::vector<std::string> GetKeys(const std::string &key) noexcept
{
	Internal::CommonState* commonState = Internal::CommonState::GetInstance(Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER);
	if (!commonState)
	{
		return;
	}

	Internal::ServerState* serverState = static_cast<Internal::ServerState*>(commonState);
	return serverState->Keys(key);
}