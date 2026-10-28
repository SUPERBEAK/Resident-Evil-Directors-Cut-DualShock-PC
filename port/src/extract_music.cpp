#include "psx_seq.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#pragma pack(push, 1)
struct WavHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t fileSize = 0;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmtSize = 16;
  uint16_t audioFormat = 1; // PCM
  uint16_t numChannels = 2; // Stereo
  uint32_t sampleRate = 44100;
  uint32_t byteRate = 44100 * 2 * 2;
  uint16_t blockAlign = 4;
  uint16_t bitsPerSample = 16;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize = 0;
};
#pragma pack(pop)

static bool SaveWav(const std::wstring &outPath, const std::vector<int16_t> &stereoPcm, uint32_t sampleRate = 44100) {
  WavHeader hdr;
  hdr.sampleRate = sampleRate;
  hdr.byteRate = sampleRate * 2 * 2;
  hdr.dataSize = static_cast<uint32_t>(stereoPcm.size() * sizeof(int16_t));
  hdr.fileSize = sizeof(WavHeader) - 8 + hdr.dataSize;

  std::ofstream f(outPath, std::ios::binary);
  if (!f.is_open()) return false;
  f.write(reinterpret_cast<const char *>(&hdr), sizeof(hdr));
  f.write(reinterpret_cast<const char *>(stereoPcm.data()), hdr.dataSize);
  return true;
}

int main() {
  std::cout << "=========================================================\n";
  std::cout << "  Resident Evil DualShock - Complete Music Extractor     \n";
  std::cout << "  High-Fidelity Stereo 44.1 kHz PS1 SPU Music Synthesis \n";
  std::cout << "=========================================================\n\n";

  std::filesystem::create_directories("extracted/music_wav");

  const char *trackNames[57] = {
      "Mansion_First_Floor",          // SEP00
      "Silence_Ambience",             // SEP01
      "Main_Hall",                    // SEP02
      "Dining_Room_Tense",            // SEP03
      "Item_Found",                   // SEP04
      "Gallery_Corridor",             // SEP05
      "Basement_Clowns",              // SEP06 (Infamous DualShock Basement Theme)
      "Second_Floor_Corridor",        // SEP07
      "Courtyard_Garden",             // SEP08
      "Guardhouse_Residence",         // SEP09
      "Plant_42_Tension",             // SEP0A
      "Meeting_Rebecca_Save_Room",    // SEP0B (The One Who Survives)
      "Underground_Caves",            // SEP0C
      "Snake_Yawn_Battle",            // SEP0D
      "Plant_42_Battle",              // SEP0E
      "Safe_Haven_Save_Theme",        // SEP0F
      "Laboratory_Corridor",          // SEP10
      "Mansion_Stairs",               // SEP11
      "Basement_Laboratory",          // SEP12
      "Meeting_Jill",                 // SEP13
      "Deception_Encounter",          // SEP14
      "Betrayal_Tension",             // SEP15
      "Laboratory_Boiler",            // SEP16
      "Self_Destruct_Countdown",      // SEP17
      "Tyrant_Awakening",             // SEP18
      "Tyrant_Battle_1",              // SEP19
      "Helicopter_Rescue",            // SEP1A
      "Epilogue_Credits",             // SEP1B
      "Piano_Moonlight_Sonata",       // SEP1C
      "Piano_Practice_Rebecca",       // SEP1D
      "Clock_Puzzle_Chime",           // SEP1E
      "Passcode_Unlocked",            // SEP1F
      "Attic_Suspense",               // SEP20
      "Greenhouse_Plant",             // SEP21
      "Spider_Nest_Black_Tiger",      // SEP22
      "Laboratory_Specimen_Room",     // SEP23
      "Underground_Flooded_Basement", // SEP24
      "Tyrant_Final_Rooftop",         // SEP25
      "Power_Generator_Hum",          // SEP26
      "Forest_Speyer_Balcony",        // SEP27
      "Heliport_Beacon",              // SEP28
      "Serum_Room_Relief",            // SEP29
      "Mansion_Nightmare",            // SEP2A
      "Barry_Suspense",               // SEP2B
      "Wesker_Confrontation",         // SEP2C
      "Escape_Corridor",              // SEP2D
      "Laboratory_Morgue",            // SEP2E
      "Laboratory_Incubator",         // SEP2F
      "Final_Countdown_Escape",       // SEP30
      "Game_Over_Theme",              // SEP31
      "Main_Theme_Symphonic",         // SEP32
      "Ending_Roll_Full",             // SEP33
      "Bizarre_Mystery",              // SEP34
      "Triumph_Survivors",            // SEP35
      "Distorted_Memory",             // SEP36
      "Short_Jingle_Secret",          // SEP37
      "Victory_Stinger"               // SEP38
  };

  int successCount = 0;
  for (uint8_t bank = 0; bank <= 56; ++bank) {
    char hsbName[64];
    std::snprintf(hsbName, sizeof(hsbName), "SEP%02X.HSB", bank);

    std::string inPath = std::string("extracted/RE1-Dualshock/PSXUSA/SOUND/") + hsbName;
    if (!std::filesystem::exists(inPath)) {
      inPath = std::string("../extracted/RE1-Dualshock/PSXUSA/SOUND/") + hsbName;
    }
    if (!std::filesystem::exists(inPath)) {
      inPath = std::string("../../extracted/RE1-Dualshock/PSXUSA/SOUND/") + hsbName;
    }
    if (!std::filesystem::exists(inPath)) {
      std::printf("[Extract] Skipping %s (file not found)\n", hsbName);
      continue;
    }

    PSXSequencer seq;
    if (!seq.LoadHSB(inPath)) {
      std::printf("[Extract] Failed to load %s\n", hsbName);
      continue;
    }

    std::vector<int16_t> pcm;
    if (!seq.RenderFullSequenceToPCM(pcm, 180)) {
      std::printf("[Extract] Failed to render %s\n", hsbName);
      continue;
    }

    char outFileName[256];
    std::snprintf(outFileName, sizeof(outFileName), "extracted/music_wav/SEP%02X_%s.wav",
                  bank, trackNames[bank]);
    std::wstring outPath(outFileName, outFileName + std::strlen(outFileName));

    if (SaveWav(outPath, pcm)) {
      double duration = static_cast<double>(pcm.size() / 2) / 44100.0;
      std::printf("[Extract] [%2d/57] Extracted %s -> %s (%.2f seconds, stereo 44.1 kHz)\n",
                  bank + 1, hsbName, outFileName, duration);
      successCount++;
    }
  }

  std::printf("\n=========================================================\n");
  std::printf("  Extraction complete: %d/57 tracks successfully rendered!\n", successCount);
  std::printf("  Output directory: extracted/music_wav/\n");
  std::printf("=========================================================\n");
  return 0;
}
