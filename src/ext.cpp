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

// The "Player" Engine (slow to load, and fast to play)
ma_engine PlayerEngine;
ma_resource_manager player_rm, *PlayerRM;
std::unordered_map<ma_resource_manager_data_source*, void*> PlayerResources;   // Handle, CopiedBuffer
std::unordered_map<ma_sound*, AmUnit> PlayerUnits;   // Handle, {Handle, IsPlaying}

// Resource Level
static int AmCreateResource(lua_State* L) {
	const auto LB = dmScript::CheckBuffer(L, 1);   // Buf

	// Copy the ByteArray from Defold Lua
	void *OB;
	uint32_t BSize;
	dmBuffer::GetBytes(LB -> m_Buffer, &OB, &BSize);

	void *B = malloc(BSize);
	memcpy(B, OB, BSize);

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
		PlayerResources.emplace(R, B);
	}
	else {
		lua_pushboolean(L, false);   // OK
		lua_pushstring(L, "[!] Audio format not supported by miniaudio");   // Resource Handle or Msg
		delete R;
		free(B);
	}
	return 2;
}
static int AmReleaseResource(lua_State* L) {
	/*
	 * Notice:
	 * You CANNOT release a resource refed by some unit(s).
	 */
	const auto RH = (ma_resource_manager_data_source*)lua_touserdata(L, 1);   // Resource Handle
	if( PlayerResources.count(RH) ) {
		ma_resource_manager_data_source_uninit(RH);
		free(PlayerResources[RH]);			PlayerResources.erase(RH);
		lua_pushboolean(L, true);   // OK
		delete RH;
	}
	else
		lua_pushboolean(L, false);   // OK
	return 1;
}

// Unit Level
static int AmCreateUnit(lua_State* L) {
	// Create a Sound
	const auto S = new ma_sound;
	const auto RH = (ma_resource_manager_data_source*)lua_touserdata(L, 1);   // Resource Handle
	const auto result = ma_sound_init_from_data_source(
		&PlayerEngine, RH,
		// Notice that some "sound" flags same as "resource manager data source" flags are omitted here
		MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION,
		nullptr, S   // Set Group to nullptr is allowed here
		);

	// Do Returns
	if(result == MA_SUCCESS) {
		lua_pushboolean(L, true);   // OK
		lua_pushlightuserdata(L, S);   // Unit Handle or Msg

		// Audio Length in Ms
		float len = 0;   // The length getter needs to return a ma_result value
		ma_sound_get_length_in_seconds(S, &len);
		lua_pushnumber( L, (uint64_t)(len * 1000.0) );

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
			lua_pushboolean(L, true);   // OK
		}
		else {
			PlayerUnits[UH].playing = false;
			lua_pushboolean(L, false);   // OK
		}
	}
	else
		lua_pushboolean(L, false);   // OK

	return 1;
}
static int AmStopUnit(lua_State* L) {
	const auto UH = (ma_sound*)lua_touserdata(L, 1);   // Unit Handle

	if( PlayerUnits.count(UH) ) {
		if( ma_sound_stop(UH) == MA_SUCCESS) {
			if( lua_toboolean(L, 2) )   // Rewind to Start
				ma_sound_set_start_time_in_pcm_frames(UH, 0);
			PlayerUnits[UH].playing = false;
			lua_pushboolean(L, true);   // OK
		}
		else
			lua_pushboolean(L, false);   // OK
	}
	else
		lua_pushboolean(L, false);   // OK

	return 1;
}
static int AmCheckPlaying(lua_State* L) {
	const auto UH = (ma_sound*)lua_touserdata(L, 1);   // Unit Handle

	if( PlayerUnits.count(UH) ) {
		const bool p = ma_sound_is_playing(UH);
		PlayerUnits[UH].playing = p;
		lua_pushboolean(L, p);   // Status
	}
	else
		lua_pushnil(L);   // Status

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
static int AmSetTime(lua_State* L) {
	/* Keep in mind that this is an ASYNC API. */
	const auto U = (ma_sound*)lua_touserdata(L, 1);   // Unit Handle
	auto ms = (int64_t)luaL_checknumber(L, 2);   // mstime

	if( PlayerUnits.count(U) && (!PlayerUnits[U].playing) ) {
		// Get the sound length
		float len = 0;
		ma_sound_get_length_in_seconds(U, &len);		len *= 1000.0f;

		// Set the time
		ms = (ms > 0) ? ms : 0;
		ms = (ms < len-2.0) ? ms : len-2.0;
		const auto result = ma_sound_seek_to_pcm_frame(U,
			(uint64_t)(ms * ma_engine_get_sample_rate(&PlayerEngine) / 1000.0)
		);
		lua_pushboolean(L, result == MA_SUCCESS);   // OK
	}
	else
		lua_pushboolean(L, false);   // OK

	return 1;
}

// Preview Functions
static int AmStopPreview(lua_State* L) {   // Should be always safe
	if(PreviewUnit.sound_handle) {
		ma_sound_stop(PreviewUnit.sound_handle);
		ma_sound_uninit(PreviewUnit.sound_handle);
		delete PreviewUnit.sound_handle;

		PreviewUnit.sound_handle = nullptr;
		PreviewUnit.playing = false;

		ma_resource_manager_data_source_uninit(PreviewResource);
		delete PreviewResource;
		PreviewResource = nullptr;
	}
	return 0;
}
static int AmPlayPreview(lua_State* L) {
	const auto LB = dmScript::CheckBuffer(L, 1);   // Buf
	const bool is_looping = lua_toboolean(L, 2);   // IsLooping

	// Get the ByteArray & Pre-Cleaning
	uint32_t BSize, *B;
	dmBuffer::GetBytes(LB -> m_Buffer, (void**)&B, &BSize);
	AmStopPreview(L);

	// Load Resource
	PreviewResource = new ma_resource_manager_data_source;
	const auto N = ma_resource_manager_pipeline_notifications_init();
	ma_resource_manager_register_encoded_data(PreviewRM, "PD", B, (size_t)BSize);
	const auto res_result = ma_resource_manager_data_source_init(
		PreviewRM, "PD",
		MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT,
		&N, PreviewResource);
	ma_resource_manager_unregister_data(PreviewRM, "PD");

	// Load Unit & Play
	if(res_result == MA_SUCCESS) {
		PreviewUnit.sound_handle = new ma_sound;
		const auto unit_result = ma_sound_init_from_data_source(
			&PreviewEngine, PreviewResource,
			MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION,
			nullptr, PreviewUnit.sound_handle
		);
		if(unit_result == MA_SUCCESS) {
			// Set Looping
			ma_sound_set_looping( PreviewUnit.sound_handle, is_looping );

			// Start
			if( ma_sound_start(PreviewUnit.sound_handle) == MA_SUCCESS ) {
				lua_pushboolean(L, true);   // OK
				PreviewUnit.playing = true;
			}
			else {
				// Clean Up 1
				ma_sound_stop(PreviewUnit.sound_handle);
				ma_sound_uninit(PreviewUnit.sound_handle);

				delete PreviewUnit.sound_handle;
				PreviewUnit.sound_handle = nullptr;
				PreviewUnit.playing = false;

				// Clean Up 2
				lua_pushboolean(L, false);   // OK
				ma_resource_manager_data_source_uninit(PreviewResource);
				delete PreviewResource;		PreviewResource = nullptr;
			}
		}
		else {
			// Clean Up 1
			delete PreviewUnit.sound_handle;
			PreviewUnit.sound_handle = nullptr;
			PreviewUnit.playing = false;

			// Clean Up 2
			lua_pushboolean(L, false);   // OK
			ma_resource_manager_data_source_uninit(PreviewResource);
			delete PreviewResource;		PreviewResource = nullptr;
		}
	}
	else {   // Clean Up 2
		lua_pushboolean(L, false);   // OK
		delete PreviewResource;
		PreviewResource = nullptr;
	}

	return 1;
}


/* Binding Stuff */ constexpr luaL_reg AmFuncs[] = {
	{"PlayPreview", AmPlayPreview}, {"StopPreview", AmStopPreview},
	{"CreateResource", AmCreateResource}, {"ReleaseResource", AmReleaseResource},
	{"CreateUnit", AmCreateUnit}, {"ReleaseUnit", AmReleaseUnit},
	{"PlayUnit", AmPlayUnit}, {"StopUnit", AmStopUnit},
	{"GetTime", AmGetTime}, {"SetTime", AmSetTime},
	{"CheckPlaying", AmCheckPlaying},
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
	if( !PlayerUnits.empty() )   // No free() calls since it's the finalizer
		for(auto it = PlayerUnits.cbegin(); it != PlayerUnits.cend(); ++it) {
			ma_sound_stop(it->first);
			ma_sound_uninit(it->first);
		}

	// Close Existing Resources(miniaudio data sources)
	if(PreviewResource)
		ma_resource_manager_data_source_uninit(PreviewResource);
	if( !PlayerResources.empty() )
		for(auto it = PlayerResources.cbegin(); it != PlayerResources.cend(); ++it)
			ma_resource_manager_data_source_uninit(it->first);

	// Uninit (miniaudio)Engines; resource managers will be uninitialized automatically here.
	ma_engine_uninit(&PreviewEngine);			ma_engine_uninit(&PlayerEngine);
	return dmExtension::RESULT_OK;   // No further cleranup since it's the finalizer
}

inline dmExtension::Result AmAPPOK(dmExtension::AppParams* params) {
	return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(AcAudio, "AcAudio", AmAPPOK, AmAPPOK, AmInit, nullptr, AmOnEvent, AmFinal)