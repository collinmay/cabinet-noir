#include "pcapng.hpp"

#include<algorithm>
#include<iterator>
#include<vector>

#include<malloc.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>

#include "noir.hpp"

namespace noir {
namespace pcapng {

Writer::Writer(Sink &sink) : sink(sink) {
}

void Writer::OpenBlock(uint32_t block_type) {
	memset(block_buffer, 0, sizeof(block_buffer));
	block_buffer_head = 8;
	((uint32_t*) block_buffer)[0] = block_type;
	block_opened = true;
}

bool Writer::TryCommitBlock() {
	if(!block_opened) {
		return true;
	}
	if(!block_closed) {
		block_buffer_head+= 4;
		((uint32_t*) block_buffer)[1] = block_buffer_head;
		*((uint32_t*) (block_buffer + block_buffer_head - 4)) = block_buffer_head;
		block_closed = true;
	}
	if(sink.WriteAvailable() >= block_buffer_head) {
		sink.Write((void*) block_buffer, block_buffer_head);
		block_opened = false;
		block_closed = false;
		return true;
	} else {
		return false;
	}
}

void Writer::CommitBlock() {
	while(!TryCommitBlock()) {}
}

bool Writer::IsReady() {
	return !block_opened;
}

void Writer::AppendToBlock(const uint8_t *data, size_t size) {
	if(block_buffer_head + size > sizeof(block_buffer)) {
		noir::panic(PanicType::WriterBufferOverrun);
	}
	memcpy(block_buffer + block_buffer_head, data, size);
	block_buffer_head+= size;
}

void Writer::Align() {
	while(block_buffer_head & 3) {
		block_buffer[block_buffer_head++] = 0;
	}
}

void Writer::AppendOptions(Option *options) {
	for(int i = 0; options != NULL && options[i].code != 0; i++) {
		AppendToBlock((uint8_t*) &options[i], 4);
		AppendToBlock((uint8_t*) options[i].value, options[i].length);
		Align();
	}
	Option end = {.code = 0, .length = 0, .value = NULL};
	AppendToBlock((uint8_t*) &end, 4);
}

void Writer::WriteSHB(Option *options) {
	CommitBlock();
	
	struct {
		uint32_t bom;
		uint16_t major;
		uint16_t minor;
		int64_t length;
	} shb_head;

	OpenBlock(0x0A0D0D0A);
	
	shb_head.bom = 0x1A2B3C4D; // byte order magic
	shb_head.major = 1;
	shb_head.minor = 0;
	shb_head.length = -1;

	AppendToBlock((uint8_t*) &shb_head, sizeof(shb_head));
	AppendOptions(options);

	interface_id = 0; // local to section
}

uint32_t Writer::WriteIDB(uint16_t link_type, uint32_t snap_len, Option *options) {
	CommitBlock();
	
	struct {
		uint16_t link_type;
		uint16_t reserved;
		uint32_t snap_len;
	} idb_head;

	OpenBlock(0x1);

	idb_head.link_type = link_type;
	idb_head.reserved = 0;
	idb_head.snap_len = snap_len;

	AppendToBlock((uint8_t*) &idb_head, sizeof(idb_head));
	AppendOptions(options);

	return interface_id++;
}

void Writer::WriteEPB(uint32_t if_id, uint64_t timestamp, uint32_t cap_length, uint32_t orig_length, const void *data, Option *options) {
	CommitBlock();
	
	struct __attribute__((packed)) {
		uint32_t if_id;
		uint32_t ts_hi;
		uint32_t ts_lo;
		uint32_t cap_length;
		uint32_t orig_length;
	} epb_head;

	OpenBlock(0x6);

	epb_head.if_id = if_id;
	epb_head.ts_hi = timestamp >> 32;
	epb_head.ts_lo = timestamp & 0xFFFFFFFF;
	epb_head.cap_length = cap_length;
	epb_head.orig_length = orig_length;

	AppendToBlock((uint8_t*) &epb_head, sizeof(epb_head));
	AppendToBlock((uint8_t*) data, cap_length);
	Align();
	AppendOptions(options);
}

} // namespace pcapng
} // namespace noir
