#pragma once

#include<cstring>

namespace noir {

class Sink {
 public:
	virtual size_t WriteAvailable() = 0;
	virtual void Write(const void *data, size_t size) = 0;
	virtual void Flush();
};

}; // namespace noir
