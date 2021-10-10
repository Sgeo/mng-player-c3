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
        .function("Update", &MusicManager::Update)
        .function("BeginTrack", optional_override([](MusicManager& self, std::string track) {
            self.BeginTrack(track.c_str());
        }))
        ;
}