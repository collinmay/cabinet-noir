#include "noir_gpio.hpp"

namespace noir {
namespace gpio {

Output::Output(int no, Flag flags) : no(no), flags(flags) {
	init();
}

void Output::init() {
	NRF_P0->PIN_CNF[no] =
		1 | // direction: output
		((flags & Flag::HighDrive0) != Flag::None ? 1 : 0) << 8 |
		((flags & Flag::HighDrive1) != Flag::None ? 1 : 0) << 9;
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

Input::Input(int no, Flag flags) : no(no), flags(flags) {
	init();
}

void Input::init() {
	NRF_P0->PIN_CNF[no] =
		0 | // direction: input
		((flags & Flag::Pullup)   != Flag::None ? 3 : 0) << 2 |
		((flags & Flag::Pulldown) != Flag::None ? 1 : 0) << 2;
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
