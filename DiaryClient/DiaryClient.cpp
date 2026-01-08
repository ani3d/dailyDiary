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

void cl_update_diary(SOCKET sock){
    DiaryPacket packet;
    memset(&packet,0,sizeof(packet));
    packet.type=Update_Diary;

    std::cout<<"\n[일기 수정] 날짜 입력 (연 월 일): ";
    std::cin>>packet.year>>packet.month>>packet.day;
    std::cin.ignore();

    std::cout<<"새로운 내용 입력 : ";
    std::cin.getline(packet.content,2048);
    
    send(sock,(char*)&packet,sizeof(packet),0);
    std::cout<<"서버에 수정 요청 보냄"<<std::endl;
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


bool cl_login(SOCKET sock) {
    LoginPacket lp;
    memset(&lp, 0, sizeof(lp));
    lp.type = Login_Request;

    std::cout << "  로그인   " << std::endl;
    std::cout << "비밀번호 : "; std::cin >> lp.password;

    send(sock, (char*)&lp, sizeof(lp), 0);
    recv(sock, (char*)&lp, sizeof(lp), 0);

    if(lp.type==Response_Ok){
        std::cout<<"로그인 성공!\n";
        return true;
    }
    else{
        std::cout<<"로그인 실패 ! 비밀번호 확인 요망. "<<std::endl;
        return false;
    }
}

int main(){
    system("chcp 65001");
    std::setlocale(LC_ALL,"ko_KR.UTF-8");
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    bool is_logged_in = false;

    while (1) {
        if(!is_logged_in){
            std::cout<<"\n1.로그인 -1.종료 \n 선택 :";
            int choice;
            if (!(std::cin >> choice)) {
                std::cin.clear();
                std::cin.ignore(INT_MAX, '\n');
            
                continue;
            }
            std::cin.ignore();
            if (choice == -1) break;
            if(choice==1){
                SOCKET tempSock=socket(AF_INET,SOCK_STREAM,0);
                if(connect(tempSock,(sockaddr*)&serverAddr,sizeof(serverAddr))!=SOCKET_ERROR){
                    if(cl_login(tempSock)) is_logged_in=true;
                }
                closesocket(tempSock);
            }
        }

        else{
            int choice;
            std::cout<<"\n========[ 일기장 ]========\n";
            std::cout<<"1. 일기 작성\n2. 내 일기장 열기(조회/수정)\n3. 로그아웃\n-1. 종료\n입력 : ";
            std::cin>>choice;std::cin.ignore();
            if(choice==-1) break;
            if(choice==3){
                is_logged_in=false;continue;
            }
            SOCKET tempSock=socket(AF_INET,SOCK_STREAM,0);
            if(connect(tempSock,(sockaddr*)&serverAddr,sizeof(serverAddr))!=SOCKET_ERROR){
                if(choice==1){
                    cl_write_diary(tempSock);
                    std::cout<<"저장 완료\n";
                }
                else if(choice==2){
                    DiaryPacket packet;
                    memset(&packet,0,sizeof(packet));
                    packet.type=List_Diary;
                    std::cout<<"\n조회할 연도와 월 입력 : ";
                    if(!(std::cin>>packet.year>>packet.month)){
                        std::cout<<"잘못된 입력. 숫자만 입력하세요\n";
                        std::cin.clear();
                        std::cin.ignore(INT_MAX,'\n');
                        closesocket(tempSock);
                        continue;
                    }
                    std::cin.ignore();
                    
                    send(tempSock,(char*)&packet,sizeof(packet),0);
                    recv(tempSock,(char*)&packet,sizeof(packet),0);


                    if(packet.type==Response_Ok){
                        std::cout<<"\n--- 작성된 일기 목록 ---\n"<<packet.content<<'\n';
                        std::cout<<"상세 보기 또는 수정을 원하는 날짜 입력 : (취소 : 0) ";
                        int tDay;std::cin>>tDay;std::cin.ignore();

                        if(tDay>0){
                            std::cout<<"1. 읽기 2. 수정 (취소 : 0): ";
                            int act;std::cin>>act; std::cin.ignore();
                        

                        
                            closesocket(tempSock);
                            tempSock=socket(AF_INET,SOCK_STREAM,0);
                            connect(tempSock,(sockaddr*)&serverAddr,sizeof(serverAddr));
                            
                            if(act==1){
                                packet.type=Read_Diary; packet.day=tDay;
                                send(tempSock,(char*)&packet,sizeof(packet),0);
                                recv(tempSock,(char*)&packet,sizeof(packet),0);
                                std::cout<<"\n["<<tDay<<"일 내용]\n"<<packet.content<<std::endl;
                            }
                            else if(act==2){
                                packet.type=Update_Diary; packet.day=tDay;
                                std::cout<<"새로운 내용 입력 : ";
                                std::cin.getline(packet.content,2048);
                                std::cin.ignore();
                                send(tempSock,(char*)&packet,sizeof(packet),0);
                                std::cout<<"수정 및 업데이트 완료";

                            }
                        }
                        
                    }
                    else{
                        std::cout<<"해당 월 작성 일기 없음";
                    }
                }
            }
            closesocket(tempSock);
        }


    }

    WSACleanup();

    return 0;
}
    

