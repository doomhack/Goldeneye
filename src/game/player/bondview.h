#ifndef _BOND_H_
#define _BOND_H_

#include <ultra64.h>
#include <bondtypes.h>
#include "game/chr/chr.h"
#include "game/math/matrix.h"
#include "game/menu/watchmenu.h"
#include "game/gfx/image.h"

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
 * We know this is a struct from the compiler auto-generated
 * code to copy structs in bondview.c playerDie.
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
    struct coord3d theta_transform;

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
    struct coord3d applied_view;

    /**
     * Offset 0x44.
     */
    struct coord3d applied_view2;

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
  s32 prevTrigger; // weapon_hold_time last frame
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
  f32 blendscale;
  f32 blendscale1;
  s32 sideflag;
  f32 weapon_theta_displacement;
  f32 weapon_verta_displacement;
  s32 field_A24;
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
  s32 gunmodel[8]; /* the first-person gun model: a Model without the animation part */
  s32 gunmodelRwData[32]; /* its node rwdata */
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

  /**
   * Offset 0x0004.
   */
  coord3d pos; // canonical "memcampos" ?

  /**
   * Offset 0x0010.
   */
  coord3d pos2;

  /**
   * Offset 0x001c.
   */
  coord3d offset;

  /**
   * Offset 0x0028.
   */
  coord3d pos3;

  /**
   * Offset 0x0034.
   */
  StandTile *cameratile;

  /**
   * Offset 0x0038.
   */
  coord3d current_model_pos;

  /**
   * Offset 0x0044.
   */
  coord3d previous_model_pos;

  /**
   * Offset 0x0050.
   */
  coord3d current_room_pos;
  f32 sumGround; /* damped sum of manGround, to smooth the eye height over steps */
  f32 manGround; /* Y of Bond's feet, eased towards stanHeight when falling or climbing */

  /* 0x0074 0x800c67d4 */ f32 stanHeight; /* Y of the ground under Bond */

  coord3d deltaPos; /* movement this frame; only y is used, the fall speed (negative when falling) */
  f32 sumCrouch; /* damped sum of vertical_bounce_adjust */
  f32 crouchHeight; /* how far the view dips after landing (negative) */
  s32 crouchTime240; /* frames left holding the landing dip */

  /**
   * Collision / clipping related.
   * When Bond falls off a ladder or similar, will "overshoot" vertical default.
   * Current offset is stored in this field.
   *
   * Offset 0x0090.
   */
  /* 0x0090 */ f32 vertical_bounce_adjust;

  /**
   * Flag: 0, 1, 2
  */
  /* 0x009c */ s32 crouchpos; // canonical name

  /**
   * Varies from 0.0f to -100.0f
   * /
  /* 0x00a0 */ f32 ducking_height_offset; // canonical "crouchoffset" ?
  f32 crouchSpeed; /* speed of the crouch transition */
  /* 0x00a8 */ PropRecord* prop;
  /* 0x00ac */ s32 perimEnabled; /* other props collide with Bond */

  /**
   * Offset 0x00b0.
   */
  struct rect4f collision_bounds;

  /* 0x00d0 */ s32 activatePressed; /* B was pressed this frame and not used to enter the tank */

  /**
   * bodyModel is the third person model - it's seen by the other players in MP, and
   * during intro/outros/death scenes in SP.
   */
  /* 0x00d4 */ struct Model *bodyModel; //canonically bondsub
  
  /* 0x00d8 */ s32 bonddead; // canonical name
  /* 0x00dc */ f32 bondhealth; // canonical name
  /* 0x00e0 */ f32 bondarmour;
  /* 0x00e4 */ f32 oldhealth; // canonical name
  /* 0x00e8 */ f32 oldarmour; // canonical name
  /* 0x00ec */ f32 apparenthealth; // canonical name
  /* 0x00f0 */ f32 apparentarmour; // canonical name

#if defined (VERSION_EU)

    /*
    * When a non-negative integer:
    * - hide ammo and aim sight
    * - hide any active speech text
    * - disable shoot and B press interact
    * Otherwise,
    * - undo the above.
    * 0x00f4
    **/
    f32 damageshowtime;

    /**
     * When a non-negative integer:
     * - Show health and body armor overlay if Bond isn't dead.
     * Otherwise,
     * - undo the above.
     * 0x00f8
     **/
    f32 healthshowtime;
#else
  /* See comments above. 0x00f4 */ s32 damageshowtime; // canonical name
  /* See comments above. 0x00f8 */ s32 healthshowtime; // canonical name
#endif


  /* 0x00fc */ s32 healthshowmode; // canonical name
  /* 0x0100 */ s32 field_100; // unused

  /**
  * The field names from this point up to bondbreathing are canonical names
  * with the exceptions of autoaim_target_y and autoaim_target_x.
  */

  /* 0x0104 */ bool docentreupdown;

  /**
   * Assigned to 0 but never read.
   * Probably "lastupdown60" based on this list: https://gist.github.com/kholdfuzion/ec713f2c0a36fbfbd4f71568073f47bc
   * 0x0108
   */
  s32 lastupdown60;

  /**
   * When the player is doing manual pitch inputs, this is set to true so
   * the automatic look ahead pitch adjust is interrupted.
   * 0x010c
   */
  bool prevupdown;

  /**
  * If the player is moving forward quickly, and has made a manual pitch adjustment, suppress
  * the automatic look ahead pitch adjusting until the player has slowed down.
  * 0x0110
  */
  bool movecentrerelease;

  /**
   * If enabled, test the tiles ahead of the player for the look ahead functionality.
   * This is always enabled.
   * 0x0114
   */
  bool lookaheadcentreenabled;

  /**
   * Look Ahead Setting in the watch menu.
   * 0x0118
   */
  bool automovecentreenabled;

  /**
   * Always false.
   * 0x011c
   */
  bool fastmovecentreenabled;

  /**
   * True when Look Ahead is enabled and the player is giving strong
   * forwards or backwards input.
   * 0x0120
   */
  bool automovecentre;

  /**
   * 0: crosshair shown on screen
   * 1: crosshair hidden
   * Only applies in level, when in control of bond.
  */
  /* 0x0124 */ s32 insightaimmode;
  /* 0x0128 */ s32 autoyaimenabled;
  /* 0x012c */ f32 autoaimy;
  /* 0x0130 */ struct PropRecord *autoaim_target_y;
  /* 0x0134 */ s32 autoyaimtime60;
  /* 0x0138 */ s32 autoxaimenabled;
  /* 0x013c */ f32 autoaimx;
  /* 0x0140 */ struct PropRecord *autoaim_target_x;
  /* 0x0144 */ s32 autoxaimtime60;

  /* 0x0148 */ f32 vv_theta;
  /* 0x014c */ f32 speedtheta;

  /**
   * Computed value from vv_theta, used to calculate boost direction.
   * 0x0150
   **/
  f32 vv_costheta;

  /**
   * Computed value from vv_theta, used to calculate boost direction.
   * 0x0154
   **/
  f32 vv_sintheta;

  /**
   * Vertical look angle.
   * 0x0158
   **/
  f32 vv_verta;

  /**
   * Vertical look angle. Computed value from vv_verta, should always be between 0 and 360 degrees.
   * 0x015c
   **/
  f32 vv_verta360;

  /* 0x0160 */ f32 speedverta;

  /**
   * Computed value from vv_verta360, but otherwise unused?
   * 0x0164
   **/
  f32 vv_cosverta;

  /**
   * Computed value from vv_verta360, but otherwise unused?
   * 0x0168
   **/
  f32 vv_sinverta;

  /* 0x016c */ f32 speedsideways;
  /* 0x0170 */ f32 speedstrafe;
  /* 0x0174 */ f32 speedforwards;

  /**
   * Normal run speed: 1.0
   * After 3 seconds of running: 1.35.
   * Address 0x80c68d8
  */
  /* 0x0178 */ f32 speedboost;

  /**
   * How long Bond has been running
  */
  /* 0x017c */ s32 speedmaxtime60;

  coord3d bondshotspeed; //0x180

  // offset 0x18c
  f32 bondfadetime60;

  // offset 0x190
  f32 bondfadetimemax60;

  // offset 0x194
  f32 bondfadefracold;

  // offset 0x198
  f32 bondfadefracnew;

  // offset 0x19c
  f32 bondbreathing;

  s32 field_1A4;
  s32 field_1A8;
  s32 field_1AC;
  s32 field_1B0;
  s32 field_1B4;
  s32 field_1B8;
  s32 field_1BC;

  /**
   * Current pause time.
   * Resets after every new pause.
  */
  s32 watch_pause_time;

  /* 0x01c4 */ s32 timer_1C4;
  /* 0x01c8 */ s32 watch_animation_state;

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
  s32 field_1F8;
  s32 field_1FC;
  /**
   * Pausing flag.
   * 0 = level is active
   * 1 = begin pause animation. Set as soon as moving arm begins, cleared when moving arm ends.
   * 0x800C6960
  */
  s32 pausing_flag; // 0x200
  f32 pause_starting_angle; // 0x204
  f32 pause_saved_verta; // 0x208, camera pitch to restore after pause
  f32 pause_target_verta; // 0x20c
  f32 pause_transition_time; // 0x210, elapsed time
  f32 pause_transition_duration; // 0x214, max/duration

  /**
   * Pausing states.
   * 0 = Unpaused
   * 1 = Entering pause.
   * 2 = Leaving pause.
   * 3 = Paused.
   */
  s32 pause_state; // 0x218
  s32 watchSfxPending; /* the watch-on sound still has to play */
  s32 step_in_view_watch_animation;
  f32 pause_animation_counter;

  /**
   * Offset 0x0228.
   */
  f32 pause_watch_related;

  /**
   * Looks to be same as pause_watch_related, but adjusted by scale factor.
   * Offset 0x022c.
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

  /**
   * Offset 0x3e4.
   */
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
  * Offset 0x408.
  */
  coord3d bondprevpos;

  f32 thetadie; //0x414
  f32 vertadie; //0x418
  s32 bondtype; //0x41c
  s32 startnewbonddie; //0x420

  /**
   * Multiplayer related.
   * Offset 0x424.
   */
  s32 redbloodfinished;

  /**
   * Multiplayer related.
   * Offset 0x428.
   */
  s32 deathanimfinished;
  s32 controldef; //0x430 canonical name

  struct PlayerBond bondDie; /* bond saved at death, restored for the death replay */
  struct PlayerBond bond; /* Bond's body: tile, position and look vectors */

  /**
   * Canonical names from here up to standcnt.
   */

  s32 resetheadpos; // bool
  s32 resetheadrot; // bool
  s32 resetheadtick; // bool 0x4e4

  s32 headanim; // index into array of pointers. Pointers are for animations.

  // has NTSC<->PAL difference, but not (5/6)
  f32 headdamp;
  s32 headwalkingtime60; //0x4f0
  f32 headamplitude; //0x4f4
  f32 sideamplitude; //0x4f8
  coord3d headpos;
  vec3d headlook;

  vec3d headup;
  coord3d headpossum;

  // headlooksum[2] has NTSC->PAL conversion rate (5/6)
  coord3d headlooksum;

  // headupsum[1] has NTSC->PAL conversion rate (5/6)
  vec3d headupsum;
  coord3d headbodyoffset; //0x544
  f32 standheight; // old name stationary_ground_offset

  // f32[4] ?? or 2 x f32[2] ??
  coord3d standbodyoffset; //0x554
  f32 standfrac; //0x560

  // offset 0x564
  vec3d standlook[2];

  // offset 0x57C
  vec3d standup[2];

  // offset 0x594
  s32 standcnt;
  Model model; /* Bond's head bob animation */
  u32 modelRwData[31];

  Mtxf bondheadmatrices[4];

  Vp viewports[2];

  /**
   * Offset 0x7f0.
   */
  s16 viewx;

  /**
   * Offset 0x7f2.
   */
  s16 viewy;

  /**
   * Offset 0x7f4.
   */
  s16 viewleft;

  /**
   * Offset 0x7f6.
   */
  s16 viewtop;

  s32 handModelState[2]; /* 1 loaded, -1 load pending (counts down) */
  ITEM_IDS handModelItem[2];
  u8 *handModelBuffer[2];

  /**
   * Offset 0x810.
   */
  ModelFileHeader handModelHeader[2];

  struct texpool handModelTexPool[2];

  /**
   * Offset 0x870.
   */
  struct hand hands[2];
  f32 gunposamplitude; // canonical name
  f32 gunxamplitude; // canonical name

  // Whether the trigger was released this frame.
  s32 trigger_released;

  /**
   * Whether player is currently holding the trigger button.
   * Offset 0xfcc.
   */
  s32 trigger_down;

  // Whether player was holding the trigger button on the previous frame.
  s32 prev_trigger_down;

  s32 z_trigger_timer;

  // Which hand will fire when the Z trigger is pressed.
  s32 current_trigger_hand;
  
  struct rgba_u8 tileColor;

  s32 resetshadecol; // canonical name

  // unused. Name comes from XBLA debug.
  s32 aimtype;

  /**
   * Crosshair or bullet angle related.
   * Offset 0xfe8.
  */
  coord2d crosshair_angle;
  f32 crosshair_x_pos;
  f32 crosshair_y_pos;
  f32 guncrossdamp; // canonical name
  coord2d gunAimScreenPos; /* where the gun points on screen */
  f32 gun_azimuth_angle;
  f32 gun_azimuth_turning;
  f32 gunaimdamp; // canonical name
  Mtxf field_101C;
  s32 last_z_trigger_timer;
  s32 copiedgoldeneye; // canonical name
  s32 gunammooff; // canonical name
  s32 field_1068;
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

  /**
   * Offset 0x1090.
   */
  f32 c_screenwidth;

  /**
   * Offset 0x1094.
   */
  f32 c_screenheight;

  /**
   * Offset 0x1098.
   */
  f32 c_screenleft;

  /**
   * Offset 0x109c.
   */
  f32 c_screentop;

  /**
   * Offset 0x10a0.
   */
  f32 c_perspnear;

  /**
   * Offset 0x10a4.
   */
  f32 c_perspfovy;

  /**
   * Offset 0x10a8.
   */
  f32 c_perspaspect;

  /**
   * Offset 0x10ac.
   */
  f32 c_halfwidth;

  /**
   * Offset 0x10b0.
   */
  f32 c_halfheight;

  /**
   * Offset 0x10b4.
   */
  f32 c_scalex;

  /**
   * Offset 0x10b8.
   */
  f32 c_scaley;

  /**
   * Offset 0x10bc.
   */
  f32 c_recipscalex;
  f32 c_recipscaley;

  Mtx *worldToViewScaledMtx;
  Mtx *worldToViewMtx;
  Mtxf *worldToScreenMtxf;

  /**
   * Offset 0x10d4.
   */
  Mtxf* viewtoworldmtxf;
  Mtx* projmatrix;
  Mtxf* projmatrixf;
  Mtx *projViewMtx;
  LookAt *lookAt;
  Mtxf *prevWorldToScreenMtxf;
  Mtxf *prevViewToWorldMtxf;
  f32 c_scalelod60; // canonical name
  f32 c_scalelod; // canonical name
  f32 c_lodscalez; // canonical name
  u32 c_lodscalezu32; // canonical name
  coord3d c_cameratopnorm; // canonical name
  coord3d c_cameraleftnorm; // canonical name

  /**
   * Offset 0x1118.
   */
  struct bbox2d screensize;

  /**
   * Used during level.
   * 0x2: no crosshair
   * 0x0: cross hair shown on screen.
   *
   * Offset 0x1128.
   */
  s32 gunsightmode;
  s32 field_112C;
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
   * Offset 0x128c.
   */
  u16 cheatInputBuffer[20]; // canonically "buthist"

  /**
   * Offset 0x12b4.
   */
  /* 0x12B4 */ u8 cheatInputBufferIndex; // canonically "buthistindex"
  /* 0x12B5 */ u8 cheatInputCount; // canonically "buthistlen"
  /* 0x12B6 */ u8 cheatBondInvincible; // canonically "bondinvincible"
  /* 0x12B7 */ u8 field_12B7;
  /* 0x12B8 */ struct damage_display_parent armor_display_values[23];
  /* 0x1598 */ struct damage_display_parent health_display_values[23];

  /**
   * Offset 0x1878
  */
  struct WatchRectangle watchTabsVtx[WATCH_NUMBER_SCREENS];

  /**
   * Watch static, not "static vertices".
   * Offset 0x19b8.
  */
  struct WatchRectangle watchStaticVtx[1];
  Gfx watchArmorGdl[230];
  Gfx watchHealthGdl[230];
  Gfx watchTabsGdl[WATCH_NUMBER_SCREENS * 8];
  Gfx watchStaticGdl[4];

  s32 healthdamagetype; // canonical name

  /**
   * Related to player perspective.
   * Offset 0x29bc.
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
   * Offset 0x29f4.
   */
  s32 lifestarttime60; // canonical name

  s32 kills_this_life;    // 29F8 canonically "killsthislife"
  s32 autocrouchpos;
  s32 healthdisplaytime; // canonically "healthdisplaytime60"

  /**
   * Current tile pointer -> room.
   *
   * Offset 0x2a04.
   */
  s16 registeredroom;
  f32 bodyAimPitch; /* multiplayer body: aim angles towards the crosshair target */
  f32 bodyAimYaw;
  s32 ptr_text_first_mp_award;
  s32 ptr_text_second_mp_award;
  coord3d bodyHandPos[2]; /* multiplayer body: where each gun is, beams start there */
  s32 bodyHandPosTick[2]; /* g_LvTickCount when bodyHandPos was last found */
  s32 cur_item_weapon_getname;
  /* 0x2a3c */ f32 actual_health;
  /* 0x2a40 */ f32 actual_armor;
  ITEM_IDS handModelNextItem[2]; /* gun model to load next, -1 for none */
  f32 speedgo;
  s32 handModelLocked[2];
  s32 cur_player_control_type_0;
  s32 cur_player_control_type_1;             //0x2a5c
  f32 cur_player_control_type_2;             //0x2a60
  s32 neg_vspacing_for_control_type_entry;   //0x2a64
  u32 has_set_control_type_data;             //0x2a68

  s32 field_2A74;
  s32 field_2A78;
  s32 field_2A7C;
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
struct move_bond_temp_struct {
    s32 unk00;
    s32 unk04;
};

extern s32 g_bondviewForceDisarm;
extern s32 resolution;
extern s32 cameraBufferToggle;
extern s32 cameraFrameCounter1;
extern s32 cameraFrameCounter2;
extern s32 g_CreditsScrollFrame;
extern s32 credits_state;
extern CreditsEntry *credits_pointer;
extern s32 g_SurroundBondWithExplosionsFlag;
extern s32 g_PlayerIsInTank;
extern struct PropRecord *g_WorldTankProp;

extern struct PropRecord *g_PlayerTankProp;

/**
 * Related to g_PlayerTankProp.
 */
extern f32 g_PlayerTankYOffset;

extern ALSoundState * g_TankSfxState[2];

extern f32 g_TankTurnSpeed;
extern f32 g_TankOrientationAngle;
extern f32 tank_turret_unused_angle;
extern f32 g_TankTurretVerticalAngle;
extern f32 g_TankTurretVerticalAngleRelated;
extern f32 g_TankTurretOrientationAngleRad;
extern f32 g_TankTurretOrientationAngleDeg;
extern f32 tank_turret_turn_speed;
extern s32 g_BondCanEnterTank;
extern f32 g_TankTurretAngle;
extern f32 g_TankTurretTurn;
extern s32 g_ExplodeTankOnDeathFlag;
extern s32 g_TankDamagePenaltyTicks;
extern enum CAMERAMODE g_CameraMode;
extern enum CAMERAMODE g_CameraAfterCinema;
extern s32 camera_fade_active;
extern s32 stop_time_flag;
extern f32 camera_transition_timer;
extern s32 intro_camera_index;
extern struct SetupIntroSwirl *g_IntroSwirl;
extern s32 is_timer_active;
/**
 * Used to make the player invincible upon level completion.
 * This is separate from the invincibility cheat, stored in cheatBondInvincible.
 */
extern bool g_PlayerInvincible;
extern struct SetupIntroCamera* g_CurrentSetupIntroCamera;
extern s32 g_SetupIntroCameraCount;
extern struct SetupIntroCamera *g_IntroCamera;
extern s32 g_VisibleToGuardsFlag;
extern s32 g_PlayerCollisionOn;
extern f32 g_CamPosScale;
extern f32 g_PlayerWorldScale;
extern f32 g_PlayerWorldScaleInv;
extern s32 g_bondviewBondDeathAnimations[];

extern s32 g_bondviewBondDeathAnimationsCount;
extern enum CAMERAMODE camera_mode;
extern s32 g_IntroAnimationIndex;
/*
D:80036518     stage_intro_anim_table:struct_4 <0x5744, 95.0, -1.0, 0.02>
D:80036528                     struct_4 <0x6254, 7.0, 40.0, 0.5>
D:80036538                     struct_4 <0x78C8, 0.0, -1.0, 0.5>
D:80036548                     struct_4 <0x7AA8, 0.0, -1.0, 0.5>
D:80036558                     struct_4 <0x7C4C, 0.0, -1.0, 0.5>
D:80036568                     struct_4 <0x7D04, 0.0, -1.0, 0.5>
D:80036578                     struct_4 <0x7F0C, 0.0, -1.0, 0.5>
D:80036588                     struct_4 <0x7FB4, 0.0, -1.0, 0.5>
D:80036598                     struct_4 <0xD89C, 0.0, -1.0, 0.5>
D:800365A8     flt_watch_transition_time: .float 0.90909088
D:800365AC     dummy_08_pp7_obj:.word 0x1000008
D:800365B0                     .word 0xBF4000
D:800365B4                     .word 0, 0, 0, 0
D:800365C4                     .byte 0x3F
D:800365C5                     .byte 0x80, 0, 0
D:800365C8                     .word 0, 0, 0, 0
D:800365D8                     .byte 0x3F
D:800365D9                     .byte 0x80, 0, 0
D:800365DC                     .word 0, 0, 0, 0
D:800365EC                     .byte 0x3F
D:800365ED                     .byte 0x80, 0, 0
D:800365F0                     .align 5
D:80036600                     .byte 0x3F
D:80036601                     .byte 0x80, 0, 0
D:80036604                     .align 5
D:80036620                     .byte 0x44
D:80036621                     .byte 0x7A, 0, 0
D:80036624                     .word 0xFFFFFF00, 0xFFFFFF00, 0x4FFFFFF
*/
extern struct DamageType g_DamageTypes[];
/*
D:80036638                     .byte 0
D:80036639                     .byte 0, 0, 0xA
D:8003663C                     .word 0x3C, 0x3F19999A, 0
D:80036648                     .byte 0
D:80036649                     .byte 0, 0, 5
D:8003664C                     .word 0x28, 0x3F800000, 0xFF, 0xFF, 0xFF, 0
D:80036664                     .byte 0
D:80036665                     .byte 0, 0, 0xA
D:80036668                     .word 0x3C, 0x3F19999A, 0
D:80036674                     .byte 0
D:80036675                     .byte 0, 0, 5
D:80036678                     .word 0x28, 0x3F800000, 0xFF, 0xFF, 0xFF, 0
D:80036690                     .byte 0
D:80036691                     .byte 0, 0, 0xA
D:80036694                     .word 0x32, 0x3F19999A, 0
D:800366A0                     .word 5, 0x1E, 0x3F4CCCCD, 0xFF, 0xFF, 0xFF, 0
D:800366BC                     .byte 0
D:800366BD                     .byte 0, 0, 0xA
D:800366C0                     .word 0x28, 0x3F19999A, 0
D:800366CC                     .byte 0
D:800366CD                     .byte 0, 0, 5
D:800366D0                     .word 0x19, 0x3F19999A, 0xFF, 0xFF, 0xFF, 0
D:800366E8                     .byte 0
D:800366E9                     .byte 0, 0, 0xA
D:800366EC                     .word 0x23, 0x3F19999A, 0
D:800366F8                     .byte 0
D:800366F9                     .byte 0, 0, 5
D:800366FC                     .word 0x16, 0x3F0CCCCD, 0xFF, 0xFF, 0xFF, 0
D:80036714                     .byte 0
D:80036715                     .byte 0, 0, 0xA
D:80036718                     .word 0x1E, 0x3F19999A, 0
D:80036724                     .word 5, 0x13, 0x3F000000, 0xFF, 0xFF, 0xFF, 0
D:80036740                     .byte 0
D:80036741                     .byte 0, 0, 0xA
D:80036744                     .word 0x1E, 0x3F19999A, 0
D:80036750                     .word 5, 0x11, 0x3EE66666, 0xFF, 0xFF, 0xFF, 0
D:8003676C                     .byte 0
D:8003676D                     .byte 0, 0, 0xA
D:80036770                     .word 0x1E, 0x3F19999A, 0
D:8003677C                     .word 5, 0xF, 0x3ECCCCCD, 0xFF, 0xFF, 0xFF
D:80036794     g_HealthDisplayDurations:.word 0
D:80036798                     .byte 0
D:80036799                     .byte 0, 0, 0x28
D:8003679C                     .word 0x64, 0
D:800367A4                     .byte 0
D:800367A5                     .byte 0, 0, 0x1E
D:800367A8                     .word 0x50, 0
D:800367B0                     .byte 0
D:800367B1                     .byte 0, 0, 0x14
D:800367B4                     .word 0x3C, 0
D:800367BC                     .byte 0
D:800367BD                     .byte 0, 0, 0x14
D:800367C0                     .word 0x3C, 0
D:800367C8                     .byte 0
D:800367C9                     .byte 0, 0, 0x14
D:800367CC                     .word 0x3C, 0
D:800367D4                     .byte 0
D:800367D5                     .byte 0, 0, 0x14
D:800367D8                     .word 0x32, 0
D:800367E0                     .word 0x14, 0x32, 0
D:800367EC                     .byte 0
D:800367ED                     .byte 0, 0, 0x14
D:800367F0                     .word 0x32
*/

extern struct coord3d g_DefaultFrozenPlayerPos;
extern struct coord3d g_DefaultFrozenPlayerPos2;
extern struct coord3d g_DefaultFrozenPlayerOffset;
extern struct coord3d g_DefaultFrozenMoveOffset;

extern coord3d ZeroCoordWatchPos;
extern coord3d ZeroCoordSpawnPos;
extern s32 status_bar_text_buffer_index;
extern s32 display_statusbar;
extern s32 copy_1stfonttable;
extern s32 copy_2ndfonttable;
extern s32 upper_text_buffer_index;
extern s32 display_upper_text_window;
extern s32 upper_text_window_timer;
extern s32 g_UpperTextDisplayFlag;
/*
D:800368D8     firing_animation_groups:firing_anim_struct <pistol_firing_animation_group1, 0, 0.1, 79.0, 87.0>
D:800368D8                                              # DATA XREF: sub_CODE_7F08B0F0+720o
D:800368EC                     firing_anim_struct <stru_D_800306F0, 0, 0.5, 0.0, -1.0>
D:80036900                     firing_anim_struct <stru_D_80030738, 0, 0.5, 0.0, -1.0>
D:80036914                     firing_anim_struct <stru_D_800308A0, 0, 0.5, 0.0, -1.0>
D:80036928                     firing_anim_struct <stru_D_800308E8, 0, 0.5, 0.0, -1.0>
D:8003693C                     firing_anim_struct <crouched_pistol_firing_animation_group1, 0, 0.1, 56.0, 68.0>
D:80036950                     firing_anim_struct <rifle_firing_animation_group1, 0, 0.050000001, 35.0, 40.0>
D:80036964                     firing_anim_struct <stru_D_80030660, 0, 0.5, 0.0, -1.0>
D:80036978                     firing_anim_struct <stru_D_800306A8, 0, 0.5, 0.0, -1.0>
D:8003698C                     firing_anim_struct <stru_D_800308A0, 0, 0.5, 0.0, -1.0>
D:800369A0                     firing_anim_struct <stru_D_800308E8, 0, 0.5, 0.0, -1.0>
D:800369B4                     firing_anim_struct <stru_D_8002F7A8, 0, 0.1, 45.0, 55.0>
D:800369C8                     firing_anim_struct <0, 0x8194, 0.25, 0.0, -1.0>
D:800369DC                     firing_anim_struct <0, 0x8204, 0.5, 0.0, -1.0>
D:800369F0                     firing_anim_struct <0, 0x777C, 0.5, 0.0, -1.0>
D:80036A04                     firing_anim_struct <stru_D_800308A0, 0, 0.5, 0.0, -1.0>
D:80036A18                     firing_anim_struct <stru_D_800308E8, 0, 0.5, 0.0, -1.0>
D:80036A2C                     firing_anim_struct <0, 0x6C18, 0.050000001, 28.0, 29.0>
D:80036A40                     firing_anim_struct <doubles_firing_animation_group1, 0, 0.1, 32.0, 42.0>
D:80036A54                     firing_anim_struct <stru_D_80030780, 0, 0.5, 0.0, -1.0>
D:80036A68                     firing_anim_struct <stru_D_800307C8, 0, 0.5, 0.0, -1.0>
D:80036A7C                     firing_anim_struct <stru_D_800308A0, 0, 0.5, 0.0, -1.0>
D:80036A90                     firing_anim_struct <stru_D_800308A0, 0, 0.5, 0.0, -1.0>
D:80036AA4                     firing_anim_struct <crouched_doubles_firing_animation_group1, 0, 0.1, 37.0, 47.0>
*/

extern s32 startpadcount;
extern vec3d g_ForceBondMoveOffset;
extern s32 g_CoopForceMovePlayer;
extern s32 mission_timer;

#if defined(VERSION_EU)
extern f32 watch_time_0;
#else
extern s32 watch_time_0;
#endif

extern f32 watch_transition_time;
// ITEM_IDS
extern ITEM_IDS starting_weapon[2];
extern PadRecord *g_Startpad[];
extern s32 startpadcount;



PropRecord* currentPlayerGetProp(void);

f32 currentPlayerGetHealth(void);
f32 currentPlayerGetArmor(void);

bool currentPlayerGetIsAiming(void);

void camSetScreenSize(f32 width, f32 height);
void camUpdateScale(void);
void camSetScreenPosition(f32 left, f32 top);
void camSetPerspective(f32 near, f32 fovy, f32 aspect);

f32 camGetScreenWidth(void);
f32 camGetScreenHeight(void);
f32 camGetScreenLeft(void);
f32 camGetScreenTop(void);
f32 camGetPerspAspect(void);
void currentPlayerSetXAutoAimEnabled(bool enabled);
bool currentPlayerGetXAutoAimEnabled(void);

bool currentPlayerGetXAutoAimEnabledRedirect(void);
void currentPlayerSetYAutoAimEnabled(s32 enabled);
bool currentPlayerGetYAutoAimEnabled(void);
bool currentPlayerGetYAutoAimEnabledRedirect(void);
void currentPlayerSetLookAheadSetting(bool enabled);

u32 playerGetCameraMode(void);

void watchZoomTo(f32 zoominfovy);

void watchStartZoom(f32 final, f32 time);

void watchRequestToggle(void);

void playerResetLife(void);
void playerStartNewLife(void);


void playerUpdate(s8 arg0, s8 arg1, u16 arg2);

#if defined(BUGFIX_R1)
#define HUDMESSAGEBOTTOM hudmsgBottomShowText
void hudmsgBottomShow(char *string, s32 arg1, s32 arg2);
void hudmsgBottomShowText(char *string);
#else
// VERSION_US
#define HUDMESSAGEBOTTOM hudmsgBottomShow
void hudmsgBottomShow(char *string);
void hudmsgSetFonts(s32 arg0, s32 arg1);
#endif

Gfx * camSetupFrame(Gfx *arg0);
s32 playerPressedActivate(void);
Gfx * hudRender(Gfx *arg0);
bool camIsPosInScreen(coord3d *vec_scale, f32 norm_scale);
bool camIsPosInScreenBox(coord3d *, f32, struct bbox2d *);
s32 lvGetMissionTime60(void);
void playerLoadBody(void);
void playerUpdateAutoAimY(struct PropRecord *autoaim_target, f32 auto_aim_y);
void playerUpdateAutoAimX(struct PropRecord *autoaim_target, f32 auto_aim_x);
void tankGetTurretDir(coord3d *arg0);
f32 playerGetFloorY(struct StandTile *arg0, f32 arg1, f32 arg2);
void playerUpdateCollisionBounds(void);
void coopFindPartnerSpawn(struct coord3d *pos, StandTile **stan, f32 look_angle);
void coopRespawnPlayer(void);
void counterOpResetForStage(void);
f32 counterOpGetGuardHealth(void);
s32 counterOpGetGuardModels(s32 *head, s32 *body);
s32 counterOpTakeOverGuard(void);
void counterOpTick(void);
s32 counterOpSuicidePillTick(s32 triggerOn);
s32 counterOpGetBodiesUsed(void);
f32 counterOpGetDamageToBond(void);
void counterOpNoteDamageToBond(f32 percent);
f32 counterOpPlayerDamage(s32 shooter, f32 damage, s32 weaponid);
extern s32 g_CoopTankPlayer;
s32 coopTankUsedByPartner(void);
void coopTankBeginPlayer(void);
void coopTankEndPlayer(s32 moved);
extern s32 g_CoopCinemaPlayer;
void coopUpdateSoloView(void);
f32 playerGetEyeHeight(struct player *player);
void playerGetCollisionSize(PropRecord* arg0, f32 *collision_radius, f32 *height, f32 *always_30);
void playerUpdateVertical(s32 use_stanHeight, f32 stanHeight_offset);
void currentPlayerSetFadeColour(s32 r, s32 g, s32 b, f32 frac);
void currentPlayerSetFadeFrac(f32 maxfadetime, f32 frac);
f32 playerGetStanHeight(struct player *player);
void playerTakeDamage(f32, f32, f32, s32, s32);
void playerTakeDamageFromAngle(f32 arg0, f32 rad, s32 arg2, s32 arg3);
int playerIsShowingDamage(void);
int playerIsShowingHealth(void);
u8 playerGetRoom(void);
coord3d *playerGetPos(void);
void playerSetMoving(PropRecord *prop, s32 flag);
void playerGetPropBounds(PropRecord *arg0, struct rect4f **field_B0, s32 *arg2, f32 *height_related, f32 *collision);
void currentPlayerAddArmor(f32 arg0);
void hudmsgResetIntroText(void);
void hudmsgsSetOn(s32 flag);
void hudmsgsSetOff(s32 flags);
Gfx* camLoadWorldToViewMtx(Gfx* gdl);
Mtxf *camGetViewToWorldMtxf(void);
void camMtxsToRoomSpace(Mtxf *arg0, s32 arg1);
void currentPlayerStartChrFade(f32 duration60, f32 targetfrac);
s32 playerGetVisibleToGuards(void);
u8 playerGetInvincible(void);
void playerSetInvincible(u32 arg0);
void playerSetVisibleToGuards(s32 param_1);
Mtxf *camGetPrevViewToWorldMtxf(void);
void coopViewsBegin(void);
void coopViewsEnd(s32 lastplayer);
f32 bondviewGetPlayerYawRadians(void);
Mtxf *camGetWorldToScreenMtxf(void);
void camScreenToViewDir(struct coord2d *in, coord3d *out, f32 value);
void mtxsF2L(RenderPosView *arg0, s32 arg1);
LookAt *camGetLookAt(void);
Mtx *camGetProjViewMtx(void);
void camSetProjViewMtx(Mtx *matrix);
void camSetLookAt(LookAt *lookat);
void camSetWorldToViewMtx(Mtx *matrix);
void camSetWorldToViewScaledMtx(Mtx *matrix);
Mtx *camGetWorldToViewMtx(void);
Mtx *camGetProjectionMtx(void);
Gfx *playerRenderProp(PropRecord *arg0, Gfx *arg1, s32 arg2);
f32 camGetLodScaleZ(void);
f32 playerGetBreathing(void);
void     hudmsgTopClearFlag(int param_1);

void     hudmsgTopSetFlag(PLAYERFLAG flag);
void     playerSetCameraMode(s32 arg0);
bool     playerIsInTank(void);
void     hudmsgTopShow(char* string);
void     playerSurroundWithExplosions(int delay);
s32 watchIsZooming(void);
f32 watchGetOpenFrac(void);
struct coord3d *playerGetPrevPos(void);
struct PropRecord *playerGetTank(void);
s32 playerChooseSpawnPad(void);
void playerResetCollision(struct PlayerBond* arg0, struct coord3d *arg1, struct StandTile *arg2);
void playerSetWorldScale(f32);
void hudmsgTopReset(void);
Mtxf *camGetProjectionMtxf(void);
void camViewToScreen(coord3d *in, coord2d *out);
void playerRemoveBody(void);
void currentPlayerAdjustFade(f32 maxfadetime, s32 r, s32 g, s32 b, f32 frac);
void playerSelectCuff(Model *model, ModelFileHeader *header, s32 switchindex);
void camSetPosScale(f32 param_1);

#endif
