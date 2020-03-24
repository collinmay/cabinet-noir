/**
 * Copyright (c) 2014 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

/** @file
 *
 * @defgroup nrf_dev_radio_rx_example_main main.c
 * @{
 * @ingroup nrf_dev_radio_rx_example
 * @brief Radio Receiver example Application main file.
 *
 * This file contains the source code for a sample application using the NRF_RADIO peripheral.
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
//#include "radio_config.h"
#include "nrf_gpio.h"
#include "boards.h"
#include "app_timer.h"
#include "app_uart.h"
#include "nordic_common.h"
#include "nrf_error.h"
#include "app_error.h"
//#include "nrf_log.h"
//#include "nrf_log_ctrl.h"
#include "nrf_uart.h"

#define APP_TIMER_PRESCALER      0                     /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE  2                     /**< Size of timer operation queues. */

static char packet_buffer[256];

void clock_initialization() {
  //NRF_CLOCK->XTALFREQ = CLOCK_XTALFREQ_XTALFREQ_16MHz;
  NRF_CLOCK->LFCLKSRC = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
  
  NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
  NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
  NRF_CLOCK->TASKS_LFCLKSTART    = 1;
  NRF_CLOCK->TASKS_HFCLKSTART    = 1;

  /* Wait for the external oscillator to start up */
  while(NRF_CLOCK->EVENTS_HFCLKSTARTED == 0 ||
	NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) { }

  NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
  NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
}


void uart_error_handle(app_uart_evt_t *event) {
  if(event->evt_type == APP_UART_COMMUNICATION_ERROR) {
    APP_ERROR_HANDLER(event->data.error_communication);
  } else if(event->evt_type == APP_UART_FIFO_ERROR) {
    APP_ERROR_HANDLER(event->data.error_code);
  }
}

void noir_radio_configure(int channel) { // 0x1a
  int packet_maxlen = 0x46;

  // cycle power to clear registers
  NRF_RADIO->POWER = 0;
  NRF_RADIO->POWER = 1;

  __disable_irq();
  NRF_RADIO->TASKS_DISABLE = 1;
  while(NRF_RADIO->EVENTS_DISABLED == 0) { }
  NRF_RADIO->EVENTS_DISABLED = 0;
  while(NRF_RADIO->STATE != RADIO_STATE_STATE_Disabled) {}
  __enable_irq();
	
  NRF_RADIO->MODECNF0 = 1; // fast ramp up

  NRF_RADIO->PCNF0 = 0x100108; // LFLEN: 8 bits, S0LEN: 1 byte, S1LEN: 0, S1INCL: always, PLEN: 8-bit preamble
  NRF_RADIO->PCNF1 = packet_maxlen | 0x1040000; // STATLEN: 0, 4 byte base address, big endian, no whitening

  NRF_RADIO->PREFIX0 = 0xf0;
  NRF_RADIO->BASE0 = 0xdfb2124b;
  NRF_RADIO->RXADDRESSES = 0xff;
  
  NRF_RADIO->FREQUENCY = channel;
  NRF_RADIO->MODE = (RADIO_MODE_MODE_Nrf_2Mbit << RADIO_MODE_MODE_Pos); // not sure why this doesn't exist in sdk

  NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_0dBm << RADIO_TXPOWER_TXPOWER_Pos);
  NRF_RADIO->DACNF = 0; // no device address matching
  NRF_RADIO->SHORTS = 0;
  NRF_RADIO->CRCINIT = 0xffffff;
  NRF_RADIO->CRCCNF = 3;
  NRF_RADIO->CRCPOLY = 0x1108421;
  
  //NRF_RADIO->INTENSET = 0x1a; // ADDRESS, END, DISABLED
  NRF_RADIO->INTENSET =
    (RADIO_INTENSET_READY_Set << RADIO_INTENSET_READY_Pos) |
    (RADIO_INTENSET_ADDRESS_Set << RADIO_INTENSET_ADDRESS_Pos) |
    (RADIO_INTENSET_PAYLOAD_Set << RADIO_INTENSET_PAYLOAD_Pos) |
    (RADIO_INTENSET_END_Set << RADIO_INTENSET_END_Pos) |
    (RADIO_INTENSET_DISABLED_Set << RADIO_INTENSET_DISABLED_Pos);

  NRF_RADIO->PACKETPTR = (uint32_t) &packet_buffer[0];
	
  // no timer here

  NRF_RADIO->EVENTS_READY = 0;
  NRF_RADIO->EVENTS_ADDRESS = 0;
  NRF_RADIO->EVENTS_PAYLOAD = 0;
  NRF_RADIO->EVENTS_END = 0;
  NRF_RADIO->EVENTS_DISABLED = 0;
  NRF_RADIO->EVENTS_DEVMATCH = 0;
  NRF_RADIO->EVENTS_DEVMISS = 0;
  NRF_RADIO->EVENTS_RSSIEND = 0;
  NRF_RADIO->EVENTS_BCMATCH = 0;
  NRF_RADIO->EVENTS_CRCOK = 0;
  NRF_RADIO->EVENTS_CRCERROR = 0;
	
  NVIC_EnableIRQ(RADIO_IRQn);
}

void noir_timer_setup(int delayms) {
  NRF_TIMER0->TASKS_STOP = 1;
  NRF_TIMER0->SHORTS = (TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos);
  NRF_TIMER0->MODE = TIMER_MODE_MODE_Timer;
  NRF_TIMER0->BITMODE = (TIMER_BITMODE_BITMODE_24Bit << TIMER_BITMODE_BITMODE_Pos);
  NRF_TIMER0->PRESCALER = 4; // 1us resolution
  NRF_TIMER0->INTENSET = (TIMER_INTENSET_COMPARE0_Set << TIMER_INTENSET_COMPARE0_Pos);

  NRF_TIMER0->CC[0] = (uint32_t) delayms * 1000;
	
  NVIC_EnableIRQ(TIMER0_IRQn);
}

void noir_timer_begin() {
  NRF_TIMER0->EVENTS_COMPARE[0] = 0;
  NRF_TIMER0->TASKS_START = 1;
}

volatile uint32_t noir_irq_hits = 0;
volatile uint32_t noir_irq_overruns = 0;
volatile uint32_t noir_irq_event_buffer[16];
volatile uint32_t noir_irq_event_buffer_head = 0;
volatile uint32_t noir_irq_event_buffer_tail = 0;

enum {
      NOIR_IRQ_EVENT_TIMER,
      NOIR_IRQ_EVENT_RADIO_READY,
      NOIR_IRQ_EVENT_RADIO_ADDRESS,
      NOIR_IRQ_EVENT_RADIO_PAYLOAD,
      NOIR_IRQ_EVENT_RADIO_END,
      NOIR_IRQ_EVENT_RADIO_DISABLED,
};

const int beacon_freqs[] = {
			    4,  6 , 8,
			    10, 12, 14, 16, 18,
			    20, 22, 24,     28,
			    30, 32, 34, 36, 38,
			    40, 42, 44, 46, 48,
			    50, 52, 54, 56, 58,
			    60, 62, 64, 66, 68,
			    70, 72, 74, 76, 78
};

int main() {
  uint32_t err_code = NRF_SUCCESS;

  /* Initialize clock */
  clock_initialization();

  const app_uart_comm_params_t comm_params = {
					      RX_PIN_NUMBER,
					      TX_PIN_NUMBER,
					      RTS_PIN_NUMBER,
					      CTS_PIN_NUMBER,
					      APP_UART_FLOW_CONTROL_ENABLED,
					      false,
					      NRF_UART_BAUDRATE_115200
  };

  APP_UART_FIFO_INIT(&comm_params,
		     256, // rx buffer size
		     512, // tx buffer size
		     uart_error_handle,
		     APP_IRQ_PRIORITY_LOWEST,
		     err_code);
  APP_ERROR_CHECK(err_code);

  puts("what?\r\n");
  printf("ok cool\r\n");

  noir_timer_setup(1000);
  noir_timer_begin();

  printf("began timer\r\n");

  noir_radio_configure(0x1a);
  printf("configured radio\r\n");
	
  NRF_RADIO->TASKS_RXEN = 1;

  int freq_idx = 0;
	
  while(true) {
    __enable_irq();
    while(noir_irq_hits == 0) { /* do nothing */ }
    noir_irq_hits = 0;
		
    if(noir_irq_overruns > 0) {
      printf("overran %ld events\r\n", noir_irq_overruns);
      noir_irq_overruns = 0;
    }

    if(NRF_RADIO->EVENTS_DEVMISS) {
      NRF_RADIO->EVENTS_DEVMISS = 0;
    }
		
    while(noir_irq_event_buffer_tail != noir_irq_event_buffer_head) {
      uint32_t event = noir_irq_event_buffer[noir_irq_event_buffer_tail++];
      noir_irq_event_buffer_tail%= sizeof(noir_irq_event_buffer)/sizeof(noir_irq_event_buffer[0]);

      switch(event) {
      case NOIR_IRQ_EVENT_TIMER:
	//printf("timer: HIT\r\n");
	freq_idx++;
	freq_idx%= sizeof(beacon_freqs)/sizeof(beacon_freqs[0]);
	//printf("pending frequency change to %d\r\n", freq);
	NRF_RADIO->TASKS_DISABLE = 1;
	printf("radio: coming down.\r\n");
	break;
      case NOIR_IRQ_EVENT_RADIO_READY:
	//printf("radio: READY\r\n");
	printf("radio: up at %d MHz...\r\n", 2400 + beacon_freqs[freq_idx]);
	fflush(stdout);
	NRF_RADIO->TASKS_START = 1;
	break;
      case NOIR_IRQ_EVENT_RADIO_ADDRESS:
	printf("radio: ADDRESS\r\n");
	break;
      case NOIR_IRQ_EVENT_RADIO_PAYLOAD:
	printf("radio: PAYLOAD\r\n");
	break;
      case NOIR_IRQ_EVENT_RADIO_END:
	printf("radio: END\r\n");
	for(int i = 0; i < sizeof(packet_buffer); i++) {
	  printf(" %02x", (int) packet_buffer[i]);
	  if(i % 8 == 7) {
	    printf(" ");
	  }
	  if(i % 16 == 15) {
	    printf("\r\n");
	  }
	}
	break;
      case NOIR_IRQ_EVENT_RADIO_DISABLED:
	//printf("radio: DISABLED\r\n");
	NRF_RADIO->FREQUENCY = beacon_freqs[freq_idx];
	NRF_RADIO->TASKS_RXEN = 1;
	break;
      default:
	printf("unknown IRQ event\r\n");
	break;
      }
    }
  }
}

void noir_irq_event_buffer_push(uint32_t event) {
  size_t buf_size = sizeof(noir_irq_event_buffer)/sizeof(noir_irq_event_buffer[0]);
  if(((noir_irq_event_buffer_head + 1) % buf_size) == noir_irq_event_buffer_tail) {
    noir_irq_overruns++;
  } else {
    noir_irq_event_buffer[noir_irq_event_buffer_head++] = event;
    noir_irq_event_buffer_head%= buf_size;
  }
}

void noir_irq_consume_event(volatile uint32_t *reg, uint32_t noir_event) {
  if(*reg) {
    noir_irq_event_buffer_push(noir_event);
    *reg = 0;
  }
}

void RADIO_IRQHandler(void) {
  noir_irq_hits++;
  noir_irq_consume_event(&NRF_RADIO->EVENTS_READY, NOIR_IRQ_EVENT_RADIO_READY);
  noir_irq_consume_event(&NRF_RADIO->EVENTS_ADDRESS, NOIR_IRQ_EVENT_RADIO_ADDRESS);
  noir_irq_consume_event(&NRF_RADIO->EVENTS_PAYLOAD, NOIR_IRQ_EVENT_RADIO_PAYLOAD);
  noir_irq_consume_event(&NRF_RADIO->EVENTS_END, NOIR_IRQ_EVENT_RADIO_END);
  noir_irq_consume_event(&NRF_RADIO->EVENTS_DISABLED, NOIR_IRQ_EVENT_RADIO_DISABLED);
}

void TIMER0_IRQHandler(void) {
  noir_irq_hits++;
  noir_irq_consume_event(&NRF_TIMER0->EVENTS_COMPARE[0], NOIR_IRQ_EVENT_TIMER);
}
