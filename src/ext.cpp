/* Aerials Audio System */
#pragma once
#define MINIAUDIO_IMPLEMENTATION

/* Trimming the miniaudio Library */
#define MA_NO_FLAC
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_API static

#ifdef DM_PLATFORM_WINDOWS
	#define MA_ENABLE_WASAPI
#endif

#ifdef DM_PLATFORM_ANDROID
	#define MA_ENABLE_AAUDIO
	#define MA_ENABLE_OPENSL
#endif

#ifdef DM_PLATFORM_IOS
	#define MA_ENABLE_COREAUDIO
	#define MA_NO_RUNTIME_LINKING   // Avoid iOS Runtime Linking
#endif

#ifdef DM_PLATFORM_OSX
	#define MA_ENABLE_COREAUDIO
	#define MA_NO_RUNTIME_LINKING   // Avoid macOS Runtime Linking
#endif

/* Includes */
#include <miniaudio.h>
#include <dmsdk/sdk.h>


/* Lua API Implementations */


/* Binding Stuff */
static const luaL_reg __AcAudio__[] =
{
	{0, 0}
};

inline dmExtension::Result __AcA_Init__(dmExtension::Params* p) {
	lua_State* L = p->m_L;

	// Register Modules
	luaL_register(L, "Audio", __AcAudio__);

	// Do API Stuff
	lua_pop(L, 1);   // Defold Restriction: Must Get the Lua Stack Balanced in the Initiation Process.
	return dmExtension::RESULT_OK;
}

inline dmExtension::Result __AcA_OK__(dmExtension::Params* params) {
	return dmExtension::RESULT_OK;
}

inline dmExtension::Result __AcA_APPOK__(dmExtension::AppParams* params) {
	return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(AcAudio, "AcAudio", __AcA_APPOK__, __AcA_APPOK__, __AcA_Init__, 0, 0, __AcA_OK__)