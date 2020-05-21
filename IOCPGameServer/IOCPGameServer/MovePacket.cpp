//#include "MovePacket.h"
//#include "PacketHandler.h"
//
//CMovePacket::CMovePacket()
//{
//}
//
//CMovePacket::~CMovePacket()
//{
//}
//
//void CMovePacket::do_move(int user_id, int dir)
//{
//	//CLIENT& user = g_clients[user_id];
//	//int x = user.x;
//	//int y = user.y;
//
//	//switch (direction)
//	//{
//	//case D_UP:
//	//	if (y > 0)	y--;
//	//	break;
//	//case D_DOWN:
//	//	if (y < WORLD_HEIGHT - 1) y++;
//	//	break;
//	//case D_LEFT:
//	//	if (x > 0) x--;
//	//	break;
//	//case D_RIGHT:
//	//	if (x < WORLD_WIDTH - 1) x++;
//	//	break;
//	//default:
//	//	cout << "Unkown Direction from Client move packet!\n";
//	//	DebugBreak();
//	//	exit(-1);
//	//}
//
//	//user.x = x;
//	//user.y = y;
//
//	//for (auto& clients : g_clients)
//	//{
//	//	clients.m_cl.lock();
//	//	if (ST_ACTIVE == clients.m_status)
//	//	{
//	//		send_move_packet(clients.m_id, user_id);
//	//	}
//	//	clients.m_cl.unlock();
//	//}
//}
//
//void CMovePacket::send_move_packet(int c_id, int user_id)
//{
//	//sc_packet_move packet;
//	//packet.id = mover;
//	//packet.size = sizeof(packet);
//	//packet.type = S2C_MOVE;
//	//packet.x = g_clients[mover].x;
//	//packet.y = g_clients[mover].y;
//
//	//CPacketHandler::GetInst()->send_packet(user_id, &packet);
//}
