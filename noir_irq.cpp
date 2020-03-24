#include "noir_irq.hpp"

#include<unistd.h>

namespace noir {
namespace irq {

volatile uint32_t hits = 0;
volatile uint32_t overruns = 0;
volatile EventType event_buffer[16];
volatile uint32_t event_buffer_head = 0;
volatile uint32_t event_buffer_tail = 0;

uint32_t pop_overruns() {
	uint32_t old = overruns;
	overruns = 0;
	return old;
}

bool has_next() {
	return event_buffer_tail != event_buffer_head;
}

EventType pop() {
	EventType e = event_buffer[event_buffer_tail++];
	event_buffer_tail%= sizeof(event_buffer)/sizeof(event_buffer[0]);
	return e;
}

void push(EventType e) {
	constexpr size_t buffer_size = sizeof(event_buffer)/sizeof(event_buffer[0]);
	if(((event_buffer_head + 1) % buffer_size) == event_buffer_tail) {
		overruns++;
	} else {
		event_buffer[event_buffer_head++] = e;
		event_buffer_head%= buffer_size;
	}
}

void consume_event(EventType e, volatile uint32_t *reg) {
	if(*reg) {
		push(e);
		*reg = 0;
	}
}

} // namespace irq
} // namespace noir
