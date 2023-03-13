#include <aussruntime/internal/ServerState.h>

using namespace Auss::Runtime;

Auss::Runtime::Internal::ServerState::ServerState()
	: m_redis_server_ip("127.0.0.1")
	, m_redis_server_pw("")
	, m_redis_server_port(6379)
	, m_redis_client(nullptr)
{
	// TODO(nkaptx): read from local config file
	
	static cpp_redis::client client;
	client.connect(m_redis_server_ip, m_redis_server_port);
	client.auth(m_redis_server_pw);

	m_redis_client = &client;
}

Auss::Runtime::Internal::ServerState::~ServerState()
{
	Internal::CommonState::SetInstance(nullptr);
	if (m_redis_client)
	{
		delete m_redis_client;
		m_redis_client = nullptr;
	}
}

Auss::Runtime::Internal::ServerState*
Auss::Runtime::Internal::ServerState::CreateInstance()
{
	if (CommonState::GetInstance() != nullptr)
	{
		return (Internal::ServerState*)CommonState::GetInstance();
	}

	ServerState* newState = new ServerState;
	CommonState::SetInstance(newState);
	return newState;
}

std::string Auss::Runtime::Internal::ServerState::Get(const std::string &key)
{
	auto value = m_redis_client->get(key);
	m_redis_client->sync_commit();

	cpp_redis::reply reply = value.get();
	if (reply.is_null())
	{
		// TODO(nkaptx): log or return error
		return "";
	}

	return reply.as_string().c_str();
}

void Auss::Runtime::Internal::ServerState::Set(const std::string &key, const std::string &value)
{
	m_redis_client->set(key, value);
	m_redis_client->commit();
}

void Auss::Runtime::Internal::ServerState::SyncSet(const std::string &key, const std::string &value)
{
	m_redis_client->set(key, value);
	m_redis_client->sync_commit();
}

void Auss::Runtime::Internal::ServerState::Del(const std::string &key)
{
	std::vector<std::string> keys;
	keys.push_back(key);
	Internal::ServerState::Del(keys);
}

void Auss::Runtime::Internal::ServerState::Del(const std::vector<std::string> &keys)
{
	m_redis_client->del(keys);
	m_redis_client->sync_commit();
}

std::vector<std::string> Auss::Runtime::Internal::ServerState::Keys(const std::string &key)
{
	std::vector<std::string> result;
	auto value = m_redis_client->keys(key);
	m_redis_client->sync_commit();

	std::vector<cpp_redis::reply> tmpKeys = value.get().as_array();
	for (cpp_redis::reply tmp : tmpKeys)
	{
		FString key = tmp.as_string().c_str();
		result.push_back(TCHAR_TO_UTF8(*key));
	}

	return result;
}