#pragma once;
#include "PacketHandler.h"
#include "ViewProcessing.h"
#include "DBHandler.h"

CPacketHandler::CPacketHandler()
{
}

CPacketHandler::~CPacketHandler()
{
}

void CPacketHandler::recv_packet_construct(int user_id, int io_byte) // io_byte�� dword�̱���
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

void CPacketHandler::process_packet(int user_id, char buf[])
{
	switch (buf[1])
	{
	case C2S_LOGIN:
	{
		//wcout.imbue(locale("korean"));
		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(buf);
		CDBHandler::GetInst()->DB_login(user_id, packet->name); // ���⼭ enter
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

void CPacketHandler::send_packet(int user_id, void* packet)
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

void CPacketHandler::enter_game(int user_id, WCHAR name[])
{
	g_clients[user_id].m_cl.lock();
	wcscpy_s(g_clients[user_id].name, name);
	g_clients[user_id].name[MAX_ID_LEN] = NULL; 
	send_login_ok_packet(user_id);
	g_clients[user_id].m_status = ST_ACTIVE; // deadlock ������ ��ġ �ٲ�
	g_clients[user_id].m_cl.unlock();

	//CLIENT& user = g_clients[user_id];

	//if (user.x / COL_GAP != user.col || user.y / ROW_GAP != user.row)
	//{
	//	int iPrevCol = user.col;
	//	int iPrevRow = user.row;

	//	int iCol = user.x / COL_GAP;
	//	int iRow = user.y / ROW_GAP;

	//	if (iCol > MAX_COL - 1)
	//		iCol = iPrevCol;
	//	if (iRow > MAX_ROW - 1)
	//		iRow = iPrevRow;

	//	g_sectors[iPrevRow][iPrevCol].sector_lock.lock();
	//	g_sectors[iPrevRow][iPrevCol].m_setPlayerList.erase(user.m_id);
	//	g_sectors[iPrevRow][iPrevCol].sector_lock.unlock();

	//	g_sectors[iRow][iCol].sector_lock.lock();
	//	g_sectors[iRow][iCol].m_setPlayerList.emplace(user.m_id);
	//	user.row = iRow;
	//	user.col = iCol;
	//	g_sectors[iRow][iCol].sector_lock.unlock();
	//}

	//CViewProcessing::GetInst()->create_nearlist(user_id, (user.y - VIEW_RADIUS / 2) / ROW_GAP,
	//	(user.x - VIEW_RADIUS / 2) / COL_GAP, (user.y + VIEW_RADIUS / 2) / ROW_GAP, (user.x + VIEW_RADIUS / 2) / COL_GAP);

	// ������ �ڵ�
	for (auto &cl : g_clients) // ���ͷ� ������ �κ� // �̺κе� ���ɿ� ���� ������.
	{
		int i = cl.m_id;
		if (user_id == i) continue;
		if (true == CViewProcessing::GetInst()->is_near_check_pp(user_id, i))
		{
			//g_clients[i].m_cl.lock();
			if (ST_SLEEP == g_clients[i].m_status){
				activate_npc(i);
			}

			if (ST_ACTIVE == g_clients[i].m_status) // �ٸ� ��������� �����ϰ� active�� �ǵ���� �Ѵ�. ������ ����� �����Ѵٰ� �Ͻ�. �ʿ��� ����.
			{
				send_enter_packet(user_id, i);
				
				if(true == is_player(i))
					send_enter_packet(i, user_id);
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

	packet.x = g_clients[o_id].x;
	packet.y = g_clients[o_id].y;
	wcscpy_s(packet.name, g_clients[o_id].name);
	packet.o_type = g_clients[o_id].m_otype;

	// ������ �ڵ�
	g_clients[user_id].m_cl.lock();
	g_clients[user_id].m_viewlist.viewlist.emplace(o_id);
	g_clients[user_id].m_cl.unlock();
	//

	send_packet(user_id, &packet);
}

void CPacketHandler::send_chat_packet(int user_id, int chatter, char mess[])
{
	sc_packet_chat packet;
	packet.id = chatter;
	packet.size = sizeof(sc_packet_chat);
	packet.type = S2C_CHAT;
	strcpy_s(packet.chat, mess);
	send_packet(user_id, &packet);
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
	send_leave_packet(user_id, user_id);
	g_clients[user_id].m_cl.lock();
	g_clients[user_id].m_status = ST_ALLOC;
	//if(!g_clients[user_id].m_bConnected) --g_curr_user_id;
	closesocket(g_clients[user_id].m_socket);

	for (int i = 0;i<NPC_START_IDX;++i)
	{
		CLIENT& cl = g_clients[i];

		if (user_id == cl.m_id) continue; // �׷��� send_leave_packet�� ������ ��

		//cl.m_cl.lock(); // �ּ�
		if (ST_ACTIVE == cl.m_status)
			send_leave_packet(cl.m_id, user_id);
		//cl.m_cl.unlock(); // �ּ�
	}

	CDBHandler::GetInst()->DB_logout(user_id);
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
	case D_UP: if (y > 0)	y--; break;// 0
	case D_DOWN: if (y < WORLD_HEIGHT - 1) y++; break; //		
	case D_LEFT: if (x > 0) x--; break;// 		
	case D_RIGHT: if (x < WORLD_WIDTH - 1) x++; break;// 
	default: cout << "Unkown Direction from Client move packet!\n";
		DebugBreak(); exit(-1);
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

void CPacketHandler::npc_move(int id, ENUMOP op) // a*�˰��� ���� �� Ÿ�ϸ��� possess�Ǿ��ִ��� Ȯ���Ѵ�.  
{												// �� ã�� ����� ����������� ���� �𸣴�

	int x = g_clients[id].x;
	int y = g_clients[id].y;

	// ���⼭ 18~20% ���̳� ����.
	//if (g_clients[id].m_nearlist.nearlist.empty() && g_clients[id].m_viewlist.viewlist.empty())
	//{
		CViewProcessing::GetInst()->create_viewlist_pn(id, (g_clients[id].y - VIEW_RADIUS / 2) / ROW_GAP,
			(g_clients[id].x - VIEW_RADIUS / 2) / COL_GAP, (g_clients[id].y + VIEW_RADIUS / 2) / ROW_GAP, (g_clients[id].x + VIEW_RADIUS / 2) / COL_GAP);
	//}

	switch (op)
	{
	case ENUMOP::OP_RANDOM_MOVE:
	{
		switch (rand() % 4)
		{
		case D_UP: if (y > 0) y--; break;
		case D_DOWN:if (y < WORLD_HEIGHT - 1) y++; break;
		case D_LEFT: if (x > 0) x--; break;
		case D_RIGHT: if (x < WORLD_WIDTH - 1) x++;	break;
		}
	}
	break;
	}

	g_clients[id].x = x;
	g_clients[id].y = y;
	if(g_clients[id].m_movenum>-1)
		g_clients[id].m_movenum -= 1;

	if (0 == g_clients[id].m_movenum)
	{
		if (false == CPacketHandler::GetInst()->is_player(id)) {
			// ��ü�� ����ִ°ɷ��ص� ������ ���̰� ����
			//CLIENT& cl = g_clients[id];
			//ZeroMemory(&cl.m_recv_over.over, sizeof(cl.m_recv_over.op));
			//cl.m_recv_over.op = OP_RANDOM_MOVE_FINISH;
			//cl.m_recv_over.player_id = id;
			//PostQueuedCompletionStatus(g_iocp, 1, cl.m_recv_over.player_id, &cl.m_recv_over.over);
			EXOVER* over = new EXOVER;
			over->op = OP_RANDOM_MOVE_FINISH;
			over->player_id = id;
			PostQueuedCompletionStatus(g_iocp, 1, g_clients[id].m_id, &over->over);
		}
	}

	if (g_clients[id].x / COL_GAP != g_clients[id].col || g_clients[id].y / ROW_GAP != g_clients[id].row)
	{
		int iPrevCol = g_clients[id].col;
		int iPrevRow = g_clients[id].row;

		int iCol = g_clients[id].x / COL_GAP;
		int iRow = g_clients[id].y / ROW_GAP;

		if (iCol > MAX_COL - 1)
			iCol = iPrevCol;
		if (iRow > MAX_ROW - 1)
			iRow = iPrevRow;

		g_sectors[iPrevRow][iPrevCol].sector_lock.lock();
		g_sectors[iPrevRow][iPrevCol].m_setPlayerList.erase(g_clients[id].m_id);
		g_sectors[iPrevRow][iPrevCol].sector_lock.unlock();

		g_sectors[iRow][iCol].sector_lock.lock();
		g_sectors[iRow][iCol].m_setPlayerList.emplace(g_clients[id].m_id);
		g_clients[id].row = iRow;
		g_clients[id].col = iCol;
		g_sectors[iRow][iCol].sector_lock.unlock();
	}

	CViewProcessing::GetInst()->create_nearlist_pn(id, (g_clients[id].y - VIEW_RADIUS / 2) / ROW_GAP,
			(g_clients[id].x - VIEW_RADIUS / 2) / COL_GAP, (g_clients[id].y + VIEW_RADIUS / 2) / ROW_GAP, (g_clients[id].x + VIEW_RADIUS / 2) / COL_GAP);


	unordered_set<int> near_vl = g_clients[id].m_nearlist.nearlist;
	unordered_set<int> view_vl = g_clients[id].m_viewlist.viewlist;

	// �������ڸ��� ������ ������ viewlist nearlist �Ѵ� ��� �������� ����.

	bool keep_alive = false;

	for (auto& n_p : near_vl)
	{
		if (ST_ACTIVE != g_clients[n_p].m_status) continue;
		if (true == CViewProcessing::GetInst()->is_near_check_pp(n_p, id))
		{
			keep_alive = true;
			g_clients[n_p].m_cl.lock();
			if (0 != g_clients[n_p].m_viewlist.viewlist.count(id)){
				g_clients[n_p].m_cl.unlock();
				CPacketHandler::GetInst()->send_move_packet(n_p, id);
			}
			else{
				g_clients[n_p].m_cl.unlock();
				CPacketHandler::GetInst()->send_enter_packet(n_p, id);
			}
		}
	}

	
	for (auto& v_p : view_vl)
	{
		if (true != CViewProcessing::GetInst()->is_near_check_pp(v_p, id)) // ��ó�� ����.
		{
			g_clients[v_p].m_cl.lock();
			if (0 != g_clients[v_p].m_viewlist.viewlist.count(id))	{ // �÷��̾� viewlist�� �ִ�
				g_clients[v_p].m_cl.unlock();
				CPacketHandler::GetInst()->send_leave_packet(v_p, id);
			}
			else{
				g_clients[v_p].m_cl.unlock();
			}
		}
	}

	if (true == keep_alive) addtimer(id, OP_RANDOM_MOVE, 1000);
	else g_clients[id].m_status = ST_SLEEP;
}

void CPacketHandler::activate_npc(int id)
{
	C_STATUS old_state = C_STATUS::ST_SLEEP;
	if (true == atomic_compare_exchange_strong(&g_clients[id].m_status, &old_state, ST_ACTIVE)) // �ѹ��� addtimer���� 
		addtimer(id, OP_RANDOM_MOVE, 1000);
}