#ifndef _PLAYER_H_
#define _PLAYER_H_
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chr/chr.h"
#include "game/math/matrix.h"
#include "game/menu/watchmenu.h"
#include "game/gfx/texload.h"

typedef struct invitem_weap
{
	s32 weapon;
} invitem_weap;

typedef struct invitem_prop
{
  PropRecord *prop;
} invitem_prop;

typedef struct invitem_dual
{
	s32 weapon_right;
	s32 weapon_left;
} invitem_dual;

/**
 * Copied whole in playerDie.
 *
 * sizeof == 84 (0x54)
 */
struct PlayerBond {

    /**
     * Offset 0x00.
     */
    struct StandTile *current_tile_ptr;

    /**
     * Offset 0x04.
     */
    coord3d collision_position;

    /**
     * This affects Bond's movement, but not the viewport.
     * This does not affect boost direction.
     *
     * f[0]: forward component (sin theta) in radians
     * f[1]: zero
     * f[2]: sideways component (cos theta) in radians.
     * Offset 0x10.
     */
    coord3d theta_transform;

    /**
     * Some kind of alternative to pos3 (in player struct).
     * Offset 0x1c.
     */
    coord3d pos3;

    /**
     * Offset 0x28.
     */
    f32 collision_radius;

    /**
     * Some kind of alternative to pos (in player struct).
     * Offset 0x2c.
     */
    coord3d pos;

    /**
     * playerTickMoveAnim calculates 4x4 view matrix transform and then writes rotation
     * to this property and applied_view2. Removing the update to this property will fix
     * Bond's camera orientation such that Bond can still turn, but the camera is always
     * locked facing the starting position.
     * Offset 0x38.
     */
    coord3d applied_view;

    /**
     * Offset 0x44.
     */
    coord3d applied_view2;

    /**
     * Offset 0x50.
     */
    StandTile *current_tile_ptr_for_portals;
};

typedef struct
{
  u8 labels[9][3];
} DirectionLabels;

typedef struct TopMessageLocals
{
  s32 textwidth;
  s32 textheight;
  s32 bottom;
  s32 y;
  s32 x;
} TopMessageLocals;

typedef struct DebugTextBuffers
{
  char angle[0x10];
  char z[0x10];
  char y[0x10];
  char x[0x10];
  char room[0x10];
} DebugTextBuffers;

/**
 * first hand: 0x800c6fd0
*/
struct hand
{
  ITEM_IDS weaponnum;
  ITEM_IDS weaponnum_watchmenu;
  ITEM_IDS previous_weapon;

  s8 weapon_firing_status;

  s8 firing; // makes the multiplayer body's gun fire too
  s8 heldItemVisible; // hidden while a grenade or mine is being thrown
  s8 handVisible;

  /**
   * Time holding weapon
  */
  s32 weapon_hold_time;
  s32 triggerReleased;
  s32 stateFrames; // frames in weapon_action_state
  s32 stateTime; // time in weapon_action_state, in 60 Hz ticks
  s32 weapon_action_state;
  s32 weapon_current_animation;
  s32 weapon_ammo_in_magazine;
  s32 numShots; // EU: the moonraker laser overheats after 200
  s32 numvisibleshells;
  s32 recoilReturnTime;
  s32 weapon_next_weapon;
  s32 switchTime; // when a weapon change was asked for again mid-switch
  s32 weapon_animation_trigger;
  s32 cycleDirection;
  coord3d recoilFromPos; // recoil kick: the gun moves and tilts from "from" to "to"
  f32 recoilFromAngle;
  coord3d recoilToPos;
  f32 recoilToAngle;
  coord3d recoilPos;
  f32 recoilAngle;
  Mtxf animMtx; // recoil or throw keyframe transform of the gun
  s32 useAnimMtx;
  f32 sway_pos_x;
  f32 sway_pos_y;
  f32 sway_pos_z;
  f32 sway_look_x;
  f32 sway_look_y;
  f32 sway_look_z;
  f32 sway_up_x;
  f32 sway_up_y;
  f32 sway_up_z;
  f32 spring_pos_x;
  f32 spring_pos_y;
  f32 spring_pos_z;
  f32 spring_look_x;
  f32 spring_look_y;
  f32 spring_look_z;
  f32 spring_up_x;
  f32 spring_up_y;
  f32 spring_up_z;
  coord3d blendpos[4];
  coord3d blendlook[4];
  coord3d blendup[4];
  s32 curblendpos;
  f32 dampt;
  f32 blendscale1;
  s32 sideflag;
  f32 weapon_theta_displacement;
  f32 weapon_verta_displacement;
  f32 gunofs2_x;
  f32 gunofs2_y;
  f32 gunofs2_z;
  f32 dualOffset; // 0..2: slides the guns apart when dual wielding
  coord3d aimPos;
  ALSoundState * audioHandle;
  ALSoundState *audioHandle2;
  s32 nextSoundTime;
  s32 lastSoundTime;
  BeamRecord weapon_beam;
  f32 noise;
  f32 hingeAngle;
  f32 boltRecoil;
  s32 boltRecoiling;
  struct ObjectRecord *rocket;
  s32 firedrocket;
  Mtxf gunmtx_camspace;
  // offset 0xad8
  Mtxf gunWorldMtx;
  // offset 0xb18
  Mtxf prevGunWorldMtx;
  coord3d muzzlePos; // world position of the muzzle flash
  f32 muzzleDepth; // its distance in front of the camera
  ModelBase gunmodel; /* the first-person gun model */
  u32 gunmodelRwData[32]; /* its node rwdata */
  s32 volley;  // Number of bullets discharged in a row. For pistols, it's always 1 even if the fire button is held.
  coord3d item_related;
};

typedef struct InvItem {
    s32 type;

    union {
		struct invitem_weap type_weap;
		struct invitem_prop type_prop;
		struct invitem_dual type_dual;

	} type_inv_item;

    struct InvItem *next;
    struct InvItem *prev;

} InvItem;

typedef struct textoverride {

  /*0x00*/ u32 unk1;
	/*0x04*/ s32 objoffset;
	/*0x08*/ s32 weapon;
	/*0x0c*/ u32 titletext1; //1st title
	/*0x10*/ u32 titletext2;//2nd title
	/*0x14*/ u32 shorttext;
	/*0x18*/ u32 longtext; //longname
	/*0x1c*/ u32 pickuptext;
	/*0x20*/ struct textoverride *next;
	/*0x24*/ struct ObjectRecord *obj;
} textoverride;


typedef struct gunheld {
	s32 weapon1;
	s32 weapon2;
	s32 totaltime;
} gunheld;


struct player
{
  s32 cameramode; // canonical name

  coord3d pos; // canonical "memcampos" ?

  coord3d pos2;

  coord3d offset;

  coord3d pos3;

  StandTile *cameratile;

  coord3d current_model_pos;

  coord3d previous_model_pos;

  f32 sumGround; /* damped sum of manGround, to smooth the eye height over steps */
  f32 manGround; /* Y of Bond's feet, eased towards stanHeight when falling or climbing */

  f32 stanHeight; /* Y of the ground under Bond */

  coord3d deltaPos; /* movement this frame; only y is used, the fall speed (negative when falling) */
  f32 sumCrouch; /* damped sum of vertical_bounce_adjust */
  f32 crouchHeight; /* how far the view dips after landing (negative) */
  s32 crouchTime240; /* frames left holding the landing dip */

  /**
   * Collision / clipping related.
   * When Bond falls off a ladder or similar, will "overshoot" vertical default.
   * Current offset is stored in this field.
   *
   */
  f32 vertical_bounce_adjust;

  /**
   * Flag: 0, 1, 2
  */
  s32 crouchpos; // canonical name

  /**
   * Varies from 0.0f to -100.0f
   */
  f32 ducking_height_offset; // canonical "crouchoffset" ?
  f32 crouchSpeed; /* speed of the crouch transition */
  PropRecord* prop;
  s32 perimEnabled; /* other props collide with Bond */

  coord2d collision_bounds[4];

  s32 activatePressed; /* B was pressed this frame and not used to enter the tank */

  /**
   * bodyModel is the third person model - it's seen by the other players in MP, and
   * during intro/outros/death scenes in SP.
   */
  struct Model *bodyModel; //canonically bondsub
  
  s32 bonddead; // canonical name
  f32 bondhealth; // canonical name
  f32 bondarmour;
  f32 oldhealth; // canonical name
  f32 oldarmour; // canonical name
  f32 apparenthealth; // canonical name
  f32 apparentarmour; // canonical name

#if defined (VERSION_EU)

    /*
    * When a non-negative integer:
    * - hide ammo and aim sight
    * - hide any active speech text
    * - disable shoot and B press interact
    * Otherwise,
    * - undo the above.
    **/
    f32 damageshowtime;

    /**
     * When a non-negative integer:
     * - Show health and body armor overlay if Bond isn't dead.
     * Otherwise,
     * - undo the above.
     **/
    f32 healthshowtime;
#else
  s32 damageshowtime; // canonical name
  s32 healthshowtime; // canonical name
#endif

  /**
  * The field names from this point up to bondbreathing are canonical names
  * with the exceptions of autoaim_target_y and autoaim_target_x.
  */

  bool32 docentreupdown;

  /**
   * When the player is doing manual pitch inputs, this is set to true so
   * the automatic look ahead pitch adjust is interrupted.
   */
  bool32 prevupdown;

  /**
  * If the player is moving forward quickly, and has made a manual pitch adjustment, suppress
  * the automatic look ahead pitch adjusting until the player has slowed down.
  */
  bool32 movecentrerelease;

  /**
   * If enabled, test the tiles ahead of the player for the look ahead functionality.
   * This is always enabled.
   */
  bool32 lookaheadcentreenabled;

  /**
   * Look Ahead Setting in the watch menu.
   */
  bool32 automovecentreenabled;

  /**
   * Always false.
   */
  bool32 fastmovecentreenabled;

  /**
   * True when Look Ahead is enabled and the player is giving strong
   * forwards or backwards input.
   */
  bool32 automovecentre;

  /**
   * 0: crosshair shown on screen
   * 1: crosshair hidden
   * Only applies in level, when in control of bond.
  */
  s32 insightaimmode;
  s32 autoyaimenabled;
  f32 autoaimy;
  struct PropRecord *autoaim_target_y;
  s32 autoyaimtime60;
  s32 autoxaimenabled;
  f32 autoaimx;
  struct PropRecord *autoaim_target_x;
  s32 autoxaimtime60;

  f32 vv_theta;
  f32 speedtheta;

  /**
   * Computed value from vv_theta, used to calculate boost direction.
   **/
  f32 vv_costheta;

  /**
   * Computed value from vv_theta, used to calculate boost direction.
   **/
  f32 vv_sintheta;

  /**
   * Vertical look angle.
   **/
  f32 vv_verta;

  /**
   * Vertical look angle. Computed value from vv_verta, should always be between 0 and 360 degrees.
   **/
  f32 vv_verta360;

  f32 speedverta;

  /**
   * Computed value from vv_verta360, but otherwise unused?
   **/
  f32 vv_cosverta;

  f32 speedsideways;
  f32 speedstrafe;
  f32 speedforwards;

  /**
   * Normal run speed: 1.0
   * After 3 seconds of running: 1.35.
  */
  f32 speedboost;

  /**
   * How long Bond has been running
  */
  s32 speedmaxtime60;

  coord3d bondshotspeed;

  f32 bondfadetime60;

  f32 bondfadetimemax60;

  f32 bondfadefracold;

  f32 bondfadefracnew;

  f32 bondbreathing;

  /**
   * Current pause time.
   * Resets after every new pause.
  */
  s32 watch_pause_time;

  s32 timer_1C4;
  s32 watch_animation_state;

  /**
   * 1 = level is active
   * 0 = game is paused
   * Changes to zero only after fully entering watch.
  */
  s32 outside_watch_menu;
  s32 open_close_solo_watch_menu;
  /* where the watch is held when looking at it (constant: in front of Bond, facing up) */
  coord3d watchPos;
  coord3d watchLook;
  coord3d watchUp;
  /**
   * Pausing flag.
   * 0 = level is active
   * 1 = begin pause animation. Set as soon as moving arm begins, cleared when moving arm ends.
  */
  s32 pausing_flag;
  f32 pause_starting_angle;
  f32 pause_saved_verta; // camera pitch to restore after pause
  f32 pause_target_verta;
  f32 pause_transition_time; // elapsed time
  f32 pause_transition_duration; // max/duration

  /**
   * Pausing states.
   * 0 = Unpaused
   * 1 = Entering pause.
   * 2 = Leaving pause.
   * 3 = Paused.
   */
  s32 pause_state;
  s32 watchSfxPending; /* the watch-on sound still has to play */
  s32 step_in_view_watch_animation;
  f32 pause_animation_counter;

  /**
   * Looks to be same as pause_watch_related, but adjusted by scale factor.
   */
  f32 pause_watch_related_scaled;

  Model watchModel; /* the first-person watch and arm; its frame is the open/close animation time */
  u32 watchModelRwData[50];

  /**
   * Copy of buttons pressed.
  */
  u16 buttons_pressed;
  u16 prev_buttons_pressed;

  coord3d dampEyeSum; /* damped sum of bond.pos */
  coord3d dampEye; /* smoothed eye offset, eases the camera when moving */

  /**
   * Canonical names from here up through deathanimfinished.
   */
  s32 colourscreenred;
  s32 colourscreengreen;
  s32 colourscreenblue;
  f32 colourscreenfrac;
  f32 colourfadetime60;

  f32 colourfadetimemax60;
  s32 colourfaderedold;
  s32 colourfaderednew;
  s32 colourfadegreenold;
  s32 colourfadegreennew;
  s32 colourfadeblueold;
  s32 colourfadebluenew;
  f32 colourfadefracold;
  f32 colourfadefracnew;

  /*
  * Something with position, like previous x position.
  */
  coord3d bondprevpos;

  f32 thetadie;
  f32 vertadie;
  s32 bondtype;
  s32 startnewbonddie;

  /**
   * Multiplayer related.
   */
  s32 redbloodfinished;

  /**
   * Multiplayer related.
   */
  s32 deathanimfinished;
  s32 controldef; // canonical name

  struct PlayerBond bondDie; /* bond saved at death, restored for the death replay */
  struct PlayerBond bond; /* Bond's body: tile, position and look vectors */

  /**
   * Canonical names from here up to standcnt.
   */

  s32 resetheadpos; // bool
  s32 resetheadrot; // bool

  s32 headanim; // index into array of pointers. Pointers are for animations.

  // has NTSC<->PAL difference, but not (5/6)
  f32 headdamp;
  s32 headwalkingtime60;
  f32 headamplitude;
  f32 sideamplitude;
  coord3d headpos;
  coord3d headlook;

  coord3d headup;
  coord3d headpossum;

  // headlooksum[2] has NTSC->PAL conversion rate (5/6)
  coord3d headlooksum;

  // headupsum[1] has NTSC->PAL conversion rate (5/6)
  coord3d headupsum;
  coord3d headbodyoffset;
  f32 standheight; // old name stationary_ground_offset

  // f32[4] ?? or 2 x f32[2] ??
  coord3d standbodyoffset;
  f32 standfrac;

  coord3d standlook[2];

  coord3d standup[2];

  s32 standcnt;
  Model model; /* Bond's head bob animation */
  u32 modelRwData[31];

  Mtxf bondheadmatrices[4];

  Vp viewports[2];

  s16 viewx;

  s16 viewy;

  s16 viewleft;

  s16 viewtop;

  s32 handModelState[2]; /* 1 loaded, -1 load pending (counts down) */
  ITEM_IDS handModelItem[2];
  u8 *handModelBuffer[2];

  ModelFileHeader handModelHeader[2];

  struct texpool handModelTexPool[2];

  struct hand hands[2];
  f32 gunposamplitude; // canonical name

  // Whether the trigger was released this frame.
  s32 trigger_released;

  /**
   * Whether player is currently holding the trigger button.
   */
  s32 trigger_down;

  // Whether player was holding the trigger button on the previous frame.
  s32 prev_trigger_down;

  s32 z_trigger_timer;

  // Which hand will fire when the Z trigger is pressed.
  s32 current_trigger_hand;
  
  rgba_u8 tileColor;

  s32 resetshadecol; // canonical name

  /**
   * Crosshair or bullet angle related.
  */
  coord2d crosshair_angle;
  f32 crosshair_x_pos;
  f32 crosshair_y_pos;
  f32 guncrossdamp; // canonical name
  coord2d gunAimScreenPos; /* where the gun points on screen */
  f32 gun_azimuth_angle;
  f32 gun_azimuth_turning;
  f32 gunaimdamp; // canonical name
  s32 last_z_trigger_timer;
  s32 copiedgoldeneye; // canonical name
  s32 gunammooff; // canonical name
  f32 gunsync; // canonical name
  f32 syncchange; // canonical name
  f32 synccount; // canonical name
  s32 syncoffset; // canonical name
  f32 cycleSum; /* damped breathing, sways the gun */
  f32 gunAmpSum; /* damped gunposamplitude */
  f32 sniper_zoom;
  f32 camera_zoom;
  s32 curRoomIndex;

  /**
   * Canonical names from here up through c_recipscaley.
   */

  f32 c_screenwidth;

  f32 c_screenheight;

  f32 c_screenleft;

  f32 c_screentop;

  f32 c_perspfovy;

  f32 c_perspaspect;

  f32 c_halfwidth;

  f32 c_halfheight;

  f32 c_scalex;

  f32 c_scaley;

  f32 c_recipscalex;
  f32 c_recipscaley;
  Mtx *worldToViewMtx;
  Mtxf *worldToScreenMtxf;

  Mtxf* viewtoworldmtxf;
  Mtx* projmatrix;
  Mtxf* projmatrixf;
  Mtx *projViewMtx;
  LookAt *lookAt;
  Mtxf *prevViewToWorldMtxf;
  f32 c_scalelod60; // canonical name
  f32 c_scalelod; // canonical name
  f32 c_lodscalez; // canonical name

  bbox2d screensize;

  /**
   * Used during level.
   * 0x2: no crosshair
   * 0x0: cross hair shown on screen.
   *
   */
  s32 gunsightmode;
  s32 ammoheldarr[30]; // canonical name
  u8 *bloodImgCur;
  u8 *bloodImgNxt;
  u8 *bloodImgBufPtrArray[2];
  s32 bloodImgIdx;

  /**
   * Canonical names from here up through bondmesscnt.
   */

  f32 zoomintime;
  f32 zoomintimemax;
  f32 zoominfovy;
  f32 zoominfovyold;
  f32 zoominfovynew;
  f32 fovy;
  f32 aspect;
  s32 hudmessoff;
  s32 bondmesscnt;

  InvItem *ptr_inventory_first_in_cycle;
  InvItem *p_itemcur;
  s32 equipmaxitems; // canonical name
  s32 equipallguns;  // canonical name
  s32 equipcuritem;  // canonical name
  textoverride *textoverrides;
  gunheld gunheldarr[10]; // canonical name

  // -1 for inactive, 0 or greater for active.
  s32 magnetattracttime; // canonical name

  f32 swaytarget;
  f32 swayoffset0;
  f32 swayoffset2;
  f32 bodyStrafeTurn; /* multiplayer body: turn while strafing, -90 to 90 */
  s32 players_cur_animation;
  f32 bodyAnimSpeed; /* multiplayer body: speed of the current animation */

  /**
   * This buffers button presses.
   * L_TRIGGER and R_TRIGGER do not add entry by themselves.
   * Entry seems to be added only on other button presses.
   */
  u16 cheatInputBuffer[20]; // canonically "buthist"

  u8 cheatInputBufferIndex; // canonically "buthistindex"
  u8 cheatInputCount; // canonically "buthistlen"
  u8 cheatBondInvincible; // canonically "bondinvincible"
  struct damage_display_val armor_display_values[46]; // watch gauge vertices
  struct damage_display_val health_display_values[46];

  struct WatchRectangle watchTabsVtx[WATCH_NUMBER_SCREENS];

  /**
   * Watch static, not "static vertices".
  */
  struct WatchRectangle watchStaticVtx[1];
  Gfx watchArmorGdl[230];
  Gfx watchHealthGdl[230];
  Gfx watchTabsGdl[WATCH_NUMBER_SCREENS * 8];
  Gfx watchStaticGdl[4];

  s32 healthdamagetype; // canonical name

  /**
   * Related to player perspective.
   */
  f32 eyeheight;
  f32 lookDownDrop; /* lowers the eye when looking down; 15, falls to 0 when dead */

  s32 mpmenuon;           // 29C4 canonincal name
  s32 mpmenumode;         // 29C8 canonincal name
  s32 mpquitconfirm;      // 29CC canonincal name
  s32 mpjoywascentre;     // 29D0 canonincal name
  s32 damagetype;         // 29D4 canonincal name
  s32 deathcount;         // 29D8 canonincal name
  s32 num_suicides;       // 29DC
  s32 spawnPadSeed; /* counts up to pick the next multiplayer spawn pad */
  s32 last_kill_time[4];  // 29E4

  /**
   * Holds mission offset timer value.
   */
  s32 lifestarttime60; // canonical name

  s32 kills_this_life;    // 29F8 canonically "killsthislife"
  s32 autocrouchpos;
  s32 healthdisplaytime; // canonically "healthdisplaytime60"

  /**
   * Current tile pointer -> room.
   *
   */
  s16 registeredroom;
  f32 bodyAimPitch; /* multiplayer body: aim angles towards the crosshair target */
  f32 bodyAimYaw;
  char *firstAwardText;
  char *secondAwardText;
  coord3d bodyHandPos[2]; /* multiplayer body: where each gun is, beams start there */
  s32 bodyHandPosTick[2]; /* g_LvTickCount when bodyHandPos was last found */
  s32 cur_item_weapon_getname;
  f32 actual_health;
  f32 actual_armor;
  ITEM_IDS handModelNextItem[2]; /* gun model to load next, -1 for none */
  f32 speedgo;
  s32 handModelLocked[2];
  s32 cur_player_control_type_0;
  s32 cur_player_control_type_1;
  f32 cur_player_control_type_2;
  s32 neg_vspacing_for_control_type_entry;
  u32 has_set_control_type_data;
};

struct firing_anim_struct {
    struct weapon_firing_animation_table * pointer;
    u32 anim;
    f32 x;
    f32 y;
    f32 z;
};

struct struct_4 {
    int anonymous_0;
    float anonymous_1;
    float anonymous_2;
    float anonymous_3;
};

#ifdef BUGFIX_R0
typedef struct DamageType
{
    u32 fadeStartFrame; // a second fade; only its end frame is used
    u32 fadeFullFrame;
    s32 fadeEndFrame;
    f32 fadeMax;
    s32 flashStartFrame;
    u32 flashFullFrame;
    s32 flashEndFrame;
    f32 maxAlpha;
    u32 red;
    u32 green;
    u32 blue;
} DamageType;
#else
typedef struct DamageType
{
    f32 fadeStartFrame;
    f32 fadeFullFrame;
    f32 fadeEndFrame;
    f32 fadeMax;
    f32 flashStartFrame;
    f32 flashFullFrame;
    f32 flashEndFrame;
    f32 maxAlpha;
    u32 red;
    u32 green;
    u32 blue;
} DamageType;
#endif

typedef struct HealthDisplayDuration 
{
    s32 validStartFrame;          // When positive the health display does not show up at all.
    s32 updateToRealHealthFrame;  // Frame to switch from showing the health before taking damage, to showing the new health value.
    s32 hideHealthFrame;          // Frame to remove the health/armor gauge from the screen.
} HealthDisplayDuration;

/**
 * First person weapon animation keyframe. Used for special weapons like the Throwing Knife and Taser.
 */
typedef struct Weapon1PTransformKeyframe {
    u32 isFinalKey;
    coord3d pos; // local space (+right/-left, +up/-down, +backward/-forward)
    coord3d rot;
    f32 interpParam; // Not sure, maybe some kind of interpolation between frames.
    f32 duration;
} Weapon1PTransformKeyframe;

// placeholder while matching


struct player_data {
    /* 0x0 */
    s32 shot_count[7];

    /* 0x1c */
    int kill_count;

    /* 0x20 */
    int killed_gg_owner_count;

    /* 0x24 */
    int kill_counts[MAX_PLAYER_COUNT];

    /* 0x34 */
    int time_other_players_on_screen;

    /* 0x38 */
    float distance_traveled;

    /* 0x3c */
    int damage_to_backside;

    /* 0x40 */
    float body_armor_pickups;

    /* 0x44 */
    int min_time_between_kills;

    /* 0x48 */
    int max_time_between_kills;

    /* 0x4c */
    int longest_inning;

    /* 0x50 */
    s32 shortest_inning;

    /* 0x54 */
    int most_killed_one_life;

    /* 0x58 */
    int most_killed_one_time;

    /* 0x5c */
    float handicap;

    /* 0x60 */
    int flag_counter;

    /* 0x64 */
    float player_perspective_height;

    /* 0x68 */
    u8 order_out_in_yolt;

    /* 0x69 */
    u8 have_token_or_goldengun;

    /* 0x6a */
    u8 autoaim;

    /* 0x6b */
    u8 sight;

    /* 0x6c */
    int killed_civilians;
};

extern struct player *g_playerPointers[4];

// extern struct player_data player1_player_data;
// extern struct player_data player2_player_data;
// extern struct player_data player3_player_data;
// extern struct player_data player4_player_data;

extern struct player_data g_playerPlayerData[4];

extern struct player *g_CurrentPlayer;
extern struct player_data *g_playerPerm;
extern s32 player_num;
extern s32 random_byte;
extern PLAYER_ID array_PLAYER_IDs[4];


void playersReset(void);
void playersInit(int playercount);
void playersShuffleOrder(void);
s32 playerGetShuffled(PLAYER_ID id);
void playerSetCurrent(s32 playernum);
s32 getPlayerCount(void);
s32 getViewCount(void);
s32 coopSplitSideBySide(void);
s32 viewIsHalfWidth(void);
s32 isMpGame(void);
s32 coopPartnerAlive(void);
s32 coopPartnerCanRevive(void);
#define COUNTEROP_BOND  0
#define COUNTEROP_GUARD 1
void coopNoteActivator(void);
extern s32 g_CoopLastActivator;
extern s32 g_CoopMode;
extern s32 g_CounterOpMode;
extern s32 g_CounterOpBondPad;
s32 coopPlayerPerson(s32 player);
s32 counterOpIsGuardPlayer(s32 player);
extern s32 g_CoopPlayersOverlap;
extern s32 g_CoopSoloView;
extern s32 g_CoopSoloViewIsCinema;
void playerSetScreenSize(u32 width, u32 height);
void playerSetViewportPos(u32 ulx, u32 uly);


s32 playerGetCurrentNum(void);
#ifdef __GNUC__
/* Inlined at call sites; player.c still provides the real function */
extern inline __attribute__((gnu_inline)) s32 playerGetCurrentNum(void) { return player_num; }
#endif
void playerSetFovY(f32 fovy);
s32 playerGetIndexForProp( PropRecord* prop);
void playerSetAspect(f32 aspect);
void playerInit(s32 player_num);
s32 playerGetShuffledIndex(s32 current_player_num);
void playerBodyRemoveGun(enum GUNHAND hand);
void playerBodyGiveGun(enum GUNHAND hand);

void playerInitHands(void);
void playersStoreFavouriteWeapons(void);

void playersResetHandicaps(void);
PROP weaponGetChrModel(ITEM_IDS item);


// bondview2.c
extern coord3d g_TankModelPositionOffset;
extern s32 g_EnterTankAudioState;
extern f32 g_TankEnteringSitHeight;
extern f32 g_TankEnteringSitHeightRemain;
extern f32 g_TankEnterBondHorizAngleDeg;
extern f32 g_TankEnterBondVertAngleDeg;
extern coord3d g_EnterTankCoord;
extern s32 g_bondviewForceDisarm;
extern s32 resolution;
extern s32 g_VisibleToGuardsFlag;
extern f32 g_PlayerWorldScale;
extern s32 g_bondviewBondDeathAnimations[];
extern s32 g_bondviewBondDeathAnimationsCount;
extern coord3d g_ForceBondMoveOffset;
extern s32 g_CoopForceMovePlayer;
extern s32 mission_timer;
#if defined(VERSION_EU)
extern f32 watch_time_0;
#else
extern s32 watch_time_0;
#endif
extern ITEM_IDS starting_weapon[2];
PropRecord* currentPlayerGetProp(void);
void playerUpdate(s8 stick_x, s8 stick_y, u16 buttons);
s32 playerPressedActivate(void);
s32 lvGetMissionTime60(void);
void playerLoadBody(void);
f32 playerGetFloorY(struct StandTile *tile, f32 x, f32 z);
void playerUpdateCollisionBounds(void);
f32 playerGetEyeHeight(struct player *player);
void playerGetCollisionSize(PropRecord* prop, f32 *collision_radius, f32 *height, f32 *always_30);
void playerUpdateVertical(s32 use_stanHeight, f32 stanHeight_offset);
f32 playerGetStanHeight(struct player *player);
u8 playerGetRoom(void);
coord3d *playerGetPos(void);
void playerSetMoving(PropRecord *prop, s32 flag);
void playerGetPropBounds(PropRecord *prop, coord2d **field_B0, s32 *numpoints, f32 *height_related, f32 *collision);
s32 playerGetVisibleToGuards(void);
void playerSetVisibleToGuards(s32 visible);
f32 bondviewGetPlayerYawRadians(void);
Gfx *playerRenderProp(PropRecord *prop, Gfx *gdl, s32 withalpha);
f32 playerGetBreathing(void);
coord3d *playerGetPrevPos(void);
void playerSetWorldScale(f32);
void playerRemoveBody(void);
void playerSelectCuff(Model *model, ModelFileHeader *header, s32 switchindex);
coord3d * playerGetPos3(void);
s32 playerTick(PropRecord *prop);
extern struct firing_anim_struct firing_animation_groups[][6];
void playerMoveFrozen(u16, u16);
void playerMove(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons);
void playerUpdatePosFromCollision(void);
void playerUpdateVertaTheta(void);
void playerTickMoveAnim(f32 speed, f32 speedforwards, f32 speedsideways);

#include "game/hud/hudmsg.h"
#include "game/player/watch.h"
#include "game/player/respawn.h"
#include "game/player/counterop.h"
#include "game/player/playermove.h"
#include "game/player/tank.h"
#include "game/player/cammode.h"
#include "game/player/camview.h"
#include "game/player/playerdamage.h"
#include "game/player/playerfade.h"
#include "game/player/playerinput.h"
#include "game/player/playerlife.h"

#endif
