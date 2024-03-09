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
struct AmUnit {   // "Am": Aerials miniaudio binding module
	ma_sound* sound_handle;
	bool playing;   // Log the playing-or-not statuses of miniaudio sounds for suspending usage
};

// The "Preview" Engine (fast to load, and slow to play)
ma_engine PreviewEngine;
ma_resource_manager* PreviewRM;
ma_resource_manager_data_source* PreviewResource;   // delete & Set nullptr
AmUnit PreviewUnit;   // sound_handle: delete & Set nullptr
void* PreviewData;   // free & Set nullptr

// The "Player" Engine (slow to load, and fast to play)
ma_engine PlayerEngine;
ma_resource_manager player_rm, *PlayerRM;
std::unordered_set<ma_resource_manager_data_source*> PlayerResources;
std::unordered_map<ma_sound*, AmUnit> PlayerUnits;

// Resource Level
static int AmCreateResource(lua_State* L) {
	// Get the ByteArray from Defold Lua
	void* B;	uint32_t BSize;
	const auto LB = dmScript::CheckBuffer(L, 1);   // Buf
	dmBuffer::GetBytes(LB -> m_Buffer, &B, &BSize);

	// Decoding
	auto R = new ma_resource_manager_data_source;
	const auto N = ma_resource_manager_pipeline_notifications_init();
	ma_resource_manager_register_encoded_data(PlayerRM, "B", B, (size_t)BSize);
	const auto result = ma_resource_manager_data_source_init(
		PlayerRM, "B",
		// For "flags", using bor for the combination is recommended here
		MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT,
		&N, R);
	ma_resource_manager_unregister_data(PlayerRM, "B");

	// Do Returns
	if(result == MA_SUCCESS) {
		lua_pushboolean(L, true);   // OK
		lua_pushlightuserdata(L, R);   // Resource Handle or Msg
		PlayerResources.emplace(R);
	}
	else {
		lua_pushboolean(L, false);   // OK
		lua_pushstring(L, "[!] Audio format not supported by miniaudio");   // Resource Handle or Msg
		delete R;
	}
	return 2;
}
static int AmReleaseResource(lua_State* L) {
	/*
	 * Notice:
	 * You CANNOT release a resource refed by some unit(s).
	 */
	const auto R = (ma_resource_manager_data_source*)lua_touserdata(L, 1);   // Resource Handle
	if( PlayerResources.count(R) ) {
		PlayerResources.erase(R);
		ma_resource_manager_data_source_uninit(R);
		lua_pushboolean(L, true);   // OK
		delete R;
	}
	else
		lua_pushboolean(L, false);   // OK
	return 1;
}

// Unit Level
static int AmCreateUnit(lua_State* L) {
	// Create a Sound
	const auto S = new ma_sound;
	const auto R = (ma_resource_manager_data_source*)lua_touserdata(L, 1);   // Resource Handle
	const auto result = ma_sound_init_from_data_source(
		&PlayerEngine, R,
		// Notice that some "sound" flags same as "resource manager data source" flags are omitted here
		MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION,
		nullptr, S   // Set Group to nullptr is allowed here
		);

	// Do Returns
	if(result == MA_SUCCESS) {
		lua_pushboolean(L, true);   // OK
		lua_pushlightuserdata(L, S);   // Unit Handle or Msg

		// Audio Length in Ms
		uint64_t pcmlen = 0;   // The PCM length getter needs to return a ma_result value
		ma_resource_manager_data_source_get_length_in_pcm_frames(R, &pcmlen);
		lua_pushnumber( L, (double)pcmlen / (double)ma_engine_get_sample_rate(&PlayerEngine) * 1000.0 );

		// Unit Emplacing
		PlayerUnits[S] = {S, false};
		return 3;
	}
	else {
		lua_pushboolean(L, false);   // OK
		lua_pushstring(L, "[!] Failed to Initialize the Unit");   // Unit Handle or Msg
		delete S;
		return 2;
	}
}
static int AmReleaseUnit(lua_State* L) {
	const auto S = (ma_sound*)lua_touserdata(L, 1);   // Unit Handle

	if( PlayerUnits.count(S) ) {
		// Stop & Uninitialize
		if( PlayerUnits[S].playing )
			ma_sound_stop(S);
		ma_sound_uninit(S);

		// Clean Up & Return
		lua_pushboolean(L, true);   // OK
		PlayerUnits.erase(S);
		delete S;   // Remind to pair the "new" operator
	}
	else
		lua_pushboolean(L, false);   // OK

	return 1;
}
static int AmPlayUnit(lua_State* L) {
	const auto UH = (ma_sound*)lua_touserdata(L, 1);   // Unit Handle
	const bool is_looping = lua_toboolean(L, 2);   // IsLooping

	if( PlayerUnits.count(UH) ) {
		// Set Looping
		ma_sound_set_looping(UH, is_looping);

		// Start
		if( ma_sound_start(UH) == MA_SUCCESS ) {
			PlayerUnits[UH].playing = true;
			lua_pushnumber( L, ma_sound_get_time_in_milliseconds(UH) );   // Actual ms or nil
		}
		else {
			PlayerUnits[UH].playing = false;
			lua_pushnil(L);   // Actual ms or nil
		}
	}
	else
		lua_pushnil(L);   // Actual ms or nil

	return 1;
}
static int AmStopUnit(lua_State* L) {
	const auto UH = (ma_sound*)lua_touserdata(L, 1);   // Unit Handle

	if( PlayerUnits.count(UH) ) {
		ma_sound_stop(UH);
		if( lua_toboolean(L, 2) )   // Rewind to Start
			ma_sound_set_start_time_in_pcm_frames(UH, 0);
		PlayerUnits[UH].playing = false;
		lua_pushboolean(L, true);   // OK
	}
	else
		lua_pushboolean(L, false);   // OK

	return 1;
}
static int AmGetTime(lua_State* L) {
	const auto UH = (ma_sound*)lua_touserdata(L, 1);   // Unit Handle

	if( PlayerUnits.count(UH) )
		lua_pushnumber( L, ma_sound_get_time_in_milliseconds(UH) );   // Actual ms or nil
	else
		lua_pushnil(L);   // Actual ms or nil

	return 1;
}
static int AmSetTime(lua_State* L) {   // Supports both setting when playing & setting when stopped(paused)
	const auto UH = (ma_sound*)lua_touserdata(L, 1);   // Unit Handle
	auto ms = (int64_t)luaL_checknumber(L, 2);   // mstime

	if( PlayerUnits.count(UH) ) {
		// Get the sound length
		float len = 0;
		ma_sound_get_length_in_seconds(UH, &len);
		len *= 1000.0f;

		// Set the time
		ma_sound_stop(UH);
		ms = (ms > 0) ? ms : 0;
		ms = (ms < len) ? ms : len;
		ma_sound_set_start_time_in_milliseconds(UH, ms);

		// Return & Continue(if Playing)
		lua_pushnumber( L, ma_sound_get_time_in_milliseconds(UH) );   // Actual ms or nil
		if( PlayerUnits[UH].playing )
			ma_sound_start(UH);
	}
	else
		lua_pushnil(L);   // Actual ms or nil

	return 1;
}

// Preview Functions
inline void stop_preview() {
	// This function only consider the success circumstance,
	//     so there are lots of if judgements omitted.
	ma_sound_stop(PreviewUnit.sound_handle);
	ma_sound_uninit(PreviewUnit.sound_handle);
	delete PreviewUnit.sound_handle;

	PreviewUnit.sound_handle = nullptr;
	PreviewUnit.playing = false;

	ma_resource_manager_data_source_uninit(PreviewResource);
	delete PreviewResource;
	PreviewResource = nullptr;

	free(PreviewData);
	PreviewData = nullptr;
}
static int AmPlayPreview(lua_State* L) {
	const auto LB = dmScript::CheckBuffer(L, 1);   // Buf
	const bool is_looping = lua_toboolean(L, 2);   // IsLooping

	// Pre-Cleaning to avoid the memory leaking
	if(PreviewUnit.sound_handle)
		stop_preview();

	// Get the byte copy. This is for memory safety concerns.
	void* B;
	uint32_t BSize;
	dmBuffer::GetBytes(LB -> m_Buffer, &B, &BSize);
	PreviewData = malloc(BSize);
	memcpy(PreviewData, B, BSize);

	// Load Resource
	PreviewResource = new ma_resource_manager_data_source;
	const auto N = ma_resource_manager_pipeline_notifications_init();
	ma_resource_manager_register_encoded_data(PreviewRM, "PD", PreviewData, (size_t)BSize);
	const auto res_result = ma_resource_manager_data_source_init(
		PreviewRM, "PD",
		// For "flags", using bor for the combination is recommended here
		MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT,
		&N, PreviewResource);
	ma_resource_manager_unregister_data(PreviewRM, "PD");

	// Load Unit & Play
	if(res_result == MA_SUCCESS) {
		PreviewUnit.sound_handle = new ma_sound;
		const auto unit_result = ma_sound_init_from_data_source(
			&PreviewEngine, PreviewData,
			MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION,
			nullptr, PreviewUnit.sound_handle
		);
		if(unit_result == MA_SUCCESS) {
			// Set Looping
			ma_sound_set_looping( PreviewUnit.sound_handle, is_looping );

			// Start
			if( ma_sound_start(PreviewUnit.sound_handle) == MA_SUCCESS ) {
				lua_pushboolean(L, true);
				PreviewUnit.playing = true;
			}
			else {
				ma_sound_stop(PreviewUnit.sound_handle);

				// Clean Up 1
				ma_sound_uninit(PreviewUnit.sound_handle);
				delete PreviewUnit.sound_handle;
				PreviewUnit.sound_handle = nullptr;
				PreviewUnit.playing = false;

				// Clean Up 2,3
				lua_pushboolean(L, false);
				ma_resource_manager_data_source_uninit(PreviewResource);
				delete PreviewResource;		PreviewResource = nullptr;
				free(PreviewData);			PreviewData = nullptr;
			}
		}
		else {
			// Clean Up 1
			delete PreviewUnit.sound_handle;
			PreviewUnit.sound_handle = nullptr;
			PreviewUnit.playing = false;

			// Clean Up 2,3
			lua_pushboolean(L, false);
			ma_resource_manager_data_source_uninit(PreviewResource);
			delete PreviewResource;		PreviewResource = nullptr;
			free(PreviewData);			PreviewData = nullptr;
		}
	}
	else {   // Clean Up 2,3
		lua_pushboolean(L, false);
		delete PreviewResource;		PreviewResource = nullptr;
		free(PreviewData);			PreviewData = nullptr;
	}

	return 1;
}
static int AmStopPreview(lua_State* L) {   // Should Be Safe
	if(PreviewUnit.sound_handle)
		stop_preview();
	return 0;
}


/* Binding Stuff */
constexpr luaL_reg AmFuncs[] = {
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
				if( ma_sound_is_playing(PreviewUnit.sound_handle) )
					ma_sound_stop(PreviewUnit.sound_handle);
				else
					PreviewUnit.playing = false;
			if( !PlayerUnits.empty() )
				for(auto it = PlayerUnits.begin(); it != PlayerUnits.end(); ++it) {
					auto& u = it->second;   // Abandoned the const iterator
					if(u.playing)
						if( ma_sound_is_playing(u.sound_handle) )
							ma_sound_stop(u.sound_handle);
						else
							u.playing = false;
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

	// Uninit (miniaudio)Engines; resource managers will be uninitialized automatically here.
	ma_engine_uninit(&PreviewEngine);		ma_engine_uninit(&PlayerEngine);
	return dmExtension::RESULT_OK;   // Since it's the finalizer, there is no further cleranup.
}

inline dmExtension::Result AmAPPOK(dmExtension::AppParams* params) { return dmExtension::RESULT_OK; }
DM_DECLARE_EXTENSION(AcAudio, "AcAudio", AmAPPOK, AmAPPOK, AmInit, nullptr, AmOnEvent, AmFinal)