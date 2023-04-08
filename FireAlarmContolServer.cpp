#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <iostream>
#include <time.h>
#include <string.h>
#include <sstream>
#include <mutex>
#include <windows.h>
#include "PLCSignals.h"
#include "sqlite3.h"
#pragma comment(lib, "ws2_32.lib")
//g++ *.cpp sqlite3.o -lws2_32 -o FireAlarmControlServer.exe

HANDLE dbmutex; // дескриптор мьютекса WinAPI

std::mutex sendmtx;

union PLCMessageRecv // Контейнер для приёма сигнала из контроллера
{
	char msg[sizeof(PLCSignals)];
	PLCSignals plc1Signals;
};

union PLCMwssageSend // Контейнер для передачм сигнала в контроллер
{
	char pmsg[sizeof(short)];
	unsigned short t;
};

PLCMessageRecv plcMsgR;
PLCMwssageSend plcMsgS;

SOCKET newSConnection; // Сокет подключения к контроллеру по TCP/IP

sqlite3 *db = 0;
char *err = 0;
char *err2 = 0;
char *query;
char *resupdate;
char *comquery;

bool sgnval;
bool isUser = false;

bool sendException = false;
bool recvException = false;
bool dbUpdateResException = false;
bool dbPrepareException = false;
bool dbIsUserException = false;
bool working = true;
//int stopper = 5000;
unsigned short oldBooleanValues, booleanValues, sendBooleanValues;

// Получение данных из базы
static int callback(void *data, int argc, char **argv, char **azColName)
{
	std::stringstream(argv[0]) >> sgnval;
	return 0;
}
// Получение автора записи из базы
static int callbackSender(void *data, int argc, char **argv, char **azColName)
{
	std::string sender;
	std::stringstream(argv[0]) >> sender;
	isUser = sender == "user";
	return 0;
}

// Поток отправки сообщения в контроллер
void sendMsgToPLC()
{
	while (working)
	{
		try
		{
			sendmtx.lock();
			plcMsgS.t = booleanValues;
			if(send(newSConnection, plcMsgS.pmsg, sizeof(short), NULL)== 0) throw 1;
			sendmtx.unlock();
			sendException = false;
		}
		catch(...)
		{
			sendException = true;
		}
		Sleep(250);
	}	
	if (setBitInPlcSgn(&booleanValues, 15))
	{
		printf("%s\n", "Sending cmd message");
		try
		{
			plcMsgS.t = booleanValues;
			if(send(newSConnection, plcMsgS.pmsg, sizeof(short), NULL)== 0) throw 1;
		}
		catch(...)
		{
			sendException = true;
		}
		Sleep(250);
	}
	else printf("%s\n", "Not sending cmd message");
}

// Поток получения сообщения из контроллера
void recvMsgFromPLC()
{
	while (working)
	{
		try
		{
			if (recv(newSConnection, plcMsgR.msg, sizeof(PLCSignals), NULL) != 0) 
			{
				sendmtx.lock();
				recvException = false;
				for (int i = 0; i < 16; i++)
				{
					if (getBitFromPlcSgn(plcMsgR.plc1Signals.booleanValues, i) != getBitFromPlcSgn(oldBooleanValues, i))
					{
						if(getBitFromPlcSgn(plcMsgR.plc1Signals.booleanValues, i)) setBitInPlcSgn(&booleanValues, i);
						else resetBitInPlcSgn(&booleanValues, i);
					}
				}
				sendmtx.unlock();
			}
			else throw 1;
		}
		catch(...)
		{
			recvException = true;
		}		
		Sleep(200);
		oldBooleanValues = booleanValues;
	}	
}

// Поток работы с базой данных
void insertDataInDB()
{
	while(working)
	{	unsigned short prevVals = oldBooleanValues;
		unsigned short nowVals = booleanValues;
		int ns1 = plcMsgR.plc1Signals.waterLevel;
		int ns2 = plcMsgR.plc1Signals.temperLevel;
		int ns3 = plcMsgR.plc1Signals.gasLevel;
		dbmutex = CreateMutex(NULL, FALSE, "dbmutex");
		WaitForSingleObject(dbmutex, INFINITE);
		sqlite3_open("LiftProtect.db", &db);
		dbPrepareException = false;
		dbUpdateResException = false;
		dbIsUserException = false;
		int sqlstatus;
		for (int i = 0; i < 11; i++)
		{
			std::string machine;
			// Подготовка запроса для записи
			bool matched = false;
			sqlite3_free(err);
			int res;
			time_t rawtime;
 			struct tm * timeinfo;
 			time ( &rawtime );
 			timeinfo = localtime ( &rawtime );
			std::string querybeg = "INSERT INTO SrabTable (MachineName, SrabSwitch, SrabDate, Sender) VALUES (";
			std::string valFromPlc = getBitFromPlcSgn(nowVals, i) ? "1, " : "0, ";
			std::string tim = asctime(timeinfo);
			std::string timt = "'" + tim + "'";
			std::string queryend =", 'server');";
			std::string s1 = "select srabswitch from srabtable where id = (select max(id) from srabtable where sender = 'user' and machinename = ";
			std::string s2 = "select sender from srabtable where id = (select max(id) from srabtable where machinename = ";
			std::string s3 = "select srabswitch from srabtable where id = (select max(id) from srabtable where machinename = ";
			std::string s4 = ");";
			std::string selectquery1;
			std::string selectquery2;
			try
			{
				switch(i)
				{
					case 0 :
						dbPrepareException = false;
						machine = "'ПИ-1'";
						break;
					case 1 :
						dbPrepareException = false;
						machine = "'ПИ-2'";
						break;
					case 2 :
						dbPrepareException = false;
						machine = "'СПДП'";
						break;
					case 3 :
						dbPrepareException = false;
						machine = "'СПДГ'";
						break;
					case 4 :
						dbPrepareException = false;
						machine = "'КП-1'";
						break;
					case 5 :
						dbPrepareException = false;
						machine = "'КП-2'";
						break;
					case 6 :
						dbPrepareException = false;
						machine = "'ВП'";
						break;
					case 7 :
						dbPrepareException = false;
						machine = "'ВВ'";
						break;
					case 8 :
						dbPrepareException = false;
						machine = "'БДП'";
						break;
					case 9 :
						dbPrepareException = false;
						machine = "'БДГ'";
						break;
					case 10 :
						dbPrepareException = false;
						machine = "'НВО'";
						break;
					break;
				}

				selectquery1 = s2 + machine + s4;
				comquery = const_cast<char*>(selectquery1.c_str());
				if (sqlite3_exec(db, comquery, callbackSender, NULL, &err) != SQLITE_OK) throw 2;

				selectquery1 = s3 + machine + s4;		
				std::string qr = querybeg + machine + ", " + valFromPlc + timt + queryend;
				query = const_cast<char*>(qr.c_str());

				sqlite3_exec(db, selectquery1.c_str(), callback, NULL, &err);

				if((getBitFromPlcSgn(nowVals, i) != sgnval) && (isUser == false))
				{ 
					sqlstatus = sqlite3_exec(db, query, NULL, NULL, &err);
					if(sqlstatus != SQLITE_OK) throw 1;	
				}

				if(isUser = true)
				{
					selectquery2 = s1 + machine + s4;
					comquery = const_cast<char*>(selectquery2.c_str());
					sqlstatus = sqlite3_exec(db, comquery, callback, NULL, &err);
					sendmtx.lock();
					if(sgnval) setBitInPlcSgn(&booleanValues, i);
					else resetBitInPlcSgn(&booleanValues, i);
					sendmtx.unlock();
					isUser = false;
					if(sqlstatus != SQLITE_OK) throw 2;
				}
				printf(isUser ? "isUser = true " : "-");
			}
			catch(int a)
			{
				if (a == 1)
				dbPrepareException = true;
			
				if (a == 2)
				dbIsUserException = true;
			}
		}
		try
		{
			std::string sbeg = "update ResTable set waterLevel = ";
			std::string s11 =  ", temperLevel = "; 
			std::string s12 = ", gasLevel = "; 
			std::string s13 = " where id = 1;";
			std::string n1 = std::to_string(ns1);
			std::string n2 = std::to_string(ns2);
			std::string n3 = std::to_string(ns3);

			std::string querystring = sbeg + n1 + s11 + n2 + s12 + n3 + s13;

			resupdate = const_cast<char*>(querystring.c_str());

			sqlstatus = sqlite3_exec(db, resupdate, NULL, NULL, &err2);

			if(sqlstatus != SQLITE_OK) throw 1;
		}
		catch(int a)
		{
			dbUpdateResException = true;
		}
		
		sqlite3_close(db);
		ReleaseMutex(dbmutex);
		Sleep(500);
	}
}

// Основной поток выполнения программы
int main(int argc, char **argv)
{
	// Инициализация
	
	if( sqlite3_open("LiftProtect.db", &db) )
	fprintf(stderr, "Data base connection failed!\n", sqlite3_errmsg(db));
	sqlite3_close(db);
	Sleep(50);
	printf("DB connected\n");
	WSAData wsdata;
	WORD DLLVersion = MAKEWORD(2, 1);
	if(WSAStartup(DLLVersion, &wsdata) != 0)
	{
		fprintf(stderr, "Loading library failed!\n", err);
		exit(1);
	}
	else printf("%s\n", "Loading library success!");
	SOCKADDR_IN addrs;
	addrs.sin_addr.s_addr = inet_addr("127.0.0.1");
	addrs.sin_port = htons(1111);
	addrs.sin_family = AF_INET;
	int sizeofaddrs = sizeof(addrs);

    printf("%s\n", "Socket boot success");
	SOCKET sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	bind(sListen, (SOCKADDR*)&addrs, sizeofaddrs);
	listen(sListen, SOMAXCONN);

	printf("%s\n", "Listening...");
	// Подключение к контроллеру
	newSConnection = accept(sListen, (SOCKADDR*)&addrs, &sizeofaddrs);

	if(newSConnection == 0)
	{
		printf("%s\n", "No connections");
		while (accept(sListen, (SOCKADDR*)&addrs, &sizeofaddrs) == 0)
		{
			printf("%s\n", "No connections");
			listen(sListen, SOMAXCONN);
			Sleep(2000);
		}	
	}
	else
	{
		// Запуск потоков
		printf("%s\n", "Client connected");
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)sendMsgToPLC, NULL, NULL, NULL);
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)recvMsgFromPLC, NULL, NULL, NULL);
		printf("%s\n", "Send/recive threads created");
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)insertDataInDB, NULL, NULL, NULL);
		printf("%s\n", "DB threads created");
		// Рабочий цикл программы
		while(working)
		{
			// Вывод системных данных в консоль
			system("cls");
			time_t rawtime;
 			struct tm * timeinfo;
 			time ( &rawtime );
 			timeinfo = localtime ( &rawtime );
 			printf("Datetime %s", asctime (timeinfo));
			printf("PLC Data:\n");
			printf("%s%d\n", "Water level = ", plcMsgR.plc1Signals.waterLevel);
			printf("%s%d\n", "Temprature level = ", plcMsgR.plc1Signals.temperLevel);
			printf("%s%d/%d\n", "Boolean Values Array = ", plcMsgR.plc1Signals.booleanValues, plcMsgS.t);
			printf("%s%d\n", "Gas level = ", plcMsgR.plc1Signals.gasLevel);
			printf("%s%d/%d\n", "Fire Alarm 1 = ", getBitFromPlcSgn(booleanValues, FA_1_NUM), getBitFromPlcSgn(oldBooleanValues, FA_1_NUM));
			printf("%s%d/%d\n", "Fire Alarm 2 = ", getBitFromPlcSgn(booleanValues, FA_2_NUM), getBitFromPlcSgn(oldBooleanValues, FA_2_NUM));
			printf("%s%d/%d\n", "SPDP = ", getBitFromPlcSgn(booleanValues, SPDP_NUM), getBitFromPlcSgn(oldBooleanValues, SPDP_NUM));
			printf("%s%d/%d\n", "CPDG = ", getBitFromPlcSgn(booleanValues, CPDG_NUM), getBitFromPlcSgn(oldBooleanValues, CPDG_NUM));
			printf("%s%d/%d\n", "KP 1 = ", getBitFromPlcSgn(booleanValues, KP_1_NUM), getBitFromPlcSgn(oldBooleanValues, KP_1_NUM));
			printf("%s%d/%d\n", "KP 2 = ", getBitFromPlcSgn(booleanValues, KP_2_NUM), getBitFromPlcSgn(oldBooleanValues, KP_2_NUM));
			printf("%s%d/%d\n", "VP = ", getBitFromPlcSgn(booleanValues, VP_NUM), getBitFromPlcSgn(oldBooleanValues, VP_NUM));
			printf("%s%d/%d\n", "VV = ", getBitFromPlcSgn(booleanValues, VV_NUM), getBitFromPlcSgn(oldBooleanValues, VV_NUM));
			printf("%s%d/%d\n", "BDP = ", getBitFromPlcSgn(booleanValues, BDP_NUM), getBitFromPlcSgn(oldBooleanValues, BDP_NUM));
			printf("%s%d/%d\n", "BDG = ", getBitFromPlcSgn(booleanValues, BDG_NUM), getBitFromPlcSgn(oldBooleanValues, BDG_NUM));
			printf("%s%d/%d\n", "NVO = ", getBitFromPlcSgn(booleanValues, NVO_NUM), getBitFromPlcSgn(oldBooleanValues, NVO_NUM));

			if(sendException)printf("Exception on sending data\n");
			if(recvException)printf("Exception on receiving data\n");

			if(dbPrepareException)printf("Incertion is not executed:\n%s\n%s\n", query, err);
			if(dbUpdateResException)printf("Resupdate is not executed:\n%s\n%s\n", resupdate, err2);
			if(dbIsUserException)printf("Sender status getting exeption:\n%s\n%s\n", comquery, err);
			//if (--stopper == 0) working = false;

			Sleep(1000);
		}
	}

	system("pause");
	sqlite3_close(db);
	WSACleanup();
	return 0;
}