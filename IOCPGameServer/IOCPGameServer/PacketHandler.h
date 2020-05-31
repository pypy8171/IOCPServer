#pragma once
#include "define.h"
#include "extern.h"

class CPacketHandler
{
public:
	void recv_packet_construct(int user_id, int io_byte);
	void process_packet(int user_id, char buf[]);
	void send_packet(int user_id, void* packet);

	void enter_game(int user_id, char name[]);
	void send_login_ok_packet(int user_id);
	void send_enter_packet(int user_id, int o_id);
	void send_leave_packet(int user_id, int o_id);
	void disconnect(int user_id);

	void send_move_packet(int user_id, int mover);

	void do_move(int user_id, int direction);
	void npc_move(int id, ENUMOP op);

public:
	SINGLE(CPacketHandler);
};

