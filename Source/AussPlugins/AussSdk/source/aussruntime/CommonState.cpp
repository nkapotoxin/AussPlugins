#include <aussruntime/internal/CommonState.h>

using namespace Auss::Runtime;

Auss::Runtime::Internal::CommonState* Auss::Runtime::Internal::CommonState::m_instance;

Auss::Runtime::Internal::CommonState::CommonState()
{
}

Auss::Runtime::Internal::CommonState::~CommonState()
{
}

void Auss::Runtime::Internal::CommonState::SetInstance(Auss::Runtime::Internal::CommonState* instance)
{
	m_instance = instance;
}

Auss::Runtime::Internal::CommonState*
Auss::Runtime::Internal::CommonState::GetInstance()
{
	return m_instance;
}

Auss::Runtime::Internal::CommonState*
Auss::Runtime::Internal::CommonState::GetInstance(Auss::Runtime::Internal::AUSSRUNTIME_INTERNAL_STATE_TYPE stateType)
{
	if (!m_instance || m_instance->GetStateType() != stateType)
	{
		return nullptr;
	}

	return m_instance;
}

void Auss::Runtime::Internal::CommonState::DestroyInstance()
{
	if (m_instance)
	{
		delete m_instance;
		m_instance = nullptr;
	}
}