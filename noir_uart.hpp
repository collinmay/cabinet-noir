#pragma once

#include<unistd.h>

#include "noir_gpio.hpp"
#include "noir_sink.hpp"

namespace noir {
namespace uart {

class Uart : public noir::Sink {
 public:
	Uart(gpio::Input &&rxd, gpio::Input &&cts, gpio::Output &&rts, gpio::Output &&txd);

	virtual size_t WriteAvailable() override;
	virtual void Write(const void *data, size_t size) override;
	virtual void Flush() override;

	void Wash();
	
 private:
	gpio::Input rxd;
	gpio::Input cts;
	gpio::Output rts;
	gpio::Output txd;
	
	uint8_t tx_buffer[768];
	size_t tx_buffer_head = 0;
	volatile size_t tx_buffer_tail = 0;

	void Compact();
	void Advance();
	static Uart *instance;
	bool tx_active = false;
	bool tx_finished = false;
	static void irq_handler();
}; // class Uart

} // namespace uart
} // namespace noir
