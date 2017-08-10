/*
 * main.cpp
 *
 *  Created on: May 9, 2016
 *      Author: xasin
 */


#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "Libcode/TIMER/Timer0.h"
#include "Localcode/Board/Board.h"

#include "Localcode/Connector.h"

#include "Localcode/Game/Player.h"

#include "Localcode/IRComs/IR_RX.h"
#include "Localcode/IRComs/IR_TX.h"

#include "Localcode/ESPComs/ESPUART.h"

volatile bool hasBeenShot = false;

uint8_t playerID = 0;

ESPComs::Endpoint PlayerIDEP(100, &playerID, 1, 0);

ESPComs::Endpoint ColorEP(101, &Board::Vest::team, 1, 0);
ESPComs::Endpoint VestBrightnessEP(200, &Board::Vest::mode, 1, 0);

struct BuzzCommand {
	uint8_t length;
	uint8_t startFreq;
	uint8_t endFreq;
};
void playPing() {
	BuzzCommand buzzCommand = *(BuzzCommand*)&ESPComs::Endpoint::pubBuffer;

	Board::Buzzer::sweep(buzzCommand.startFreq*60, buzzCommand.endFreq*60, buzzCommand.length*10);
}
ESPComs::Endpoint PingEndpoint(11, &ESPComs::Endpoint::pubBuffer, 3, playPing);

void playVibration() {
	Board::Vibrator::vibrate(ESPComs::Endpoint::pubBuffer[0]*10);
}
ESPComs::Endpoint VibrationEP(10, &ESPComs::Endpoint::pubBuffer, 1, playVibration);

void handleShots() {
	if(ESPComs::Endpoint::pubBuffer[0] == 99) {
		Board::Vibrator::vibrate(200);
		Board::Nozzle::flash(0b10 << Board::Vest::team);
		Board::Buzzer::sweep(3000, 1000, 150);
		IR::TX::startTX({playerID, 0});
	}
}
ESPComs::Endpoint ShootCommandEP(0, &ESPComs::Endpoint::pubBuffer, 1, handleShots);

void IRRXCB(IR::ShotPacket data) {
	if(data.playerID == playerID)
		return;
	hasBeenShot = true;
}

int main() {
	_delay_ms(1500);
	ESPComs::init();

	Board::Vest::mode = 3;

	Connector::init();

	IR::RX::setCallback(IRRXCB);

	Board::Nozzle::flash(1);

	while(true) {
		while(!hasBeenShot) {}
		Board::Vibrator::vibrate(1000);
		Board::Vest::mode = 5;
		_delay_ms(1000);
		Board::Vest::mode = 0;
		hasBeenShot = false;
	}
	return 0;
}
