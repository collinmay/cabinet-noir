#pragma once

#include "nrf_gpio.h"

namespace noir {
namespace gpio {

void reset();

class Output {
 public:
	Output(int no);
	Output(Output &) = delete;
	Output(Output &&) = default;
	Output &operator=(Output &) = delete;
	Output &operator=(Output &&) = default;

	void init();
	void set(bool on);
	void on();
	void off();

	void highdrive();

	int no;
};

class Input {
 public:
	Input(int no);
	Input(Input &) = delete;
	Input(Input &&) = default;
	Input &operator=(Input &) = delete;
	Input &operator=(Input &&) = default;

	void init();
	void pullup();
	bool read();
	
	int no;
};

const int PIN_LED1 = 17;
const int PIN_LED2 = 18;
const int PIN_LED3 = 19;
const int PIN_LED4 = 20;

} // namespace gpio
} // namespace noir
