#pragma once

namespace noir {

enum class PanicType {
	Invalid,
	UnknownIRQ,
	WriterIOError,
	WriterBufferOverrun,
	UartBufferOverrun
};

[[noreturn]] void panic(PanicType code);

} // namespace noir
