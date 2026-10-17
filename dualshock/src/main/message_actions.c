/* message_actions.c - The actions run after a message closes (g_MessageActionFuncs): take the item,
   use a room action item, remove an inventory item, none. */
#include "common.h"
#include "psyq_types.h"
#include "re1_types.h"
#include "globals.h"

/* Functions defined in the files before this one (tools/splitc.py) - what these
   functions saw when everything was one file. Functions defined later stay unseen. */
void rearrange_item_slots(void);
void room_event_item_pickup(void);


void room_event_take_item(void) {
    s32 room_event_item_pickup_a1() __asm__("room_event_item_pickup");
    s32 Flg_on();
    if (SCD_ZONE(ROOM_ACTION(g_pRoomActionEntry)->rec)->item_id == 0x4D) {
        Flg_on(ADDR_g_ScenarioFlags, 0x7F);
        return;
    }
    room_event_item_pickup_a1();
}

void use_room_action_item(void) {
    extern u8 * g_ItemSlotsPointer_u8p __asm__("g_ItemSlotsPointer");
    u8 i;

    g_UsedItemId = LIT(u8, g_SelectedItemId);
    if (LIT(u8, g_SelectedItemId) == 0xD) {
        return;
    }
    for (i = 0; LIT(u8, g_SelectedItemId) != g_ItemSlotsPointer_u8p[i * 2]; i++) {
    }
    if (LIT(u8, g_SelectedItemId) < 11) {
        g_ItemSlotsPointer_u8p[i * 2] = 0;
        if (LIT(u8, g_EquippedSlot) == i + 1) {
            LIT(u8, g_EquippedSlot) = 0;
        }
    } else {
        if (g_ItemSlotsPointer_u8p[i * 2 + 1] == 0) {
            return;
        }
        g_ItemSlotsPointer_u8p[i * 2 + 1]--;
        if (g_ItemSlotsPointer_u8p[i * 2 + 1] != 0) {
            return;
        }
        if ((u8)(LIT(u8, g_SelectedItemId) - 0x33) < 10) {
            LIT(s32, g_GameStateFlags) |= MENU_REQUEST(MENU_MODE_KEY_USED_UP);
            return;
        }
        g_ItemSlotsPointer_u8p[i * 2] = 0;
    }
    rearrange_item_slots();
}

typedef struct { u8 a, b; } ItemSlotPair;

void inventory_remove_item(void) {
    s32 rearrange_item_slots_a2(ItemSlotPair *, u8) __asm__("rearrange_item_slots");
    extern ItemSlotPair * g_ItemSlotsPointer_sp __asm__("g_ItemSlotsPointer");
    ItemSlotPair *p = g_ItemSlotsPointer_sp;
    s32 id = g_SelectedItemId;
    u8 i;

    for (i = 0; id != p[i].a; i++) {
    }
    g_ItemSlotsPointer_sp[i].a = 0;
    rearrange_item_slots_a2(p, id);
}

void message_action_none(void) {
}
