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
	g_sectors[row][col].sector_lock.lock();
	for (auto& playerlist : g_sectors[row][col].m_setPlayerList) {
		//g_clients[user_id].m_cl.lock();
		if (ST_ACTIVE == g_clients[user_id].m_status)
		{
			if (is_near_check(user_id, playerlist)) {
				g_clients[user_id].m_nearlist.nearlist.emplace(playerlist);
			}
		}
		//g_clients[user_id].m_cl.unlock();
	}
	g_sectors[row][col].sector_lock.unlock();
}

void CViewProcessing::check_near_view(int user_id)
{
	ViewList user_VL = g_clients[user_id].m_viewlist;
	NearList user_NL = g_clients[user_id].m_nearlist;
	RemoveList user_RL = g_clients[user_id].m_removelist;

	for (auto& nearlist : g_clients[user_id].m_nearlist.nearlist) // 모든 near객체에 대해서 
	{
		ViewList near_VL = g_clients[nearlist].m_viewlist;

		if (user_VL.viewlist.find(nearlist) != user_VL.viewlist.end()) // user의 viewlist에 있으면
		{
			if (near_VL.viewlist.find(user_id) != near_VL.viewlist.end()) // 상대방 viewlist에 있으면
			{
				//g_clients[nearlist].m_cl.lock();
				if (ST_ACTIVE == g_clients[nearlist].m_status)
				{
					CPacketHandler::GetInst()->send_move_packet(nearlist, user_id);
				}
				//g_clients[nearlist].m_cl.unlock();
			}
			else // 상대방 viewlist에 없으면
			{
				//g_clients[nearlist].m_cl.unlock();
				if (ST_ACTIVE == g_clients[nearlist].m_status)
				{
					near_VL.viewlist.emplace(user_id);
					CPacketHandler::GetInst()->send_enter_packet(nearlist, user_id);
				}
				//g_clients[nearlist].m_cl.unlock();
			}
		}
		else // user의 viewlist에 없으면
		{
			//g_clients[user_id].m_cl.lock(); // 체크
			if (ST_ACTIVE == g_clients[user_id].m_status)
			{
				user_VL.viewlist.emplace(nearlist);
				CPacketHandler::GetInst()->send_enter_packet(user_id, nearlist);
			}
			//g_clients[user_id].m_cl.unlock();

			if (near_VL.viewlist.find(user_id) != near_VL.viewlist.end()) // 상대방 viewlist에 있으면
			{
				//g_clients[nearlist].m_cl.lock();
				if (ST_ACTIVE == g_clients[nearlist].m_status)
				{
					CPacketHandler::GetInst()->send_move_packet(nearlist, user_id);
				}
				//g_clients[nearlist].m_cl.unlock();
			}
			else // 상대방 viewlist에 없으면
			{
				//g_clients[nearlist].m_cl.lock();
				if (ST_ACTIVE == g_clients[nearlist].m_status)
				{
					near_VL.viewlist.emplace(user_id);
					CPacketHandler::GetInst()->send_enter_packet(nearlist, user_id);
				}
				//g_clients[nearlist].m_cl.unlock();
			}
		}
		g_clients[nearlist].m_cl.lock();
		g_clients[nearlist].m_viewlist = near_VL;
		g_clients[nearlist].m_cl.unlock();
	}

	for (auto& viewlist : user_VL.viewlist)
	{
		// nearlist 에서 viewlist를 못찾으면 viwwlist를 removelist 로 넣는다.
		if (user_NL.nearlist.find(viewlist) == user_NL.nearlist.end())
		{
			//g_clients[user_id].m_cl.lock();
			user_RL.removelist.emplace(viewlist);
			//g_clients[user_id].m_cl.unlock();
		}
	}

	for (auto& removelist : user_RL.removelist)
	{
		//g_clients[user_id].m_cl.lock();
		if (ST_ACTIVE == g_clients[user_id].m_status)
		{
			user_VL.viewlist.erase(removelist);
			CPacketHandler::GetInst()->send_leave_packet(user_id, removelist);
		}
		//g_clients[user_id].m_cl.unlock();

		ViewList rmlist_VL = g_clients[removelist].m_viewlist;

		if (rmlist_VL.viewlist.find(user_id) != rmlist_VL.viewlist.end())
		{
			//g_clients[removelist].m_cl.lock();
			if (ST_ACTIVE == g_clients[removelist].m_status)
			{
				rmlist_VL.viewlist.erase(user_id);
				CPacketHandler::GetInst()->send_leave_packet(removelist, user_id);
			}
			//g_clients[removelist].m_cl.unlock();
		}

		g_clients[removelist].m_cl.lock();
		g_clients[removelist].m_viewlist = rmlist_VL;
		g_clients[removelist].m_cl.unlock();
	}

	g_clients[user_id].m_cl.lock();
	g_clients[user_id].m_viewlist = user_VL;
	g_clients[user_id].m_cl.unlock();

	g_clients[user_id].m_nearlist.nearlist.clear();
	g_clients[user_id].m_removelist.removelist.clear();
}

bool CViewProcessing::is_near_check(int id1, int id2)
{
	if (SIGHT_RANGE / 2 < abs(g_clients[id1].x - g_clients[id2].x))
		return false;
	if (SIGHT_RANGE / 2 < abs(g_clients[id1].y - g_clients[id2].y))
		return false;

	return true;
}
