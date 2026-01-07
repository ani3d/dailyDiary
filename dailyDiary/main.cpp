#include <winsock2.h>
#include <iostream>
#include "Packet.h"

#pragma comment(lib,"ws2_32.lib")

void sv_handle_client(SOCKET client_sock);
void sv_init_db();

int main() {
	std::setlocale(LC_ALL,"ko_KR.UTF-8");
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return -1;
	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
	
	sv_init_db();

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(9000);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
	listen(listenSock, SOMAXCONN);

	std::cout << "일기장 서버 시작(포트 9000)" << std::endl;

	while (1) {
		sockaddr_in clientAddr;
		int addrLen = sizeof(clientAddr);

		SOCKET clientSock = accept(listenSock, (sockaddr*)&clientAddr, &addrLen);
		if (clientSock != INVALID_SOCKET) {
			std::cout << "클라이언트 접속 성공!"  << std::endl;
			sv_handle_client(clientSock);
		}
	}
	

	closesocket(listenSock);
	WSACleanup();
	return 0;

}