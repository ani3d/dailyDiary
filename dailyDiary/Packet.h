#pragma once
#include<winsock2.h>

enum PacketType {
	Write_Diary = 1,
	Read_Diary = 2,
	Response_Ok = 3,
	Response_Fail = 4
};

#pragma pack(push,1)
struct DiaryPacket {
	int type;
	int dataLength;
	char content[1024];
};
#pragma pack(pop)