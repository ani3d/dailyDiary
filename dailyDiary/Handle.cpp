#include<winsock2.h>
#include<iostream>
#include<fstream>
#include<string>
#include "Packet.h"
#include <filesystem>
#include<chrono>
#include <ctime>
#include "sqlite3.h"


sqlite3* db = nullptr;
namespace fs = std::filesystem;

#pragma comment(lib,"ws2_32.lib")

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
		"UPDATE diary SET content = '%q' WHERE year = & AND month = %d AND day = %d;",
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