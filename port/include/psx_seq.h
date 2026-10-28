#ifndef PSX_SEQ_H
#define PSX_SEQ_H

#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

#pragma pack(push, 1)
struct PSXVabHdr {
  char form[4];
  uint32_t ver;
  uint32_t id;
  uint32_t fsize;
  uint16_t reserved0;
  uint16_t ps;
  uint16_t ts;
  uint16_t vs;
  uint8_t mvol;
  uint8_t mpan;
  uint8_t attr1;
  uint8_t attr2;
  uint32_t reserved1;
};

struct PSXProgAtr {
  uint8_t tones;
  uint8_t mvol;
  uint8_t prior;
  uint8_t mode;
  uint8_t mpan;
  int8_t reserved0;
  int16_t attr;
  uint32_t reserved1;
  uint16_t reserved2;
  uint16_t reserved3;
};

struct PSXVagAtr {
  uint8_t prior;
  uint8_t mode;
  uint8_t vol;
  uint8_t pan;
  uint8_t center;
  uint8_t shift;
  uint8_t min_key;
  uint8_t max_key;
  uint8_t vibW;
  uint8_t vibT;
  uint8_t porW;
  uint8_t porT;
  uint8_t pbmin;
  uint8_t pbmax;
  int8_t reserved1;
  int8_t reserved2;
  uint16_t adsr1;
  uint16_t adsr2;
  int16_t prog;
  int16_t vag;
  int16_t reserved[4];
};
#pragma pack(pop)

struct VagInfo {
  std::vector<int16_t> pcm;
  size_t loopStart = SIZE_MAX;
  size_t loopEnd = SIZE_MAX;
  bool isLooping = false;
};

struct PSXSeqTrack {
  uint16_t seqNumber;
  uint16_t resolution;
  uint32_t tempoUsec;
  uint32_t length;
  std::vector<uint8_t> eventData;
};

class PSXSequencer {
public:
  PSXSequencer();
  ~PSXSequencer();

  bool LoadHSB(const std::wstring &filePath);
  bool LoadHSB(const std::string &filePath);

  bool RenderTrackToPCM(uint32_t trackIndex, std::vector<int16_t> &outStereoPcm,
                        uint32_t maxSeconds = 180);
  bool RenderFullSequenceToPCM(std::vector<int16_t> &outStereoPcm,
                               uint32_t maxSeconds = 180);

  size_t GetTrackCount() const { return m_tracks.size(); }
  bool IsLoaded() const { return !m_tracks.empty(); }
  void Clear();

private:
  PSXVabHdr m_vabHdr;
  std::vector<PSXProgAtr> m_programs;
  std::vector<PSXVagAtr> m_tones;
  std::vector<uint16_t> m_vagSizes;
  std::unordered_map<uint16_t, VagInfo> m_decodedVags;
  std::vector<PSXSeqTrack> m_tracks;

  void DecodeSpuAdpcm(const uint8_t *adpcm, size_t size, std::vector<int16_t> &outPcm,
                       size_t &outLoopStart, size_t &outLoopEnd, bool &outIsLooping);
};

#endif // PSX_SEQ_H
