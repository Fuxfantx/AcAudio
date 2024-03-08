/* Aerials Audio System */
#pragma once
#define MINIAUDIO_IMPLEMENTATION

/* Trimming the miniaudio Library */
#define MA_NO_FLAC
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_API inline

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
#include <dmsdk/dlib/buffer.h>
#include <dmsdk/script/script.h>
#include <dmsdk/dlib/log.h>
#include <unordered_set>
#include <unordered_map>


/* Lua API Implementations */
// "Am": Aerials miniaudio binding module
struct AmUnit {   // Log the playing-or-not statuses of miniaudio sounds for suspending usage
	bool playing;
	ma_sound* sound_handle;
};

// The "Preview" Engine (fast to load, and slow to play)
ma_engine PreviewEngine;
ma_resource_manager* PreviewRM;
ma_resource_manager_data_source* PreviewResource;
AmUnit PreviewUnit;

// The "Player" Engine (slow to load, and fast to play)
ma_engine PlayerEngine;
ma_resource_manager player_rm, *PlayerRM;
std::unordered_set<ma_resource_manager_data_source*> PlayerResources;
std::unordered_map<ma_sound*, AmUnit> PlayerUnits;

// Resource Level
static int AmCreateResource(lua_State* L) {
	// ByteArray from Defold Lua
	void* B;	uint32_t BSize;
	const auto LB = dmScript::CheckBuffer(L, 1);
	dmBuffer::GetBytes(LB -> m_Buffer, &B, &BSize);

	// Decoding
	const auto N = ma_resource_manager_pipeline_notifications_init();
	ma_resource_manager_data_source* R = nullptr;
	ma_resource_manager_register_encoded_data(PlayerRM, "B", B, (size_t)BSize);
	const auto result = ma_resource_manager_data_source_init(
		PlayerRM, "B",
		MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE + MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT,
		&N, R);
	ma_resource_manager_unregister_data(PlayerRM, "B");

	// Do Returns
	if(result != MA_SUCCESS) {
		lua_pushboolean(L, false);
		lua_pushstring(L, "[!] Audio format not supported by miniaudio");
	}
	else {
		lua_pushboolean(L, true);
		lua_pushlightuserdata(L, R);
		PlayerResources.emplace(R);
	}
	return 2;
}
static int AmReleaseResource(lua_State* L) {
	const auto R = (ma_resource_manager_data_source*)lua_touserdata(L, 1);
	if( PlayerResources.count(R) ) {
		PlayerResources.erase(R);
		ma_resource_manager_data_source_uninit(R);
		lua_pushboolean(L, true);
	}
	else lua_pushboolean(L, false);
	return 1;
}

// Unit Level
static int AmCreateUnit(lua_State* L) {
	return 0;
}
static int AmReleaseUnit(lua_State* L) {
	return 0;
}
static int AmPlayUnit(lua_State* L) {
	return 0;
}
static int AmStopUnit(lua_State* L) {
	return 0;
}
static int AmGetTime(lua_State* L) {
	return 0;
}
static int AmSetTime(lua_State* L) {
	return 0;
}

// Preview Functions
static int AmPlayPreview(lua_State* L) {
	return 0;
}
static int AmStopPreview(lua_State* L) {
	return 0;
}


/* Binding Stuff */
constexpr luaL_reg AmFuncs[] =
{
	{"PlayPreview", AmPlayPreview}, {"StopPreview", AmStopPreview},
	{"CreateResource", AmCreateResource}, {"ReleaseResource", AmReleaseResource},
	{"CreateUnit", AmCreateUnit}, {"ReleaseUnit", AmReleaseUnit},
	{"PlayUnit", AmPlayUnit}, {"StopUnit", AmStopUnit},
	{"GetTime", AmGetTime}, {"SetTime", AmSetTime},
	{0, 0}
};

inline dmExtension::Result AmInit(dmExtension::Params* p) {
	// Init the Preview Engine, with Default Behaviors
	if( ma_engine_init(nullptr, &PreviewEngine) != MA_SUCCESS ) {
		dmLogFatal("Failed to Init the miniaudio Engine \"Preview\".");
		return dmExtension::RESULT_INIT_ERROR;
	}
	PreviewRM = ma_engine_get_resource_manager(&PreviewEngine);

	// Init the Player Engine: a custom resource manager
	const auto device = ma_engine_get_device(&PreviewEngine);   // The default device info
	auto rm_config		= ma_resource_manager_config_init();
		 rm_config.decodedFormat			= device -> playback.format;
		 rm_config.decodedChannels			= device -> playback.channels;
		 rm_config.decodedSampleRate		= device -> sampleRate;
	if( ma_resource_manager_init(&rm_config, &player_rm) != MA_SUCCESS) {
		dmLogFatal("Failed to Init the miniaudio Resource Manager \"PlayerRM\".");
		return dmExtension::RESULT_INIT_ERROR;
	}
	PlayerRM = &player_rm;

	// Init the Player Engine: a custom engine config
	auto engine_config			= ma_engine_config_init();
		 engine_config.pResourceManager		= PlayerRM;
	if( ma_engine_init(&engine_config, &PlayerEngine) != MA_SUCCESS ) {
		dmLogFatal("Failed to Init the miniaudio Engine \"Player\".");
		return dmExtension::RESULT_INIT_ERROR;
	}

	// Lua Registration
	luaL_register(p->m_L, "AcAudio", AmFuncs);
	lua_pop(p->m_L, 1);
	return dmExtension::RESULT_OK;
}

inline void AmOnEvent(dmExtension::Params* p, const dmExtension::Event* e) {
	switch(e->m_Event) {   // PreviewUnit.sound_handle won't be nullptr when playing
		case dmExtension::EVENT_ID_ACTIVATEAPP: {
			if( (PreviewUnit.playing) && !ma_sound_is_playing(PreviewUnit.sound_handle) )
				ma_sound_start(PreviewUnit.sound_handle);
			if( !PlayerUnits.empty() )
				for(auto it = PlayerUnits.cbegin(); it != PlayerUnits.cend(); ++it) {
					auto& u = it->second;
					if( (u.playing) && !ma_sound_is_playing(u.sound_handle) )
						ma_sound_start(u.sound_handle);
				}
		}
		break;

		case dmExtension::EVENT_ID_DEACTIVATEAPP: {   // Sounds won't rewind when "stopping"
			if(PreviewUnit.playing)
				ma_sound_stop(PreviewUnit.sound_handle);
			if( !PlayerUnits.empty() )
				for(auto it = PlayerUnits.cbegin(); it != PlayerUnits.cend(); ++it) {
					auto& u = it->second;
					if(u.playing)
						ma_sound_stop(u.sound_handle);
				}
		}

		default:;   // break omitted
	}
}

inline dmExtension::Result AmFinal(dmExtension::Params* p) {
	// Close Exisiting Units(miniaudio sounds)
	if(PreviewUnit.sound_handle) {
		ma_sound_stop(PreviewUnit.sound_handle);
		ma_sound_uninit(PreviewUnit.sound_handle);
	}
	if( !PlayerUnits.empty() )
		for(auto it = PlayerUnits.cbegin(); it != PlayerUnits.cend(); ++it) {
			ma_sound_stop(it->first);
			ma_sound_uninit(it->first);
		}

	// Close Existing Resources(miniaudio data sources)
	if(PreviewResource)
		ma_resource_manager_data_source_uninit(PreviewResource);
	if( !PlayerResources.empty() )
		for(auto it = PlayerResources.cbegin(); it != PlayerResources.cend(); ++it)
			ma_resource_manager_data_source_uninit(*it);

	// Uninit (miniaudio)Engines
	// Resource managers will be uninitialized automatically.
	ma_engine_uninit(&PreviewEngine);
	ma_engine_uninit(&PlayerEngine);

	// Since it's the finalizer, there is no further cleranup.
	return dmExtension::RESULT_OK;
}

inline dmExtension::Result AmAPPOK(dmExtension::AppParams* params) {
	return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(AcAudio, "AcAudio", AmAPPOK, AmAPPOK, AmInit, nullptr, AmOnEvent, AmFinal)