#include "xas_decoder.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>

static const float K0[5] = {0.0f, 60.0f / 64.0f, 115.0f / 64.0f, 98.0f / 64.0f, 122.0f / 64.0f};
static const float K1[5] = {0.0f, 0.0f, -52.0f / 64.0f, -55.0f / 64.0f, -60.0f / 64.0f};

// Pre-extracted voice offset tables directly from authentic PS1 STAGE1.EXE .. STAGE5.EXE binaries
// Stage 6 uses STAGE1 table (Mansion revisited 1F)
// Stage 7 uses STAGE2 table (Mansion revisited 2F)
static const uint16_t s_Stage1VoiceOffsetTbl[151] = {
    0x0000, 0x0016, 0x0050, 0x0060, 0x00A7, 0x00C1, 0x00DB, 0x00FF, 0x0122, 0x814A, 0x015E, 0x0000, 0x0026, 0x005D, 0x00CA, 0x810B, 0x0155, 0x0000,
    0x0030, 0x0043, 0x009E, 0x00C0, 0x00DB, 0x010C, 0x813A, 0x015D, 0x0000, 0x006A, 0x0086, 0x00BE, 0x80E2, 0x0116, 0x0000, 0x008E, 0x00AC, 0x00C1,
    0x00E2, 0x0103, 0x813D, 0x015A, 0x0000, 0x002C, 0x0041, 0x0062, 0x0082, 0x00AD, 0x00C0, 0x00FA, 0x0115, 0x8143, 0x0166, 0x0000, 0x0013, 0x002C,
    0x004F, 0x0062, 0x0088, 0x00B2, 0x00D4, 0x00E5, 0x00F0, 0x813C, 0x0181, 0x0000, 0x004C, 0x8066, 0x00B2, 0x8000, 0x018B, 0x0000, 0x0029, 0x0066,
    0x009D, 0x00BF, 0x00EE, 0x00FC, 0x010B, 0x0116, 0x0138, 0x8158, 0x0164, 0x0000, 0x0011, 0x002B, 0x0044, 0x0062, 0x006A, 0x0083, 0x008D, 0x009B,
    0x00BD, 0x00CE, 0x00DF, 0x00F2, 0x0102, 0x012A, 0x814B, 0x0169, 0x0000, 0x000B, 0x001A, 0x0027, 0x003C, 0x0067, 0x0093, 0x009F, 0x00AA, 0x00B6,
    0x00F1, 0x0100, 0x010B, 0x0129, 0x813F, 0x0169, 0x0000, 0x0020, 0x0081, 0x00A7, 0x00C6, 0x00D6, 0x0106, 0x8133, 0x0158, 0x0000, 0x0021, 0x0035,
    0x0063, 0x00A1, 0x00CA, 0x0124, 0x8141, 0x0157, 0x0000, 0x0032, 0x0040, 0x008D, 0x009F, 0x00B7, 0x00D2, 0x00F8, 0x8121, 0x015B, 0x0000, 0x0023,
    0x002C, 0x007C, 0x0092, 0x016B, 0x824C, 0x026A, 0x3051
};

static const uint16_t s_Stage2VoiceOffsetTbl[111] = {
    0x0000, 0x000A, 0x0027, 0x0056, 0x008C, 0x80B4, 0x00C5, 0x0000, 0x0047, 0x005F, 0x006A, 0x00A4, 0x00B3, 0x80D2, 0x00DE, 0x0000, 0x002B, 0x0057,
    0x0062, 0x0086, 0x0096, 0x80B2, 0x00DA, 0x0000, 0x0025, 0x0056, 0x0067, 0x008F, 0x80C8, 0x00D5, 0x0000, 0x0027, 0x0036, 0x0040, 0x005A, 0x0062,
    0x0085, 0x80B7, 0x00D2, 0x0000, 0x8016, 0x00EF, 0x8000, 0x00E1, 0x0000, 0x000C, 0x0047, 0x008D, 0x80A0, 0x00DA, 0x0000, 0x002E, 0x0058, 0x0068,
    0x0076, 0x8096, 0x00B0, 0x0000, 0x0037, 0x0064, 0x008A, 0x80B3, 0x0101, 0x0000, 0x001F, 0x005D, 0x0075, 0x0085, 0x8095, 0x00AD, 0x0000, 0x0042,
    0x0066, 0x009A, 0x80C5, 0x00D6, 0x0000, 0x0024, 0x0041, 0x004D, 0x007F, 0x80BB, 0x00CF, 0x0000, 0x002E, 0x003E, 0x0058, 0x0071, 0x0096, 0x00B9,
    0x80BF, 0x00EE, 0x0000, 0x0007, 0x0026, 0x0039, 0x006C, 0x007E, 0x00A9, 0x80B8, 0x00EF, 0x0000, 0x0026, 0x0040, 0x0056, 0x006F, 0x0092, 0x00A7,
    0x80E3, 0x0101, 0x53EE
};

static const uint16_t s_Stage3VoiceOffsetTbl[67] = {
    0x0000, 0x0017, 0x004D, 0x8075, 0x0097, 0x0000, 0x0038, 0x0060, 0x8090, 0x00CA, 0x0000, 0x0036, 0x8073, 0x00C7, 0x0000, 0x0017, 0x0032, 0x005A,
    0x0093, 0x80B0, 0x00C4, 0x0000, 0x0025, 0x0042, 0x806D, 0x00BC, 0x0000, 0x0019, 0x8084, 0x00BB, 0x0000, 0x0031, 0x808A, 0x00B7, 0x0000, 0x005F,
    0x808F, 0x00E9, 0x0000, 0x0052, 0x805D, 0x00C1, 0x0000, 0x0010, 0x0053, 0x807D, 0x00CF, 0x0000, 0x0013, 0x0077, 0x808D, 0x00D0, 0x0000, 0x0033,
    0x006A, 0x80B0, 0x00C5, 0x8000, 0x0050, 0x8000, 0x026C, 0x0000, 0x8011, 0x0073, 0x8000, 0x0010, 0x1E06
};

static const uint16_t s_Stage4VoiceOffsetTbl[87] = {
    0x8000, 0x00F6, 0x0000, 0x000F, 0x0030, 0x8070, 0x00BF, 0x0000, 0x8074, 0x00B2, 0x0000, 0x0026, 0x0030, 0x0038, 0x808D, 0x0097, 0x0000, 0x0064,
    0x807E, 0x00AC, 0x0000, 0x0045, 0x005D, 0x006F, 0x0091, 0x80A0, 0x00DF, 0x0000, 0x0021, 0x003C, 0x8094, 0x00AB, 0x0000, 0x003B, 0x0076, 0x0095,
    0x80BA, 0x00CC, 0x0000, 0x000A, 0x0024, 0x8067, 0x00D4, 0x0000, 0x000A, 0x0012, 0x0025, 0x0038, 0x8051, 0x00A4, 0x0000, 0x8059, 0x00BA, 0x0000,
    0x0065, 0x006F, 0x007A, 0x0085, 0x809D, 0x00D6, 0x0000, 0x005A, 0x8080, 0x00A9, 0x0000, 0x0038, 0x0058, 0x0078, 0x8087, 0x00CE, 0x0000, 0x0029,
    0x0040, 0x004B, 0x8095, 0x00BB, 0x0000, 0x001F, 0x002F, 0x005F, 0x0072, 0x0097, 0x00B8, 0x00CC, 0x80FA, 0x0110, 0x3572
};

static const uint16_t s_Stage5VoiceOffsetTbl[204] = {
    0x0000, 0x0008, 0x0043, 0x0067, 0x009C, 0x00A6, 0x00B4, 0x00C2, 0x00EC, 0x0112, 0x013B, 0x0157, 0x0162, 0x0174, 0x01A4, 0x01D3, 0x01F1, 0x01FC,
    0x8204, 0x020D, 0x0000, 0x0061, 0x00BF, 0x8161, 0x01C8, 0x0000, 0x0077, 0x00C9, 0x0137, 0x8183, 0x0227, 0x0000, 0x0029, 0x0061, 0x006D, 0x009E,
    0x0100, 0x0165, 0x01A0, 0x01C8, 0x01D3, 0x01E9, 0x81F3, 0x0217, 0x0000, 0x0014, 0x0025, 0x00DF, 0x010A, 0x0129, 0x0157, 0x0168, 0x017F, 0x819D,
    0x01F2, 0x0000, 0x0052, 0x0096, 0x00B0, 0x00BA, 0x00E4, 0x00ED, 0x0131, 0x014D, 0x0176, 0x018C, 0x01AC, 0x01C8, 0x01D6, 0x81ED, 0x0211, 0x0000,
    0x0022, 0x007B, 0x00E8, 0x00F5, 0x0106, 0x0117, 0x0127, 0x015D, 0x0193, 0x01B1, 0x81FB, 0x0230, 0x0000, 0x0022, 0x002E, 0x0052, 0x006B, 0x00C9,
    0x00F4, 0x00FF, 0x812A, 0x01AC, 0x0000, 0x0111, 0x0125, 0x0145, 0x0152, 0x0163, 0x0174, 0x0180, 0x0190, 0x01A6, 0x81F0, 0x0209, 0x0000, 0x0037,
    0x0050, 0x00B7, 0x00EC, 0x011A, 0x0198, 0x01B2, 0x01BB, 0x81E3, 0x0218, 0x0000, 0x007B, 0x0097, 0x00EB, 0x0100, 0x0164, 0x017F, 0x01A4, 0x01B6,
    0x01C0, 0x81CF, 0x023D, 0x0000, 0x0028, 0x0061, 0x0068, 0x00AD, 0x0101, 0x0125, 0x0141, 0x0173, 0x018E, 0x019D, 0x01B1, 0x01D9, 0x81E7, 0x01FC,
    0x0000, 0x0070, 0x00A2, 0x00D1, 0x015C, 0x0176, 0x01C5, 0x81D8, 0x0228, 0x0000, 0x001E, 0x0044, 0x0051, 0x007B, 0x008A, 0x00A0, 0x00DB, 0x00ED,
    0x00FB, 0x015B, 0x0186, 0x01AF, 0x01E0, 0x01E7, 0x81FD, 0x020E, 0x0000, 0x0022, 0x0047, 0x0081, 0x009A, 0x00C6, 0x00EF, 0x010C, 0x0123, 0x014B,
    0x0163, 0x017D, 0x018E, 0x81D7, 0x01EA, 0x0000, 0x005E, 0x006F, 0x00EB, 0x0110, 0x0149, 0x0169, 0x0197, 0x01AF, 0x01DE, 0x0205, 0x0263, 0x026B,
    0x0275, 0x02C3, 0x02C9, 0x82D3, 0x02F9, 0x0000
};

static const uint16_t* GetStageOffsetTable(uint8_t stage, size_t& outCount, std::string& outXasName) {
    switch (stage) {
    case 1:
    default:
        outCount = sizeof(s_Stage1VoiceOffsetTbl) / sizeof(s_Stage1VoiceOffsetTbl[0]);
        outXasName = "VOICE1.XAS";
        return s_Stage1VoiceOffsetTbl;
    case 2:
        outCount = sizeof(s_Stage2VoiceOffsetTbl) / sizeof(s_Stage2VoiceOffsetTbl[0]);
        outXasName = "VOICE2.XAS";
        return s_Stage2VoiceOffsetTbl;
    case 3:
        outCount = sizeof(s_Stage3VoiceOffsetTbl) / sizeof(s_Stage3VoiceOffsetTbl[0]);
        outXasName = "VOICE3.XAS";
        return s_Stage3VoiceOffsetTbl;
    case 4:
        outCount = sizeof(s_Stage4VoiceOffsetTbl) / sizeof(s_Stage4VoiceOffsetTbl[0]);
        outXasName = "VOICE4.XAS";
        return s_Stage4VoiceOffsetTbl;
    case 5:
        outCount = sizeof(s_Stage5VoiceOffsetTbl) / sizeof(s_Stage5VoiceOffsetTbl[0]);
        outXasName = "VOICE5.XAS";
        return s_Stage5VoiceOffsetTbl;
    case 6:
        outCount = sizeof(s_Stage1VoiceOffsetTbl) / sizeof(s_Stage1VoiceOffsetTbl[0]);
        outXasName = "VOICE1.XAS";
        return s_Stage1VoiceOffsetTbl;
    case 7:
        outCount = sizeof(s_Stage2VoiceOffsetTbl) / sizeof(s_Stage2VoiceOffsetTbl[0]);
        outXasName = "VOICE2.XAS";
        return s_Stage2VoiceOffsetTbl;
    }
}

XASDecoder::XASDecoder() {
    ResetState();
}

XASDecoder::~XASDecoder() {
    Close();
}

void XASDecoder::ResetState() {
    m_hist0 = 0;
    m_hist1 = 0;
    m_sectorBuffer.clear();
    m_sectorReadPos = 0;
    m_currentBlock = 0;
    m_endBlock = 0xFFFFFFFF;
}

std::string XASDecoder::FindXasFilePath(const std::string& xasFileName) {
    const std::string candidates[] = {
        "extracted/RE1-Dualshock/PSXUSA/VOICE/" + xasFileName,
        "../extracted/RE1-Dualshock/PSXUSA/VOICE/" + xasFileName,
        "../../extracted/RE1-Dualshock/PSXUSA/VOICE/" + xasFileName,
        "../../../extracted/RE1-Dualshock/PSXUSA/VOICE/" + xasFileName,
    };
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return xasFileName;
}

bool XASDecoder::ResolveStageVoiceClip(uint8_t stage, uint16_t clipId, XasClipInfo& outInfo) {
    size_t count = 0;
    std::string xasFileName;
    const uint16_t* ent = GetStageOffsetTable(stage, count, xasFileName);
    if (!ent || count < 2) return false;

    size_t idx = 0;
    uint8_t chan = 0;

    for (uint16_t n = 0; n < clipId; ++n) {
        if (idx + 1 >= count) return false;
        if (ent[idx] & 0x8000) {
            chan++;
            idx++;
        }
        idx++;
    }

    if (idx + 1 >= count || chan >= 16) return false;

    uint16_t sb = ent[idx] & 0x7FFF;
    uint16_t eb = ent[idx + 1] & 0x7FFF;
    if (eb < sb) return false;

    outInfo.stage = stage;
    outInfo.clipId = clipId;
    outInfo.channel = chan;
    outInfo.startBlock = sb;
    outInfo.endBlock = eb;
    outInfo.startSector = static_cast<uint32_t>(sb) * 16 + chan;
    outInfo.endSector = static_cast<uint32_t>(eb) * 16 + chan;
    outInfo.durationSeconds = static_cast<float>(eb - sb) * 4032.0f / 37800.0f;
    outInfo.xasFileName = xasFileName;

    return true;
}

uint16_t XASDecoder::GetStageClipCount(uint8_t stage) {
    size_t count = 0;
    std::string xasFileName;
    const uint16_t* ent = GetStageOffsetTable(stage, count, xasFileName);
    if (!ent) return 0;

    size_t idx = 0;
    uint8_t chan = 0;
    uint16_t clips = 0;

    while (chan < 16 && idx + 1 < count) {
        clips++;
        if (ent[idx] & 0x8000) {
            chan++;
            idx++;
        }
        idx++;
    }
    return clips;
}

void XASDecoder::DecodeMonoXaSector(const uint8_t* sec, int32_t& hist0, int32_t& hist1, std::vector<int16_t>& outPcm) {
    for (int g = 0; g < 18; ++g) {
        const uint8_t* grp = sec + 8 + g * 128;
        uint8_t unitsParam[8] = {
            grp[0], grp[1], grp[2], grp[3],
            grp[8], grp[9], grp[10], grp[11]
        };

        for (int u = 0; u < 8; ++u) {
            int filt = (unitsParam[u] >> 4) & 0x0F;
            int shift = unitsParam[u] & 0x0F;
            if (filt > 4) filt = 0;
            if (shift > 12) shift = 9;

            int bIdx = u / 2;
            bool isHi = (u & 1) != 0;

            for (int line = 0; line < 28; ++line) {
                uint8_t val = grp[16 + line * 4 + bIdx];
                int8_t nib = isHi ? static_cast<int8_t>((val >> 4) & 0x0F) : static_cast<int8_t>(val & 0x0F);
                if (nib >= 8) nib -= 16;

                int32_t sample = static_cast<int32_t>(nib) << (12 - shift);
                float pred = static_cast<float>(sample) + K0[filt] * static_cast<float>(hist0) + K1[filt] * static_cast<float>(hist1);
                int32_t valClamped = std::clamp(static_cast<int32_t>(std::round(pred)), -32768, 32767);

                hist1 = hist0;
                hist0 = valClamped;
                outPcm.push_back(static_cast<int16_t>(valClamped));
            }
        }
    }
}

bool XASDecoder::DecodeClipRange(const std::string& filePath, uint8_t channel, uint16_t startBlock, uint16_t endBlock, std::vector<int16_t>& outPcm) {
    std::string resolved = FindXasFilePath(filePath);
    std::ifstream file(resolved, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[XAS] Failed to open XAS file: " << resolved << std::endl;
        return false;
    }

    int32_t hist0 = 0, hist1 = 0;
    uint32_t blockCount = (endBlock > startBlock) ? (endBlock - startBlock) : 0;
    outPcm.reserve(outPcm.size() + blockCount * 4032);

    uint8_t sector[2336];
    for (uint16_t blk = startBlock; blk < endBlock; ++blk) {
        uint64_t secIdx = static_cast<uint64_t>(blk) * 16 + channel;
        file.seekg(secIdx * 2336, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(sector), sizeof(sector))) {
            break;
        }

        // Subheader: sector[0]=file, sector[1]=chan, sector[2]=submode (0x04 audio), sector[3]=coding
        if (sector[1] != channel || (sector[2] & 0x04) == 0) {
            continue;
        }

        DecodeMonoXaSector(sector, hist0, hist1, outPcm);
    }

    return !outPcm.empty();
}

bool XASDecoder::DecodeStageClip(uint8_t stage, uint16_t clipId, std::vector<int16_t>& outPcm, XasClipInfo* outInfo) {
    XasClipInfo info;
    if (!ResolveStageVoiceClip(stage, clipId, info)) {
        std::cerr << "[XAS] Invalid clip ID " << clipId << " for stage " << static_cast<int>(stage) << std::endl;
        return false;
    }

    if (outInfo) {
        *outInfo = info;
    }

    return DecodeClipRange(info.xasFileName, info.channel, info.startBlock, info.endBlock, outPcm);
}

bool XASDecoder::Open(const std::string& filePath, uint8_t channelIndex) {
    return OpenClip(filePath, channelIndex, 0, 0xFFFF);
}

bool XASDecoder::OpenClip(const std::string& filePath, uint8_t channelIndex, uint16_t startBlock, uint16_t endBlock) {
    Close();
    std::string resolved = FindXasFilePath(filePath);
    m_file.open(resolved, std::ios::binary);
    if (!m_file.is_open()) {
        std::cerr << "[XAS] Failed to open XAS file: " << resolved << std::endl;
        return false;
    }

    m_channel = channelIndex;
    m_currentBlock = startBlock;
    m_endBlock = endBlock;
    ResetState();

    return true;
}

uint32_t XASDecoder::Decode(int16_t* buffer, uint32_t bufferSize) {
    if (!m_file.is_open() || !buffer || bufferSize == 0) return 0;

    uint32_t samplesWritten = 0;

    while (samplesWritten < bufferSize) {
        // Drain pending decoded sector buffer
        if (m_sectorReadPos < m_sectorBuffer.size()) {
            uint32_t avail = static_cast<uint32_t>(m_sectorBuffer.size() - m_sectorReadPos);
            uint32_t toCopy = std::min(avail, bufferSize - samplesWritten);
            std::memcpy(buffer + samplesWritten, m_sectorBuffer.data() + m_sectorReadPos, toCopy * sizeof(int16_t));
            samplesWritten += toCopy;
            m_sectorReadPos += toCopy;
            if (samplesWritten >= bufferSize) break;
        }

        if (m_currentBlock >= m_endBlock) break;

        uint64_t secIdx = static_cast<uint64_t>(m_currentBlock) * 16 + m_channel;
        m_file.seekg(secIdx * 2336, std::ios::beg);
        uint8_t sector[2336];
        if (!m_file.read(reinterpret_cast<char*>(sector), sizeof(sector))) {
            break;
        }
        m_currentBlock++;

        if (sector[1] == m_channel && (sector[2] & 0x04) != 0) {
            m_sectorBuffer.clear();
            m_sectorReadPos = 0;
            DecodeMonoXaSector(sector, m_hist0, m_hist1, m_sectorBuffer);
        }
    }

    return samplesWritten;
}

void XASDecoder::Close() {
    if (m_file.is_open()) {
        m_file.close();
    }
    ResetState();
}
