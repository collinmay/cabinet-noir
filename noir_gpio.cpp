#include "noir_gpio.hpp"

namespace noir {
namespace gpio {

Output::Output(int no) : no(no) {
	init();
}

void Output::init() {
	NRF_P0->DIRSET = 1 << no;
}

void Output::set(bool on) {
	if(on) {
		NRF_P0->OUTSET = 1 << no;
	} else {
		NRF_P0->OUTCLR = 1 << no;
	}
}

void Output::on() {
	set(true);
}

void Output::off() {
	set(false);
}

void Output::highdrive() {
	NRF_P0->PIN_CNF[no] = 0x30001; // high drive 0, high drive 1, output
}

Input::Input(int no) : no(no) {
	init();
}

void Input::init() {
	NRF_P0->DIRCLR = 1 << no;
}

void Input::pullup() {
	NRF_P0->PIN_CNF[no] = 0xc;
}

bool Input::read() {
	return (NRF_P0->IN & (1 << no)) != 0;
}

void reset() {
	NRF_P0->OUT = 0;
	NRF_P0->DIR = 0;
}

} // namespace gpio
} // namespace noir
