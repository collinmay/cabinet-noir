#pragma once

#include<stdint.h>

namespace noir {
namespace irq {

enum class EventType {
	TimerHit,
	RadioReady,
	RadioAddress,
	RadioPayload,
	RadioEnd,
	RadioDisabled,
	UartTxReady
};

uint32_t pop_overruns();
bool has_next();
EventType pop();
void push(EventType e);
void consume_event(EventType e, volatile uint32_t *reg);

template<EventType e>
inline void consume_event(volatile uint32_t *reg) {
	consume_event(e, reg);
}

} // namespace irq
} // namespace noir
