
// queue_map
// path, name, channel_id, co_initial
//
// ratio_map
// channel_id, co_initial:ratio, co_initial_ratio,

struct {
	fqueue_obj_t *obj;
	char	*channel_id;
	char	*co_initial;


}
fqueue_obj_t * select_best_obj( fqueue_obj_t *l, char *channel, char *customer_id)
{
	int rc;
	char *ratio_string=NULL;

	rc = get_ratio_string( channel, &ratio_string ); // K:70, L:30
	CHECK(rc=true);
	CHECK(ratio_string);

	co_initial = get_a_co_initial_in_this_time( ratio_string);

	tf = is_normal_status( co_initial );
	if( tf 





	return 0;
}

