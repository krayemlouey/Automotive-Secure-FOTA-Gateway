#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <cstdint>

#include "stm32h7xx.h"

Model::Model() : modelListener(0)
{

}

void Model::tick()
{
    typedef struct {
        volatile uint32_t pot_raw;
        volatile uint32_t frame_count;
        volatile uint32_t status;
        volatile uint32_t last_tick;
        volatile uint32_t can_id;
        volatile uint32_t dlc;
        volatile uint8_t  payload[8];
        volatile uint32_t delta_t;
    } CAN_Shared_t;

    /* Invalidate D-cache so CM7 reads fresh SRAM4 directly from physical RAM */
    SCB_InvalidateDCache_by_Addr((uint32_t*)0x38000000, 64);
    __DMB();

    CAN_Shared_t* shared = (CAN_Shared_t*)0x38000000;

    if (modelListener != 0)
    {
        *((volatile uint32_t*)0x38000220) += 1;
        /* Appel 60 Hz fluide sans blocage ni latence */
        modelListener->updatePotData(shared->pot_raw, shared->frame_count, shared->can_id);
    }
}
