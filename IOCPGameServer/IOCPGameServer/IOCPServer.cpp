#pragma once;
#include "extern.h"
#include "ViewProcessing.h"
#include "PacketHandler.h"

#include <thread>
#include <chrono>

using namespace chrono;

SECTOR g_sectors[MAX_ROW][MAX_COL];
CLIENT g_clients[MAX_USER];
unordered_map<int, CLIENT*> g_mapCL;
NPC g_npcs[MAX_NPC];

priority_queue<Event_Type, vector<Event_Type>, Compare > timer_queue;
mutex timer_lock;

HANDLE g_iocp;
SOCKET l_socket;

void initialize_clients();
void initialzie_npcs();
void initialize_sectors();

void worker_thread();

void addtimer(int id, ENUMOP op, unsigned int timer)
{
	Event_Type eventtype;
	eventtype.obj_id = id; // m_id인 10000부터 들어감
	eventtype.event_id = op;
	eventtype.wakeup_time = GetCurrentTime() + timer;
	timer_lock.lock();
	timer_queue.emplace(eventtype);
	timer_lock.unlock();
}

void initialize_clients() // 멀티 스레드 이전 싱글 스레드에서 돌아감. 뮤텍스 필요 없음
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

void initialzie_npcs()
{
	for (int i = 0; i < MAX_NPC; ++i)
	{
		g_npcs[i].m_id = i + NPC_START_IDX;
		g_npcs[i].m_Status = NPC_STATUS::S_NONACTIVE;
		g_npcs[i].x = rand() % WORLD_WIDTH;
		g_npcs[i].y = rand() % WORLD_HEIGHT;

		addtimer(g_npcs[i].m_id, ENUMOP::OP_AIMOVE, 1000);

		g_npcs[i].row = g_npcs[i].y / ROW_GAP;
		g_npcs[i].col = g_npcs[i].x / COL_GAP;
		g_sectors[g_npcs[i].row][g_npcs[i].col].m_setNpcList.emplace(g_npcs[i].m_id); // npc 삽입
	}
}

void timer() // 스레드 
{
	while (true)
	{
		Sleep(1); // 멀티쓰레드 동작이 원활하게 되도록
		timer_lock.lock();
		while (!timer_queue.empty())
		{
			if (timer_queue.top().wakeup_time > GetCurrentTime())
				break;
			
			Event_Type t_event = timer_queue.top();
			timer_queue.pop();
			timer_lock.unlock(); // 큐에서 빼기전까지 lock

			// 아래구조 문제 - 다른 클라 viewlist에 들어있는 npc들은 이미 active상태임 // 의미없이 ai까지 호출함
			if (g_npcs[t_event.obj_id - NPC_START_IDX].m_Status != S_ACTIVE)
				addtimer(t_event.obj_id, ENUMOP::OP_AIMOVE, 1000);
			else
			{
				EXOVER* exover = new EXOVER; 
				exover->op = OP_AIMOVE;

				PostQueuedCompletionStatus(g_iocp, 0, t_event.obj_id, &exover->over);// 어차피 getqueuedcompletionstatus 값 그대로 들어옴
			}

			timer_lock.lock(); // 위에 unlock을 했으므로 lock
		}
		timer_lock.unlock();
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
		case OP_AIMOVE:
		{
			CPacketHandler::GetInst()->npc_move(static_cast<int>(key), exover->op); // a*는 여기서 돌아간다. // 여기서 key는 npc id
			delete exover; // 어차피 send할때 delete 하니까 여기서 안해줘도 될듯. -> 성능차이 남
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

				DWORD flags = 0;
				WSARecv(c_socket, &NewClient.m_recv_over.wsabuf, 1, NULL, &flags, &NewClient.m_recv_over.over, NULL);

				// sector에 집어 넣어준다.
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

	initialize_clients();
	initialize_sectors();
	initialzie_npcs();

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

	for (auto& threads : worker_threads) threads.join();
	timer_thread.join();

	//CloseHandle(g_iocp); // 추가
}

