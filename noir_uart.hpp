#pragma once

#include<unistd.h>

#include "noir_gpio.hpp"

namespace noir {
namespace uart {

class Uart {
 public:
	Uart(gpio::Input &&rxd, gpio::Input &&cts, gpio::Output &&rts, gpio::Output &&txd);

	size_t WriteAvailable();
	void Write(const void *data, size_t size);
	void Write(const char *str);
	
 private:
	gpio::Input rxd;
	gpio::Input cts;
	gpio::Output rts;
	gpio::Output txd;
	
	uint8_t tx_buffer[21];
	size_t tx_buffer_head = 0;
	size_t tx_buffer_readout = 0;
	volatile size_t tx_buffer_tail = 0;

	void Advance();
	static Uart *instance;
	bool tx_active = false;
	bool tx_finished = false;
	static void irq_handler();
}; // class Uart

} // namespace uart
} // namespace noir
