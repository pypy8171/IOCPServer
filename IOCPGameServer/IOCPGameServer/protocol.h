#pragma once
#include <iostream>


#include <unordered_set>
#include <mutex>
#include <map>
#include <set>
#include <vector>


using namespace std;

constexpr int MAX_ID_LEN = 50;
constexpr int MAX_NAME_LEN = 50;
constexpr int MAX_STR_LEN = 80;
constexpr int NPC_START_IDX = 20000;
//constexpr int ENEMY_START_IDX = 20000;
//constexpr int MAX_ENEMY = 200000;


#define WORLD_WIDTH		800
#define WORLD_HEIGHT	800

#define SERVER_PORT		3500

#define C2S_LOGIN	1
#define C2S_MOVE	2

#define S2C_LOGIN_OK		1
#define S2C_MOVE			2
#define S2C_ENTER			3
#define S2C_LEAVE			4
#define S2C_CHAT			5

//enum class MON_TYPE{PEACE, ARGO};


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
	wchar_t name[MAX_ID_LEN];
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
	char chat[MAX_STR_LEN];
};

struct cs_packet_login {
	char	size;
	char	type;
	wchar_t	name[MAX_ID_LEN];
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