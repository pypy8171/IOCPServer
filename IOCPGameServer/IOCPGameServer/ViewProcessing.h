#pragma once
#include "define.h"
#include "protocol.h"

class CViewProcessing
{
public:
	void	create_nearlist(int user_id, int row, int col);
	void	check_near_view(int user_id);
	bool	is_near_check(int id1, int id2);

public:
	SINGLE(CViewProcessing);
};
