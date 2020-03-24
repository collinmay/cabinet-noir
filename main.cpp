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

static char packet_buffer[256];

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

[[noreturn]] void panic(PanicType t) {
	while(1) {}
}

} // namespace noir

int main() {
	noir::gpio::reset();
	noir::gpio::Output led1(noir::gpio::PIN_LED1);
	led1.off();

	noir::gpio::Input button(13);
	button.pullup();
	while(button.read()) {}

	led1.on();
	
	noir::gpio::Input rxd(8);
	noir::gpio::Input cts(7);
	noir::gpio::Output rts(5);
	noir::gpio::Output txd(6);

	noir::uart::Uart uart(std::move(rxd), std::move(cts), std::move(rts), std::move(txd));
	
	noir::clock::initialize();
	noir::pcapng::Writer pcap_writer(stdout);
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
	uint32_t interface_id = pcap_writer.WriteIDB(noir::pcapng::LINKTYPE_USER2, 0xff, idb_options);
	
	noir::timer::setup(1000);
	noir::radio::configure(0x1a, (void*) noir::packet_buffer);
	noir::timer::begin();
	noir::radio::begin_rx();

	int freq_idx = 0;
	
	while(true) {
		__enable_irq();
		while(noir::irq::has_next()) {
			int overruns = noir::irq::pop_overruns();
			if(overruns > 0) {
				//printf("overran %ld events\r\n", overruns);
			}
			
			if(NRF_RADIO->EVENTS_DEVMISS) {
				NRF_RADIO->EVENTS_DEVMISS = 0;
			}
			
			switch(noir::irq::pop()) {
			case noir::irq::EventType::TimerHit:
				freq_idx++;
				freq_idx%= sizeof(noir::beacon_freqs)/sizeof(noir::beacon_freqs[0]);
				noir::radio::stop();
				break;
			case noir::irq::EventType::RadioReady:
				noir::radio::start();
				break;
			case noir::irq::EventType::RadioEnd:
				pcap_writer.WriteEPB(interface_id, 0, 0xff, 0xff, noir::packet_buffer, nullptr);
				/*
				for(size_t i = 0; i < sizeof(noir::packet_buffer); i++) {
					printf(" %02x", (int) noir::packet_buffer[i]);
					if(i % 8 == 7) {
						printf(" ");
					}
					if(i % 16 == 15) {
						printf("\r\n");
					}
					}*/
				fflush(stdout);
				while(1) {}
				break;
			case noir::irq::EventType::RadioDisabled:
				noir::radio::set_channel(noir::beacon_freqs[freq_idx]);
				noir::radio::begin_rx();
				break;
			case noir::irq::EventType::RadioAddress:
			case noir::irq::EventType::RadioPayload:
				break;
			default:
				noir::panic(noir::PanicType::UnknownIRQ);
				break;
			}
		}
	}
}
