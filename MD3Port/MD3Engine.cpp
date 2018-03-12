#include "MD3.h"
#include "CRC.h"

uint32_t MD3CRC(const uint32_t data)
{
	uint32_t inputdata = data * 0x40;
	uint32_t crc = CRC::Calculate((const void*)&inputdata, sizeof(inputdata), CRC::CRC_6_MD3());
	return crc;
}