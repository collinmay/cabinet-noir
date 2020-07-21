#pragma once

#include<type_traits>

#include "nrf_gpio.h"

namespace noir {
namespace gpio {

void reset();

class Output {
 public:
	enum class Flag {
		None = 0,
		HighDrive0 = 1,
		HighDrive1 = 2,
		HighDrive = 3,
	};

	Output(int no, Flag flags = Flag::None);
	Output(Output &) = delete;
	Output(Output &&) = default;
	Output &operator=(Output &) = delete;
	Output &operator=(Output &&) = default;

	void init();
	void set(bool on);
	void on();
	void off();

	int no;
	Flag flags;
};

inline Output::Flag operator|(Output::Flag a, Output::Flag b) {
	using T = std::underlying_type<Output::Flag>::type;
	return (Output::Flag) (((T) a) | ((T) b));
}

inline Output::Flag operator&(Output::Flag a, Output::Flag b) {
	using T = std::underlying_type<Output::Flag>::type;
	return (Output::Flag) (((T) a) & ((T) b));
}

class Input {
 public:
	enum class Flag {
		None = 0,
		Pullup = 1,
		Pulldown = 2,
	};
		
	Input(int no, Flag flags = Flag::None);
	Input(Input &) = delete;
	Input(Input &&) = default;
	Input &operator=(Input &) = delete;
	Input &operator=(Input &&) = default;

	void init();
	bool read();
	
	int no;
	Flag flags;
};

inline Input::Flag operator|(Input::Flag a, Input::Flag b) {
	using T = std::underlying_type<Input::Flag>::type;
	return (Input::Flag) (((T) a) | ((T) b));
}

inline Input::Flag operator&(Input::Flag a, Input::Flag b) {
	using T = std::underlying_type<Input::Flag>::type;
	return (Input::Flag) (((T) a) & ((T) b));
}

} // namespace gpio
} // namespace noir
