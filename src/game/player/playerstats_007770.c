#include <ultra64.h>

void playersStoreFavouriteWeapons(void) {
    int right_hand;
    int left_hand;
    int player_count;
    int current_player;
    int player;

    player_count = getPlayerCount();
    current_player = playerGetCurrentNum();

    for (player = 0; player < player_count; player++) {
        playerSetCurrent(player);
        bondinvGetWeaponOfChoice(&right_hand, &left_hand);
        store_favorite_weapon_current_player(right_hand, left_hand);
    }

    playerSetCurrent(current_player);
}

