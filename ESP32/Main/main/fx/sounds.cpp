/*
 * sounds.cpp
 *
 *  Created on: 30 Mar 2019
 *      Author: xasin
 */


#include "sounds.h"

#include "sounds/game_start.h"

#include "sounds/kill_score.h"
#include "sounds/minor_score.h"

#include "sounds/own_death.h"
#include "sounds/own_hit.h"

#include "../core/setup.h"

using namespace Xasin::Peripheral;

namespace LZR {
namespace Sounds {

auto cassette_game_start	= AudioCassette(sound_game_start, sizeof(sound_game_start));

auto cassette_kill_scored 	= AudioCassette(sound_kill_score, sizeof(sound_kill_score));
auto cassette_minor_score	= AudioCassette(sound_minor_score, sizeof(sound_minor_score));

auto cassette_death 		= AudioCassette(sound_own_death, sizeof(sound_own_death));
auto cassette_hit			= AudioCassette(sound_own_hit, 	 sizeof(sound_own_hit));


void play_audio(std::string aName) {
	if(aName == "GAME START")
		audioManager.insert_cassette(cassette_game_start);
	else if(aName == "KILL SCORE")
		audioManager.insert_cassette(cassette_kill_scored);
	else if(aName == "MINOR SCORE")
		audioManager.insert_cassette(cassette_minor_score);
	else if(aName == "DEATH")
		audioManager.insert_cassette(cassette_death);
	else if(aName == "HIT")
		audioManager.insert_cassette(cassette_hit);
}

void init() {
	mqtt.subscribe_to(player.get_topic_base() + "/FX/Sound", [](Xasin::MQTT::MQTT_Packet data) {
		play_audio(data.data);
	});
}

}
}
