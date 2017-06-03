#ifndef DPID_H_
#define DPID_H_

#include <cstdint>

struct Params {
	uint32_t oWidth, oHeight, iWidth, iHeight, pixel_max;
	float pWidth, pHeight, lambda;
};

#endif