#include "rtprandomurandom.h"
#include "rtperrors.h"



RTPRandomURandom::RTPRandomURandom()
{
	device = 0;
}

RTPRandomURandom::~RTPRandomURandom()
{
	if (device)
		fclose(device);
}

int RTPRandomURandom::Init()
{
	if (device)
		return ERR_RTP_RTPRANDOMURANDOM_ALREADYOPEN;

	device = fopen("/dev/urandom","rb");
	if (device == 0)
		return ERR_RTP_RTPRANDOMURANDOM_CANTOPEN;

	return 0;
}

uint8_t RTPRandomURandom::GetRandom8()
{
	if (!device)
		return 0;

	uint8_t value;

	fread(&value, sizeof(uint8_t), 1, device);

	return value;
}

uint16_t RTPRandomURandom::GetRandom16()
{
	if (!device)
		return 0;

	uint16_t value;

	fread(&value, sizeof(uint16_t), 1, device);

	return value;
}

uint32_t RTPRandomURandom::GetRandom32()
{
	if (!device)
		return 0;

	uint32_t value;

	fread(&value, sizeof(uint32_t), 1, device);

	return value;
}

double RTPRandomURandom::GetRandomDouble()
{
	if (!device)
		return 0;

	uint64_t value;

	fread(&value, sizeof(uint64_t), 1, device);

#ifdef RTP_HAVE_VSUINT64SUFFIX
	value &= 0x7fffffffffffffffui64;
#else
	value &= 0x7fffffffffffffffULL;
#endif // RTP_HAVE_VSUINT64SUFFIX

	int64_t value2 = (int64_t)value;
	double x = RTPRANDOM_2POWMIN63*(double)value2;

	return x;

}

