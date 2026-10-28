#include "stage_manager.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

StageManager::StageManager() : m_baseDir("") {}

StageManager::~StageManager() {
    Shutdown();
}

bool StageManager::Initialize(const std::string& baseDir) {
    m_baseDir = baseDir;
    for (uint16_t i = 1; i <= 7; ++i) {
        if (!LoadStage(i)) {
            std::cerr << "[StageManager] Failed to load stage " << i << std::endl;
            return false;
        }
    }
    return true;
}

const StageOverlay* StageManager::GetStage(uint16_t stageId) const {
    auto it = m_loadedStages.find(stageId);
    if (it != m_loadedStages.end()) {
        return &it->second;
    }
    return nullptr;
}

bool StageManager::LoadStage(uint16_t stageId) {
    // Construct path: baseDir/STAGEx
    std::string folderName = "STAGE" + std::to_string(stageId);
    fs::path folderPath = fs::path(m_baseDir) / folderName;

    if (!fs::exists(folderPath)) {
        std::cerr << "[StageManager] Folder not found: " << folderPath << std::endl;
        return false;
    }

    // In Resident Evil, the STAGEx folder contains the binary data.
    // We need to find the main overlay file. 
    // Based on our research, we'll look for the first non-directory file or a specific naming convention.
    // Since I don't have the exact file names yet, I'll look for any file that isn't a directory.
    
    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            StageOverlay overlay;
            overlay.stageId = stageId;
            
            std::ifstream file(entry.path(), std::ios::binary);
            if (file.is_open()) {
                // Read the entire file into the rawData vector
                file.seekg(0, std::ios::end);
                size_t size = file.tellg();
                file.seekg(0, std::ios::beg);
                
                overlay.rawData.resize(size);
                file.read(reinterpret_cast<char*>(overlay.rawData.data()), size);
                file.close();
                
                m_loadedStages[stageId] = std::move(overlay);
                return true;
            }
        }
    }

    return false;
}

void StageManager::Shutdown() {
    m_loadedStages.clear();
}
