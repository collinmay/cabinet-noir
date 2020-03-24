#include "noir_timer.hpp"

#include "nordic_common.h"
#include "nrf_timer.h"

#include "noir_irq.hpp"

namespace noir {
namespace timer {

void setup(int delayms) {
	NRF_TIMER0->TASKS_STOP = 1;
	NRF_TIMER0->SHORTS = (TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos);
	NRF_TIMER0->MODE = TIMER_MODE_MODE_Timer;
	NRF_TIMER0->BITMODE = (TIMER_BITMODE_BITMODE_24Bit << TIMER_BITMODE_BITMODE_Pos);
	NRF_TIMER0->PRESCALER = 4; // 1us resolution
	NRF_TIMER0->INTENSET = (TIMER_INTENSET_COMPARE0_Set << TIMER_INTENSET_COMPARE0_Pos);

	NRF_TIMER0->CC[0] = (uint32_t) delayms * 1000;
	
	NVIC_EnableIRQ(TIMER0_IRQn);
}

void begin() {
	NRF_TIMER0->EVENTS_COMPARE[0] = 0;
	NRF_TIMER0->TASKS_START = 1;
}

extern "C" void TIMER0_IRQHandler(void) {
	irq::consume_event<irq::EventType::TimerHit>(&NRF_TIMER0->EVENTS_COMPARE[0]);
}

} // namespace timer
} // namespace noir
