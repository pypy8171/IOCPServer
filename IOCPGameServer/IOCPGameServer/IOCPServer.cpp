#pragma once;
#include "extern.h"
#include "ViewProcessing.h"
#include "PacketHandler.h"

#include <thread>


SECTOR g_sectors[MAX_ROW][MAX_COL];
CLIENT g_clients[MAX_USER + MAX_NPC];
unordered_map<int, CLIENT*> g_mapCL;
//NPC g_npcs[MAX_NPC];

priority_queue<Event_Type> timer_queue;
mutex timer_lock;

HANDLE g_iocp;
SOCKET l_socket;

void initialize_clients();
void initialize_npcs();
void initialize_sectors();

void worker_thread();

void addtimer(int id, ENUMOP op, unsigned int timer) // �̺�Ʈ new delete ���ϰ� ���� �ϴ°��� �󸶳� ������ ���������� Ȯ���ؾ���
{
	Event_Type eType;
	eType.obj_id = id;
	eType.event_id = op;
	eType.wakeup_time = high_resolution_clock::now() + milliseconds(timer);

	//g_clients[id].m_event.obj_id = id;
	//g_clients[id].m_event.event_id = op;
	//g_clients[id].m_event.wakeup_time = high_resolution_clock::now() + milliseconds(timer);
	//g_clients[id].m_event.target_id = 0;

	timer_lock.lock();
	timer_queue.emplace(eType);
	timer_lock.unlock();
}

void initialize_clients() // ��Ƽ ������ ���� �̱� �����忡�� ���ư�. ���ؽ� �ʿ� ����
{
	for (int i = 0; i < MAX_USER; ++i)
	{
		g_clients[i].m_id = i;
		g_clients[i].m_status = ST_FREE;
		InitializeSRWLock(&g_clients[i].m_viewlist.rwlock);
	}
}

void initialize_sectors()
{
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
}

int API_send_message(lua_State* L)
{
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	char* message = (char*)lua_tostring(L, -1);

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

void initialize_npcs()
{
	cout << "start npc initialize" << endl;
	for (int i = NPC_START_IDX; i < MAX_NPC + NPC_START_IDX; ++i)
	{
		g_clients[i].m_socket = 0;
		g_clients[i].m_id = i;
		sprintf_s(g_clients[i].name, "NPC%d", i);
		g_clients[i].m_status = ST_SLEEP;
		g_clients[i].x = rand() % WORLD_WIDTH;
		g_clients[i].y = rand() % WORLD_HEIGHT;
		g_clients[i].row = g_clients[i].y / ROW_GAP;
		g_clients[i].col = g_clients[i].x / COL_GAP;

		g_clients[i].m_otype = O_NPC;

		short iRow = g_clients[i].row;
		short iCol = g_clients[i].col;

		g_sectors[iRow][iCol].m_setPlayerList.emplace(i); // npc ����

		lua_State* L = g_clients[i].L = luaL_newstate();
		luaL_openlibs(L);
		luaL_loadfile(L, "NPC.LUA");
		lua_pcall(L, 0, 0, 0);
		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		lua_pcall(L, 1, 0, 0); // L, �Ķ���� ����, ���� ����, ,
		lua_pop(L, 1); // getglobal�Ѱ� ������

		lua_register(L, "API_send_message", API_send_message);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
	}
	cout << "finish npc initialize" << endl;
}

void timer() // ������ 
{
	while (true)
	{
		this_thread::sleep_for(1ms);// Sleep �� �����쿡���� �����Ѱ���.
		while (true) {
			timer_lock.lock();
			if (timer_queue.empty())
			{
				timer_lock.unlock();
				break;
			}

			if (timer_queue.top().wakeup_time > high_resolution_clock::now() )
			{
				timer_lock.unlock();
				break;
			}
			Event_Type tEvent = timer_queue.top();
			timer_queue.pop();
			timer_lock.unlock();

			switch (tEvent.event_id)
			{
			case OP_AIMOVE:
				EXOVER* exover = new EXOVER;
				exover->op = OP_AIMOVE;

				PostQueuedCompletionStatus(g_iocp, 1, tEvent.obj_id, &exover->over);

				//g_clients[tEvent.obj_id].m_recv_over.op = OP_AIMOVE;
				//
				//PostQueuedCompletionStatus(g_iocp, 1, tEvent.obj_id, &g_clients[tEvent.obj_id].m_recv_over.over);

				//CPacketHandler::GetInst()->npc_move(tEvent.obj_id, OP_AIMOVE);
				//addtimer(tEvent.obj_id, tEvent.event_id, 1000);
				break;
			}
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
		GetQueuedCompletionStatus(g_iocp, &io_byte, &key, &over, INFINITE); // accept�� ���� ���ŷ -> acceptex���
		// acceptex�ϸ� �޶����°� ���� -> ���� �ٽ� ����

		EXOVER* exover = reinterpret_cast<EXOVER*>(over);
		int user_id = static_cast<int>(key);

		CLIENT& cl = g_clients[user_id];

		switch (exover->op)
		{
		case OP_PLAYER_MOVE:
		{
			g_clients[user_id].lua_l.lock();
			lua_State* L = g_clients[user_id].L;
			lua_getglobal(L, "event_player_move");
			lua_pushnumber(L, exover->player_id);
			int error = lua_pcall(L, 1, 0, 0);
			if (error) cout << lua_tostring(L, -1)<<endl;
			//lua_pop(L, 1);
			g_clients[user_id].lua_l.unlock();
			delete exover;
		}
		break;
		case OP_AIMOVE:
		{
			CPacketHandler::GetInst()->npc_move(static_cast<int>(key), exover->op); // a*�� ���⼭ ���ư���. // ���⼭ key�� npc id
			bool keep_alive = false;
			for (int i = 0; i < NPC_START_IDX; ++i)
				if (true == CViewProcessing::GetInst()->is_near_check_pp(user_id, i))
					if (ST_ACTIVE == g_clients[i].m_status)
					{
						keep_alive = true;
						break;
					}
			if (true == keep_alive) addtimer(static_cast<int>(key), OP_AIMOVE, 1000);
			else g_clients[user_id].m_status = ST_SLEEP;

			delete exover; // ������ send�Ҷ� delete �ϴϱ� ���⼭ �����൵ �ɵ�. -> �������� ��
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
		case OP_ACCEPT: // �񵿱� accept �ϳ��� ����. main���� �񵿱� accept �ϰ� worker thread���� �ϷḦ �ϰ� // 4.28 ���� �ι�° 3��
						// �Ϸ��� thread���� accept�Ҷ����� �ٸ� thread�� �Ϸ� �ȵ�. ���� �̱۽������ �����̵�.
		{
			int user_id = -1;
			for (int i = 0; i < MAX_USER; ++i)
			{
				lock_guard<mutex> gl{ g_clients[i].m_cl }; // unlcok �� �ʿ� ����. ��Ͽ��� ���������� unlock �� ����, ������ �Ź� ���ٶ� unlock�� ���ְ� lock�� �Ǵ�.
				if (ST_FREE == g_clients[i].m_status) //  template ��ü. ������ �Ҹ��� ȣ���Ҷ� lock unlock ��. �Լ� or ��� �κп� ����ϸ� �����ϴ�
				{
					g_clients[i].m_status = ST_ALLOC;
					user_id = i;
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
				NewClient.m_id = user_id;
				NewClient.m_prev_size = 0;
				NewClient.m_recv_over.op = OP_RECV;
				ZeroMemory(&NewClient.m_recv_over.over, sizeof(NewClient.m_recv_over.op));
				NewClient.m_recv_over.wsabuf.buf = NewClient.m_recv_over.io_buf;
				NewClient.m_recv_over.wsabuf.len = MAX_BUF_SIZE;
				NewClient.m_socket = c_socket;
				NewClient.m_viewlist.viewlist.clear();
				NewClient.x = rand() % WORLD_WIDTH;
				NewClient.y = rand() % WORLD_HEIGHT;

				NewClient.m_otype = O_PLAYER;

				DWORD flags = 0;
				WSARecv(c_socket, &NewClient.m_recv_over.wsabuf, 1, NULL, &flags, &NewClient.m_recv_over.over, NULL);

				// sector�� ���� �־��ش�.
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

	SOCKADDR_IN s_address; // ��������, ���ͳ� ��Ʈ, Ŭ�� ���� �ּ�ü��
	memset(&s_address, 0, sizeof(s_address));
	//fill_n(&s_address, sizeof(s_address), 0);
	s_address.sin_family = AF_INET; // ipv4 ���ͳ� �ּ�ü��
	s_address.sin_port = htons(SERVER_PORT); // htons��� ���� // 16��Ʈ ��Ʈ����
	s_address.sin_addr.S_un.S_addr = htonl(INADDR_ANY); // 32��Ʈ ip����
	::bind(l_socket, reinterpret_cast<sockaddr*>(&s_address), sizeof(s_address)); // error +
	// c++11 bind�� �ƴ� bind�� ����ϱ� ���� ::

	listen(l_socket, SOMAXCONN); // error + 

	// <accept>
	// SOCKADDR_IN c_address;
	// memset(&c_address, 0, sizeof(c_address));
	// int c_addr_size = sizeof(c_address);

	g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

	initialize_clients();
	initialize_sectors();
	initialize_npcs();

	CreateIoCompletionPort(reinterpret_cast<HANDLE>(l_socket), g_iocp, 999, 0); // ���

	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	EXOVER accept_over;

	ZeroMemory(&accept_over.over, sizeof(accept_over.over));
	accept_over.op = OP_ACCEPT;
	accept_over.c_socket = c_socket;
	AcceptEx(l_socket, accept_over.c_socket, accept_over.io_buf, NULL,
		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &accept_over.over);
	// ��ȯ�� �ƴ϶� �ʱ�ȭ�� ������ ����

	vector<thread> worker_threads;
	for (int i = 0; i < 4; ++i) worker_threads.emplace_back(worker_thread);
	thread timer_thread(timer);

	timer_thread.join();
	for (auto& threads : worker_threads) threads.join();

	//CloseHandle(g_iocp); // �߰�
}

