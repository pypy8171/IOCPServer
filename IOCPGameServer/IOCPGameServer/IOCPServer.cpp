#pragma once;
#include "extern.h"
#include "ViewProcessing.h"
#include "PacketHandler.h"

#include <thread>


SECTOR g_sectors[25][25];
CLIENT g_clients[10000];
NPC g_npcs[200000];

HANDLE g_iocp;
SOCKET l_socket;

void initialize_clients();
void initialize_sectors();

void worker_thread();

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
			g_sectors[j][i].m_StartX = i * 16;
			g_sectors[j][i].m_StartY = j * 16;
			g_sectors[j][i].m_EndX = (i + 1) * 16;
			g_sectors[j][i].m_EndY = (j + 1) * 16;
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

	//CloseHandle(g_iocp); // �߰�
}
