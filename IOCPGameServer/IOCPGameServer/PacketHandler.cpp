//#include "PacketHandler.h"
//
//#include "MovePacket.h"
//#include "LoginPacket.h"
//
//#include <winbase.h>
//
//CPacketHandler::CPacketHandler()
//	: m_pLoginPacket(nullptr)
//	, m_pMovePacket(nullptr)
//{
//}
//
//CPacketHandler::~CPacketHandler()
//{
//}
//
//void CPacketHandler::process_packet(int user_id, char buf[])
//{
//	switch (buf[1])
//	{
//	case C2S_LOGIN:
//	{
//		cs_packet_login* packet = reinterpret_cast<cs_packet_login*>(buf);
//
//		if (nullptr == m_pLoginPacket)
//			m_pLoginPacket = new CLoginPacket();
//
//		(m_pLoginPacket)->enter_game(user_id, packet->name);
//	}
//	break;
//	case C2S_MOVE:
//	{
//		cs_packet_move* packet = reinterpret_cast<cs_packet_move*>(buf);
//
//		if (nullptr == m_pMovePacket)
//			m_pMovePacket = new CMovePacket();
//
//		(m_pMovePacket)->do_move(user_id, packet->direction);
//	}
//	break;
//	default:
//	{
//		//cout << "Unkown packet type error!\n";
//		//DebugBreak(); // 비쥬얼 스튜디오 상에서 멈추고 상태를 표시하라
//		//exit(-1);
//	}
//	}
//}
//
//void CPacketHandler::send_packet(int user_id, void* packet)
//{
//	//char* buf = reinterpret_cast<char*>(packet);
//
//	//CLIENT& user = g_clients[user_id];
//
//	//EXOVER* exover = new EXOVER; // recv에서 사용하고 있으므로 새로 할당해서 사용
//	//exover->op = OP_SEND;
//	//ZeroMemory(&exover->over, sizeof(exover->over));
//	//exover->wsabuf.buf = exover->io_buf;
//	//exover->wsabuf.len = buf[0];
//
//	//memcpy(exover->io_buf, buf, buf[0]);
//
//	//WSASend(user.m_socket, &exover->wsabuf, 1, NULL, 0, &exover->over, NULL);
//}
