#pragma once;
#include "PacketHandler.h"
#include "ViewProcessing.h"

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
	g_clients[user_id].m_status = ST_ACTIVE; // deadlock 때문에 위치 바꿈
	g_mapCL.emplace(make_pair(user_id, &g_clients[user_id])); // 나중에 사용
	g_clients[user_id].m_cl.unlock();

	CViewProcessing::GetInst()->create_nearlist(g_clients[user_id].m_id, (g_clients[user_id].y - VIEW_RADIUS / 2) / ROW_GAP,
		(g_clients[user_id].x - VIEW_RADIUS / 2) / COL_GAP, (g_clients[user_id].y + VIEW_RADIUS / 2) / ROW_GAP, (g_clients[user_id].x + VIEW_RADIUS / 2) / COL_GAP);

	g_clients[user_id].m_cl.lock(); //하나
	unordered_set<int> nr = g_clients[user_id].m_nearlist.nearlist;
	g_clients[user_id].m_cl.unlock();
	for (auto& near_vl : nr)
	{
		if (user_id == near_vl) continue;
		CPacketHandler::GetInst()->send_enter_packet(g_clients[user_id].m_id, near_vl);
	}

	// 교수님 코드
	for (int i = 0; i < MAX_USER; ++i)
	{
		if (user_id == i) continue;
		if (false == CViewProcessing::GetInst()->is_near_check_pp(user_id, i))
		{
			//g_clients[i].m_cl.lock();
			if (ST_ACTIVE == g_clients[i].m_status) // 다른 스레드들이 종료하고 active를 건드려야 한다. 문제가 생기면 수정한다고 하심. 필요한 것임.
			{
				if (user_id != i)
				{
					send_enter_packet(user_id, i);
					send_enter_packet(i, user_id);
				}
			}
			//g_clients[i].m_cl.unlock();
		}
	}
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

	if (o_id < NPC_START_IDX)
	{
		packet.x = g_clients[o_id].x;
		packet.y = g_clients[o_id].y;
		strcpy_s(packet.name, g_clients[o_id].name);
		packet.o_type = O_PLAYER;

	}
	else
	{
		int idx = o_id - NPC_START_IDX; // npc mover는 npc idx가 아니라 id값으로 옴 + 10000인 상태
		g_npcs[idx].m_Status = S_ACTIVE;

		packet.x = g_npcs[idx].x;
		packet.y = g_npcs[idx].y;
		sprintf_s(packet.name, "N%03d", o_id % 200000); // 아이디 배정

		packet.o_type = O_NPC;
	}
	// 교수님 코드
	g_clients[user_id].m_cl.lock();
	g_clients[user_id].m_viewlist.viewlist.emplace(o_id);
	g_clients[user_id].m_cl.unlock();
	//

	send_packet(user_id, &packet);
}


void CPacketHandler::send_move_packet(int user_id, int mover)
{
	sc_packet_move packet;
	packet.id = mover;
	packet.size = sizeof(packet);
	packet.type = S2C_MOVE;
	if (mover < NPC_START_IDX)
	{
		packet.x = g_clients[mover].x;
		packet.y = g_clients[mover].y;
		packet.move_time = g_clients[user_id].m_move_time;
	}
	else
	{
		int idx = mover - NPC_START_IDX; // npc mover는 npc idx가 아니라 id값으로 옴 + 10000인 상태
		packet.x = g_npcs[idx].x;
		packet.y = g_npcs[idx].y;
	}

	send_packet(user_id, &packet);
}

void CPacketHandler::send_leave_packet(int user_id, int o_id)
{
	sc_packet_leave packet;
	packet.id = o_id;
	packet.size = sizeof(packet);
	packet.type = S2C_LEAVE;

	g_clients[user_id].m_cl.lock();
	g_clients[user_id].m_viewlist.viewlist.erase(o_id);
	g_clients[user_id].m_cl.unlock();

	send_packet(user_id, &packet);
}




void CPacketHandler::disconnect(int user_id)
{
	//for (auto& [이름, 나이] : g_mapCL)
	//	cout << 이름 << 나이 << endl;

	send_leave_packet(user_id, user_id);
	g_clients[user_id].m_cl.lock();
	g_clients[user_id].m_status = ST_ALLOC;
	//if(!g_clients[user_id].m_bConnected) --g_curr_user_id;
	closesocket(g_clients[user_id].m_socket);

	for (auto& cl : g_clients)
	{
		if (user_id == cl.m_id) continue; // 그래도 send_leave_packet은 보내야 함

		//cl.m_cl.lock(); // 주석
		if (ST_ACTIVE == cl.m_status)
			send_leave_packet(cl.m_id, user_id);
		//cl.m_cl.unlock(); // 주석
	}

	//나갈때 뷰리스트에 있던 npc들 nonactive로 해야할 듯 // leave에서 해줌 -> npc움직일때 모든 플레이어에게서 벗어나면 nonactive로 만듬

	g_mapCL.erase(user_id); //나중에 사용
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


	CViewProcessing::GetInst()->create_nearlist(user_id, (user.y - VIEW_RADIUS / 2) / ROW_GAP,
		(user.x - VIEW_RADIUS / 2) / COL_GAP, (user.y + VIEW_RADIUS / 2) / ROW_GAP, (user.x + VIEW_RADIUS / 2) / COL_GAP);


	CViewProcessing::GetInst()->check_near_view(user_id);
}

void CPacketHandler::npc_move(int id, ENUMOP op) // a*알고리즘 사용시 각 타일마다 possess되어있는지 확인한다.  
{												// 길 찾는 비용이 어느정도인지 아직 모르니
	int npcIdx = id - NPC_START_IDX;
	NPC& npc = g_npcs[npcIdx];

	switch (op)
	{
	case OP_AIMOVE:
	{
		char dir = rand() % 4;
		switch (dir)
		{
		case D_UP: if (npc.y > 0) npc.y--; break;
		case D_DOWN:if (npc.y < WORLD_HEIGHT - 1) npc.y++; break;
		case D_LEFT: if (npc.x > 0) npc.x--; break;
		case D_RIGHT: if (npc.x < WORLD_WIDTH - 1) npc.x++;	break;
		}

		g_npcs[npcIdx].x = npc.x;
		g_npcs[npcIdx].y = npc.y;
		// client 시야에 들어오면 viewlist에 넣어준다. 그 전에 nearlist에 넣어준다.
		if (g_npcs[npcIdx].x / COL_GAP != g_npcs[npcIdx].col || g_npcs[npcIdx].y / ROW_GAP != g_npcs[npcIdx].row)
		{
			int iPrevCol = g_npcs[npcIdx].col;
			int iPrevRow = g_npcs[npcIdx].row;

			int iCol = g_npcs[npcIdx].x / COL_GAP;
			int iRow = g_npcs[npcIdx].y / ROW_GAP;

			if (iCol > MAX_COL - 1)
				iCol = iPrevCol;
			if (iRow > MAX_ROW - 1)
				iRow = iPrevRow;

			g_sectors[iPrevRow][iPrevCol].sector_lock.lock();
			g_sectors[iPrevRow][iPrevCol].m_setNpcList.erase(g_npcs[npcIdx].m_id); // npc의 인덱스를 넣어줌.
			g_sectors[iPrevRow][iPrevCol].sector_lock.unlock();

			g_sectors[iRow][iCol].sector_lock.lock();
			g_sectors[iRow][iCol].m_setNpcList.emplace(g_npcs[npcIdx].m_id);
			g_npcs[npcIdx].row = iRow;
			g_npcs[npcIdx].col = iCol;
			g_sectors[iRow][iCol].sector_lock.unlock();
		}

		for (auto& cl : g_mapCL) // 각 플레이어들이랑 
		{
			if (cl.second->m_status != ST_ACTIVE)
				continue;

			g_clients[cl.first].m_cl.lock();
			unordered_set <int> old_vl = g_clients[cl.first].m_viewlist.viewlist; // 이전 viewlist
			unordered_set <int> new_vl = g_clients[cl.first].m_nearlist.nearlist; // 새로운 viewlist
			g_clients[cl.first].m_cl.unlock();

			int npcID = npcIdx + NPC_START_IDX;

			if (CViewProcessing::GetInst()->is_near_check_pn(cl.first, npcIdx)) { // npc의 idx가 들어가있음
				new_vl.emplace(npcID); // npc의 id값이 들어가있음

				if (0 == old_vl.count(npcID)) // view리스트에 없다.
				{
					CPacketHandler::GetInst()->send_enter_packet(cl.first, npcID);
				}
				else // viewlist에 있다.
				{
					if (S_ACTIVE == g_npcs[npcIdx].m_Status)
					{
						CPacketHandler::GetInst()->send_move_packet(cl.first, npcID);
					}
					else
						CPacketHandler::GetInst()->send_enter_packet(cl.first, npcID);
				}
			}
			else
			{
				new_vl.erase(npcID);
			}
			
			if (0 == new_vl.count(npcID) && 0 != old_vl.count(npcID)) // 예전 viewlist에 있고, 새로운 viewlist에는 없다
			{
				CPacketHandler::GetInst()->send_leave_packet(cl.first, npcID); // viewlist에는 npc idx가 아닌 npc의 id가 들어가있음

				for (auto& o_cl : g_mapCL)
				{
					if (o_cl == cl) continue;

					g_clients[o_cl.first].m_cl.lock();
					unordered_set <int> o_old_vl = g_clients[o_cl.first].m_viewlist.viewlist; // 이전 viewlist
					g_clients[o_cl.first].m_cl.unlock();

					if (0 == o_old_vl.count(npcID)) // view리스트에 없다.
					{
						g_npcs[npcIdx].m_Status = S_NONACTIVE;
					}
					else
					{
						g_npcs[npcIdx].m_Status = S_ACTIVE;
						break;
					}

				}
			}
		}
		addtimer(g_npcs[npcIdx].m_id, ENUMOP::OP_AIMOVE, 1000);
		break;
	}
	break;
	}



}