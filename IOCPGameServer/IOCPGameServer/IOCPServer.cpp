#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <set>
#include <unordered_set>

#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "mswsock.lib")

using namespace std;

#include "protocol.h"
constexpr auto MAX_PACKET_SIZE = 255;
constexpr auto MAX_BUF_SIZE = 2024;
constexpr auto MAX_USER = 10000;

constexpr auto MAX_ROW = 25;
constexpr auto MAX_COL = 25;

constexpr auto ROW_GAP = 16;
constexpr auto COL_GAP = 16;

constexpr auto SIGHT_RANGE = 11;

enum ENUMOP { OP_RECV = 0, OP_SEND, OP_ACCEPT };
enum C_STATUS {ST_FREE, ST_ALLOC, ST_ACTIVE };

struct ViewList
{
	mutex						view_lock;
	unordered_set<int>			viewlist;
};

struct NearList
{
	mutex						near_lock;
	unordered_set<int>			nearlist;
};

struct RemoveList
{
	mutex						remove_lock;
	unordered_set<int>			removelist;
};

struct EXOVER
{
	WSAOVERLAPPED	over;
	ENUMOP			op;
	char			io_buf[MAX_BUF_SIZE];
	union {
		WSABUF			wsabuf;
		SOCKET			c_socket;
	};
};

struct CLIENT
{
	mutex		m_cl;
	SOCKET		m_socket;
	int			m_id;
	EXOVER		m_recv_over;
	int			m_prev_size;
	char		m_packet_buf[MAX_PACKET_SIZE];
	C_STATUS	m_status;

	short		x, y;
	char		name[MAX_ID_LEN + 1]; // ������ �ü� �����Ƿ� +1
	unsigned	m_move_time;

	short		row, col;
	ViewList	m_viewlist;
	NearList	m_nearlist;
	RemoveList	m_removelist;
};

struct SECTOR
{
	mutex						sector_lock;
	unordered_set<int>			m_setPlayerList;
	short						m_StartX;
	short						m_StartY;
	short						m_EndX;
	short						m_EndY;
};


SECTOR g_sectors[MAX_ROW][MAX_COL];

CLIENT g_clients[MAX_USER];
HANDLE g_iocp;
SOCKET l_socket;

void send_move_packet(int user_id, int mover);
void send_login_ok_packet(int user_id);
void send_packet(int user_id, void* packet);
void send_enter_packet(int user_id, int o_id);
void send_leave_packet(int user_id, int o_id);
void process_packet(int user_id, char* buf);

void enter_game(int user_id);
void initialize_clients();
void initialize_sectors();
void disconnect(int user_id);
void recv_packet_construct(int user_id, int io_byte);

void worker_thread();

void sector();
void manage_sector_object();
void create_nearlist(int user_id, int row, int col);
void check_near_view(int id);
bool is_near_check(int id1,int id2);

void sector()
{

}

void manage_sector_object()
{

}

void create_nearlist(int user_id, int row, int col) // ���߿� ���������� �Ұ� ����ȭ !
{
	g_sectors[row][col].sector_lock.lock();
	for (auto& playerlist : g_sectors[row][col].m_setPlayerList){
		g_clients[user_id].m_nearlist.near_lock.lock();
		if (ST_ACTIVE == g_clients[user_id].m_status)
		{
			if (is_near_check(user_id, playerlist)) {
				g_clients[user_id].m_nearlist.nearlist.emplace(playerlist);
			}
		}
		g_clients[user_id].m_nearlist.near_lock.unlock();
	}
	g_sectors[row][col].sector_lock.unlock();
}

void check_near_view(int user_id)
{
	for (auto& nearlist : g_clients[user_id].m_nearlist.nearlist) // ��� near��ü�� ���ؼ� 
	{
		if (g_clients[user_id].m_viewlist.viewlist.find(nearlist) != g_clients[user_id].m_viewlist.viewlist.end()) // user�� viewlist�� ������
		{
			if (g_clients[nearlist].m_viewlist.viewlist.find(user_id) != g_clients[nearlist].m_viewlist.viewlist.end()) // ���� viewlist�� ������
			{
				//g_clients[nearlist].m_viewlist.view_lock.lock();
				if (ST_ACTIVE == g_clients[nearlist].m_status)
				{
					send_move_packet(nearlist, user_id);
				}
				//g_clients[nearlist].m_viewlist.view_lock.unlock();
			}
			else // ���� viewlist�� ������
			{
				g_clients[nearlist].m_viewlist.view_lock.unlock();
				if (ST_ACTIVE == g_clients[nearlist].m_status)
				{
					g_clients[nearlist].m_viewlist.viewlist.emplace(user_id);
					send_enter_packet(nearlist, user_id);
				}
				g_clients[nearlist].m_viewlist.view_lock.unlock();
			}
		}
		else // user�� viewlist�� ������
		{
			g_clients[user_id].m_viewlist.view_lock.lock(); // üũ
			if (ST_ACTIVE == g_clients[user_id].m_status)
			{
				g_clients[user_id].m_viewlist.viewlist.emplace(nearlist);
				send_enter_packet(user_id, nearlist);
			}
			g_clients[user_id].m_viewlist.view_lock.unlock();

			if (g_clients[nearlist].m_viewlist.viewlist.find(user_id) != g_clients[nearlist].m_viewlist.viewlist.end()) // ���� viewlist�� ������
			{
				//g_clients[nearlist].m_viewlist.view_lock.lock();
				if (ST_ACTIVE == g_clients[nearlist].m_status)
				{
					send_move_packet(nearlist, user_id);
				}
				//g_clients[nearlist].m_viewlist.view_lock.unlock();
			}
			else // ���� viewlist�� ������
			{
				g_clients[nearlist].m_viewlist.view_lock.lock();
				if (ST_ACTIVE == g_clients[nearlist].m_status)
				{
					g_clients[nearlist].m_viewlist.viewlist.emplace(user_id);
					send_enter_packet(nearlist, user_id);
				}
				g_clients[nearlist].m_viewlist.view_lock.unlock();
			}
		}
	}

	for (auto& viewlist : g_clients[user_id].m_viewlist.viewlist)
	{
		// nearlist ���� viewlist�� ��ã���� viwwlist�� removelist �� �ִ´�.
		if (g_clients[user_id].m_nearlist.nearlist.find(viewlist) == g_clients[user_id].m_nearlist.nearlist.end()) 
		{
			g_clients[user_id].m_cl.lock();
			g_clients[user_id].m_removelist.removelist.emplace(viewlist);
			g_clients[user_id].m_cl.unlock();
		}
	}

	for (auto& removelist : g_clients[user_id].m_removelist.removelist)
	{
		g_clients[user_id].m_viewlist.view_lock.lock();
		if (ST_ACTIVE == g_clients[user_id].m_status)
		{
			g_clients[user_id].m_viewlist.viewlist.erase(removelist);
			send_leave_packet(user_id, removelist);
		}
		g_clients[user_id].m_viewlist.view_lock.unlock();

		if (g_clients[removelist].m_viewlist.viewlist.find(user_id) != g_clients[removelist].m_viewlist.viewlist.end())
		{
			g_clients[removelist].m_viewlist.view_lock.lock();
			if (ST_ACTIVE == g_clients[removelist].m_status)
			{
				g_clients[removelist].m_viewlist.viewlist.erase(user_id);
				send_leave_packet(removelist, user_id);
			}
			g_clients[removelist].m_viewlist.view_lock.unlock();
		}
	}

	g_clients[user_id].m_nearlist.nearlist.clear();
	g_clients[user_id].m_removelist.removelist.clear();
}

bool is_near_check(int id1, int id2)
{
	if(SIGHT_RANGE/2 < abs(g_clients[id1].x - g_clients[id2].x))
		return false;
	if (SIGHT_RANGE/2 < abs(g_clients[id1].y - g_clients[id2].y))
		return false;

	return true;
}


void do_move(int user_id, int direction)
{
	CLIENT& user = g_clients[user_id];
	int x = user.x;
	int y = user.y;

	switch (direction)
	{
	case D_UP:
		if (y > 0)	y--; // 0
		break;
	case D_DOWN:
		if (y < WORLD_HEIGHT - 1) y++; //
		break;
	case D_LEFT:
		if (x > 0) x--; // 
		break;
	case D_RIGHT:
		if (x < WORLD_WIDTH - 1) x++; // 
		break;
	default:
		cout << "Unkown Direction from Client move packet!\n";
		DebugBreak();
		exit(-1);
	}

	user.x = x;
	user.y = y;

	// user�� ���� ���͸� ����� ���� ���Ϳ��� ����� �� ���Ϳ� ����ִ´�.
	if (user.x / COL_GAP != user.col || user.y / ROW_GAP != user.row)
	{
		int iPrevCol = user.col;
		int iPrevRow = user.row;

		int iCol = user.x / COL_GAP;
		int iRow = user.y / ROW_GAP;

		if (iCol > 24)
			iCol = iPrevCol;
		if (iRow > 24)
			iRow = iPrevRow;

		g_sectors[iPrevRow][iPrevCol].sector_lock.lock();
		g_sectors[iPrevRow][iPrevCol].m_setPlayerList.erase(user.m_id);
		g_sectors[iPrevRow][iPrevCol].sector_lock.unlock();

		g_sectors[iRow][iCol].sector_lock.lock();
		g_sectors[iRow][iCol].m_setPlayerList.emplace(user.m_id);
		user.row = iRow;
		user.col = iCol;
		g_sectors[iRow][iCol].sector_lock.unlock();
	}

	if (24 >= (user.x + SIGHT_RANGE / 2) / COL_GAP && 24 >= (user.y + SIGHT_RANGE / 2) / ROW_GAP)
	{
		for (int i = (user.x - SIGHT_RANGE / 2) / COL_GAP; i <= (user.x + SIGHT_RANGE / 2) / COL_GAP; ++i)
		{
			for (int j = (user.y - SIGHT_RANGE / 2) / ROW_GAP; j <= (user.y + SIGHT_RANGE / 2) / ROW_GAP; ++j)
			{
				if (i < 0) i = 0;
				if (j < 0) j = 0;

				create_nearlist(user_id, j, i);
			}
		}
	}

	check_near_view(user_id);
}

void send_packet(int user_id, void* packet)
{
	char* buf = reinterpret_cast<char*>(packet);

	CLIENT& user = g_clients[user_id];

	EXOVER* exover = new EXOVER; // recv���� ����ϰ� �����Ƿ� ���� �Ҵ��ؼ� ���
	exover->op = OP_SEND;
	ZeroMemory(&exover->over, sizeof(exover->over));
	exover->wsabuf.buf = exover->io_buf;
	exover->wsabuf.len = buf[0];

	memcpy(exover->io_buf, buf, buf[0]);

	WSASend(user.m_socket, &exover->wsabuf, 1, NULL, 0, &exover->over, NULL);
}

void send_login_ok_packet(int user_id)
{
	sc_packet_login_ok packet;
	packet.exp = 0;
	packet.hp = 0;
	packet.id = user_id;
	packet.level = 0;
	packet.size = sizeof(packet);
	packet.type = S2C_LOGIN_OK;
	packet.x = g_clients[user_id].x;
	packet.y = g_clients[user_id].y;

	send_packet(user_id, &packet);
}

void send_move_packet(int user_id, int mover)
{
	sc_packet_move packet;
	packet.id = mover;
	packet.size = sizeof(packet);
	packet.type = S2C_MOVE;
	packet.x = g_clients[mover].x;
	packet.y = g_clients[mover].y;
	packet.move_time = g_clients[user_id].m_move_time;

	send_packet(user_id, &packet);
}

void send_enter_packet(int user_id, int o_id)
{
	sc_packet_enter packet;
	packet.id = o_id;
	packet.size = sizeof(packet);
	packet.type = S2C_ENTER;
	packet.x = g_clients[o_id].x;
	packet.y = g_clients[o_id].y;
	strcpy_s(packet.name, g_clients[o_id].name);
	packet.o_type = O_PLAYER;

	send_packet(user_id, &packet);
}

void send_leave_packet(int user_id, int o_id)
{
	sc_packet_leave packet;
	packet.id = o_id;
	packet.size = sizeof(packet);
	packet.type = S2C_LEAVE;

	send_packet(user_id, &packet);
}

void enter_game(int user_id, char name[]) // lock �������� �ߴٰ� ���� ���� // userid���� ���ɰ� i�� �� ��. ���� ���� lock�ɸ� = �����// ���� �ɰ� �� ��û -> �����
{											// ���� lock �� �� ����
	g_clients[user_id].m_cl.lock();
	strcpy_s(g_clients[user_id].name, name);
	g_clients[user_id].name[MAX_ID_LEN] = NULL;
	send_login_ok_packet(user_id);

	//for (int i = 0; i < MAX_USER; ++i)
	//{
	//	if (user_id == i) // send_enter_packet ���õ� ���� // ���� ������ send enter packet�� ���� ������ ����
	//		continue;

	//	g_clients[i].m_cl.lock();
	//	if (ST_ACTIVE == g_clients[i].m_status)
	//	{
	//		if (i != user_id)
	//		{
	//			send_enter_packet(user_id, i);
	//			send_enter_packet(i, user_id);
	//		}
	//	}
	//	g_clients[i].m_cl.unlock();
	//}
	g_clients[user_id].m_cl.unlock();
	g_clients[user_id].m_status = ST_ACTIVE;
}

void process_packet(int user_id, char* buf)
{
	switch (buf[1])
	{
	case C2S_LOGIN:
	{
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(buf);
		enter_game(user_id,packet->name);
	}
	break;
	case C2S_MOVE:
	{
		cs_packet_move* packet = reinterpret_cast<cs_packet_move*>(buf);
		g_clients[user_id].m_move_time = packet->move_time;
		do_move(user_id, packet->direction);
	}
	break;
	default:
	{
		cout << "Unkown packet type error!\n";
		DebugBreak(); // ����� ��Ʃ��� �󿡼� ���߰� ���¸� ǥ���϶�
		exit(-1);
	}
	}
}

void initialize_clients() // ��Ƽ ������ ���� �̱� �����忡�� ���ư�. ���ؽ� �ʿ� ����
{
	for (int i = 0; i < MAX_USER; ++i)
	{
		g_clients[i].m_id = i;
		g_clients[i].m_status = ST_FREE;
	}
}

void initialize_sectors()
{
	for (int i = 0; i < MAX_COL; ++i)
	{
		for (int j = 0; j < MAX_ROW; ++j)
		{
			g_sectors[j][i].m_StartX = i * 16;
			g_sectors[j][i].m_StartY = j * 16;
			g_sectors[j][i].m_EndX = (i + 1) * 16;
			g_sectors[j][i].m_EndY = (j + 1) * 16;
		}
	}
}

void disconnect(int user_id)
{
	g_clients[user_id].m_cl.lock();
	g_clients[user_id].m_status = ST_ALLOC;
	//if(!g_clients[user_id].m_bConnected) --g_curr_user_id;
	send_leave_packet(user_id, user_id);
	closesocket(g_clients[user_id].m_socket);

	for (auto& cl : g_clients)
	{
		if (user_id == cl.m_id) continue; // �׷��� send_leave_packet�� ������ ��

		cl.m_cl.lock();
		if (ST_ACTIVE == cl.m_status)
			send_leave_packet(cl.m_id, user_id);
		cl.m_cl.unlock();
	}
	g_clients[user_id].m_status = ST_FREE;
	g_clients[user_id].m_cl.unlock();
}

void recv_packet_construct(int user_id, int io_byte) // io_byte�� dword�̱���
{
	CLIENT& cu = g_clients[user_id];
	EXOVER& recv_overlapped = cu.m_recv_over;

	int rest_byte = io_byte;
	char* p = recv_overlapped.io_buf;

	int packet_size = 0;

	if (0 != cu.m_prev_size) packet_size = cu.m_packet_buf[0];

	while (rest_byte > 0)
	{
		if (0 == packet_size) packet_size = *p;
		if (packet_size <= rest_byte + cu.m_prev_size)
		{
			memcpy(cu.m_packet_buf + cu.m_prev_size, p, packet_size - cu.m_prev_size);
			p += packet_size - cu.m_prev_size;
			rest_byte -= packet_size - cu.m_prev_size;
			packet_size = 0;
			process_packet(user_id, cu.m_packet_buf);
			cu.m_prev_size = 0;
		}
		else
		{
			
			memcpy(cu.m_packet_buf + cu.m_prev_size, p, rest_byte); // cu.m_packet_buf �̹� prev_size�� 0�� �ƴ϶� �׳� ���� �ȵ�
			cu.m_prev_size += rest_byte;
			rest_byte = 0;
			p += rest_byte; // rest_byte�� 0�ε� += �ǹ̰� �ճ� ���ϰ� �ٲ�� �ҰŰ���
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
		case OP_RECV:
		{
			if (0 == io_byte)
				disconnect(user_id);
			else
			{
				recv_packet_construct(user_id, io_byte);
				//process_packet(user_id, exover->io_buf);
				ZeroMemory(&cl.m_recv_over.over, sizeof(cl.m_recv_over.op));
				DWORD flags = 0;
				WSARecv(cl.m_socket, &cl.m_recv_over.wsabuf, 1, NULL, &flags, &cl.m_recv_over.over, NULL);
			}
		}
		break;
		case OP_SEND:
			if (0 == io_byte)
				disconnect(user_id);
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
				NewClient.x = rand() % WORLD_WIDTH;
				NewClient.y = rand() % WORLD_HEIGHT;

				DWORD flags = 0;
				WSARecv(c_socket, &NewClient.m_recv_over.wsabuf, 1, NULL, &flags, &NewClient.m_recv_over.over, NULL);

				// sector�� ���� �־��ش�.
				short col = NewClient.x / COL_GAP;
				short row = NewClient.y / ROW_GAP;
				g_sectors[row][col].m_setPlayerList.emplace(NewClient.m_id);
				NewClient.row = row;
				NewClient.col = col;
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
	for (auto& threads : worker_threads) threads.join();
}

//iobuf �� packetbuf ����