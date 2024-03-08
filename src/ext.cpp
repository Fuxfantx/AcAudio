/* Aerials Audio System */
#pragma once
#ifdef DM_PLATFORM_IOS   // Avoid iOS Runtime Linking
#define MA_NO_RUNTIME_LINKING
#endif


/* SDK Includes */
#include <dmsdk/sdk.h>


/* Lua API Implementations */
#define MA_NO_FLAC
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"





/* Binding Stuff */
static const luaL_reg __AcAudio__[] =
{
	{0, 0}
};

inline dmExtension::Result __AcAudioLuaInit__(dmExtension::Params* p) {
	lua_State* L = p->m_L;

	// Register Modules
	luaL_register(L, "Audio", __AcAudio__);

	// Do API Stuff
	lua_pop(L, 1);   // Defold Restriction: Must Get the Lua Stack Balanced in the Initiation Process.
	return dmExtension::RESULT_OK;
}

inline dmExtension::Result __AcAudioOK__(dmExtension::Params* params) {
	return dmExtension::RESULT_OK;
}

inline dmExtension::Result __AcAudioAPPOK__(dmExtension::AppParams* params) {
	return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(AcAudio, "AcAudio", __AcAudioAPPOK__, __AcAudioAPPOK__, __AcAudioLuaInit__, nullptr, nullptr, __AcAudioOK__)