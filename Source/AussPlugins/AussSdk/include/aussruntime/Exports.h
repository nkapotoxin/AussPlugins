#pragma once

#ifdef USE_IMPORT_EXPORT
	#ifdef AUSS_RUNTIME_EXPORTS
		#define AUSS_RUNTIME_API __declspec(dllexport)
	#else
		#define AUSS_RUNTIME_API __declspec(dllimport)
	#endif /* AUSS_RUNTIME_EXPORTS */
#else
	#define AUSS_RUNTIME_API
#endif // USE_IMPORT_EXPORT
