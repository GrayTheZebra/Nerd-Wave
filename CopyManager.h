#pragma once
#include <Arduino.h>
class MusicApi;

// Asynchroner Kopier-Task: Zustandsverwaltung und threadsicherer Fortschritt.
// Dateioperationen verbleiben bis zur separaten Entkopplung in der Runtime.
namespace CopyManager {
  enum class State : uint8_t { Idle=0, Running=1, Finished=2 };
  struct Progress {
    char name[96] = {};
    uint64_t done = 0;
    uint64_t total = 0;
    uint32_t files = 0;
    uint32_t serial = 0;
  };
  struct TransferContext {
    MusicApi& api;
    int& destination;
    volatile bool& abort;
    String& result;
    bool& useWifi;
    bool& progressDrawn;
  };
  void setTransferContext(TransferContext& context);
  bool copySafePath(const String& path);
  bool copyReadyDestination();
  bool copyMkdir(const String& path);
  bool copyOneFile(uint8_t origin,const String& from,const String& to,uint32_t& files);
  bool copyTree(uint8_t origin,const String& folder,const String& destination,uint32_t& files,int depth);
  bool start(TaskFunction_t worker);
  void finish();
  State state();
  bool consumeFinished();
  void publish(const String& name, uint64_t done, uint64_t total, uint32_t files);
  void snapshot(Progress& output);
}
