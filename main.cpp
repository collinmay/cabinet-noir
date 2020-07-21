#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "nrf_gpio.h"
#include "boards.h"
#include "nordic_common.h"
#include "nrf_error.h"
#include "app_error.h"

#include<utility>

#include "noir.hpp"
#include "noir_irq.hpp"
#include "noir_radio.hpp"
#include "noir_timer.hpp"
#include "noir_gpio.hpp"
#include "noir_uart.hpp"
#include "pcapng.hpp"

namespace noir {

namespace clock {

void initialize() {
	//NRF_CLOCK->XTALFREQ = CLOCK_XTALFREQ_XTALFREQ_16MHz;
	NRF_CLOCK->LFCLKSRC = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
	
	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
	NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
	NRF_CLOCK->TASKS_LFCLKSTART		 = 1;
	NRF_CLOCK->TASKS_HFCLKSTART		 = 1;

	/* Wait for the external oscillator to start up */
	while(NRF_CLOCK->EVENTS_HFCLKSTARTED == 0 ||
				NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) { }

	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
	NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
}

} // namespace clock

const int beacon_freqs[] = {
	4,           6,  8,
	10, 12, 14, 16, 18,
	20, 22, 24,     28,
	30, 32, 34, 36, 38,
	40, 42, 44, 46, 48,
	50, 52, 54, 56, 58,
	60, 62, 64, 66, 68,
	70, 72, 74, 76, 78
};

namespace pins {

gpio::Input button1(13, gpio::Input::Flag::Pullup);
gpio::Input button2(14, gpio::Input::Flag::Pullup);
gpio::Input button3(15, gpio::Input::Flag::Pullup);
gpio::Input button4(16, gpio::Input::Flag::Pullup);

gpio::Output led1(17);
gpio::Output led2(18);
gpio::Output led3(19);
gpio::Output led4(20);

gpio::Input rxd(8);
gpio::Input cts(7);
gpio::Output rts(5, gpio::Output::Flag::HighDrive);
gpio::Output txd(6, gpio::Output::Flag::HighDrive);

} // namespace pins

[[noreturn]] void panic(PanicType t) {
	noir::pins::led4.off();
	for(uint32_t i = 0; true; i++) {
		noir::pins::led4.set((i & 0x40000) == 0);
	}
	while(1) {}
}

} // namespace noir

struct {
	int frequency;
	int rssi;
	int crcstatus;
	int rxmatch;
	union {
		char payload[256];
		struct {
			uint8_t s0;
			uint8_t length;
			uint8_t s1; // not present on air
		} radio_packet;
	};
} cap_packet;

char rng() {
	while(NRF_RNG->EVENTS_VALRDY == 0) {}
	NRF_RNG->EVENTS_VALRDY = 0;
	return NRF_RNG->VALUE;
}

int main() {
	noir::pins::led1.off();
	noir::pins::led2.on();
	noir::pins::led3.on();
	noir::pins::led4.on();

	static noir::uart::Uart uart(
		std::move(noir::pins::rxd), std::move(noir::pins::cts),
		std::move(noir::pins::rts), std::move(noir::pins::txd));
	
	__enable_irq();

	uart.Wash();
	
	noir::pins::led1.off();
	for(uint32_t i = 0; noir::pins::button1.read(); i++) {
		noir::pins::led1.set((i & 0x80000) == 0);
	}
	noir::pins::led1.on();

	/*

	// UART reliability testing.

	NRF_RNG->EVENTS_VALRDY = 0;
	NRF_RNG->TASKS_START = 1;
	
	while(1) {
		char buf[280];
		uint16_t length = rng() + 1;
		*(uint16_t*) &buf[0] = length;
		uint16_t sum = 0;
		for(int i = 0; i < length; i++) {
			while(NRF_RNG->EVENTS_VALRDY == 0) {}
			NRF_RNG->EVENTS_VALRDY = 0;
			
			buf[i+2] = NRF_RNG->VALUE;
			sum+= buf[i+2];
		}
		*(uint16_t*) &buf[length+2] = sum;
		if(uart.WriteAvailable() >= length+4) {
			uart.Write((void*) buf, length+4);
		}
	}
	*/
	
	noir::clock::initialize();
	noir::pcapng::Writer pcap_writer(uart);
	static const char shb_hardware[] = "nrf52-DK";
	static const char shb_os[] = "Baremetal";
	static const char shb_userappl[] = "noir";
	noir::pcapng::Option shb_options[] = {
		{.code = noir::pcapng::SHB_HARDWARE, .length = sizeof(shb_hardware), .value = shb_hardware},
		{.code = noir::pcapng::SHB_OS, .length = sizeof(shb_os), .value = shb_os},
		{.code = noir::pcapng::SHB_USERAPPL, .length = sizeof(shb_userappl), .value = shb_userappl},
		{.code = 0, .length = 0, .value = 0}
	};
	pcap_writer.WriteSHB(shb_options);

	static const char idb_name[] = "radio";
	noir::pcapng::Option idb_options[] = {
		{.code = 2, .length = sizeof(idb_name), .value = idb_name},
		{.code = 0, .length = 0, .value = 0}
	};
	uint32_t interface_id = pcap_writer.WriteIDB(162, 0xff, idb_options); // DLT_USER15

	pcap_writer.CommitBlock();
	uart.Flush();
	
	noir::timer::setup(200);
	noir::radio::configure(0x1a, (void*) cap_packet.payload);
	noir::timer::begin();
	noir::radio::begin_rx();

	int freq_idx = 0;
	uint64_t drop_count = 0;
	
	while(true) {
		while(noir::irq::has_next()) {
			int overruns = noir::irq::pop_overruns();
			if(overruns > 0) {
				//printf("overran %ld events\r\n", overruns);
			}
			
			if(NRF_RADIO->EVENTS_DEVMISS) {
				NRF_RADIO->EVENTS_DEVMISS = 0;
			}

			pcap_writer.TryCommitBlock();
			
			switch(noir::irq::pop()) {
			case noir::irq::EventType::TimerHit:
				freq_idx++;
				freq_idx%= sizeof(noir::beacon_freqs)/sizeof(noir::beacon_freqs[0]);
				noir::pins::led2.on();
				noir::radio::stop();
				break;
			case noir::irq::EventType::RadioReady:
				noir::radio::start();
				break;
			case noir::irq::EventType::RadioEnd:
				noir::pins::led2.off();
				cap_packet.frequency = NRF_RADIO->FREQUENCY;
				cap_packet.rssi = NRF_RADIO->RSSISAMPLE;
				cap_packet.crcstatus = NRF_RADIO->CRCSTATUS;
				cap_packet.rxmatch = NRF_RADIO->RXMATCH;
				if(pcap_writer.IsReady()) {
					noir::pcapng::Option epb_options[] = {
						{.code = noir::pcapng::EPB_DROPCOUNT, .length = sizeof(drop_count), .value = (void*) &drop_count},
						{.code = 0, .length = 0, .value = 0}
					};
					size_t cap_size = 16 + 3 + cap_packet.radio_packet.length;
					pcap_writer.WriteEPB(interface_id, 0, cap_size, cap_size, (uint8_t*) &cap_packet, epb_options);
					drop_count = 0;
				} else {
					drop_count++;
				}
				break;
			case noir::irq::EventType::RadioDisabled:
				noir::radio::set_channel(noir::beacon_freqs[freq_idx]);
				noir::radio::begin_rx();
				break;
			default:
				break;
			}
		}
	}
}
