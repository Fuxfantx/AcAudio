/* Aerials Audio System */
#pragma once
#ifdef DM_PLATFORM_IOS  // Avoid iOS Runtime Linking
#define MA_NO_RUNTIME_LINKING
#endif


/* Lua API Implementations */
#define MA_NO_FLAC
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"





/* Binding Stuff */
static const luaL_reg Audio[] =
{

};

inline dmExtension::Result LuaInit(dmExtension::Params* p) {
	lua_State* L = p->m_L;

	// Register Modules
	luaL_register(L, "Audio", Audio);

	// Do API Stuff
    lua_pop(L, 1);   // Defold Restriction: Must Get the Lua Stack Balanced in the Initiation Process.
    return dmExtension::RESULT_OK;
}

inline dmExtension::Result OK(dmExtension::Params* params) {
    return dmExtension::RESULT_OK;
}

inline dmExtension::Result APPOK(dmExtension::AppParams* params) {
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(AcAudio, "AcAudio", APPOK, APPOK, LuaInit, nullptr, nullptr, OK)