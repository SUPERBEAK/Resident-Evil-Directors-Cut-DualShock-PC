#ifndef XAS_DECODER_H
#define XAS_DECODER_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

struct XasClipInfo {
  uint8_t stage = 1;
  uint16_t clipId = 0;
  uint8_t channel = 0;
  uint16_t startBlock = 0;
  uint16_t endBlock = 0;
  uint32_t startSector = 0;
  uint32_t endSector = 0;
  float durationSeconds = 0.0f;
  std::string xasFileName;
};

/**
 * XASDecoder handles the decoding of authentic PS1 CD-ROM XA voice files
 * (VOICE1.XAS .. VOICE5.XAS).
 * Each file consists of 2,336-byte Mode 2 Form 2 XA sectors containing
 * 16 interleaved channels (0..15) of 4-bit Sony XA ADPCM mono audio at 37.8 kHz.
 */
class XASDecoder {
public:
  XASDecoder();
  ~XASDecoder();

  /**
   * Opens an XAS file and prepares it for streaming a specific channel.
   * @param filePath Path to the .XAS file.
   * @param channelIndex Which channel to decode (0-15).
   * @return true if successful.
   */
  bool Open(const std::string &filePath, uint8_t channelIndex);

  /**
   * Opens a bounded clip range within an XAS file for streaming.
   */
  bool OpenClip(const std::string &filePath, uint8_t channelIndex,
                uint16_t startBlock, uint16_t endBlock);

  /**
   * Decodes the next chunk of 37.8 kHz mono PCM samples.
   * @param buffer Output buffer to store decoded PCM samples.
   * @param bufferSize Maximum number of samples to write into buffer.
   * @return Number of samples decoded.
   */
  uint32_t Decode(int16_t *buffer, uint32_t bufferSize);

  void Close();

  /**
   * Resolves a (stage, clipId) pair using the authentic stage_voice_offset_tbl
   * from STAGE1.EXE..STAGE7.EXE matching voice_play_clip() in cd.c.
   */
  static bool ResolveStageVoiceClip(uint8_t stage, uint16_t clipId,
                                    XasClipInfo &outInfo);

  /**
   * Returns the total number of valid voice clips in the given stage (1..7).
   */
  static uint16_t GetStageClipCount(uint8_t stage);

  /**
   * Resolves and decodes an entire voice clip for (stage, clipId) into 37.8 kHz
   * 16-bit mono PCM samples.
   */
  static bool DecodeStageClip(uint8_t stage, uint16_t clipId,
                              std::vector<int16_t> &outPcm,
                              XasClipInfo *outInfo = nullptr);

  /**
   * Decodes a specific (channel, startBlock..endBlock) range from an XAS file.
   */
  static bool DecodeClipRange(const std::string &filePath, uint8_t channel,
                              uint16_t startBlock, uint16_t endBlock,
                              std::vector<int16_t> &outPcm);

  /**
   * Locates the on-disk path for VOICE1.XAS .. VOICE5.XAS.
   */
  static std::string FindXasFilePath(const std::string &xasFileName);

private:
  std::ifstream m_file;
  uint8_t m_channel = 0;
  uint32_t m_currentBlock = 0;
  uint32_t m_endBlock = 0xFFFFFFFF;

  // Sony XA 4-bit ADPCM IIR filter history
  int32_t m_hist0 = 0;
  int32_t m_hist1 = 0;

  // Pending decoded sector samples for streaming Decode() calls
  std::vector<int16_t> m_sectorBuffer;
  size_t m_sectorReadPos = 0;

  void ResetState();
  static void DecodeMonoXaSector(const uint8_t *sec, int32_t &hist0,
                                 int32_t &hist1, std::vector<int16_t> &outPcm);
};

#endif // XAS_DECODER_H
