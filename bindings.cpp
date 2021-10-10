#include <emscripten/bind.h>

#include <optional>

#include "MusicManager.h"
#include "Soundlib.h"

using namespace emscripten;

SoundManager *getSoundManager(MusicManager& self) {
    return theMusicSoundManager;
}

MusicManager *initMusicManager() {
    if(!theMusicSoundManager) {
        theMusicSoundManager = new SoundManager();
    }
    return new MusicManager();
}

EMSCRIPTEN_BINDINGS(mngplayer) {
    class_<SoundManager>("SoundManager")
        .function("Update", &SoundManager::Update)
        ;
    class_<MusicManager>("MusicManager")
        .constructor(&initMusicManager)
        .function("LoadScrambled", &MusicManager::LoadScrambled)
        .function("UpdateSettings", &MusicManager::UpdateSettings)
        .function("GetMood", &MusicManager::GetMood)
        .function("GetThreat", &MusicManager::GetThreat)
        .function("StartReadingTracks", &MusicManager::StartReadingTracks)
        .function("GetNextTrack", optional_override([](MusicManager& self) {
            std::string result;
            self.GetNextTrack(result);
            return result;
        }))
        .function("NumberOfTracks", &MusicManager::NumberOfTracks)
        .function("getSoundManager", &getSoundManager, allow_raw_pointers())
        .function("GetCurrentTrackName", optional_override([](MusicManager& self) {
            std::string result;
            LPCTSTR track = self.GetCurrentTrackName();
            if(track) {
                result = std::string(track);
            }
            return result;
        }))
        .function("Play", &MusicManager::Play)
        .function("Pause", &MusicManager::Pause)
        .function("Update", &MusicManager::Update)
        .function("BeginTrack", optional_override([](MusicManager& self, std::string track) {
            self.BeginTrack(track.c_str());
        }))
        .function("InteruptTrack", optional_override([](MusicManager& self, std::string track) {
            self.InteruptTrack(track.c_str());
        }))
        .function("Fade", &MusicManager::Fade)
        .function("SetVolume", &MusicManager::SetVolume)
        .function("GetVolume", &MusicManager::GetVolume)
        .function("IsPlaying", &MusicManager::IsPlaying)
        
        ;
}