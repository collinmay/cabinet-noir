#pragma once

#include<stdint.h>
#include<unistd.h>
#include<stdio.h>

namespace noir {
namespace pcapng {

struct Option {
	uint16_t code;
	uint16_t length;
	const void *value;
};

const uint32_t SHB_HARDWARE = 2;
const uint32_t SHB_OS = 3;
const uint32_t SHB_USERAPPL = 4;

const uint32_t LINKTYPE_USER2 = 149; // pulsar link-layer packet

class Writer {
 public:
	Writer(FILE *file);
	 
	void WriteSHB(Option *options);
	uint32_t WriteIDB(uint16_t link_type, uint32_t snap_len, Option *options);
	void WriteEPB(uint32_t if_id, uint64_t timestamp, uint32_t cap_length, uint32_t orig_length, const void *data, Option *options);
 private:
	FILE *file;

	uint8_t block_buffer[300];
	size_t block_buffer_head = 0;

	void OpenBlock(uint32_t block_type);
	void AppendToBlock(const uint8_t *data, size_t size);
	void Align();
	void AppendOptions(Option *options);
	void CommitBlock();

	int interface_id = 0;
};

} // namespace pcapng
} // namespace noir
