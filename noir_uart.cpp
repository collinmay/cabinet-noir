#include "noir_uart.hpp"

#include<utility>
#include<cstring>

#include "nrf_uarte.h"
#include "nrfx_prs.h"

#include "noir.hpp"
#include "noir_irq.hpp"

namespace noir {
namespace pins {
extern gpio::Output led4;
};


namespace uart {

Uart *Uart::instance = nullptr;

Uart::Uart(
	gpio::Input &&rxd, gpio::Input &&cts,
	gpio::Output &&rts, gpio::Output &&txd) :
	rxd(std::move(rxd)), cts(std::move(cts)), rts(std::move(rts)), txd(std::move(txd)) {
	NRF_UARTE0->ENABLE = 0;
	rts.init();
	txd.init();
	cts.init();
	rxd.init();
	rts.on();
	txd.on();
	NRF_UARTE0->PSEL.RTS = rts.no;
	NRF_UARTE0->PSEL.TXD = txd.no;
	NRF_UARTE0->PSEL.CTS = cts.no;
	NRF_UARTE0->PSEL.RXD = rxd.no;
	NRF_UARTE0->BAUDRATE = 0x10000000;//0x01D60000;//
	NRF_UARTE0->CONFIG = 0; // no hardware flow control, no parity
	NRF_UARTE0->INTEN = 0x100; // ENDTX
	NRF_UARTE0->ENABLE = 8;
	
	instance = this;
	nrfx_prs_acquire((void*) NRF_UARTE0_BASE, irq_handler);
	NVIC_EnableIRQ(UARTE0_UART0_IRQn);
}

size_t Uart::WriteAvailable() {
	return (sizeof(tx_buffer) + tx_buffer_tail - tx_buffer_head - 1) % sizeof(tx_buffer);
}

void Uart::Write(const void *data, size_t size) {
	while(size > 0) {
		if(size > WriteAvailable()) {
			panic(PanicType::UartBufferOverrun);
		}
		
		size_t limit;
		if(tx_buffer_head >= tx_buffer_tail) {
			limit = sizeof(tx_buffer) - tx_buffer_head;
		} else {
			limit = tx_buffer_tail - tx_buffer_head - 1;
		}
		
		if(limit > size) { limit = size; }
		
		memcpy(tx_buffer + tx_buffer_head, data, limit);
		tx_buffer_head+= limit;
		tx_buffer_head%= sizeof(tx_buffer);
		data = (uint8_t*) data + limit;
		size-= limit;
	}

	__disable_irq();
	Advance();
	__enable_irq();
}

void Uart::Flush() {
	pins::led4.off();
	while(tx_buffer_tail != tx_buffer_head) { }
	pins::led4.on();
}

void Uart::Wash() {
	while(tx_active) {
		if(NRF_UARTE0->EVENTS_ENDTX) {
			NRF_UARTE0->EVENTS_ENDTX = 0;

			tx_active = false;
			tx_finished = false;
		}
	}
	
	tx_buffer_head = 0;
	tx_buffer_tail = 0;
	memset(tx_buffer, 0, sizeof(tx_buffer));

	NVIC_DisableIRQ(UARTE0_UART0_IRQn);
	
	NRF_UARTE0->TXD.PTR = (uint32_t) tx_buffer;
	NRF_UARTE0->TXD.MAXCNT = 15;
	NRF_UARTE0->EVENTS_TXSTARTED = 0;
	NRF_UARTE0->EVENTS_ENDTX = 0;
	NRF_UARTE0->TASKS_STARTTX = 1;
	while(NRF_UARTE0->EVENTS_ENDTX == 0) {}
	NRF_UARTE0->EVENTS_ENDTX = 0;
	
	NVIC_EnableIRQ(UARTE0_UART0_IRQn);
}

void Uart::Compact() {
	if(tx_buffer_tail > 0) {
		if(tx_buffer_head > tx_buffer_tail) {
			memmove(tx_buffer, tx_buffer + tx_buffer_tail, tx_buffer_head - tx_buffer_tail);
			tx_buffer_head+= sizeof(tx_buffer);
			tx_buffer_head-= tx_buffer_tail;
			tx_buffer_head%= sizeof(tx_buffer);
			tx_buffer_tail = 0;
		} else {
			// give up
		}
	}
}

void Uart::Advance() {
	if(tx_finished) {
		tx_buffer_tail+= NRF_UARTE0->TXD.AMOUNT;
		tx_buffer_tail%= sizeof(tx_buffer);
		tx_finished = false;
	}
	
	if(!tx_active) {
		//Compact();
		size_t maxcnt = sizeof(tx_buffer) - tx_buffer_tail;
		if(tx_buffer_head >= tx_buffer_tail) {
			maxcnt = tx_buffer_head - tx_buffer_tail;
		}
		
		if(maxcnt > 255) {
			maxcnt = 255; // hardware limit
		}

		if(maxcnt > 0) {
			NRF_UARTE0->TXD.PTR = (uint32_t) tx_buffer + tx_buffer_tail;
			NRF_UARTE0->TXD.MAXCNT = maxcnt;
			NRF_UARTE0->EVENTS_TXSTARTED = 0;
			NRF_UARTE0->EVENTS_ENDTX = 0;
			NRF_UARTE0->TASKS_STARTTX = 1;
			tx_active = true;
		}
	}
}

void Uart::irq_handler(void) {
	if(NRF_UARTE0->EVENTS_ENDTX) {
		NRF_UARTE0->EVENTS_ENDTX = 0;

		instance->tx_active = false;
		instance->tx_finished = true;
		instance->Advance();
		irq::push(irq::EventType::UartTxReady);
	}
}

} // namespace uart
} // namespace noir
