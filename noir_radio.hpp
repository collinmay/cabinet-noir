#pragma once

namespace noir {
namespace radio {

void configure(int channel, void *packet_buffer);
void set_channel(int channel);
void begin_rx();
void start();
void stop();

} // namespace radio
} // namespace noir
