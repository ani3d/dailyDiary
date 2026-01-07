// DiaryClient.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include<winsock2.h>
#include<string>
#include "Packet.h"
#include<WS2tcpip.h>
#include<chrono>
#include<ctime>


#pragma comment(lib,"ws2_32.lib")

void cl_write_diary(SOCKET sock) {
    DiaryPacket packet;
    memset(&packet, 0, sizeof(packet));
    packet.type = Write_Diary;

    time_t now = time(0);
    struct tm tstruct;
    localtime_s(&tstruct, &now);
    packet.year = tstruct.tm_year + 1900;
    packet.month = tstruct.tm_mon + 1;
    packet.day = tstruct.tm_mday;

    std::cout << "날짜 자동 설정// " << packet.year << "년 " << packet.month << "월 " << packet.day << "일\n";
    std::cout << "내용 입력 : " << std::endl;
    std::cin.getline(packet.content, 2048);
    packet.dataLength = (int)strlen(packet.content);

    send(sock, (char*)&packet, sizeof(packet), 0);
}

void cl_read_diary(SOCKET sock) {
    DiaryPacket packet;
    memset(&packet, 0, sizeof(packet));
    packet.type = Read_Diary;

    std::cout << "연 월 일 입력 : ";
    std::cin >> packet.year >> packet.month >> packet.day;
    std::cin.ignore();

    send(sock, (char*)&packet, sizeof(packet), 0);

    int revSize = recv(sock, (char*)&packet, sizeof(packet), 0);
    if (revSize > 0 && packet.type == Response_Ok) {
        std::cout << "일기 내용 \n" << packet.content << std::endl;
    }
    else {
        std::cout << "일기가 존재하지 않습니다." << std::endl;
    }
}

void cl_list_diary(SOCKET sock) {
    DiaryPacket packet;
    memset(&packet, 0, sizeof(packet));
    packet.type = List_Diary;

    std::cout << "목록 확인을 위한 연도와 월 입력 : ";
    std::cin >> packet.year >> packet.month;
    std::cin.ignore();

    send(sock, (char*)&packet, sizeof(packet), 0);
    int revSize = recv(sock, (char*)&packet, sizeof(packet), 0);
    if (revSize > 0 && packet.type == Response_Ok) {
        std::cout << '\n' << packet.content << std::endl;
    }
    else {
        std::cout << "\n해당 월에 작성된 일기가 없습니다" << std::endl;
    }
}

void cl_signup(SOCKET sock) {
    LoginPacket lp;
    memset(&lp, 0, sizeof(lp));
    lp.type = Signup_Request;
    std::cout << "\n--- 회원가입 ---" << std::endl;
    std::cout << "아이디 : "; std::cin >> lp.username;
    std::cout << "패스워드 : "; std::cin >> lp.password;
    send(sock, (char*)&lp, sizeof(lp), 0);
    recv(sock, (char*)&lp, sizeof(lp), 0);
    if (lp.type == Response_Ok) {
        std::cout << "회원가입 성공! 로그인 진행하기" << std::endl;
    }
    else {
        std::cout << "이미 존재하는 아이디이거나 가입에 실패했습니다. " << std::endl;
    }

}
void cl_login(SOCKET sock) {
    LoginPacket lp;
    memset(&lp, 0, sizeof(lp));
    lp.type = Login_Request;

    std::cout << "  로그인   " << std::endl;
    std::cout << "아이디 : "; std::cin >> lp.username;
    std::cout << "비밀번호 : "; std::cin >> lp.password;

    send(sock, (char*)&lp, sizeof(lp), 0);
    recv(sock, (char*)&lp, sizeof(lp), 0);

}
int main()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    bool is_loggedn_in = false;

    while (1) {

        std::cout << "--메뉴--" << std::endl;
        std::cout << "0.회원 가입" << std::endl;
        std::cout << "1. 일기 쓰기" << std::endl;
        std::cout << "2. 일기 확인" << std::endl;
        std::cout << "3. 일기 목록" << std::endl;
        std::cout << "4. 로그인" << std::endl;
        std::cout << "-1. 종료" << std::endl;
        std::cout << "선택 : ";
        int choice;


        DiaryPacket packet;
        memset(&packet, 0, sizeof(packet));

        

        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(INT_MAX, '\n');
            continue;
        }
        std::cin.ignore();
        if (choice == -1) break;

        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        
        if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            std::cout << "서버 접속 실패!에러코드: "<< err << std::endl;
            closesocket(sock);
            continue;
        }
        
        std::cout << "서버 접속 성공 !" << std::endl;
        switch (choice) {
            case Write_Diary:
                cl_write_diary(sock);
                std::cout << "저장 완료!\n";
                break;
            case Read_Diary:
                cl_read_diary(sock);
                break;
            case List_Diary:
                cl_list_diary(sock);
                break;
            case Signup_Request:
                cl_signup(sock);
                break;
            default:
                std::cout << "잘못된 선택" << std::endl;
                break;
        
        }
        closesocket(sock);
    }

    
    WSACleanup();

    return 0;
}

