#pragma once
#include "ViewProcessing.h"
#include "extern.h"
#include "PacketHandler.h"


CViewProcessing::CViewProcessing()
{
}

CViewProcessing::~CViewProcessing()
{
}

void CViewProcessing::create_nearlist(int user_id, int row_start, int col_start, int row_end, int col_end)
{
	if (ST_ACTIVE != g_clients[user_id].m_status) // 시작시 npc띄우는것때메  // 이거 안하면 터짐
		return;

	g_clients[user_id].m_cl.lock(); // 둘
	unordered_set <int> near_vl;
	near_vl.clear();
	g_clients[user_id].m_cl.unlock();

	for (int i = col_start; i <= col_end; ++i)	{
		for (int j = row_start; j <= row_end; ++j)		{
			int iCol = -1;
			int iRow = -1;

			iCol = i;
			iRow = j;

			if (iCol < 0) iCol = 0;
			if (iRow < 0) iRow = 0;

			if (iCol > MAX_COL - 1)	iCol = MAX_COL - 1;
			if (iRow > MAX_ROW - 1) iRow = MAX_ROW - 1;

			g_sectors[iRow][iCol].sector_lock.lock(); // 섹터락
			for (auto& playerlist : g_sectors[iRow][iCol].m_setPlayerList) {
				if (is_near_check_pp(user_id, playerlist)) {
					if (false == CPacketHandler::GetInst()->is_player(playerlist)) {
						//CLIENT& cl = g_clients[playerlist];
						//ZeroMemory(&cl.m_recv_over.over, sizeof(cl.m_recv_over.op));
						//cl.m_recv_over.op = OP_PLAYER_MOVE;
						//cl.m_recv_over.player_id = playerlist;
						//PostQueuedCompletionStatus(g_iocp, 1, cl.m_recv_over.player_id, &cl.m_recv_over.over);

						EXOVER* over = new EXOVER;
						over->op = OP_PLAYER_MOVE;
						over->player_id = user_id;
						PostQueuedCompletionStatus(g_iocp, 1, g_clients[playerlist].m_id, &over->over);
					}
					near_vl.emplace(playerlist);
				}
			}
			g_sectors[iRow][iCol].sector_lock.unlock();
		}
	}

	g_clients[user_id].m_cl.lock(); // 넷
	g_clients[user_id].m_nearlist.nearlist = near_vl;
	g_clients[user_id].m_cl.unlock();
}

void CViewProcessing::create_nearlist_pn(int npc_id, int row_start, int col_start, int row_end, int col_end)
{
	if (ST_ACTIVE != g_clients[npc_id].m_status) // 시작시 npc띄우는것때메  // 이거 안하면 터짐
		return;

	unordered_set <int> near_vl;
	near_vl.clear();

	for (int i = col_start; i <= col_end; ++i)	{
		for (int j = row_start; j <= row_end; ++j)	{
			int iCol = -1;
			int iRow = -1;

			iCol = i;
			iRow = j;

			if (iCol < 0) iCol = 0;
			if (iRow < 0) iRow = 0;

			if (iCol > MAX_COL - 1)	iCol = MAX_COL - 1;
			if (iRow > MAX_ROW - 1) iRow = MAX_ROW - 1;

			g_sectors[iRow][iCol].sector_lock.lock(); // 섹터락
			for (auto& playerlist : g_sectors[iRow][iCol].m_setPlayerList) {
				if (CPacketHandler::GetInst()->is_player(playerlist)) {
					if (is_near_check_pp(npc_id, playerlist))
						near_vl.emplace(playerlist);
				}
			}
			g_sectors[iRow][iCol].sector_lock.unlock();
		}
	}
		
	g_clients[npc_id].m_nearlist.nearlist = near_vl;
}

void CViewProcessing::create_viewlist_pn(int npc_id, int row_start, int col_start, int row_end, int col_end)
{
	if (ST_ACTIVE != g_clients[npc_id].m_status) // 시작시 npc띄우는것때메  // 이거 안하면 터짐
		return;

	unordered_set <int> view_vl;
	view_vl.clear();

	for (int i = col_start; i <= col_end; ++i){
		for (int j = row_start; j <= row_end; ++j){
			int iCol = -1;	int iRow = -1;
			iCol = i;	iRow = j;

			if (iCol < 0) iCol = 0;
			if (iRow < 0) iRow = 0;

			if (iCol > MAX_COL - 1)	iCol = MAX_COL - 1;
			if (iRow > MAX_ROW - 1) iRow = MAX_ROW - 1;

			g_sectors[iRow][iCol].sector_lock.lock(); // 섹터락
			for (auto& playerlist : g_sectors[iRow][iCol].m_setPlayerList) {
				if (CPacketHandler::GetInst()->is_player(playerlist)) {
					if (is_near_check_pp(npc_id, playerlist))
						view_vl.emplace(playerlist);
				}
			}
			g_sectors[iRow][iCol].sector_lock.unlock();
		}
	}
	g_clients[npc_id].m_viewlist.viewlist = view_vl;
}



void CViewProcessing::check_near_view(int user_id)
{
	// 일단 나를 nearlist에 등록하고 시작 - create_nearlist에서 // remove 넣기

	g_clients[user_id].m_cl.lock();
	unordered_set <int> old_vl = g_clients[user_id].m_viewlist.viewlist;
	unordered_set <int> new_vl = g_clients[user_id].m_nearlist.nearlist;
	g_clients[user_id].m_cl.unlock();

	// ST_SLEEP activate_npc 다른 부분에서 해야할 수도 수업  12-5, 21분
	for (auto np : new_vl)
	{
		if (0 == old_vl.count(np)) // viewlist에 nearlist가 없다.
		{
			CPacketHandler::GetInst()->send_enter_packet(user_id, np);
			if (false == CPacketHandler::GetInst()->is_player(np)) {
				CPacketHandler::GetInst()->activate_npc(np);
				
				continue;
			}
			g_clients[np].m_cl.lock();
			if (0 == g_clients[np].m_viewlist.viewlist.count(user_id))	{
				g_clients[np].m_cl.unlock();
				CPacketHandler::GetInst()->send_enter_packet(np, user_id);
			}
			else{
				g_clients[np].m_cl.unlock();
				CPacketHandler::GetInst()->send_move_packet(np, user_id);
			}
		}
		else // viewlist에 nearlist가 있다.
		{
			if (false == CPacketHandler::GetInst()->is_player(np)) continue;

			g_clients[np].m_cl.lock();
			if (0 != g_clients[np].m_viewlist.viewlist.count(user_id))	{
				g_clients[np].m_cl.unlock();
				CPacketHandler::GetInst()->send_move_packet(np, user_id);
			}
			else{
				g_clients[np].m_cl.unlock();
				CPacketHandler::GetInst()->send_enter_packet(np, user_id);
			}
		}
	}

	for (auto old_p : old_vl)
	{
		if (0 == new_vl.count(old_p))
		{
			CPacketHandler::GetInst()->send_leave_packet(user_id, old_p);
			if (false == CPacketHandler::GetInst()->is_player(old_p)) continue;

			g_clients[old_p].m_cl.lock();
			if (0 != g_clients[old_p].m_viewlist.viewlist.count(user_id)){
				g_clients[old_p].m_cl.unlock();
				CPacketHandler::GetInst()->send_leave_packet(old_p, user_id);
			}
			else{
				g_clients[old_p].m_cl.unlock();
			}
		}
	}
}

void CViewProcessing::check_near_view_pn(int npc_id)
{
	g_clients[npc_id].m_cl.lock();
	unordered_set <int> old_vl = g_clients[npc_id].m_viewlist.viewlist;
	unordered_set <int> new_vl = g_clients[npc_id].m_nearlist.nearlist;
	g_clients[npc_id].m_cl.unlock();

	for (auto np : new_vl)
	{
		g_clients[npc_id].m_cl.lock();
		if (0 != g_clients[npc_id].m_viewlist.viewlist.count(np)){
			g_clients[npc_id].m_cl.unlock();
			CPacketHandler::GetInst()->send_move_packet(np, npc_id);
		}
		else{
			g_clients[npc_id].m_cl.unlock();
			CPacketHandler::GetInst()->send_enter_packet(np, npc_id);
		}
	}

	for (auto old_p : old_vl)
	{
		g_clients[npc_id].m_cl.lock();
		if (0 != g_clients[npc_id].m_viewlist.viewlist.count(old_p)){
			g_clients[npc_id].m_cl.unlock();
			CPacketHandler::GetInst()->send_leave_packet(old_p, npc_id);
		}
		else{
			g_clients[npc_id].m_cl.unlock();
		}
	}
}


bool CViewProcessing::is_near_check_pp(int id1, int id2)
{
	if (VIEW_RADIUS / 2 < abs(g_clients[id1].x - g_clients[id2].x))
		return false;
	if (VIEW_RADIUS / 2 < abs(g_clients[id1].y - g_clients[id2].y))
		return false;

	return true;
}
