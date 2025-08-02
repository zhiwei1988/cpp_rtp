#include "rtcppacket.h"
#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

#ifdef RTPDEBUG

void RTCPPacket::Dump()
{
	switch(packettype)
	{
	case SR:
		std::cout << "RTCP Sender Report      ";
		break;
	case RR:
		std::cout << "RTCP Receiver Report    ";
		break;
	case SDES:
		std::cout << "RTCP Source Description ";
		break;
	case APP:
		std::cout << "RTCP APP Packet         ";
		break;
	case BYE:
		std::cout << "RTCP Bye Packet         ";
		break;
	case Unknown:
		std::cout << "Unknown RTCP Packet     ";
		break;
	default:
		std::cout << "ERROR: Invalid packet type!" << std::endl;		
	}
	std::cout << "Length: " << datalen;
	std::cout << std::endl;
}

#endif // RTPDEBUG
