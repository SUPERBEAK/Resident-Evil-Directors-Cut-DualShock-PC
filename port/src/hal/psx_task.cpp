#include <cstdint>
#include <cstdio>

// Emulation of Capcom's cooperative task system (TaskSlot g_Tasks[3])
extern "C" {

void task_scheduler_init() {
    std::printf("[PSX::Task] Task scheduler initialized\n");
}

void task_sleep(int32_t ticks) {
    // In cooperative execution or fiber scheduler, this yields execution
}

} // extern "C"
