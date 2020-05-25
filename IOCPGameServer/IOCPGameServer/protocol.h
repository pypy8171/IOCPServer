#pragma once
#include <iostream>


#include <unordered_set>
#include <mutex>
#include <map>
#include <set>
#include <vector>


using namespace std;

constexpr int MAX_ID_LEN = 50;
constexpr int MAX_STR_LEN = 255;

constexpr auto MAX_PACKET_SIZE = 255;
constexpr auto MAX_BUF_SIZE = 255;
constexpr auto MAX_USER = 10000;

constexpr auto MAX_ROW = 25;
constexpr auto MAX_COL = 25;

constexpr auto ROW_GAP = 16;
constexpr auto COL_GAP = 16;

constexpr auto SIGHT_RANGE = 11;
constexpr int MAX_NPC = 200000;

#define WORLD_WIDTH		400
#define WORLD_HEIGHT	400

#define SERVER_PORT		9000

#define C2S_LOGIN	1
#define C2S_MOVE	2

#define S2C_LOGIN_OK		1
#define S2C_MOVE			2
#define S2C_ENTER			3
#define S2C_LEAVE			4

//enum class MON_TYPE{PEACE, ARGO};
enum C_STATUS { ST_FREE, ST_ALLOC, ST_ACTIVE };

struct ViewList
{
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

#pragma pack(push ,1)

struct sc_packet_login_ok { // 16
	char size;
	char type;
	int id;
	short x, y;
	short hp;
	short level;
	int	exp;
};

struct sc_packet_move {  // 8
	char size;
	char type;
	int id;
	short x, y;
	unsigned move_time;
};

constexpr unsigned char O_PLAYER = 0;
constexpr unsigned char O_NPC = 1;

struct sc_packet_enter { //
	char size;
	char type;
	int id;
	char name[MAX_ID_LEN];
	char o_type;
	short x, y;
};

struct sc_packet_leave {
	char size;
	char type;
	int id;
};

struct sc_packet_chat {
	char size;
	char type;
	int	 id;
	char chat[100];
};

struct cs_packet_login {
	char	size;
	char	type;
	char	name[MAX_ID_LEN];
};

constexpr unsigned char D_UP = 0;
constexpr unsigned char D_DOWN = 1;
constexpr unsigned char D_LEFT = 2;
constexpr unsigned char D_RIGHT = 3;

struct cs_packet_move {
	char	size;
	char	type;
	char	direction;
	unsigned move_time;
};

#pragma pack (pop)