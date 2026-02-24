#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <thread>      // 멀티스레딩 필수
#include <chrono>
#include <string>
#include<fstream>
#include <filesystem>
#include "sqlite3.h"
#include "Packet.h"
//병합 성공여부 확인하기
#pragma comment(lib, "ws2_32.lib")

sqlite3* db;
namespace fs = std::filesystem;
void sv_init_db() {
	int rc = sqlite3_open("Diary.db", &db);
	if (rc != SQLITE_OK) {
		std::cerr << "[에러] DB를 열 수 없습니다 : " << sqlite3_errmsg(db) << std::endl;
		return;

	}

	const char* sql =
		"CREATE TABLE IF NOT EXISTS config("
		"key TEXT PRIMARY KEY, "
		"value TEXT);"
		
		"CREATE TABLE IF NOT EXISTS diary ("
		"year INTEGER, "
		"month INTEGER, "
		"day INTEGER, "
		"content TEXT,"
		"PRIMARY KEY(year, month, day));"

		"INSERT OR IGNORE INTO config(key, value) VALUES ('master_password', '1234');";
	char* errMsg = nullptr;

	rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);

	if (rc != SQLITE_OK) {
		std::cerr << "[에러] 테이블 생성 실패 : " << errMsg << std::endl;
		sqlite3_free(errMsg);
	}
	else {
		std::cout << "[서버] SQLite 데이터 베이스 준비 완료(Diary.db)" << std::endl;
	}

}

void sv_login(SOCKET client_sock, LoginPacket& lp) {
    sqlite3_stmt* stmt;
    // 아이디 상관없이 DB에 저장된 단 하나의 마스터 패스워드와 비교합니다.
    const char* sql = "SELECT value FROM config WHERE key = 'master_password' AND value = ?;";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, lp.password, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        lp.type = Response_Ok;
        std::cout << "[서버] 인증 성공" << std::endl;
    } else {
        lp.type = Response_Fail;
        std::cout << "[서버] 인증 실패 (잘못된 비밀번호)" << std::endl;
    }
    
    sqlite3_finalize(stmt);
    send(client_sock, (char*)&lp, sizeof(lp), 0);
}

void sv_save_to_txt(int y,int m,int d, const char* content){
	try{

		std::filesystem::create_directories("DiaryData");
		char dirPath[256];
		sprintf(dirPath,"DiaryData/Diary/%04d/%02d",y,m);
		fs::create_directories(dirPath);
	
		char fileName[128];
		sprintf(fileName,"%s/%02d.txt",dirPath,d);
		std::ofstream outFile(fileName,std::ios::binary|std::ios::trunc);
		if(outFile.is_open()){
			unsigned char bom[]={0xEF,0xBB,0xBF};
			outFile.write(reinterpret_cast<char*>(bom),3);
			outFile<<"---"<<y<<"년"<<m<<"월"<<d<<"일 의 기록 ---\n";
			outFile<<content;
			outFile.close();
			std::cout<<"[서버] 텍스트파일 저장/갱신 완료 : "<<fileName<<std::endl;
		}
	}catch(const std::exception& e){
		std::cerr << "[서버] 파일 저장 중 예외 발생 : "<<e.what()<<std::endl;
	}

}


void sv_write_diary(DiaryPacket& packet) {
	char* sql = sqlite3_mprintf(
		"INSERT OR REPLACE INTO diary(year, month, day, content) VALUES (%d, %d,%d,'%q');",
		packet.year, packet.month, packet.day, packet.content);

	char* errMsg = nullptr;

	int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);

	if (rc != SQLITE_OK) {
		std::cerr << "{서버} DB 저장 에러 : " << errMsg << std::endl;
		sqlite3_free(errMsg);
	}
	else {
		std::cout << "{서버} DB에 일기 저장 성공 ("
			<< packet.year << "/" << packet.month << "/" << packet.day << ")" << std::endl;
			sv_save_to_txt(packet.year,packet.month,packet.day,packet.content);
	}
	sqlite3_free(sql);

}
void sv_update_diary(DiaryPacket& packet){
	char * sql=sqlite3_mprintf(
		"UPDATE diary SET content = '%q' WHERE year = %d AND month = %d AND day = %d;",
		packet.content,packet.year,packet.month,packet.day);
	
	char *errMsg=nullptr;
	if(sqlite3_exec(db,sql,nullptr,nullptr,&errMsg)!=SQLITE_OK){
		std::cerr<<"[서버] SQL 수정 에러 : "<<errMsg<<std::endl;
		sqlite3_free(errMsg);

	}else{
		std::cout<<"[서버] DB 수정 성공. 텍스트 파일 갱신\n";
		sv_save_to_txt(packet.year,packet.month,packet.day,packet.content);
	}
	sqlite3_free(sql);
}



void sv_read_diary(SOCKET client_sock, DiaryPacket& packet) {
	sqlite3_stmt* stmt;

	const char* sql = "SELECT content FROM diary WHERE year = ? AND month = ? AND day = ?;";
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		std::cerr << "준비 실패 : " << sqlite3_errmsg(db) << std::endl;
		return;
	}
	sqlite3_bind_int(stmt, 1, packet.year);
	sqlite3_bind_int(stmt, 2, packet.month);
	sqlite3_bind_int(stmt, 3, packet.day);

	int rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		const unsigned char* content = sqlite3_column_text(stmt, 0);
		packet.type = Response_Ok;
		memset(packet.content, 0, sizeof(packet.content));
		strncpy_s(packet.content, (char*)content, sizeof(packet.content) - 1);
		std::cout << "{서버} DB에서 일기 검색 성공 : " << packet.year << "/" << packet.month << '/' << packet.day << std::endl;

	}
	else {
		packet.type = Response_Fail;
		std::cout << "{서버} 해당 날짜에 일기가 없음" << std::endl;
	}
	sqlite3_finalize(stmt);
	send(client_sock, (char*)&packet, sizeof(packet), 0);
}

void sv_diary_list(SOCKET client_sock, DiaryPacket& packet) {
	sqlite3_stmt* stmt;

	const char* sql = "SELECT day FROM diary WHERE year = ? AND month = ? ORDER BY day ASC;";

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		std::cerr << "목록 조회 준비 실패 : " << sqlite3_errmsg(db) << std::endl;
		return;
	}
	sqlite3_bind_int(stmt, 1, packet.year);
	sqlite3_bind_int(stmt, 2, packet.month);
	std::string listContent = "[" + std::to_string(packet.month) + "월 일기 목록 ] : ";

	bool found = false;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int day = sqlite3_column_int(stmt, 0);
		listContent += std::to_string(day) + "일, ";
		found = true;
	}

	if (found) {
		packet.type = Response_Ok;
		memset(packet.content, 0, sizeof(packet.content));
		strncpy_s(packet.content, listContent.c_str(), sizeof(packet.content) - 1);
	}
	else {
		packet.type = Response_Fail;
	}
	sqlite3_finalize(stmt);

	send(client_sock, (char*)&packet, sizeof(packet), 0);
	std::cout << "{서버} DB 목록 전송 완료 : " << packet.year << "/" << packet.month << std::endl;
	

}

void sv_handle_client(SOCKET client_sock) {
	DiaryPacket packet;
	int revSize = recv(client_sock, (char*)&packet, sizeof(packet), 0);
	
	if (revSize <= 0) {
		closesocket(client_sock);
		return;

	}
	

	switch (packet.type) {
		case Login_Request:
			sv_login(client_sock,*(LoginPacket*)&packet);
			break;
		case Write_Diary:
			sv_write_diary(packet);
			break;

		case Read_Diary:
			sv_read_diary(client_sock,packet);
			break;

		case List_Diary:
			sv_diary_list(client_sock,packet);
			break;
		
		case Update_Diary:
			sv_update_diary(packet);
			break;
		
		
	}
	closesocket(client_sock);

}

void run_server_bground(){
    sv_init_db();

    SOCKET listen_sock=socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in serverAddr={};
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_port=htons(9000);
    serverAddr.sin_addr.s_addr=htonl(INADDR_ANY);

    bind(listen_sock,(sockaddr*)&serverAddr,sizeof(serverAddr));
    listen(listen_sock,5);
    while(true){
        sockaddr_in clientAddr;
        int addrLen=sizeof(clientAddr);
        SOCKET client_sock=accept(listen_sock,(sockaddr*)&clientAddr,&addrLen);
        if(client_sock!=INVALID_SOCKET){
            sv_handle_client(client_sock);
        }
    }
}



//----------------클라이언트 로직 시작

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

void run_client_ui(sockaddr_in serverAddr) {
    bool is_logged_in = false;
    
    while (true) {
        if (!is_logged_in) {
            std::cout << "\n1.로그인 -1.종료\n 선택 : ";
            int choice;
            if (!(std::cin >> choice)) {
                std::cin.clear();
                std::cin.ignore(INT_MAX, '\n');
                continue;
            }
            std::cin.ignore();

            if (choice == -1) exit(0);
            if (choice == 1) {
                SOCKET tempSock = socket(AF_INET, SOCK_STREAM, 0);
                if (connect(tempSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) != SOCKET_ERROR) {
                    if (cl_login(tempSock)) is_logged_in = true;
                }
                closesocket(tempSock);
            }
        } 
        else {
            // 로그인 된 상태의 메인 메뉴
            int choice;
            std::cout << "\n========[ 일기장 ]========\n";
            std::cout << "1. 일기 작성\n2. 내 일기장 열기(조회/수정)\n3. 로그아웃\n-1. 종료\n입력 : ";
            std::cin >> choice; std::cin.ignore();

            if (choice == -1) break;
            if (choice == 3) { is_logged_in = false; continue; }

            SOCKET tempSock = socket(AF_INET, SOCK_STREAM, 0);
            if (connect(tempSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) != SOCKET_ERROR) {
                if (choice == 1) {
                    cl_write_diary(tempSock);
                    std::cout << ">> 저장 완료\n";
                } 
                else if (choice == 2) {
                    DiaryPacket packet;
                    memset(&packet, 0, sizeof(packet));
                    packet.type = List_Diary;

                    std::cout << "\n조회할 연도와 월 입력 : ";
                    if (!(std::cin >> packet.year >> packet.month)) {
                        std::cout << "잘못된 입력. 숫자만 입력하세요\n";
                        std::cin.clear();
                        std::cin.ignore(INT_MAX, '\n');
                        closesocket(tempSock);
                        continue;
                    }
                    std::cin.ignore();

                    send(tempSock, (char*)&packet, sizeof(packet), 0);
                    recv(tempSock, (char*)&packet, sizeof(packet), 0);

                    if (packet.type == Response_Ok) {
                        std::cout << "\n--- 작성된 일기 목록 ---\n" << packet.content << '\n';
                        std::cout << "상세 보기 또는 수정을 원하는 날짜 입력 (취소 : 0): ";
                        int tDay; std::cin >> tDay; std::cin.ignore();

                        if (tDay > 0) {
                            std::cout << "1. 읽기 2. 수정 (취소 : 0): ";
                            int act; std::cin >> act; std::cin.ignore();

                            // 서버는 1회 통신 후 소켓을 닫으므로 재접속
                            closesocket(tempSock);
                            tempSock = socket(AF_INET, SOCK_STREAM, 0);
                            connect(tempSock, (sockaddr*)&serverAddr, sizeof(serverAddr));

                            if (act == 1) {
                                packet.type = Read_Diary; packet.day = tDay;
                                send(tempSock, (char*)&packet, sizeof(packet), 0);
                                recv(tempSock, (char*)&packet, sizeof(packet), 0);
                                std::cout << "\n[" << tDay << "일 내용]\n" << packet.content << std::endl;
                            } 
                            else if (act == 2) {
                                packet.type = Update_Diary; packet.day = tDay;
                                std::cout << "새로운 내용 입력 : ";
                                std::cin.getline(packet.content, 2048);
                                // ★ 주의: getline은 엔터까지 가져오므로 여기서 cin.ignore()를 하면 안 됩니다!
                                send(tempSock, (char*)&packet, sizeof(packet), 0);
                                std::cout << ">> 수정 및 업데이트 완료\n";
                            }
                        }
                    } 
                    else {
                        std::cout << ">> 해당 월에 작성된 일기가 없습니다.\n";
                    }
                }
            }
            closesocket(tempSock); // 모든 choice 처리가 끝나고 소켓 닫기
        }
    }
}


int main(){
    system("chcp 65001");
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2),&wsaData);

    std::thread server_thread(run_server_bground);
    server_thread.detach();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    sockaddr_in serverAddr={};
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_port=htons(9000);
    inet_pton(AF_INET,"127.0.0.1",&serverAddr.sin_addr);

    run_client_ui(serverAddr);
    WSACleanup();

    return 0;

}
