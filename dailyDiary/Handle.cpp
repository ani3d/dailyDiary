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
		"CREATE TABLE IF NOT EXISTS users("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"username TEXT NOT NULL UNIQUE,"
		"password TEXT NOT NULL);"
		
		"CREATE TABLE IF NOT EXISTS diary ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"year INTEGER, "
		"month INTEGER, "
		"day INTEGER, "
		"content TEXT);";

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

	const char* sql = "SELECT id FROM users WHERE username = ? AND password = ?;";
	sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	sqlite3_bind_text(stmt, 1, lp.username, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, lp.password, -1, SQLITE_STATIC);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		lp.type = Response_Ok;
		std::cout << "[서버] 로그인 성공 : " << lp.username << std::endl;
	}
	else {
		lp.type = Response_Fail;
		std::cout << "[서버] 로그인 실패 : " << lp.username << std::endl;

	}
	sqlite3_finalize(stmt);
	send(client_sock, (char*)&lp, sizeof(lp), 0);
}

void sv_signup(SOCKET client_sock, LoginPacket &lp) {
	sqlite3_stmt* stmt;
	const char* sql = "INSERT INTO users(username,password) VALUES (?, ?);";

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		std::cerr << "[서버] 가입 준비 실패 : " << sqlite3_errmsg(db) << std::endl;
		return;
	}
	sqlite3_bind_text(stmt, 1, lp.username, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, lp.password, -1, SQLITE_STATIC);

	int rc = sqlite3_step(stmt);

	if (rc == SQLITE_DONE) {
		lp.type = Response_Ok;
		std::cout << "[서버] 회원 가입 성공 : " << lp.username << std::endl;

	}
	else {
		lp.type = Response_Fail;
		std::cout << "[서버] 회원 가입 실패 : " << sqlite3_errmsg(db) << std::endl;
	}
	sqlite3_finalize(stmt);
	send(client_sock, (char*)&lp, sizeof(lp), 0);
}

void sv_write_diary(DiaryPacket& packet) {
	char* sql = sqlite3_mprintf(
		"INSERT INTO diary(year, month, day, content) VALUES (%d, %d,%d,'%q');",
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
		case Write_Diary:
			sv_write_diary(packet);
			break;

		case Read_Diary:
			sv_read_diary(client_sock, packet);
			break;

		case List_Diary:
			sv_diary_list(client_sock, packet);
			break;
	}
	closesocket(client_sock);

}