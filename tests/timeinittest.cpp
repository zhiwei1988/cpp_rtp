#include "rtptimeutilities.h"
#include "rtpudpv4transmitter.h"
#include <iostream>

int main(void)
{
	RTPUDPv4Transmitter trans(0);
	RTPTime t(0);
	std::cout << t.GetDouble() << std::endl;
	return 0;
}
