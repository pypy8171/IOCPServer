#pragma once
#include "define.h"
#include "protocol.h"

class CViewProcessing
{
public:
	void	create_nearlist(int user_id, int row_start, int col_start, int row_end, int col_end);
	void	create_nearlist_pn(int user_id, int row_start, int col_start, int row_end, int col_end);
	void	check_near_view(int user_id);
	void	check_near_view_pn(int user_id);
	bool	is_near_check_pp(int id1, int id2);
	bool	is_near_check_pn(int id1, int id2);

public:
	SINGLE(CViewProcessing);
};
