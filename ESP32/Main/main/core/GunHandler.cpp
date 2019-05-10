/*
 * GunHandler.cpp
 *
 *  Created on: 18 Jan 2019
 *      Author: xasin
 */

#include "GunHandler.h"

#include "setup.h"
#include "IR.h"

#include "../weapons/wyre.h"
#include "../weapons/mylin.h"
#include "../weapons/zinger.h"

#include "empty_click.h"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

namespace Lasertag {

const AudioCassette emptyClick(empty_click, sizeof(empty_click));

const GunSpecs defaultGun = {
	.maxAmmo = 50,
	.currentAmmo = 50,

	.postTriggerTicks   = 0,
	.postTriggerRelease = false,

	.shotsPerSalve = 1,

	.perShotDelay   = 70,

	.postShotCooldownTicks = 600,
	.postShotVibrationTicks = 35,

	.postSalveDelay = 200,
	.postSalveRelease = false,

	.postShotReloadBlock =   600,
	.postReloadReloadBlock = 60,
	.perReloadRecharge = 5,

	.perShotHeatup 	 = 0.007,
	.perTickCooldown = 0.9985,

	.chargeSounds	= CassetteCollection(),
	.shotSounds 	= CassetteCollection(),
	.cooldownSounds = CassetteCollection(),
};

GunHandler::GunHandler(gpio_num_t trgPin, AudioHandler &audio)
	:	mqttAmmo(0), lastMQTTPush(0),
		fireState(WAIT_ON_VALID),
		reloadState(FULL),
		shotTick(0), salveCounter(0), lastShotTick(0),
		emptyClickPlayed(false),
		reloadTick(0),
		lastTick(0),
		gunHeat(0),
		triggerPin(trgPin),
		shot_performed(false),
		audio(audio) {

	currentAmmo = cGun().maxAmmo;

	gpio_set_direction(triggerPin, GPIO_MODE_INPUT);
	gpio_set_pull_mode(triggerPin, GPIO_PULLUP_ONLY);
	gpio_pullup_en(triggerPin);
}

GunSpecs &GunHandler::cGun() {
	int gNum = LZR::player.get_gun_num();
	if(gNum == 0 || gNum > 3)
		return LZR::Weapons::wyre;

	GunSpecs * gunSets[] = {
			&LZR::Weapons::wyre,
			&LZR::Weapons::mylin,
			&LZR::Weapons::zinger,
	};

	return *gunSets[gNum-1];
}

bool GunHandler::triggerPressed() {
	return gpio_get_level(triggerPin) == 0;
}

void GunHandler::handle_shot() {
	currentAmmo--;
	gunHeat += cGun().perShotHeatup;

	fireState = POST_SHOT_DELAY;
	shotTick  = xTaskGetTickCount() + (cGun().perShotDelay*(95 + esp_random()%10))/100;
	lastShotTick = xTaskGetTickCount();

	reloadState = POST_SHOT_WAIT;
	reloadTick  = xTaskGetTickCount() + cGun().postShotReloadBlock;

	shot_performed = true;

	audio.insert_cassette(cGun().shotSounds);

	LZR::IR::send_signal();

	ESP_LOGD(GUN_TAG, "Fired, ammo : %3d", currentAmmo);
}

void GunHandler::shot_tick() {
	shot_performed = false;
	bool fireStateChanged = true;

	while(fireStateChanged) {
		fireStateChanged = false;

		switch(fireState) {
		case WAIT_ON_VALID:
			if(!triggerPressed())
				break;
			if(!LZR::player.can_shoot())
				break;
			if(currentAmmo < cGun().shotsPerSalve) {
				if(!emptyClickPlayed) {
					audio.insert_cassette(emptyClick);
					emptyClickPlayed = true;
				}

				break;
			}

			if(cGun().postTriggerTicks != 0) {
				audio.insert_cassette(cGun().chargeSounds);
			}

			shotTick  = xTaskGetTickCount() + cGun().postTriggerTicks;
			fireState = POST_TRIGGER_DELAY;

		case POST_TRIGGER_DELAY:
			if(xTaskGetTickCount() < shotTick)
				break;
			fireState = POST_TRIGGER_RELEASE;

		case POST_TRIGGER_RELEASE:
			if(!triggerPressed() || !cGun().postTriggerRelease) {
				salveCounter = cGun().shotsPerSalve;
				handle_shot();
			}
		break;

		case POST_SHOT_DELAY:
			if(xTaskGetTickCount() < shotTick)
				break;

			if(--salveCounter > 0) {
				handle_shot();
				break;
			}

			fireState = POST_SALVE_DELAY;
			shotTick = xTaskGetTickCount() + cGun().postSalveDelay;

		case POST_SALVE_DELAY:
			if(xTaskGetTickCount() < shotTick)
				break;

			fireState = POST_SALVE_RELEASE;

		case POST_SALVE_RELEASE:
			if(!triggerPressed() || !cGun().postSalveRelease) {
				fireState = WAIT_ON_VALID;
				fireStateChanged = true;

				if(!triggerPressed() && !cGun().postSalveRelease && !cGun().postTriggerRelease) {
					puts("Cooldown!");
					if(gunHeat > 0.2)
						audio.insert_cassette(cGun().cooldownSounds);
				}
			}
		break;
		}
	}

	if(!triggerPressed())
		emptyClickPlayed = false;
}

void GunHandler::reload_tick() {
	if(currentAmmo >= cGun().maxAmmo)
		return;

	if(reloadTick > xTaskGetTickCount())
		return;

	currentAmmo += cGun().perReloadRecharge;
	if(currentAmmo >= cGun().maxAmmo) {
		audio.insert_cassette(cGun().chargeSounds);
		currentAmmo = cGun().maxAmmo;
	}

	ESP_LOGD(GUN_TAG, "Reloaded, ammo: %3d", currentAmmo);

	reloadTick = xTaskGetTickCount() + cGun().postReloadReloadBlock;
}

void GunHandler::fx_tick() {
	gunHeat *= cGun().perTickCooldown;

//	TickType_t cooldownSoundTick = lastShotTick + cGun().postShotCooldownTicks;
//	if(xTaskGetTickCount() >= cooldownSoundTick && lastTick < cooldownSoundTick)
//		audio.insert_cassette(cGun().cooldownSounds);
}

bool GunHandler::was_shot_tick() {
	return shot_performed;
}
TickType_t GunHandler::timeSinceLastShot() {
	return xTaskGetTickCount() - lastShotTick;
}
uint8_t GunHandler::getGunHeat() {
	if(gunHeat > 1)
		return 255;
	if(gunHeat < 0)
		return 0;

	return gunHeat * (255-3) + 3;
}

void GunHandler::tick() {
	reload_tick();
	shot_tick();

	fx_tick();

	if((currentAmmo != mqttAmmo) && (xTaskGetTickCount() > (lastMQTTPush+300))) {
		struct {
			int32_t currentAmmo;
			int32_t maxAmmo;
		} ammoData = {currentAmmo, cGun().maxAmmo};
		LZR::mqtt.publish_to("Lasertag/Players/"+LZR::player.deviceID+"/Ammo", &ammoData, sizeof(ammoData), 0, true);

		mqttAmmo = currentAmmo;
		lastMQTTPush = xTaskGetTickCount();
	}

	lastTick = xTaskGetTickCount();
}

} /* namespace Lasertag */
