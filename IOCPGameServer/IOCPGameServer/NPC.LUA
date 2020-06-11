myid = 9999

function set_uid(x)
myid = x
end

function event_player_move(p_id)
	if API_get_x(p_id) == API_get_x(myid) then
		if API_get_y(p_id) == API_get_y(myid) then
			API_send_message(myid, p_id, "hello~");
			API_set_move_num(myid,3);
		end
	end
end

function event_ai_finish(myid, p_id)
	if API_get_move_num(myid) == 0 then
		API_send_message(myid, p_id, "bye~");
	end
end

function  event_ai_move(myid, eventtype)
	API_random_move(myid, eventtype);
end
