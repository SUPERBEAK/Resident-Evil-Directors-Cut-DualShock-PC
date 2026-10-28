#include "psx_memory.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>

namespace PSX {

uint8_t* Memory::s_Ram = nullptr;
uint8_t* Memory::s_Scratchpad = nullptr;
uint8_t* Memory::s_IoDummy = nullptr;

bool Memory::Initialize() {
    if (s_Ram) return true;

    // Allocate host memory
    s_Ram = static_cast<uint8_t*>(VirtualAlloc(nullptr, RAM_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    s_Scratchpad = static_cast<uint8_t*>(VirtualAlloc(nullptr, SCRATCHPAD_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    s_IoDummy = static_cast<uint8_t*>(VirtualAlloc(nullptr, 64 * 1024, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    if (!s_Ram || !s_Scratchpad || !s_IoDummy) {
        std::fprintf(stderr, "[PSX::Memory] Failed to allocate virtual memory arena!\n");
        return false;
    }

    std::printf("[PSX::Memory] Arena initialized: RAM @ %p (2MB), Scratchpad @ %p (1KB)\n", s_Ram, s_Scratchpad);
    return true;
}

void Memory::Shutdown() {
    if (s_Ram) {
        VirtualFree(s_Ram, 0, MEM_RELEASE);
        s_Ram = nullptr;
    }
    if (s_Scratchpad) {
        VirtualFree(s_Scratchpad, 0, MEM_RELEASE);
        s_Scratchpad = nullptr;
    }
    if (s_IoDummy) {
        VirtualFree(s_IoDummy, 0, MEM_RELEASE);
        s_IoDummy = nullptr;
    }
}

void* Memory::Translate(uint32_t psxAddr) {
    // Scratchpad: 0x1F800000 - 0x1F8003FF
    if ((psxAddr & 0xFFFFF000) == 0x1F800000) {
        uint32_t offset = psxAddr & (SCRATCHPAD_SIZE - 1);
        return s_Scratchpad + offset;
    }

    // Hardware I/O dummy registers (0x1F801000 - 0x1F802FFF)
    if ((psxAddr & 0xFFFF0000) == 0x1F800000) {
        uint32_t offset = (psxAddr - 0x1F801000) & (64 * 1024 - 1);
        return s_IoDummy + offset;
    }

    // Strip MIPS KSEG bits:
    // 0x80000000..0x801FFFFF -> 0x00000000..0x001FFFFF
    // 0xA0000000..0xA01FFFFF -> 0x00000000..0x001FFFFF
    uint32_t pAddr = psxAddr & 0x1FFFFFFF;
    if (pAddr < RAM_SIZE) {
        return s_Ram + pAddr;
    }

    std::fprintf(stderr, "[PSX::Memory] Warning: Out of bounds PS1 address access: 0x%08X (phys: 0x%08X)\n", psxAddr, pAddr);
    return s_Ram; // fallback to avoid immediate crash
}

} // namespace PSX
