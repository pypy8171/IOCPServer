#pragma once;
//SQLConnect()
#include "extern.h"
#include "ViewProcessing.h"
#include "PacketHandler.h"
#include "DBHandler.h"

#include <algorithm>
#include <random>

#include <thread>


SECTOR g_sectors[MAX_ROW][MAX_COL];
CLIENT g_clients[MAX_USER + MAX_NPC];
unordered_map<int, CLIENT*> g_mapCL;
CTile g_tiles[MAX_TILE_ROW][MAX_TILE_COL];

priority_queue<Event_Type*, vector<Event_Type*>, Compare> timer_queue;
mutex timer_lock;

HANDLE g_iocp;
SOCKET l_socket;

int g_curUser = 0;

SQLHENV henv;
SQLHDBC hdbc;
SQLHSTMT hstmt = 0;
SQLRETURN retcode;
SQLWCHAR dUser_name[MAX_NAME_LEN];
SQLINTEGER dUser_id, dUser_Level, dUser_posx, dUser_posy, dUser_hp;
SQLLEN cbName = 0, cbID = 0, cbLevel = 0, cbPosx, cbPosy, cbHp;

void initialize_tiles();
void initialize_clients();
void initialize_npcs();
void initialize_sectors();
void initialize_db();

void worker_thread();

void show_error() {
	printf("error\n");
}

void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
	SQLSMALLINT iRec = 0;
	SQLINTEGER iError;
	WCHAR wszMessage[1000];
	WCHAR wszState[SQL_SQLSTATE_SIZE + 1];
	if (RetCode == SQL_INVALID_HANDLE) {
		fwprintf(stderr, L"Invalid handle!\n");
		return;
	}
	while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS) {
		// Hide data truncated..
		if (wcsncmp(wszState, L"01004", 5)) {
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
	}
}

void addtimer(int id, ENUMOP op, unsigned int timer) // 이벤트 new delete 안하고 재사용 하는것이 얼마나 성능이 좋아지는지 확인해야함
{
	g_clients[id].m_event.obj_id = id;
	g_clients[id].m_event.event_id = op;
	g_clients[id].m_event.wakeup_time = high_resolution_clock::now() + milliseconds(timer);

	timer_lock.lock();	
	timer_queue.emplace(&g_clients[id].m_event);
	timer_lock.unlock();
}


int API_send_message(lua_State* L)
{
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	char* message = (char*)lua_tostring(L, -1);

	g_clients[my_id].m_event.target_id = user_id;

	CPacketHandler::GetInst()->send_chat_packet(user_id, my_id, message);
	lua_pop(L, 3);
	return 0;
}

int API_get_x(lua_State* L)
{
	int obj_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = g_clients[obj_id].x;
	lua_pushnumber(L, x);
	return 1;
}

int API_get_y(lua_State* L)
{
	int obj_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = g_clients[obj_id].y;
	lua_pushnumber(L, y);
	return 1;
}

int API_get_move_num(lua_State* L)
{
	int obj_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);
	lua_pushnumber(L, g_clients[obj_id].m_movenum);
	return 1;
}

int API_set_move_num(lua_State* L)
{
	int obj_id = (int)lua_tointeger(L, -2);
	int move_num = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	g_clients[obj_id].m_movenum = move_num;
	lua_pushnumber(L, move_num);
	return 1;
}

int API_random_move(lua_State* L)
{
	int obj_id = (int)lua_tointeger(L, -2);
	ENUMOP event_type = (ENUMOP)lua_tointeger(L, -1);
	lua_pop(L, 2);

	CPacketHandler::GetInst()->npc_move(obj_id, event_type); // a*는 여기서 돌아간다. // 여기서 key는 npc id

	return 1;
}


void initialize_db()
{
	CDBHandler::GetInst()->init();
}

void initialize_tiles()
{
	cout << "start tiles initialize" << endl;
	vector<short> v1;
	vector<short> v2;

	for (short i = 0; i < WORLD_WIDTH; ++i)
	{
		v1.emplace_back(i);
		v2.emplace_back(i);
	}

	random_device rd;
	mt19937 g(rd());
	shuffle(v1.begin(), v1.end(), g);
	shuffle(v2.begin(), v2.end(), g);

	for (int i = 0; i < MAX_TILE_COL; ++i)
	{
		for (int j = 0; j < MAX_TILE_ROW; ++j)
		{
			g_tiles[j][i].m_id = i * MAX_TILE_COL + j;
			g_tiles[j][i].m_x = v1[i];
			g_tiles[j][i].m_y = v2[j];
			g_tiles[j][i].m_TileType = TILE_TYPE::OBSTACLE;
		}
	}
	cout << "finish tiles initialize" << endl << endl;
}

void initialize_clients() // 멀티 스레드 이전 싱글 스레드에서 돌아감. 뮤텍스 필요 없음
{
	cout << "start clients initialize" << endl;
	for (int i = 0; i < MAX_USER; ++i)
	{
		g_clients[i].m_id = i;
		g_clients[i].m_status = ST_FREE;
		InitializeSRWLock(&g_clients[i].m_viewlist.rwlock);
	}
	cout << "end clients initialize" << endl<<endl;
}

void initialize_sectors()
{
	cout << "start sectors initialize" << endl;
	for (int i = 0; i < MAX_COL; ++i)
	{
		for (int j = 0; j < MAX_ROW; ++j)
		{
			g_sectors[j][i].m_StartX = i * COL_GAP;
			g_sectors[j][i].m_StartY = j * ROW_GAP;
			g_sectors[j][i].m_EndX = (i + 1) * COL_GAP;
			g_sectors[j][i].m_EndY = (j + 1) * ROW_GAP;
		}
	}
	cout << "end sectors initialize" << endl << endl;
}

void initialize_npcs()
{
	cout << "start npc initialize" << endl;
	for (int i = NPC_START_IDX; i < MAX_NPC + NPC_START_IDX; ++i)
	{
		g_clients[i].m_socket = 0;
		g_clients[i].m_id = i;
		wprintf_s(g_clients[i].name, "NPC%d", i);
		g_clients[i].m_status = ST_SLEEP;
		g_clients[i].x = rand() % WORLD_WIDTH;
		g_clients[i].y = rand() % WORLD_HEIGHT;
		g_clients[i].row = g_clients[i].y / ROW_GAP;
		g_clients[i].col = g_clients[i].x / COL_GAP;

		g_clients[i].m_otype = O_NPC;

		short iRow = g_clients[i].row;
		short iCol = g_clients[i].col;

		g_sectors[iRow][iCol].m_setPlayerList.emplace(i); // npc 삽입

		lua_State* L = g_clients[i].L = luaL_newstate();
		luaL_openlibs(L);
		luaL_loadfile(L, "NPC.LUA");
		lua_pcall(L, 0, 0, 0);
		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		lua_pcall(L, 1, 0, 0); // L, 파라미터 개수, 리턴 개수, ,
		lua_pop(L, 1); // getglobal한거 날려줌

		lua_register(L, "API_send_message", API_send_message);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
		lua_register(L, "API_set_move_num", API_set_move_num);
		lua_register(L, "API_get_move_num", API_get_move_num);
		lua_register(L, "API_random_move", API_random_move);
	}
	cout << "finish npc initialize" << endl;
}

void timer() // 스레드 
{
	while (true)
	{
		this_thread::sleep_for(1ms);// Sleep 은 윈도우에서만 가능한거임.
		while (true) {
			timer_lock.lock();
			if (timer_queue.empty())
			{
				timer_lock.unlock();
				break;
			}

			if (timer_queue.top()->wakeup_time > high_resolution_clock::now() )
			{
				timer_lock.unlock();
				break;
			}
			Event_Type* tEvent = timer_queue.top();
			timer_queue.pop();
			timer_lock.unlock();

			EXOVER* exover = new EXOVER;
			exover->op = OP_RANDOM_MOVE;
			exover->player_id = tEvent->obj_id;
			PostQueuedCompletionStatus(g_iocp, 1, exover->player_id, &exover->over);
		
		}
	}
}

void worker_thread()
{
	while (true)
	{
		DWORD io_byte;
		ULONG_PTR key;
		WSAOVERLAPPED* over;
		GetQueuedCompletionStatus(g_iocp, &io_byte, &key, &over, INFINITE); // accept와 같이 블로킹 -> acceptex사용
		// acceptex하면 달라지는게 뭐지 -> 강의 다시 보기

		EXOVER* exover = reinterpret_cast<EXOVER*>(over);
		int user_id = static_cast<int>(key);

		CLIENT& cl = g_clients[user_id];

		switch (exover->op)
		{
		case OP_PLAYER_MOVE: // 이런 이벤트만 처리하게 묶기 // send recv accept eventprocess 이렇게만.
		{
			g_clients[user_id].lua_l.lock(); // npc id
			lua_State* L = g_clients[user_id].L;
			lua_getglobal(L, "event_player_move");
			lua_pushnumber(L, exover->player_id);  // client id
			int error = lua_pcall(L, 1, 0, 0);
			if (error) cout << lua_tostring(L, -1)<<endl;
			g_clients[user_id].lua_l.unlock();
			delete exover;
		}
		break;
		case OP_RANDOM_MOVE_FINISH:
		{
			g_clients[user_id].lua_l.lock();
			lua_State* L = g_clients[user_id].L;
			lua_getglobal(L, "event_ai_finish");
			lua_pushnumber(L, exover->player_id);
			lua_pushnumber(L, g_clients[user_id].m_event.target_id);
			int error = lua_pcall(L, 2, 0, 0);
			if (error) cout << lua_tostring(L, -1) << endl;
			g_clients[user_id].lua_l.unlock();
			delete exover;
		}
		break;
		case OP_RANDOM_MOVE:
		{
			g_clients[user_id].lua_l.lock();
			lua_State* L = g_clients[user_id].L;
			lua_getglobal(L, "event_ai_move");
			lua_pushnumber(L, exover->player_id);
			lua_pushnumber(L, exover->op);
			int error = lua_pcall(L, 2, 0, 0);
			if (error) cout << lua_tostring(L, -1) << endl;
			g_clients[user_id].lua_l.unlock();
			//delete exover;
		}
		break;
		case OP_RECV:
		{
			if (0 == io_byte)
				CPacketHandler::GetInst()->disconnect(user_id);
			else
			{
				CPacketHandler::GetInst()->recv_packet_construct(user_id, io_byte);
				//process_packet(user_id, exover->io_buf);
				ZeroMemory(&cl.m_recv_over.over, sizeof(cl.m_recv_over.op));
				DWORD flags = 0;
				WSARecv(cl.m_socket, &cl.m_recv_over.wsabuf, 1, NULL, &flags, &cl.m_recv_over.over, NULL);
			}
		}
		break;
		case OP_SEND:
			if (0 == io_byte)
				CPacketHandler::GetInst()->disconnect(user_id);
			else
			{
				delete exover;
			}
			break;
		case OP_ACCEPT: // 비동기 accept 하나만 실행. main에서 비동기 accept 하고 worker thread에서 완료를 하고 // 4.28 강의 두번째 3분
						// 완료한 thread에서 accept할때까지 다른 thread는 완료 안됨. 따라서 싱글스레드로 진행이됨.
		{
			int user_id = -1;
			for (int i = 0; i < MAX_USER; ++i)
			{
				lock_guard<mutex> gl{ g_clients[i].m_cl }; // unlcok 이 필요 없다. 블록에서 빠져나갈때 unlock 을 해줌, 루프를 매번 해줄때 unlock을 해주고 lock을 건다.
				if (ST_FREE == g_clients[i].m_status) //  template 객체. 생성자 소멸자 호출할때 lock unlock 함. 함수 or 블록 부분에 사용하면 유용하다
				{
					int cur = i;

					g_clients[i].m_status = ST_ALLOC;
					user_id = i;

					if(g_curUser <= cur)
						++g_curUser;
					break;
				}
			}

			SOCKET	c_socket = exover->c_socket;

			if (-1 == user_id) //send_login_fail_packet;
				closesocket(exover->c_socket);
			else
			{
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(c_socket), g_iocp, user_id, 0);

				CLIENT& NewClient = g_clients[user_id];
				//wcscpy_s(NewClient.name, reinterpret_cast<WCHAR*>(dUser_name));
				NewClient.m_id = user_id;
				NewClient.m_prev_size = 0;
				NewClient.m_recv_over.op = OP_RECV;
				ZeroMemory(&NewClient.m_recv_over.over, sizeof(NewClient.m_recv_over.op));
				NewClient.m_recv_over.wsabuf.buf = NewClient.m_recv_over.io_buf;
				NewClient.m_recv_over.wsabuf.len = MAX_BUF_SIZE;
				NewClient.m_socket = c_socket;
				NewClient.m_viewlist.viewlist.clear();
				NewClient.x = rand() % WORLD_WIDTH;
				NewClient.y = rand() % WORLD_WIDTH;
				//NewClient.level = dUser_Level;
				//NewClient.hp = dUser_hp;

				NewClient.m_otype = O_PLAYER;

				DWORD flags = 0;
				WSARecv(c_socket, &NewClient.m_recv_over.wsabuf, 1, NULL, &flags, &NewClient.m_recv_over.over, NULL);
				
				//// sector에 집어 넣어준다.
				short col = NewClient.x / COL_GAP;
				short row = NewClient.y / ROW_GAP;
				g_sectors[row][col].m_setPlayerList.emplace(NewClient.m_id);
				NewClient.row = row;
				NewClient.col = col;

				//g_mapCL.emplace(make_pair(NewClient.m_id, &NewClient));
			}

			c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

			exover->c_socket = c_socket;
			ZeroMemory(&exover->over, sizeof(exover->over));
			AcceptEx(l_socket, exover->c_socket, exover->io_buf, NULL,
				sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &exover->over);

		}
		break;
		}
	}
}

int main()
{
	wcout.imbue(locale("korean"));

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	l_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN s_address; // 프로토콜, 인터넷 포트, 클라 서버 주소체계
	memset(&s_address, 0, sizeof(s_address));
	//fill_n(&s_address, sizeof(s_address), 0);
	s_address.sin_family = AF_INET; // ipv4 인터넷 주소체계
	s_address.sin_port = htons(SERVER_PORT); // htons사용 이유 // 16비트 포트정보
	s_address.sin_addr.S_un.S_addr = htonl(INADDR_ANY); // 32비트 ip정보
	::bind(l_socket, reinterpret_cast<sockaddr*>(&s_address), sizeof(s_address)); // error +
	// c++11 bind가 아닌 bind를 사용하기 위해 ::

	listen(l_socket, SOMAXCONN); // error + 

	// <accept>
	// SOCKADDR_IN c_address;
	// memset(&c_address, 0, sizeof(c_address));
	// int c_addr_size = sizeof(c_address);

	g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

	initialize_tiles();
	initialize_clients();
	initialize_sectors();
	initialize_npcs();
	initialize_db();

	CreateIoCompletionPort(reinterpret_cast<HANDLE>(l_socket), g_iocp, 999, 0); // 등록

	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	EXOVER accept_over;

	ZeroMemory(&accept_over.over, sizeof(accept_over.over));
	accept_over.op = OP_ACCEPT;
	accept_over.c_socket = c_socket;
	AcceptEx(l_socket, accept_over.c_socket, accept_over.io_buf, NULL,
		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &accept_over.over);
	// 반환이 아니라 초기화된 소켓을 넣음

	vector<thread> worker_threads;
	for (int i = 0; i < 4; ++i) worker_threads.emplace_back(worker_thread);
	thread timer_thread(timer);

	timer_thread.join();
	for (auto& threads : worker_threads) threads.join();

	//CloseHandle(g_iocp); // 추가

	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		SQLCancel(hstmt);
		SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
	}

	SQLDisconnect(hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
}

