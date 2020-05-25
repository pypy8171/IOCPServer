#pragma once;
#include "PacketHandler.h"
#include "ViewProcessing.h"
#include "extern.h"

CPacketHandler::CPacketHandler()
{
}

CPacketHandler::~CPacketHandler()
{
}

void CPacketHandler::recv_packet_construct(int user_id, int io_byte) // io_byte는 dword이긴함
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

			memcpy(cu.m_packet_buf + cu.m_prev_size, p, rest_byte); // cu.m_packet_buf 이미 prev_size가 0이 아니라 그냥 쓰면 안됨
			cu.m_prev_size += rest_byte;
			rest_byte = 0;
			p += rest_byte; // rest_byte가 0인데 += 의미가 잇나 위하고 바꿔야 할거같음
		}
	}
}

void CPacketHandler::process_packet(int user_id, char buf[])
{
	switch (buf[1])
	{
	case C2S_LOGIN:
	{
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(buf);
		enter_game(user_id, packet->name);
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
		DebugBreak(); // 비쥬얼 스튜디오 상에서 멈추고 상태를 표시하라
		exit(-1);
	}
	}
}

void CPacketHandler::send_packet(int user_id, void* packet)
{
	char* buf = reinterpret_cast<char*>(packet);

	CLIENT& user = g_clients[user_id];

	EXOVER* exover = new EXOVER; // recv에서 사용하고 있으므로 새로 할당해서 사용
	exover->op = OP_SEND;
	ZeroMemory(&exover->over, sizeof(exover->over));
	exover->wsabuf.buf = exover->io_buf;
	exover->wsabuf.len = buf[0];

	memcpy(exover->io_buf, buf, buf[0]);

	WSASend(user.m_socket, &exover->wsabuf, 1, NULL, 0, &exover->over, NULL);
}

void CPacketHandler::enter_game(int user_id, char name[])
{
	g_clients[user_id].m_cl.lock();
	strcpy_s(g_clients[user_id].name, name);
	g_clients[user_id].name[MAX_ID_LEN] = NULL;
	send_login_ok_packet(user_id);

	g_clients[user_id].m_cl.unlock();
	g_clients[user_id].m_status = ST_ACTIVE;

}

void CPacketHandler::send_login_ok_packet(int user_id)
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

void CPacketHandler::send_enter_packet(int user_id, int o_id)
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

void CPacketHandler::send_leave_packet(int user_id, int o_id)
{
	sc_packet_leave packet;
	packet.id = o_id;
	packet.size = sizeof(packet);
	packet.type = S2C_LEAVE;

	send_packet(user_id, &packet);
}

void CPacketHandler::disconnect(int user_id)
{
	g_clients[user_id].m_cl.lock();
	g_clients[user_id].m_status = ST_ALLOC;
	//if(!g_clients[user_id].m_bConnected) --g_curr_user_id;
	send_leave_packet(user_id, user_id);
	closesocket(g_clients[user_id].m_socket);

	for (auto& cl : g_clients)
	{
		if (user_id == cl.m_id) continue; // 그래도 send_leave_packet은 보내야 함

		cl.m_cl.lock();
		if (ST_ACTIVE == cl.m_status)
			send_leave_packet(cl.m_id, user_id);
		cl.m_cl.unlock();
	}
	g_clients[user_id].m_status = ST_FREE;
	g_clients[user_id].m_cl.unlock();
}

void CPacketHandler::do_move(int user_id, int direction)
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

	// user가 이전 섹터를 벗어나면 이전 섹터에서 지우고 새 섹터에 집어넣는다.
	if (user.x / COL_GAP != user.col || user.y / ROW_GAP != user.row)
	{
		int iPrevCol = user.col;
		int iPrevRow = user.row;

		int iCol = user.x / COL_GAP;
		int iRow = user.y / ROW_GAP;

		if (iCol > MAX_COL - 1)
			iCol = iPrevCol;
		if (iRow > MAX_ROW - 1)
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

	for (int i = (user.x - SIGHT_RANGE / 2) / COL_GAP; i <= (user.x + SIGHT_RANGE / 2) / COL_GAP; ++i)
	{
		for (int j = (user.y - SIGHT_RANGE / 2) / ROW_GAP; j <= (user.y + SIGHT_RANGE / 2) / ROW_GAP; ++j)
		{
			int iCol = -1;
			int iRow = -1;

			iCol = i;
			iRow = j;

			if (iCol < 0) iCol = 0;
			if (iRow < 0) iRow = 0;

			if (iCol > 24)	iCol = 24;
			if (iRow > 24) iRow = 24;

			CViewProcessing::GetInst()->create_nearlist(user_id, iRow, iCol);
		}
	}

	CViewProcessing::GetInst()->check_near_view(user_id);
}

void CPacketHandler::send_move_packet(int user_id, int mover)
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
