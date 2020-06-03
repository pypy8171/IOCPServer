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
	if (ST_ACTIVE != g_clients[user_id].m_status) // ���۽� npc���°Ͷ���  // �̰� ���ϸ� ����
		return;

	g_clients[user_id].m_cl.lock(); // ��
	unordered_set <int> near_vl;
	near_vl.clear();
	g_clients[user_id].m_cl.unlock();

	for (int i = col_start; i <= col_end; ++i)
	{
		for (int j = row_start; j <= row_end; ++j)
		{
			int iCol = -1;
			int iRow = -1;

			iCol = i;
			iRow = j;

			if (iCol < 0) iCol = 0;
			if (iRow < 0) iRow = 0;

			if (iCol > MAX_COL - 1)	iCol = MAX_COL - 1;
			if (iRow > MAX_ROW - 1) iRow = MAX_ROW - 1;

			g_sectors[iRow][iCol].sector_lock.lock(); // ���Ͷ�
			for (auto& playerlist : g_sectors[iRow][iCol].m_setPlayerList) {
				if (is_near_check_pp(user_id, playerlist)) {
					near_vl.emplace(playerlist);
				}
			}
			g_sectors[iRow][iCol].sector_lock.unlock();

			g_sectors[iRow][iCol].sector_lock.lock(); // ���Ͷ�
			for (auto& npclist : g_sectors[iRow][iCol].m_setNpcList)
			{
				if (is_near_check_pn(user_id, npclist - NPC_START_IDX)) { // npc�� idx�� ������
					near_vl.emplace(npclist); // npc�� id���� ������
				}
			}
			g_sectors[iRow][iCol].sector_lock.unlock();
		}
	}

	g_clients[user_id].m_cl.lock(); // ��
	g_clients[user_id].m_nearlist.nearlist = near_vl;
	g_clients[user_id].m_cl.unlock();
}

//void CViewProcessing::create_nearlist_pn(int user_id, int row_start, int col_start, int row_end, int col_end)
//{
//	if (ST_ACTIVE != g_clients[user_id].m_status) // ���۽� npc���°Ͷ���  // �̰� ���ϸ� ����
//		return;
//
//	g_clients[user_id].m_cl.lock(); // ��
//	unordered_set <int> near_vl = g_clients[user_id].m_nearlist.nearlist;
//	//near_vl.clear();
//	g_clients[user_id].m_cl.unlock();
//
//	for (int i = col_start; i <= col_end; ++i)
//	{
//		for (int j = row_start; j <= row_end; ++j)
//		{
//			int iCol = -1;
//			int iRow = -1;
//
//			iCol = i;
//			iRow = j;
//
//			if (iCol < 0) iCol = 0;
//			if (iRow < 0) iRow = 0;
//
//			if (iCol > MAX_COL - 1)	iCol = MAX_COL - 1;
//			if (iRow > MAX_ROW - 1) iRow = MAX_ROW - 1;
//
//			//g_sectors[iRow][iCol].sector_lock.lock(); // ���Ͷ�
//			for (auto& npclist : g_sectors[iRow][iCol].m_setNpcList)
//			{
//				if (is_near_check_pn(user_id, npclist - NPC_START_IDX)) { // npc�� idx�� ������
//					near_vl.emplace(npclist); // npc�� id���� ������
//				}
//			}
//			//g_sectors[iRow][iCol].sector_lock.unlock();
//		}
//	}
//
//	g_clients[user_id].m_cl.lock(); // ��
//	g_clients[user_id].m_nearlist.nearlist = near_vl;
//	g_clients[user_id].m_cl.unlock();
//}


void CViewProcessing::check_near_view(int user_id)
{
	// �ϴ� ���� nearlist�� ����ϰ� ���� - create_nearlist���� // remove �ֱ�

	g_clients[user_id].m_cl.lock();
	unordered_set <int> old_vl = g_clients[user_id].m_viewlist.viewlist;
	unordered_set <int> new_vl = g_clients[user_id].m_nearlist.nearlist;
	g_clients[user_id].m_cl.unlock();

	for (auto np : new_vl)
	{
		if (np < NPC_START_IDX)
		{
			if (0 == old_vl.count(np)) // viewlist�� nearlist�� ����.
			{
				CPacketHandler::GetInst()->send_enter_packet(user_id, np);
				g_clients[np].m_cl.lock();
				if (0 == g_clients[np].m_viewlist.viewlist.count(user_id))
				{
					g_clients[np].m_cl.unlock();
					CPacketHandler::GetInst()->send_enter_packet(np, user_id);
				}
				else
				{
					g_clients[np].m_cl.unlock();
					CPacketHandler::GetInst()->send_move_packet(np, user_id);
				}
			}
			else // viewlist�� nearlist�� �ִ�.
			{
				//CPacketHandler::GetInst()->send_move_packet(user_id, np);
				g_clients[np].m_cl.lock();
				if (0 != g_clients[np].m_viewlist.viewlist.count(user_id))
				{
					g_clients[np].m_cl.unlock();
					CPacketHandler::GetInst()->send_move_packet(np, user_id);

				}
				else
				{
					g_clients[np].m_cl.unlock();
					CPacketHandler::GetInst()->send_enter_packet(np, user_id);
				}
			}
		}
		else
		{
			if (0 == old_vl.count(np)) // view����Ʈ�� ����.
			{
				int idx = np - NPC_START_IDX;
				CPacketHandler::GetInst()->send_enter_packet(user_id, np);
			}
			else // viewlist�� �ִ�.
			{
				int idx = np - NPC_START_IDX;
				if (S_ACTIVE == g_npcs[idx].m_Status)
				{
					CPacketHandler::GetInst()->send_move_packet(user_id, np);
				}
				else
					CPacketHandler::GetInst()->send_enter_packet(user_id, np);
			}
		}
	}

	for (auto old_p : old_vl)
	{
		if (old_p < NPC_START_IDX)
		{
			if (0 == new_vl.count(old_p))
			{
				CPacketHandler::GetInst()->send_leave_packet(user_id, old_p);
				g_clients[old_p].m_cl.lock();
				if (0 != g_clients[old_p].m_viewlist.viewlist.count(user_id))
				{
					g_clients[old_p].m_cl.unlock();
					CPacketHandler::GetInst()->send_leave_packet(old_p, user_id);
				}
				else
				{
					g_clients[old_p].m_cl.unlock();
				}
			}
		}
		else
		{
			if (0 == new_vl.count(old_p))
			{
				int idx = old_p - NPC_START_IDX;
				CPacketHandler::GetInst()->send_leave_packet(user_id, old_p); // viewlist���� npc idx�� �ƴ� npc�� id�� ������
			}
		}
	}
}

//void CViewProcessing::check_near_view_pn(int user_id)
//{
//	g_clients[user_id].m_cl.lock();
//	unordered_set <int> old_vl = g_clients[user_id].m_viewlist.viewlist;
//	unordered_set <int> new_vl = g_clients[user_id].m_nearlist.nearlist;
//	g_clients[user_id].m_cl.unlock();
//
//	for (auto np : new_vl)
//	{
//		if (np >= NPC_START_IDX)
//		{
//			if (0 == old_vl.count(np)) // view����Ʈ�� ����.
//			{
//				int idx = np - NPC_START_IDX;
//				CPacketHandler::GetInst()->send_enter_packet(user_id, np);
//			}
//			else // viewlist�� �ִ�.
//			{
//				int idx = np - NPC_START_IDX;
//				if (S_ACTIVE == g_npcs[idx].m_Status)
//				{
//					CPacketHandler::GetInst()->send_move_packet(user_id, np);
//				}
//				else
//					CPacketHandler::GetInst()->send_enter_packet(user_id, np);
//			}
//		}
//	}
//
//	for (auto old_p : old_vl)
//	{
//		if (old_p >= NPC_START_IDX)
//		{
//			if (0 == new_vl.count(old_p))
//			{
//				int idx = old_p - NPC_START_IDX;
//				CPacketHandler::GetInst()->send_leave_packet(user_id, old_p); // viewlist���� npc idx�� �ƴ� npc�� id�� ������
//				g_npcs[idx].m_Status = S_NONACTIVE;
//			}
//		}
//	}
//
//	g_clients[user_id].m_cl.lock();
//	g_clients[user_id].m_viewlist.viewlist = new_vl;
//	g_clients[user_id].m_cl.unlock();
//}

bool CViewProcessing::is_near_check_pp(int id1, int id2)
{
	if (VIEW_RADIUS / 2 < abs(g_clients[id1].x - g_clients[id2].x))
		return false;
	if (VIEW_RADIUS / 2 < abs(g_clients[id1].y - g_clients[id2].y))
		return false;

	return true;
}

bool CViewProcessing::is_near_check_pn(int id1, int id2)
{
	if (VIEW_RADIUS / 2 < abs(g_clients[id1].x - g_npcs[id2].x))
		return false;
	if (VIEW_RADIUS / 2 < abs(g_clients[id1].y - g_npcs[id2].y))
		return false;

	//g_npcs[id2].m_Status = NPC_STATUS::S_ACTIVE;
	return true;
}
