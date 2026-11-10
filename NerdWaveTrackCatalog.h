#pragma once
#include <Arduino.h>
#include "MusicApi.h"

namespace NerdWaveTrackCatalog {
String parentFolder(const String& path);
bool isMp3File(const MusicItem& item);
bool pickRandomTrack(MusicApi& api, const String& folder, bool recursive,
                     const String& currentTrack, String& path, String& name);
bool trackExists(MusicApi& api, const String& path);
bool firstPlayable(MusicApi& api, String& path, String& name);
}
