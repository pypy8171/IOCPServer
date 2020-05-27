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

void CViewProcessing::create_nearlist(int user_id, int row, int col)
{
	if (ST_ACTIVE != g_clients[user_id].m_status)
		return;

	g_clients[user_id].m_cl.lock();
	unordered_set <int> near_vl = g_clients[user_id].m_nearlist.nearlist;
	g_clients[user_id].m_cl.unlock();

	g_sectors[row][col].sector_lock.lock();
	for (auto& playerlist : g_sectors[row][col].m_setPlayerList) {
		if (is_near_check(user_id, playerlist)) {
			near_vl.emplace(playerlist);
		}
	}
	g_sectors[row][col].sector_lock.unlock();

	g_clients[user_id].m_cl.lock();
	g_clients[user_id].m_nearlist.nearlist = near_vl;
	g_clients[user_id].m_cl.unlock();
}

void CViewProcessing::check_near_view(int user_id)
{
	// �ϴ� ���� nearlist�� ����ϰ� ���� - create_nearlist���� // remove �ֱ�
	// �� �ڵ� + ������ �ڵ� ����
	g_clients[user_id].m_cl.lock();
	unordered_set <int> old_vl = g_clients[user_id].m_viewlist.viewlist;
	unordered_set <int> new_vl = g_clients[user_id].m_nearlist.nearlist;
	g_clients[user_id].m_cl.unlock();

	for (auto np : new_vl)
	{
		if (0 == old_vl.count(np))
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
		else
		{
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

	for (auto old_p : old_vl)
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

	g_clients[user_id].m_cl.lock();
	g_clients[user_id].m_viewlist.viewlist = new_vl;
	g_clients[user_id].m_nearlist.nearlist.clear();
	g_clients[user_id].m_cl.unlock();
}

bool CViewProcessing::is_near_check(int id1, int id2)
{
	if (VIEW_RADIUS / 2 < abs(g_clients[id1].x - g_clients[id2].x))
		return false;
	if (VIEW_RADIUS / 2 < abs(g_clients[id1].y - g_clients[id2].y))
		return false;

	return true;
}


//void CViewProcessing::check_near_view(int user_id)
//{
//	ViewList user_VL = g_clients[user_id].m_viewlist;
//	NearList user_NL = g_clients[user_id].m_nearlist;
//	RemoveList user_RL = g_clients[user_id].m_removelist;
//
//	for (auto& nearlist : g_clients[user_id].m_nearlist.nearlist) // ��� near��ü�� ���ؼ� 
//	{
//		ViewList near_VL = g_clients[nearlist].m_viewlist;
//
//		if (user_VL.viewlist.find(nearlist) != user_VL.viewlist.end()) // user�� viewlist�� ������
//		{
//			if (near_VL.viewlist.find(user_id) != near_VL.viewlist.end()) // ���� viewlist�� ������
//			{
//				//g_clients[nearlist].m_cl.lock();
//				if (ST_ACTIVE == g_clients[nearlist].m_status)
//				{
//					CPacketHandler::GetInst()->send_move_packet(nearlist, user_id);
//				}
//				//g_clients[nearlist].m_cl.unlock();
//			}
//			else // ���� viewlist�� ������
//			{
//				//g_clients[nearlist].m_cl.unlock();
//				if (ST_ACTIVE == g_clients[nearlist].m_status)
//				{
//					near_VL.viewlist.emplace(user_id);
//					CPacketHandler::GetInst()->send_enter_packet(nearlist, user_id);
//				}
//				//g_clients[nearlist].m_cl.unlock();
//			}
//		}
//		else // user�� viewlist�� ������
//		{
//			//g_clients[user_id].m_cl.lock(); // üũ
//			if (ST_ACTIVE == g_clients[user_id].m_status)
//			{
//				user_VL.viewlist.emplace(nearlist);
//				CPacketHandler::GetInst()->send_enter_packet(user_id, nearlist);
//			}
//			//g_clients[user_id].m_cl.unlock();
//
//			if (near_VL.viewlist.find(user_id) != near_VL.viewlist.end()) // ���� viewlist�� ������
//			{
//				//g_clients[nearlist].m_cl.lock();
//				if (ST_ACTIVE == g_clients[nearlist].m_status)
//				{
//					CPacketHandler::GetInst()->send_move_packet(nearlist, user_id);
//				}
//				//g_clients[nearlist].m_cl.unlock();
//			}
//			else // ���� viewlist�� ������
//			{
//				//g_clients[nearlist].m_cl.lock();
//				if (ST_ACTIVE == g_clients[nearlist].m_status)
//				{
//					near_VL.viewlist.emplace(user_id);
//					CPacketHandler::GetInst()->send_enter_packet(nearlist, user_id);
//				}
//				//g_clients[nearlist].m_cl.unlock();
//			}
//		}
//		g_clients[nearlist].m_cl.lock();
//		g_clients[nearlist].m_viewlist = near_VL;
//		g_clients[nearlist].m_cl.unlock();
//	}
//
//	for (auto& viewlist : user_VL.viewlist)
//	{
//		// nearlist ���� viewlist�� ��ã���� viwwlist�� removelist �� �ִ´�.
//		if (user_NL.nearlist.find(viewlist) == user_NL.nearlist.end())
//		{
//			//g_clients[user_id].m_cl.lock();
//			user_RL.removelist.emplace(viewlist);
//			//g_clients[user_id].m_cl.unlock();
//		}
//	}
//
//	for (auto& removelist : user_RL.removelist)
//	{
//		//g_clients[user_id].m_cl.lock();
//		if (ST_ACTIVE == g_clients[user_id].m_status)
//		{
//			user_VL.viewlist.erase(removelist);
//			CPacketHandler::GetInst()->send_leave_packet(user_id, removelist);
//		}
//		//g_clients[user_id].m_cl.unlock();
//
//		ViewList rmlist_VL = g_clients[removelist].m_viewlist;
//
//		if (rmlist_VL.viewlist.find(user_id) != rmlist_VL.viewlist.end())
//		{
//			//g_clients[removelist].m_cl.lock();
//			if (ST_ACTIVE == g_clients[removelist].m_status)
//			{
//				rmlist_VL.viewlist.erase(user_id);
//				CPacketHandler::GetInst()->send_leave_packet(removelist, user_id);
//			}
//			//g_clients[removelist].m_cl.unlock();
//		}
//
//		g_clients[removelist].m_cl.lock();
//		g_clients[removelist].m_viewlist = rmlist_VL;
//		g_clients[removelist].m_cl.unlock();
//	}
//
//	g_clients[user_id].m_cl.lock();
//	g_clients[user_id].m_viewlist = user_VL;
//	g_clients[user_id].m_cl.unlock();
//
//	g_clients[user_id].m_nearlist.nearlist.clear();
//	g_clients[user_id].m_removelist.removelist.clear();
//
//}