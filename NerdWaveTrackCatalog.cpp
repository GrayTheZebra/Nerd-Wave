#include "NerdWaveLog.h"
#include "NerdWaveTrackCatalog.h"
#include "RandomPick.h"
#include <esp_system.h>

namespace NerdWaveTrackCatalog {
String parentFolder(const String& path) {
  int slash=path.lastIndexOf('/');
  if(slash<=0) return "/";
  return path.substring(0,slash);
}

bool isMp3File(const MusicItem& item) {
  if(item.directory) return false;
  String s=item.name; s.toLowerCase();
  return s.endsWith(".mp3");
}


static void scanRandomFolder(MusicApi& api, const String& folder, bool recursive, const String& currentTrack, RandomPick& pick) {
  // Iterative Breitensuche: keine tiefe Rekursion auf dem Arduino-loopTask.
  // Die Namen brauchen nur wenig Speicher; keine komplette MP3-Liste noetig.
  static String pending[200];
  uint16_t head=0, tail=1;
  pending[0]=folder;
  while(head<tail && !pick.failed) {
    String dir=pending[head];
    pending[head++]="";
    ++pick.folders;
    MusicPage list;
    uint16_t offset=0;
    int total=-1;
    do {
      if(!api.fetch(dir,offset,list)) {
        NerdWaveLog::output.printf("[SHUFFLE] API-Fehler %s: %s\n",dir.c_str(),api.error().c_str());
        pick.failed=true;break;
      }
      total=list.total;
      if(list.count==0 && offset<total) {pick.failed=true;break;}
      for(int j=0;j<list.count;j++) {
        const MusicItem& it=list.items[j];
        if(it.directory) {
          if(!recursive) continue;
          if(tail>=200) {pick.failed=true;break;}
          // Nur Nachfahren des Startordners zulassen.
          if(it.path==dir || !it.path.startsWith(folder)) continue;
          pending[tail++]=it.path;
          continue;
        }
        if(!isMp3File(it) || it.path==currentTrack) continue;
        ++pick.count;
        if((esp_random()%pick.count)==0) {
          pick.path=it.path;
          pick.name=it.name;
        }
      }
      offset+=list.count;
    } while(offset<total && !pick.failed);
  }
  for(uint16_t i=head;i<tail;i++) pending[i]="";
}




bool pickRandomTrack(MusicApi& api, const String& folder, bool recursive, const String& currentTrack, String& path, String& name) {
  RandomPick pick;
  scanRandomFolder(api,folder,recursive,currentTrack,pick);
  if(pick.failed) {NerdWaveLog::output.println("[SHUFFLE] Scan abgebrochen");return false;}
  if(!pick.count) {
    NerdWaveLog::output.println("[SHUFFLE] Kein anderer MP3-Titel gefunden");
    return false;
  }
  path=pick.path;name=pick.name;
  NerdWaveLog::output.printf("[SHUFFLE] %lu Kandidaten in %u Ordner(n): %s\n",
    (unsigned long)pick.count,(unsigned)pick.folders,path.c_str());
  return true;
}


bool trackExists(MusicApi& api, const String& path) {
  if(!path.length()) return false;
  const String folder=parentFolder(path);
  MusicPage list;
  for(uint16_t offset=0;;) {
    if(!api.fetch(folder,offset,list)) return false;
    for(uint16_t i=0;i<list.count;i++)
      if(!list.items[i].directory && list.items[i].path==path && isMp3File(list.items[i])) return true;
    if(list.count==0 || offset+list.count>=list.total) return false;
    offset+=list.count;
  }
}

// Iterative Breitensuche: jede Verzeichnisseite nur einmal uebertragen.
// Zuerst MP3s im Stammverzeichnis, anschliessend die Unterordner.
bool firstPlayable(MusicApi& api, String& path, String& name) {
  static String pending[200];
  uint16_t head=0,tail=1;
  pending[0]="/";
  uint32_t examined=0;
  MusicPage page;
  while(head<tail && examined<4000) {
    String folder=pending[head];pending[head++]="";
    for(uint16_t offset=0;;) {
      if(!api.fetch(folder,offset,page)) {
        NerdWaveLog::output.printf("[AUTOBOOT] Ordner nicht lesbar: %s\n",folder.c_str());
        break;
      }
      for(uint16_t i=0;i<page.count;i++) {
        MusicItem& item=page.items[i];
        ++examined;
        if(isMp3File(item)) {
          path=item.path;name=item.name;
          // Alle zuvor gemerkten Ordnerpfade freigeben.
          for(uint16_t j=head;j<tail;j++) pending[j]="";
          return true;
        }
        if(item.directory && tail<200) pending[tail++]=item.path;
      }
      if(!page.count || (uint32_t)offset+page.count>=page.total || examined>=4000) break;
      offset+=page.count;
    }
  }
  for(uint16_t j=head;j<tail;j++) pending[j]="";
  return false;
}


} // namespace NerdWaveTrackCatalog
