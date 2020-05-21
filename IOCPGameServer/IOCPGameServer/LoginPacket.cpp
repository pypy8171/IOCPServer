//#include "LoginPacket.h"
//#include "PacketHandler.h"
//
//CLoginPacket::CLoginPacket()
//{
//}
//
//CLoginPacket::~CLoginPacket()
//{
//}
//
//void CLoginPacket::enter_game(int user_id, char buf[])
//{
//	//g_clients[user_id].m_cl.lock();
//	//strcpy_s(g_clients[user_id].name, name);
//	//g_clients[user_id].name[MAX_ID_LEN] = NULL;
//	//send_login_ok_packet(user_id);
//
//	//for (int i = 0; i < MAX_USER; ++i)
//	//{
//	//	if (user_id == i) // send_enter_packet 역시도 같음 // 내가 나한테 send enter packet을 보낼 이유가 없음
//	//		continue;
//
//	//	g_clients[i].m_cl.lock();
//	//	if (ST_ACTIVE == g_clients[i].m_status)
//	//	{
//	//		if (i != user_id)
//	//		{
//	//			send_enter_packet(user_id, i);
//	//			send_enter_packet(i, user_id);
//	//		}
//	//	}
//	//	g_clients[i].m_cl.unlock();
//	//}
//	//g_clients[user_id].m_cl.unlock();
//	//g_clients[user_id].m_status = ST_ACTIVE;
//}
//
//void CLoginPacket::send_login_ok_packet(int user_id)
//{
//	//sc_packet_login_ok packet;
//	//packet.exp = 0;
//	//packet.hp = 0;
//	//packet.id = user_id;
//	//packet.level = 0;
//	//packet.size = sizeof(packet);
//	//packet.type = S2C_LOGIN_OK;
//	//packet.x = g_clients[user_id].x;
//	//packet.y = g_clients[user_id].y;
//
//	//CPacketHandler::GetInst()->send_packet(user_id, &packet);
//}
//
//void CLoginPacket::send_enter_packet(int user_id, int o_user_id)
//{
//	//sc_packet_enter packet;
//	//packet.id = o_id;
//	//packet.size = sizeof(packet);
//	//packet.type = S2C_ENTER;
//	//packet.x = g_clients[o_id].x;
//	//packet.y = g_clients[o_id].y;
//	//strcpy_s(packet.name, g_clients[o_id].name);
//	//packet.o_type = O_PLAYER;
//
//	//CPacketHandler::GetInst()->send_packet(user_id, &packet);
//}
