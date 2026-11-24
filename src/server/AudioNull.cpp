// Dedicated simulation has no audio devices, decoders or mixer.
#include "pre_inc.h"
#include "bflib_sndlib.h"
#include "bflib_sound.h"
#include "sounds.h"
#include <cstddef>
#include "post_inc.h"

extern "C" void FreeAudio() { }
extern "C" void custom_sound_bank_clear() { }
extern "C" void sound_register_id_redirect(SoundSmplTblID from_id, SoundSmplTblID to_id) { }
extern "C" void sound_clear_id_redirects(void) { }
extern "C" void sound_register_stack_policy(SoundSmplTblID smptbl_id, unsigned char mode, short max_instances) { }
extern "C" void sound_clear_stack_policies(void) { }
extern "C" void sound_save_id_redirect_snapshot(void) { }
extern "C" void sound_restore_id_redirect_snapshot(void) { }
extern "C" void SetSoundMasterVolume(SoundVolume volume) { }
extern "C" void set_music_volume(SoundVolume value) { }
extern "C" TbBool play_music(const char * fname) { return {}; }
extern "C" TbBool play_music_fgroup(short fgroup, const char * fname) { return {}; }
extern "C" TbBool play_music_track(int track) { return {}; }
extern "C" void pause_music() { }
extern "C" void resume_music() { }
extern "C" void stop_music(TbBool fade_out) { }
extern "C" TbBool GetSoundInstalled() { return {}; }
extern "C" void MonitorStreamedSoundTrack() { }
extern "C" void * GetSoundDriver() { return {}; }
extern "C" void StopAllSamples() { }
extern "C" TbBool InitAudio(const SoundSettings * settings) { return {}; }
extern "C" TbBool IsSamplePlaying(SoundMilesID mss_id) { return {}; }
extern "C" SoundVolume GetCurrentSoundMasterVolume() { return {}; }
extern "C" void SetSampleVolume(SoundEmitterID emit_id, SoundSmplTblID smptbl_id, SoundVolume volume) { }
extern "C" void SetSamplePan(SoundEmitterID emit_id, SoundSmplTblID smptbl_id, SoundPan pan) { }
extern "C" void SetSamplePitch(SoundEmitterID emit_id, SoundSmplTblID smptbl_id, SoundPitch pitch) { }
extern "C" SoundMilesID play_sample(
	SoundEmitterID emit_id,
	SoundSmplTblID smptbl_id,
	SoundVolume volume,
	SoundPan pan,
	SoundPitch pitch,
	char repeats,
	unsigned char ctype
) { return {}; }
extern "C" void stop_sample(SoundEmitterID emit_id, SoundSmplTblID smptbl_id) { }
extern "C" SoundSFXID get_sample_sfxid(SoundSmplTblID smptbl_id) { return {}; }
extern "C" SoundSmplTblID get_speech_offset(void) { return {}; }
extern "C" SoundSmplTblID get_custom_offset(void) { return {}; }
extern "C" int InitialiseSDLAudio() { return {}; }
extern "C" void ShutDownSDLAudio() { }
extern "C" TbBool play_streamed_sample(const char* fname, SoundVolume volume) { return {}; }
extern "C" void stop_streamed_samples() { }
extern "C" void set_streamed_sample_volume(SoundVolume volume) { }
extern "C" TbBool is_streamed_sample_playing(void) { return {}; }
extern "C" void toggle_bbking_mode() { }
extern "C" int custom_sound_bank_size() { return {}; }
extern "C" TbBool custom_sound_load_wav(const char* filepath, int sample_id) { return {}; }
extern "C" TbBool custom_sound_load_wav_mem(const unsigned char* data, size_t size,
	const char* logical_name, int sample_id) { return {}; }
