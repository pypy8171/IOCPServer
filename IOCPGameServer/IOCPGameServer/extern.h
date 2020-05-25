#pragma once
#include <WS2tcpip.h>
#include <MSWSock.h>
#include "protocol.h"

#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "mswsock.lib")

enum ENUMOP { OP_RECV = 0, OP_SEND, OP_ACCEPT };

struct EXOVER
{
	WSAOVERLAPPED	over;
	ENUMOP			op;
	char			io_buf[MAX_BUF_SIZE];
	union {
		WSABUF			wsabuf;
		SOCKET			c_socket;
	};
};

struct CLIENT
{
	mutex		m_cl;
	SOCKET		m_socket;
	int			m_id;
	EXOVER		m_recv_over;
	int			m_prev_size;
	char		m_packet_buf[MAX_PACKET_SIZE];
	C_STATUS	m_status;

	short		x, y;
	char		name[MAX_ID_LEN + 1]; // 꽉차서 올수 있으므로 +1
	unsigned	m_move_time;

	short		row, col;
	ViewList	m_viewlist;
	NearList	m_nearlist;
	RemoveList	m_removelist;
};

struct SECTOR
{
	mutex						sector_lock;
	unordered_set<int>			m_setPlayerList;
	short						m_StartX;
	short						m_StartY;
	short						m_EndX;
	short						m_EndY;
};

struct NPC
{
	int id;
	int x, y;
};

extern SECTOR g_sectors[MAX_ROW][MAX_COL];
extern CLIENT g_clients[MAX_USER];
extern NPC g_npcs[MAX_NPC];