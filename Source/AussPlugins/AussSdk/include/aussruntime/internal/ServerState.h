#pragma once

#include <aussruntime/internal/CommonState.h>
#include <iostream>
#include <vector>
#include "../../ThirdParty/includes/cpp_redis/cpp_redis" // TODO(nkaptx): need optimise

namespace Auss
{
namespace Runtime
{
namespace Internal
{

	class ServerState : public CommonState
	{
	public:

		static ServerState* CreateInstance();

		virtual AUSSRUNTIME_INTERNAL_STATE_TYPE GetStateType() override { return AUSSRUNTIME_INTERNAL_STATE_TYPE::SERVER; };

		// Singleton constructors. Don't use these.
		ServerState();

		~ServerState();

		std::string Get(const std::string &key);

		void Set(const std::string &key, const std::string &value);

		void SyncSet(const std::string &key, const std::string &value);

		void Del(const std::string &key);

		void Del(const std::vector<std::string> &keys);

		std::vector<std::string> Keys(const std::string& key);

	private:

		std::string m_redis_server_ip;

		std::string m_redis_server_pw;

		int m_redis_server_port;

		cpp_redis::client* m_redis_client;

	};

} // namespace Internal
} // namespace Runtime
} // namespace Auss	