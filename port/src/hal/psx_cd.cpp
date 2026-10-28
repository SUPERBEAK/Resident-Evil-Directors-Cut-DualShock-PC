#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {

typedef struct {
    uint8_t file;
    uint8_t channel;
    uint8_t pad[2];
} CdlFILTER;

typedef struct {
    uint32_t size;
    uint32_t lba;
    char name[16];
} CdlFILE;

int32_t CdInit() {
    return 1;
}

int32_t CdReset(int32_t mode) {
    return 1;
}

int32_t CdStatus() {
    return 0; // ready
}

int32_t CdMode(int32_t mode) {
    return 1;
}

int32_t CdRead(int32_t sectors, uint32_t* buf, int32_t mode) {
    return 1;
}

int32_t CdReadSync(int32_t mode, uint8_t* result) {
    if (result) *result = 0;
    return 0; // completed
}

CdlFILE* CdSearchFile(CdlFILE* fp, const char* name) {
    std::printf("[PSX::CD] Searching file: %s\n", name);
    return fp;
}

} // extern "C"
