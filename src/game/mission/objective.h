#ifndef _OBJECTIVE_H_
#define _OBJECTIVE_H_
#include <ultra64.h>

void objectivesReset(void);

void tagAdd(struct TagObjectRecord *arg0);
void briefingTextAdd(struct setup_objective_text *arg0);
void objectiveAddRoomEntered(struct criteria_roomentered *arg0);
void objectiveAddDeposit(struct criteria_deposit *arg0);
void objectiveAddPicture(struct criteria_picture *arg0);
void objectiveAdd(struct objective_entry* objective);

#endif
