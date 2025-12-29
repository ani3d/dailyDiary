#include<winsock2.h>
#include<iostream>
#include "Packet.h"

#pragma comment(lib,"ws2_32.lib")

void handle_clinet(SOCKET client_sock) {
	DiaryPacket packet;
	int revSize = recv(client_sock, (char*)&packet, sizeof(packet), 0);

	if (revSize == SOCKET_ERROR) {
		std::cerr << "데이터 수신 실패 : " << WSAGetLastError() << std::endl;
		return;
	}

	if (revSize == 0) {
		std::cout << "클라이언트가 연결을 종료했습니다." << std::endl;
		return;
	}
	std::cout << "패킷 수신 성공 ! 타입 : " << packet.type << std::endl;

}