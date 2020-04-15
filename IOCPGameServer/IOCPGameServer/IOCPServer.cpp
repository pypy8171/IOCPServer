#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>
#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "mswsock.lib")

using namespace std;

#include "protocol.h"
constexpr auto MAX_PACKET_SIZE = 255;
constexpr auto MAX_BUF_SIZE = 2024;
constexpr auto MAX_USER = 10;

enum ENUMOP { OP_RECV = 0, OP_SEND, OP_ACCEPT };

struct EXOVER
{
	WSAOVERLAPPED	over;
	ENUMOP			op;
	char			io_buf[MAX_BUF_SIZE];
	WSABUF			wsabuf;
};

struct CLIENT
{
	SOCKET	m_socket;
	int		m_id;
	EXOVER	m_recv_over;
	int		m_prev_size;
	char	m_packet_buf[MAX_PACKET_SIZE];

	bool	m_bConnected;
	short	x, y;
	char	name[MAX_ID_LEN + 1]; // ������ �ü� �����Ƿ� +1
};

CLIENT g_clients[MAX_USER];
int g_curr_user_id = 0;
HANDLE g_iocp;

void send_move_packet(int user_id, int mover);
void send_login_ok_packet(int user_id);
void send_packet(int user_id, void* packet);
void send_enter_packet(int user_id, int o_id);
void send_leave_packet(int user_id, int o_id);
void process_packet(int user_id, char* buf);

void enter_game(int user_id);
void initialize_clients();
void disconnect(int user_id);
void recv_packet_construct(int user_id, int io_byte);

void do_move(int user_id, int direction)
{
	CLIENT& user = g_clients[user_id];
	int x = user.x;
	int y = user.y;

	switch (direction)
	{
	case D_UP:
		if(y>0)	y--;
		break;
	case D_DOWN:
		if (y < WORLD_HEIGHT - 1) y++;
		break;
	case D_LEFT:
		if (x > 0) x--;
		break;
	case D_RIGHT:
		if (x < WORLD_WIDTH - 1) x++;
		break;
	default :
		cout << "Unkown Direction from Client move packet!\n";
		DebugBreak();
		exit(-1);
	}

	user.x = x;
	user.y = y;

	for (auto& clients : g_clients)
	{
		if(true == clients.m_bConnected)
			send_move_packet(clients.m_id, user_id);
	}
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

void enter_game(int user_id)
{
	g_clients[user_id].m_bConnected = true;
	for (int i = 0; i < MAX_USER; ++i)
	{
		if (true == g_clients[i].m_bConnected)
		{
			if (i != user_id)
			{
				send_enter_packet(user_id, i);
				send_enter_packet(i, user_id);
			}
		}
	}
}

void process_packet(int user_id, char* buf)
{
	switch (buf[1])
	{
	case C2S_LOGIN:
	{
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(buf);
		strcpy_s(g_clients[user_id].name, packet->name);
		g_clients[user_id].name[MAX_ID_LEN] = NULL;
		send_login_ok_packet(user_id);
		enter_game(user_id);
	}
	break;
	case C2S_MOVE:
	{
		cs_packet_move* packet = reinterpret_cast<cs_packet_move*>(buf);
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

void initialize_clients()
{
	for (int i = 0; i < MAX_USER; ++i)
	{
		g_clients[i].m_bConnected = false;
	}
}

void disconnect(int user_id)
{
	g_clients[user_id].m_bConnected = false;
	for (auto& cl : g_clients)
	{
		if (true == g_clients[cl.m_id].m_bConnected)
			send_leave_packet(cl.m_id, user_id);

	}
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


int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	SOCKET l_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN s_address;
	memset(&s_address, 0, sizeof(s_address));
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

	CreateIoCompletionPort(reinterpret_cast<HANDLE>(l_socket), g_iocp, 999, 0); // ���

	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	EXOVER accept_over;
	ZeroMemory(&accept_over.over, sizeof(accept_over.over));
	accept_over.op = OP_ACCEPT;
	AcceptEx(l_socket, c_socket, accept_over.io_buf, NULL, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &accept_over.over);
	// ��ȯ�� �ƴ϶� �ʱ�ȭ�� ������ ����

	while (true)
	{
		DWORD io_byte;
		ULONG_PTR key;
		WSAOVERLAPPED* over;
		GetQueuedCompletionStatus(g_iocp, &io_byte, &key, &over, INFINITE); // accept�� ���� ����ŷ -> acceptex���
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
		case OP_ACCEPT:
			{
				int user_id = g_curr_user_id++;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(c_socket), g_iocp, user_id, 0);
				g_curr_user_id = g_curr_user_id % MAX_USER;

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

				c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
				ZeroMemory(&accept_over.over, sizeof(accept_over.over));
				AcceptEx(l_socket, c_socket, accept_over.io_buf, NULL, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, NULL, &accept_over.over);

			}
			break;
		}

	}
}

//iobuf �� packetbuf ����