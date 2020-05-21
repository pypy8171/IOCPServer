//#pragma once
//#include "define.h"
//#include "protocol.h"
//
//class CLoginPacket;
//class CMovePacket;
//
//enum class ERROR{ERROR, SUCCESS, BIND_ERROR, LISTEN_ERROR, ACCEPT_ERROR, LOGIN_ERROR,MOVE_ERROR};
//
//class CPacketHandler
//{
//private:
//	ERROR			m_eError;
//
//	CLoginPacket*	m_pLoginPacket;
//	CMovePacket*	m_pMovePacket ;
//
//public:
//	void process_packet(int user_id, char buf[]);
//	void send_packet(int user_id, void* packet);
//
//public:
//	SINGLE(CPacketHandler);
//};
//
