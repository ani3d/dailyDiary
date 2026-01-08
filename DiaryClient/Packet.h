#pragma once
#include<winsock2.h>

enum PacketType {
	Signup_Request = 0,
	Write_Diary = 1,
	Read_Diary = 2,
	List_Diary = 3,
	Login_Request=4,
	Update_Diary=5,

	Response_Ok = 200,
	Response_Fail = 400
};

#pragma pack(push,1)
struct DiaryPacket {
	int type;
	int year, month, day;
	int dataLength;
	char content[2048];
};

struct LoginPacket {
	PacketType type;
	char password[20];
};

#pragma pack(pop)