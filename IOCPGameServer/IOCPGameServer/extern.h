#pragma once
#include <WS2tcpip.h>
#include <MSWSock.h>

#include "protocol.h"

#include <sqlext.h>  

#include <atomic>
#include <queue>
#include <unordered_map>
#include <chrono>

#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "mswsock.lib")
#pragma comment (lib, "lua53.lib")

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}


using namespace chrono;

constexpr auto MAX_PACKET_SIZE = 255;
constexpr auto MAX_BUF_SIZE = 255;
constexpr auto MAX_USER = 20000;

constexpr auto MAX_ROW = 40;
constexpr auto MAX_COL = 40;

constexpr auto ROW_GAP = 20;
constexpr auto COL_GAP = 20;

constexpr auto VIEW_RADIUS = 17;
constexpr int MAX_NPC = 20000;//20000; // 190000;

constexpr int MAX_TILE_ROW = 100;
constexpr int MAX_TILE_COL = 100;

extern SQLHENV henv;
extern SQLHDBC hdbc;
extern SQLHSTMT hstmt;
extern SQLRETURN retcode;
extern SQLWCHAR dUser_name[MAX_NAME_LEN];
extern SQLINTEGER dUser_id, dUser_Level, dUser_posx, dUser_posy, dUser_hp;
extern SQLLEN cbName, cbID, cbLevel, cbPosx, cbPosy, cbHp;

enum ENUMOP { OP_RECV = 0, OP_SEND, OP_ACCEPT, OP_RANDOM_MOVE, OP_RANDOM_MOVE_FINISH, OP_CHASE, OP_RUNAWAY, OP_PLAYER_MOVE, 
	OP_HEAL, OP_PLAYER_ATTACK, OP_ENEMY_ATTACK};

enum C_STATUS { ST_FREE, ST_ALLOC, ST_ACTIVE, ST_SLEEP };
enum NPC_STATUS{ S_NONACTIVE, S_ACTIVE};
enum OBJ_TYPE{OBJ_PLAYER, OBJ_NPC, OBJ_ENEMY, OBJ_BOSS, OBJ_OBSTACLE};
enum class TILE_TYPE{OBSTACLE, NONOBSTACLE};
//enum class EVENT_TYPE{MOVE};

struct Event_Type { // 이벤트 큐 // 누가 언제 무엇을
	int obj_id;
	ENUMOP event_id; // 랜덤이동이다 뭐다 이런것
	high_resolution_clock::time_point wakeup_time;
	int target_id; // 누구한테 어떤 공격을 하는가 이런것. // union 사용

	constexpr bool operator < (const Event_Type& _Left)const
	{
		return (wakeup_time > _Left.wakeup_time);
	}
	constexpr bool operator < (const Event_Type* _Left)const
	{
		return (wakeup_time > _Left->wakeup_time);
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
		int				player_id;
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
	short		level, hp;
	WCHAR		name[MAX_ID_LEN + 1]; // 꽉차서 올수 있으므로 +1
	unsigned	m_move_time;

	short		row, col; // char로 바꿔도 무방
	ViewList	m_viewlist;
	NearList	m_nearlist;
	//RemoveList	m_removelist;

	char		m_otype;
	char		m_movenum = -1;
	Event_Type  m_event;
	high_resolution_clock::time_point m_last_move_time;

	lua_State*	L;
	mutex		lua_l;
};

class CTile
{
public:
	short m_id;
	short m_x, m_y;
	TILE_TYPE m_TileType;

public:
	CTile() {};
	~CTile() {};
};

struct SECTOR
{
	mutex						sector_lock;
	unordered_set<unsigned int>	m_setPlayerList;
	//unordered_set<int>			m_setNpcList;
	short						m_StartX, m_StartY, m_EndX, m_EndY;
};

struct NPC // 얘한테 오버랩 구조체 주고 해보자
{
	int m_id;
	short x, y;
	short col, row; // char로 바꿔도 무방

	
	NPC_STATUS m_Status = NPC_STATUS::S_NONACTIVE;
};

extern SECTOR g_sectors[MAX_ROW][MAX_COL];
extern CLIENT g_clients[MAX_USER + MAX_NPC];
//extern NPC g_npcs[MAX_NPC];
extern unordered_map<int, CLIENT*> g_mapCL;

extern CTile g_tiles[MAX_TILE_ROW][MAX_TILE_COL];

class Compare {
public:
	bool operator() (const Event_Type* lhs, const Event_Type* rhs) const
	{
		return (lhs->wakeup_time > rhs->wakeup_time);
	}
};


extern priority_queue < Event_Type*, vector<Event_Type*>, Compare > timer_queue;
extern mutex timer_lock;
extern HANDLE g_iocp;
extern int g_curUser;

extern void addtimer(int id, ENUMOP op, unsigned int timer);

void show_error();
void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);

// 지형 정보도 db에 있어야 하나?, 그냥 처음 서버 킬때 랜덤으로 뿌려주나, 

//template <>
//struct less<Event_Type*>
//{
//	template <class T, class U>
//	auto operator()(T&& Left, U&& Right) const
//		-> decltype(std::forward<T>(Left) < std::forward<U>(Right));
//};