#pragma once
#include <cstdint>
#include <cstddef>
#include <cassert>

// PS1 Physical and Virtual memory map:
// 0x00000000 - 0x001FFFFF : 2MB Main RAM (mirrored at KUSEG, KSEG0 0x80000000, KSEG1 0xA0000000)
// 0x1F800000 - 0x1F8003FF : 1KB Scratchpad (fast D-cache)
// 0x1F801000 - 0x1F802FFF : Hardware I/O registers

namespace PSX {

class Memory {
public:
    static constexpr uint32_t RAM_SIZE = 2 * 1024 * 1024; // 2MB
    static constexpr uint32_t SCRATCHPAD_SIZE = 1024;      // 1KB

    static bool Initialize();
    static void Shutdown();

    // Map any PS1 virtual address (0x80xxxxxx, 0xA0xxxxxx, or 0x1F80xxxx) to host pointer
    static void* Translate(uint32_t psxAddr);

    template<typename T>
    static T& Ref(uint32_t psxAddr) {
        return *reinterpret_cast<T*>(Translate(psxAddr));
    }

    template<typename T>
    static T* Ptr(uint32_t psxAddr) {
        return reinterpret_cast<T*>(Translate(psxAddr));
    }

    static uint8_t* GetRamBase() { return s_Ram; }
    static uint8_t* GetScratchpadBase() { return s_Scratchpad; }

private:
    static uint8_t* s_Ram;
    static uint8_t* s_Scratchpad;
    static uint8_t* s_IoDummy;
};

} // namespace PSX

// Macros for port compatibility with original decomp forms:
#define PSX_REF(T, addr)  (PSX::Memory::Ref<T>(addr))
#define PSX_PTR(T, addr)  (PSX::Memory::Ptr<T>(addr))
