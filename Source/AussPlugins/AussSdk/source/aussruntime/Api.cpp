#include <aussruntime/Api.h>
#include <aussruntime/internal/ServerState.h>

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

void Api::SetValue(const std::string &key, const std::string &value) noexcept
{
	Auss::Runtime::Internal::CommonState* commonState = Auss::Runtime::Internal::CommonState::GetInstance(Auss::Runtime::Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER);
	if (!commonState)
	{
		return;
	}

	Auss::Runtime::Internal::ServerState* serverState = static_cast<Auss::Runtime::Internal::ServerState*>(commonState);
	return serverState->Set(key, value);
}

void Api::SyncSetValue(const std::string &key, const std::string &value) noexcept
{
	Auss::Runtime::Internal::CommonState* commonState = Auss::Runtime::Internal::CommonState::GetInstance(Auss::Runtime::Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER);
	if (!commonState)
	{
		return;
	}

	Auss::Runtime::Internal::ServerState* serverState = static_cast<Auss::Runtime::Internal::ServerState*>(commonState);
	return serverState->SyncSet(key, value);
}

void Api::DelKey(const std::string &key) noexcept
{
	Auss::Runtime::Internal::CommonState* commonState = Auss::Runtime::Internal::CommonState::GetInstance(Auss::Runtime::Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER);
	if (!commonState)
	{
		return;
	}

	Auss::Runtime::Internal::ServerState* serverState = static_cast<Auss::Runtime::Internal::ServerState*>(commonState);
	return serverState->Del(key);
}

void Api::DelKeys(const std::vector<std::string> &keys) noexcept
{
	Auss::Runtime::Internal::CommonState* commonState = Auss::Runtime::Internal::CommonState::GetInstance(Auss::Runtime::Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER);
	if (!commonState)
	{
		return;
	}

	Auss::Runtime::Internal::ServerState* serverState = static_cast<Auss::Runtime::Internal::ServerState*>(commonState);
	return serverState->Del(keys);
}

std::vector<std::string> Api::GetKeys(const std::string &key) noexcept
{
	std::vector<std::string> result;
	Auss::Runtime::Internal::CommonState* commonState = Auss::Runtime::Internal::CommonState::GetInstance(Auss::Runtime::Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER);
	if (!commonState)
	{
		return result;
	}

	Auss::Runtime::Internal::ServerState* serverState = static_cast<Auss::Runtime::Internal::ServerState*>(commonState);
	return serverState->Keys(key);
}