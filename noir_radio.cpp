#include "noir_radio.hpp"

#include "nrf_radio.h"

#include "noir_irq.hpp"

namespace noir {
namespace radio {

void configure(int channel, void *packet_buffer) { // 0x1a
	int packet_maxlen = 0x46;

	// cycle power to clear registers
	NRF_RADIO->POWER = 0;
	NRF_RADIO->POWER = 1;

	__disable_irq();
	NRF_RADIO->TASKS_DISABLE = 1;
	while(NRF_RADIO->EVENTS_DISABLED == 0) { }
	NRF_RADIO->EVENTS_DISABLED = 0;
	while(NRF_RADIO->STATE != RADIO_STATE_STATE_Disabled) {}
	__enable_irq();
	
	NRF_RADIO->MODECNF0 = 1; // fast ramp up

	NRF_RADIO->PCNF0 = 0x100108; // LFLEN: 8 bits, S0LEN: 1 byte, S1LEN: 0, S1INCL: always, PLEN: 8-bit preamble
	NRF_RADIO->PCNF1 = packet_maxlen | 0x1040000; // STATLEN: 0, 4 byte base address, big endian, no whitening

	NRF_RADIO->PREFIX0 = 0xf0;
	NRF_RADIO->BASE0 = 0xdfb2124b;
	NRF_RADIO->BASE1 = 0xdfb2124b;
	NRF_RADIO->RXADDRESSES = 0xff;
	
	NRF_RADIO->FREQUENCY = channel;
	NRF_RADIO->MODE = (RADIO_MODE_MODE_Nrf_2Mbit << RADIO_MODE_MODE_Pos); // not sure why this doesn't exist in sdk

	NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_0dBm << RADIO_TXPOWER_TXPOWER_Pos);
	NRF_RADIO->DACNF = 0; // no device address matching
	NRF_RADIO->SHORTS =
		RADIO_SHORTS_ADDRESS_RSSISTART_Msk |
		RADIO_SHORTS_DISABLED_RSSISTOP_Msk;
	NRF_RADIO->CRCINIT = 0xffffff;
	NRF_RADIO->CRCCNF = 3;
	NRF_RADIO->CRCPOLY = 0x1108421;
	
	//NRF_RADIO->INTENSET = 0x1a; // ADDRESS, END, DISABLED
	NRF_RADIO->INTENSET =
		(RADIO_INTENSET_READY_Set << RADIO_INTENSET_READY_Pos) |
		(RADIO_INTENSET_ADDRESS_Set << RADIO_INTENSET_ADDRESS_Pos) |
		(RADIO_INTENSET_PAYLOAD_Set << RADIO_INTENSET_PAYLOAD_Pos) |
		(RADIO_INTENSET_END_Set << RADIO_INTENSET_END_Pos) |
		(RADIO_INTENSET_DISABLED_Set << RADIO_INTENSET_DISABLED_Pos);

	NRF_RADIO->PACKETPTR = (uint32_t) packet_buffer;
	
	// no timer here

	NRF_RADIO->EVENTS_READY = 0;
	NRF_RADIO->EVENTS_ADDRESS = 0;
	NRF_RADIO->EVENTS_PAYLOAD = 0;
	NRF_RADIO->EVENTS_END = 0;
	NRF_RADIO->EVENTS_DISABLED = 0;
	NRF_RADIO->EVENTS_DEVMATCH = 0;
	NRF_RADIO->EVENTS_DEVMISS = 0;
	NRF_RADIO->EVENTS_RSSIEND = 0;
	NRF_RADIO->EVENTS_BCMATCH = 0;
	NRF_RADIO->EVENTS_CRCOK = 0;
	NRF_RADIO->EVENTS_CRCERROR = 0;
	
	NVIC_EnableIRQ(RADIO_IRQn);
}

void set_channel(int chan) {
	NRF_RADIO->FREQUENCY = chan;
}

void begin_rx() {
	NRF_RADIO->TASKS_RXEN = 1;
}

void start() {
	NRF_RADIO->TASKS_START = 1;
}

void stop() {
	NRF_RADIO->TASKS_DISABLE = 1;
}

bool is_crc_ok() {
	return NRF_RADIO->CRCSTATUS != 0;
}

extern "C" void RADIO_IRQHandler(void) {
	irq::consume_event<irq::EventType::RadioReady>(&NRF_RADIO->EVENTS_READY);
	irq::consume_event<irq::EventType::RadioAddress>(&NRF_RADIO->EVENTS_ADDRESS);
	irq::consume_event<irq::EventType::RadioPayload>(&NRF_RADIO->EVENTS_PAYLOAD);
	irq::consume_event<irq::EventType::RadioEnd>(&NRF_RADIO->EVENTS_END);
	irq::consume_event<irq::EventType::RadioDisabled>(&NRF_RADIO->EVENTS_DISABLED);
}

} // namespace radio
} // namespace noir
