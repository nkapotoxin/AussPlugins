#include <aussruntime/internal/ServerState.h>

using namespace Auss::Runtime;

Internal::ServerState::ServerState()
	: m_redis_server_ip("127.0.0.1")
	, m_redis_server_pw("")
	, m_redis_server_port(6379)
	, m_redis_client(nullptr)
{
	// TODO(nkaptx): read from local config file
	
	static cpp_redis::client client;
	client->connect(std::string(TCHAR_TO_UTF8(*m_redis_server_ip)), m_redis_server_port);
	client->auth(std::string(TCHAR_TO_UTF8(*m_redis_server_pw)));

	m_redis_client = &client;
}

Internal::ServerState::~ServerState()
{
	Internal::CommonState::SetInstance(nullptr);
	if (m_redis_client)
	{
		delete m_redis_client;
		m_redis_client = nullptr;
	}
}

Internal::ServerState*
Internal::ServerState::CreateInstance()
{
	if (CommonState::GetInstance() != nullptr)
	{
		return CommonState::GetInstance();
	}

	ServerState* newState = new ServerState;
	CommonState::SetInstance(newState);
	return newState;
}

std::string Internal::Get(const std::string &key)
{
	auto value = m_redis_client->get(TCHAR_TO_UTF8(*key));
	m_redis_client->sync_commit();

	cpp_redis::reply reply = value.get();
	if (reply.is_null())
	{
		// TODO(nkaptx): log or return error
		return "";
	}

	return reply.as_string().c_str();
}

void Internal::Set(const std::string &key, const std::string &value)
{
	m_redis_client->set(TCHAR_TO_UTF8(*key), TCHAR_TO_UTF8(*value));
	m_redis_client->commit();
}

void Internal::SyncSet(const std::string &key, const std::string &value)
{
	m_redis_client->set(TCHAR_TO_UTF8(*key), TCHAR_TO_UTF8(*value));
	m_redis_client->sync_commit();
}

void Internal::Del(const std::string &key)
{
	std::vector<std::string> keys;
	keys.push_back(TCHAR_TO_UTF8(*key));
	Internal::Del(keys);
}

void Internal::Del(const std::vector<std::string> &keys)
{
	m_redis_client->del(key);
	m_redis_client->sync_commit();
}

std::vector<std::string> Internal::Keys(const std::string &key)
{
	std::vector<std::string> result;
	auto value = m_redis_client->keys(TCHAR_TO_UTF8(*key));
	m_redis_client->sync_commit();

	if (!value.is_null())
	{
		std::vector<cpp_redis::reply> tmpKeys = value.get().as_array();
		for (cpp_redis::reply tmp : tmpKeys)
		{
			FString key = tmp.as_string().c_str();
			result.push_back(TCHAR_TO_UTF8(*key));
		}
	}

	return result;
}