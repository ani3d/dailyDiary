#include<winsock2.h>
#include<iostream>
#include<fstream>
#include<string>
#include "Packet.h"

#pragma comment(lib,"ws2_32.lib")

void handle_client(SOCKET client_sock) {
	DiaryPacket packet;
	int revSize = recv(client_sock, (char*)&packet, sizeof(packet), 0);

	if (revSize == SOCKET_ERROR) {
		std::cerr << "데이터 수신 실패 : " << WSAGetLastError() << std::endl;
		closesocket(client_sock);
		return;
	}

	if (revSize == 0) {
		std::cout << "클라이언트가 연결을 종료했습니다." << std::endl;
		closesocket(client_sock);
		return;
	}

	if (packet.type == Write_Diary) {
		std::cout << "일기 쓰기 요청 수신. 내용 저장..." << std::endl;
		std::ofstream outFile("my_diary.txt", std::ios::app);

		if (outFile.is_open()) {
			outFile << "----------------------" << std::endl;
			outFile << "내용 : " << packet.content << std::endl;
			outFile << "-----------------" << std::endl << std::endl;

			outFile.close();
			std::cout << "성공적으로 저장됨!" << std::endl;
		}
		else {
			std::cerr << "파일 열기 불가." << std::endl;
		}
	}

	closesocket(client_sock);

}