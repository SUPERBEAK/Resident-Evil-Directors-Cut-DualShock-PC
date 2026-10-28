#include "psx_mc.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

PSXMemoryCard::PSXMemoryCard() 
    : m_status(CARD_NOT_FOUND), 
      m_saveDirectory("saves") {}

PSXMemoryCard::~PSXMemoryCard() {}

bool PSXMemoryCard::Initialize() {
    try {
        if (!fs::exists(m_saveDirectory)) {
            if (!fs::create_directory(m_saveDirectory)) {
                std::cerr << "[PSX_MC] Failed to create 'saves' directory." << std::endl;
                return false;
            }
        }
        
        // Simulate card detection. In a real environment, we might check 
        // for the existence of a specific physical card or a base file.
        m_status = CARD_CONNECTED;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[PSX_MC] Initialization error: " << e.what() << std::endl;
        return false;
    }
}

MemoryCardStatus PSXMemoryCard::GetStatus() const {
    return m_status;
}

bool PSXMemoryCard::SaveSlot(int cardIndex, int slotIndex, const uint8_t* data, size_t size) {
    std::string path = GetFilePath(cardIndex, slotIndex);
    std::ofstream outFile(path, std::ios::binary | std::ios::trunc);
    
    if (!outFile.is_open()) {
        std::cerr << "[PSX_MC] Failed to open " << path << " for writing." << std::endl;
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(data), size);
    outFile.close();
    
    return outFile.good();
}

bool PSXMemoryCard::LoadSlot(int cardIndex, int slotIndex, uint8_t* outData, size_t size) {
    std::string path = GetFilePath(cardIndex, slotIndex);
    std::ifstream inFile(path, std::ios::binary);
    
    if (!inFile.is_open()) {
        std::cerr << "[PSX_MC] Failed to open " << path << " for reading." << std::endl;
        return false;
    }

    inFile.read(reinterpret_cast<char*>(outData), size);
    inFile.close();
    
    return inFile.good();
}

std::string PSXMemoryCard::GetFilePath(int cardIndex, int slotIndex) const {
    // Format: saves/card_0_slot_0.mcr
    return m_saveDirectory + "/card_" + std::to_string(cardIndex) + "_slot_" + std::to_string(slotIndex) + ".mcr";
}
