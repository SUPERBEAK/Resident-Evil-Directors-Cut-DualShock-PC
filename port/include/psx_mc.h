#ifndef PSX_MC_H
#define PSX_MC_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * PS1 Memory Card Status Flags
 */
enum MemoryCardStatus {
    CARD_NOT_FOUND = 0,
    CARD_CONNECTED = 1,
    CARD_FORMATTED = 2,
    CARD_ERROR = 3
};

/**
 * PS1 Memory Card System
 * Handles redirection of hardware memory card I/O to the local filesystem.
 */
class PSXMemoryCard {
public:
    PSXMemoryCard();
    ~PSXMemoryCard();

    /**
     * Initializes the memory card system.
     * Creates the local 'saves' directory if it doesn't exist.
     * @return true if initialization was successful.
     */
    bool Initialize();

    /**
     * Returns the current status of the memory card.
     */
    MemoryCardStatus GetStatus() const;

    /**
     * Saves a data block to a specific slot on the memory card.
     * @param cardIndex The index of the card (usually 0 for bu00:).
     * @param slotIndex The slot index.
     * @param data Pointer to the buffer to save.
     * @param size Size of the buffer in bytes.
     * @return true if save was successful.
     */
    bool SaveSlot(int cardIndex, int slotIndex, const uint8_t* data, size_t size);

    /**
     * Loads a data block from a specific slot on the memory card.
     * @param cardIndex The index of the card (usually 0 for bu00:).
     * @param slotIndex The slot index.
     * @param outData Buffer to store the loaded data.
     * @param size Size of the buffer.
     * @return true if load was successful.
     */
    bool LoadSlot(int cardIndex, int slotIndex, uint8_t* outData, size_t size);

    /**
     * Simulates the PS1's standard 128KB card layout.
     * 16 blocks of 8KB (128KB total).
     */
    const size_t BLOCK_SIZE = 8192;
    const size_t TOTAL_BLOCKS = 16;

private:
    MemoryCardStatus m_status;
    std::string m_saveDirectory;
    
    // Helper to generate a file path for a specific slot.
    std::string GetFilePath(int cardIndex, int slotIndex) const;
};

#endif // PSX_MC_H
