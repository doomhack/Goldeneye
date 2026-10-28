#ifndef _ANIM_H_
#define _ANIM_H_
#include <ultra64.h>
#include <assets/animationtable_data.h>
#include <bondtypes.h>



/**
 * Struct to hold animation data. This is never instantiated.
 * Instead, only a pointer to this will exist.
 */
struct animation_table_data {
    /**
     * Array length is arbitrary and shouldn't matter. The largest offset
     * into this is for the last animation pointer 0xE7C0, so just choosing
     * a value bigger than that, like u16_max_value.
    */
    u8 data[0xffff];
};

/**
 * Data holder for animations.
 */
extern struct animation_table_data* g_AnimTable;

/**
 * Contains offsets into g_AnimTable for player and guard animations.
 * The index of each value corresponds to `enum ANIMATION`.
 * The value corresponds to (e.g. index=0) PTR_ANIM_idle (same as ANIM_DATA_idle)
*/
extern ModelAnimation *animation_table_ptrs1[];

/**
 * An animation pointer that still holds its offset into g_AnimTable. Boot code adds
 * the table's address to turn it into a real pointer (animResolveTable, initResolveAnimTable).
 */
#define ANIM_OFFSET(offset) ((ModelAnimation *)(offset))

/** An empty slot in animation_table_ptrs1 (the PTR_ANIM_null* entries, which are 1). */
#define ANIM_UNUSED ((ModelAnimation *)1)

/**
 * Contains offsets into g_AnimTable for object/vehicle animations.
 * The index of each value corresponds to `enum AIRCRAFT_ANIMATION`.
 * The value corresponds to (e.g. index=0) PTR_ANIM_helicopter_cradle (same as ANIM_DATA_helicopter_cradle)
*/
extern ModelAnimation *animation_table_ptrs2[];

/** Where animLoadFrame copies animation frames to. */
struct bondstruct_unk_animation_related {
    char* uselessPointer; // Is incremented like a count when an animation is copied from ROM to RAM but it's never read
    char* animBufferPtr1; 
    char* animBufferPtr2; 
};

extern struct bondstruct_unk_animation_related *g_AnimFrameBuffer;

void animInitFrameBuffer(struct bondstruct_unk_animation_related *animBuffer);
void animResolveTable(ModelAnimation **table);
void animLoadTable(void);

#endif
