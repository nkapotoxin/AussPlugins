#pragma once

namespace Auss
{
namespace Runtime
{
namespace Internal
{
	class CommonState;

	enum class AUSSRUNTIME_INTERNAL_STATE_TYPE
	{
		CLIENT,
		SERVER
	};

	class CommonState
	{
	public:

		static CommonState* GetInstance();

		static CommonState* GetInstance(AUSSRUNTIME_INTERNAL_STATE_TYPE stateType);

		static void SetInstance(CommonState* instance);

		static void DestroyInstance();

		virtual AUSSRUNTIME_INTERNAL_STATE_TYPE GetStateType() = 0;

		//Needs access to it public
		virtual ~CommonState();

	protected:
	
		CommonState();

	private:

		static CommonState* m_instance;
		
	};

} // namespace Internal
} // namespace Runtime
} // namespace Auss