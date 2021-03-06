/*
 * GunTable.h
 *
 *  Created on: 22.05.2016
 *      Author: xasin
 */

#ifndef LOCALCODE_GAME_WEAPON_H_
#define LOCALCODE_GAME_WEAPON_H_

#include <avr/io.h>

namespace Game {
	namespace Weapon {
	extern uint16_t ammo, reloadTimer, shotTimer;

	extern void (*on_shot)();
	extern void (*on_reload)();

	uint8_t damage_from_signature(uint8_t hitSignature);

	uint16_t shot_delay();

	bool can_shoot();

	bool shoot();

	void update();
	}
}


#endif /* LOCALCODE_GAME_WEAPON_H_ */
