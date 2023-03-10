#include <aussruntime/internal/CommonState.h>

using namespace Auss::Runtime;

Internal::CommonState* Internal::CommonState::m_instance;

Internal::CommonState::CommonState()
{
}

Internal::CommonState::~CommonState()
{
}

void Internal::SetInstance(Internal::CommonState* instance)
{
	m_instance = instance;
}

Internal::CommonState*
Internal::CommonState::GetInstance()
{
	return m_instance;
}

Internal::CommonState*
Internal::CommonState::GetInstance(Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE stateType)
{
	if (!m_instance || m_instance->GetStateType() != stateType)
	{
		return nullptr;
	}

	return m_instance;
}

void Internal::CommonState::DestroyInstance()
{
	if (m_instance)
	{
		delete m_instance;
		m_instance = nullptr;
	}
}