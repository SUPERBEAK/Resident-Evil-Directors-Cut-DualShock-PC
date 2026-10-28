#ifndef STAGE_MANAGER_H
#define STAGE_MANAGER_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>

struct StageOverlay {
    uint16_t stageId;
    std::string filename;
    std::vector<uint8_t> data;
    std::vector<uint8_t> rawData;
    bool isLoaded;
};

class StageManager {
public:
    StageManager();
    ~StageManager();

    bool Initialize(const std::string& baseDir);
    const StageOverlay* GetStage(uint16_t stageId) const;
    void Shutdown();
    std::string GetBaseDir() const { return m_baseDir; }

private:
    std::map<uint16_t, StageOverlay> m_loadedStages;
    std::string m_baseDir;
    bool LoadStage(uint16_t stageId);
};

#endif // STAGE_MANAGER_H
