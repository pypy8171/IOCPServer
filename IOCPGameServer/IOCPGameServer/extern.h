#pragma once
#include <WS2tcpip.h>
#include <MSWSock.h>

#include "protocol.h"


#include <atomic>
#include <queue>
#include <unordered_map>

#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "mswsock.lib")

constexpr auto MAX_PACKET_SIZE = 255;
constexpr auto MAX_BUF_SIZE = 255;
constexpr auto MAX_USER = 10000;

constexpr auto MAX_ROW = 40;
constexpr auto MAX_COL = 40;

constexpr auto ROW_GAP = 20;
constexpr auto COL_GAP = 20;

constexpr auto VIEW_RADIUS = 17;
constexpr int MAX_NPC = 200000;
constexpr int NPC_START_IDX = 10000;

enum ENUMOP { OP_RECV = 0, OP_SEND, OP_ACCEPT, OP_AIMOVE};
enum C_STATUS { ST_FREE, ST_ALLOC, ST_ACTIVE };
enum NPC_STATUS{ S_NONACTIVE, S_ACTIVE};
enum OBJ_TYPE{OBJ_PLAYER, OBJ_NPC};
//enum class EVENT_TYPE{MOVE};

struct Event_Type { // 이벤트 큐 // 누가 언제 무엇을
	int obj_id;
	unsigned int wakeup_time;
	int event_id; // 랜덤이동이다 뭐다 이런것
	int target_id; // 누구한테 어떤 공격을 하는가 이런것. // union 사용

	//constexpr bool operator()(const Event_Type& _Left)const
	//{
	//	return (wakeup_time > _Left.wakeup_time);
	//}

};

class Compare {

public:
	bool operator() (const Event_Type& lhs , const Event_Type& rhs) const
	{
		return (lhs.wakeup_time > rhs.wakeup_time);
	}
};


struct ViewList
{
	SRWLOCK						rwlock;
	unordered_set<int>			viewlist;
};

struct NearList
{
	unordered_set<int>			nearlist;
};

struct RemoveList
{
	unordered_set<int>			removelist;
};

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
	atomic <C_STATUS>	m_status;

	short		x, y;
	char		name[MAX_ID_LEN + 1]; // 꽉차서 올수 있으므로 +1
	unsigned	m_move_time;

	short		row, col; // char로 바꿔도 무방
	ViewList	m_viewlist;
	NearList	m_nearlist;
	RemoveList	m_removelist;
};

struct SECTOR
{
	mutex						sector_lock;
	unordered_set<int>			m_setPlayerList;
	unordered_set<int>			m_setNpcList;
	short						m_StartX, m_StartY, m_EndX, m_EndY;
};

struct NPC
{
	int m_id;
	short x, y;
	short col, row; // char로 바꿔도 무방

	NPC_STATUS m_Status = NPC_STATUS::S_NONACTIVE;
};

extern SECTOR g_sectors[MAX_ROW][MAX_COL];
extern CLIENT g_clients[MAX_USER];
extern NPC g_npcs[MAX_NPC];
extern unordered_map<int, CLIENT*> g_mapCL;

extern priority_queue<Event_Type, vector<Event_Type>, Compare > timer_queue;
extern mutex timer_lock;

extern void addtimer(int id, ENUMOP op, unsigned int timer);