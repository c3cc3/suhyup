/*
** fq_ratio_distribute.c
*/
#include <stdio.h>
#include <stdbool.h>
#include "fq_common.h"
#include "fq_ratio_distribute.h"
#include "fq_linkedlist.h"
#include "fq_delimiter_list.h"

static bool on_change_ratio_seed( fq_logger_t *l, ratio_obj_t *obj, char *new_rand_seed_string);
static char on_select( fq_logger_t *l, ratio_obj_t *obj)
{
	char buf[2];

	get_seed_ratio_rand_str(buf, sizeof(buf), obj->rand_seed_string);
	return(buf[0]);
}
/*
** Memory Leak Checking -> OK
*/
bool open_ratio_distributor( fq_logger_t *l, char *ratio_string, char delimiter, char sub_delimiter, ratio_obj_t **obj)
{
	ratio_obj_t *rc = NULL;
	float ratio_sum = 0.0;

	FQ_TRACE_ENTER(l);

	rc = (ratio_obj_t *)calloc(1, sizeof(ratio_obj_t));
	if( rc ) {
		linkedlist_t    *ll=0;
		ll_node_t *ll_node=NULL;
		delimiter_list_obj_t    *delimiter_obj=NULL;
		delimiter_list_t *this_entry=NULL;
		int ret;

		rc->l = l;
		rc->ratio_string = strdup(ratio_string);
		
		ret = open_delimiter_list(NULL, &delimiter_obj, ratio_string, delimiter);
		if( ret != TRUE ) {
			fq_log(l, FQ_LOG_ERROR, "open_delimiter_list('%s', '%c') error.", ratio_string, delimiter);
			FQ_TRACE_EXIT(l);
			return(false);
		}
		this_entry = delimiter_obj->head;
		int t_no;

		ll = linkedlist_new("ratio");

		for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
			ratio_node_t *tmp = NULL;
			fq_log(l, FQ_LOG_DEBUG, "this_entry->value=[%s]\n", this_entry->value);

			char co_name[3];
			char ratio[4];
			
			bool tf;
			tf = get_LR_value(this_entry->value, sub_delimiter, co_name, ratio);
			if(tf == false) {
				fq_log(l, FQ_LOG_ERROR, "get_LR_value('%s', '%c') error.", this_entry->value, sub_delimiter);
				if(ll) linkedlist_free(&ll);
				if( delimiter_obj ) close_delimiter_list(NULL, &delimiter_obj);
				FQ_TRACE_EXIT(l);
				return false;
			}
			fq_log(l, FQ_LOG_DEBUG, "co_name='%s', ratio=[%s].", co_name, ratio); 
			// printf("co_name='%s', ratio=[%s].\n", co_name, ratio);

			tmp = calloc(1, sizeof(ratio_node_t));
			tmp->initial = co_name[0];
			tmp->ratio = atoi(ratio);
			ratio_sum += tmp->ratio;
			ll_node = linkedlist_put(ll, co_name, (void *)tmp, sizeof(int)+sizeof(int));
			if( !ll_node ) {
				fq_log(l, FQ_LOG_ERROR, "linkedlist_put('%s') error.", co_name);
				if(tmp) free(tmp);
				if(ll) linkedlist_free(&ll);
				if( delimiter_obj ) close_delimiter_list(NULL, &delimiter_obj);
				FQ_TRACE_EXIT(l);
				return false;
			}
			free(tmp);
			this_entry = this_entry->next;
		}

		if( ratio_sum != 100 ) {
			fq_log(l, FQ_LOG_ERROR, "Check your assigned ratio.[%s].", ratio_string);
			if(ll) linkedlist_free(&ll);
			if( delimiter_obj ) close_delimiter_list(NULL, &delimiter_obj);
			FQ_TRACE_EXIT(l);
			return false;
		}

		rc->ll = ll;

		ll_node_t *p = NULL;

		int least_ratio = 100;
		char least_initial = 'Z';

		/* find a least ratio */
		// fprintf(stdout, " list '%s' contains %d elements\n", ll->key, ll->length);
		for ( p=ll->head; p != NULL ; p = p->next ) {
			// fprintf(stdout, " Address of %s is \'%p\'.\n", p->key, p->value);
			ratio_node_t *tmp;
			tmp = (ratio_node_t *) p->value;
			// printf("value size =[%zu] tmp.initial=[%c], tmp.ratio=[%d]\n", p->value_sz, tmp->initial, tmp->ratio);
			if( tmp->ratio != 0 && tmp->ratio <= least_ratio ) { 
				least_ratio = tmp->ratio;
				least_initial = tmp->initial;
			}
		}
		// printf("least ratio = '%d'\n", least_ratio);
		// printf("least initial = '%c'\n", least_initial);

		/* make s SEED data */
		char SEED[200];
		int idx = 0;
		memset(SEED, 0x00, sizeof(SEED));

		for ( p=ll->head; p != NULL ; p = p->next ) {
			// fprintf(stdout, " Address of %s is \'%p\'.\n", p->key, p->value);
			ratio_node_t *tmp;
			tmp = (ratio_node_t *) p->value;
			// printf("value size =[%zu] tmp.initial=[%c], tmp.ratio=[%d]\n", p->value_sz, tmp->initial, tmp->ratio);
		
			if( tmp->ratio > 0 ) {
				int i;
				int put_count;
				put_count = tmp->ratio / least_ratio;

				for(i=0; i<put_count; i++) {
					SEED[idx] = tmp->initial;
					idx++;
				}
			}
		}
		// printf("SEED: %s\n", SEED);
		rc->rand_seed_string = strdup(SEED);

		rc->on_select = on_select;
		rc->on_change_ratio_seed = on_change_ratio_seed;

		*obj = rc;
		FQ_TRACE_EXIT(l);
		return true;
	}

	FQ_TRACE_EXIT(l);
	return false;
}

bool close_ratio_distributor( fq_logger_t *l, ratio_obj_t **obj)
{
	FQ_TRACE_ENTER(l);
	if( !*obj ) {
        FQ_TRACE_EXIT(l);
        return true;
    }

	SAFE_FREE((*obj)->ratio_string);
	SAFE_FREE((*obj)->rand_seed_string);

	if((*obj)->ll) linkedlist_free(&(*obj)->ll);

	SAFE_FREE(*obj);
	FQ_TRACE_EXIT(l);

	return true;
}

/*
** Callback example
*/
static bool my_scan_function( size_t value_sz, void *value)
{
    ratio_node_t *tmp;
    tmp = (ratio_node_t *) value;
    printf("value size =[%zu] tmp.initial=[%c], tmp.ratio=[%d]\n",
        value_sz, tmp->initial, tmp->ratio);

    return(true);
}
/*
** parameters:
	** ratio_String: K:10,L:20,S:30,N:40
	** delimiter: ,
	** sub_delimiter: :
** return:
	pointer of linkedlist.
*/
linkedlist_t *make_a_ratio_linkedlist(fq_logger_t *l, char *ratio_string, char delimiter, char sub_delimiter)
{
	linkedlist_t    *ll=0;
	ll_node_t *ll_node=NULL;
	delimiter_list_obj_t    *delimiter_obj=NULL;
	delimiter_list_t *this_entry=NULL;
	int rc;

	FQ_TRACE_ENTER(l);

	rc = open_delimiter_list(NULL, &delimiter_obj, ratio_string, delimiter);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "open_delimiter_list('%s', '%c') error.", ratio_string, delimiter);
		FQ_TRACE_EXIT(l);
		return NULL;
	}
	this_entry = delimiter_obj->head;
	this_entry = delimiter_obj->head;
	int t_no;

	ll = linkedlist_new("ratio");

	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		ratio_node_t *tmp = NULL;
		fq_log(l, FQ_LOG_DEBUG, "this_entry->value=[%s]\n", this_entry->value);

		char co_name[3];
		char ratio[4];
		
		bool tf;
		tf = get_LR_value(this_entry->value, sub_delimiter, co_name, ratio);
		if(tf == false) {
			fq_log(l, FQ_LOG_ERROR, "get_LR_value('%s', '%c') error.", this_entry->value, sub_delimiter);
			if(ll) linkedlist_free(&ll);
			if( delimiter_obj ) close_delimiter_list(NULL, &delimiter_obj);
			FQ_TRACE_EXIT(l);
			return(NULL);
		}
		fq_log(l, FQ_LOG_DEBUG, "co_name='%s', ratio=[%s].", co_name, ratio); 
		// printf("co_name='%s', ratio=[%s].\n", co_name, ratio);

		tmp = calloc(1, sizeof(ratio_node_t));
		tmp->initial = co_name[0];
		tmp->ratio = atoi(ratio);
		ll_node = linkedlist_put(ll, co_name, (void *)tmp, sizeof(int)+sizeof(int));
		if( !ll_node ) {
			fq_log(l, FQ_LOG_ERROR, "linkedlist_put('%s') error.", co_name);
			if(tmp) free(tmp);
			if(ll) linkedlist_free(&ll);
			if( delimiter_obj ) close_delimiter_list(NULL, &delimiter_obj);
			FQ_TRACE_EXIT(l);
			return(NULL);
		}
		free(tmp);
		this_entry = this_entry->next;
	}
	if( delimiter_obj ) close_delimiter_list(NULL, &delimiter_obj);

	FQ_TRACE_EXIT(l);
	return(ll);
}
static bool on_change_ratio_seed( fq_logger_t *l, ratio_obj_t *obj, char *new_rand_seed_string)
{
	ratio_obj_t *rc = NULL;
	float ratio_sum = 0.0;

	FQ_TRACE_ENTER(l);

	free(obj->rand_seed_string);
	obj->rand_seed_string = strdup(new_rand_seed_string);

	FQ_TRACE_EXIT(l);

	return true;
}


/*
** It make new seed string with new ratio string.
*/
char *make_seed_string(fq_logger_t *l,  ratio_obj_t *obj, char *ratio_src, char delimiter, char sub_delimiter )
{
	int rc;
	delimiter_list_obj_t    *delimiter_obj=NULL;
	delimiter_list_t *this_entry=NULL;
	float ratio_sum = 0.0;

	/* We change ratio string by new_ratio_src */
	free(obj->ratio_string);
	obj->ratio_string = strdup(ratio_src);


	rc = open_delimiter_list(NULL, &delimiter_obj, ratio_src, delimiter);
	if( rc != TRUE ) {
		return(NULL);
	}

	linkedlist_t    *ll=0;
	ll_node_t *ll_node=NULL;
	ll = linkedlist_new("ratio");

	this_entry = delimiter_obj->head;
	int t_no;

	/*
	** Make a ratio_node linkedlist with delimiter_list_obj
	*/
	for( t_no=0;  (this_entry->next && this_entry->value); t_no++ ) {
		// printf("this_entry->value=[%s]\n", this_entry->value);

		char co_name[3];
		char ratio[4];
		
		bool tf;
		tf = get_LR_value(this_entry->value, sub_delimiter, co_name, ratio);
		if(tf == false) {
			if( delimiter_obj ) close_delimiter_list(NULL, &delimiter_obj);
			return NULL;
		}
		// printf("co_name='%s', ratio=[%s].\n", co_name, ratio);

		ratio_node_t *tmp=NULL;
		tmp = calloc(1, sizeof(ratio_node_t));
		tmp->initial = co_name[0];
		tmp->ratio = atoi(ratio);
		ratio_sum += tmp->ratio;
		ll_node = linkedlist_put(ll, co_name, (void *)tmp, sizeof(int)+sizeof(int));
		if( !ll_node ) {
			if(tmp) free(tmp);
			if(ll) linkedlist_free(&ll);
			if( delimiter_obj ) close_delimiter_list(NULL, &delimiter_obj);
			return NULL;
		}

		/* change obj ratio */
		
		ll_node_t *obj_ll_node=NULL;
		obj_ll_node = linkedlist_find (l, obj->ll, co_name);
		memcpy(obj_ll_node->value, tmp, sizeof(ratio_node_t));

		free(tmp);
		this_entry = this_entry->next;
	}

	if( ratio_sum != 100 ) {
		if(ll) linkedlist_free(&ll);
		if( delimiter_obj ) close_delimiter_list(NULL, &delimiter_obj);
		return NULL;
	}

	ll_node_t *p = NULL;

	int least_ratio = 100;
	char least_initial = 'Z';

	/* find a least ratio */
	// fprintf(stdout, " list '%s' contains %d elements\n", ll->key, ll->length);
	for ( p=ll->head; p != NULL ; p = p->next ) {
		// fprintf(stdout, " Address of %s is \'%p\'.\n", p->key, p->value);
		ratio_node_t *tmp;
		tmp = (ratio_node_t *) p->value;
		// printf("value size =[%zu] tmp.initial=[%c], tmp.ratio=[%d]\n", p->value_sz, tmp->initial, tmp->ratio);
		if( tmp->ratio != 0 && tmp->ratio <= least_ratio ) { 
			least_ratio = tmp->ratio;
			least_initial = tmp->initial;
		}
	}
	// printf("least ratio = '%d'\n", least_ratio);
	// printf("least initial = '%c'\n", least_initial);

	/* make s SEED data */
	char SEED[200];
	int idx = 0;
	memset(SEED, 0x00, sizeof(SEED));

	for ( p=ll->head; p != NULL ; p = p->next ) {
		// fprintf(stdout, " Address of %s is \'%p\'.\n", p->key, p->value);
		ratio_node_t *tmp;
		tmp = (ratio_node_t *) p->value;
		// printf("value size =[%zu] tmp.initial=[%c], tmp.ratio=[%d]\n", p->value_sz, tmp->initial, tmp->ratio);
	
		if( tmp->ratio > 0 ) {
			int i;
			int put_count;
			put_count = tmp->ratio / least_ratio;

			for(i=0; i<put_count; i++) {
				SEED[idx] = tmp->initial;
				idx++;
			}
		}
	}
	char *new_seed = strdup(SEED);

	fq_log(l, FQ_LOG_DEBUG, "new_seed: '%s'.", new_seed);
	
	if(ll) linkedlist_free(&ll);
	if( delimiter_obj ) close_delimiter_list(NULL, &delimiter_obj);

	return new_seed;
}
/*
** input: down_co_initial: 'K', 'L', 'S' ...etc
This function changes the distribution ratio of the failed communication company to 0, 
and redistributes the distribution ratio allocated to that communication company to the remaining carriers that have a non-zero distribution ratio. 
*/
bool co_down_new_ratio( fq_logger_t *l, ratio_obj_t *obj, char down_co_initial)
{
	ll_node_t *p = NULL;
	linkedlist_t *ll = obj->ll;
	int down_co_ratio;

	/* Make down co ratio to zero */
	for ( p=ll->head; p != NULL ; p = p->next ) {
		// fprintf(stdout, " Address of %s is \'%p\'.\n", p->key, p->value);
		ratio_node_t *tmp;
		tmp = (ratio_node_t *) p->value;
		// printf("value size =[%zu] tmp.initial=[%c], tmp.ratio=[%d]\n", p->value_sz, tmp->initial, tmp->ratio);
		if( tmp->initial == down_co_initial) { 
			down_co_ratio = tmp->ratio;
			tmp->ratio = 0;
			break;
		}
	}
	fq_log(l, FQ_LOG_DEBUG,"'%c' have had %d ratio.", down_co_initial, down_co_ratio);
	
	int found_flag=0;
	/* distribute down_co_ratio to other co */
	for( ; down_co_ratio > 0; ) {
		for ( p=ll->head; (p != NULL && down_co_ratio>0); p = p->next ) {
			// fprintf(stdout, " Address of %s is \'%p\'.\n", p->key, p->value);
			ratio_node_t *tmp;
			tmp = (ratio_node_t *) p->value;
			// printf("value size =[%zu] tmp.initial=[%c], tmp.ratio=[%d]\n", p->value_sz, tmp->initial, tmp->ratio);
			/* Don't assign to ratio is zero */
			if( tmp->ratio > 0) { 
				tmp->ratio++;
				down_co_ratio--;
				fq_log(l, FQ_LOG_DEBUG, "Assign 1 to %c.", tmp->initial); 
				found_flag =1;
			}
		}
		if( found_flag == 0 ) {
			fq_log(l, FQ_LOG_EMERG, "There is no alive co. so We don't assign the ratio.");
			break;
		}
	}

	/*
	** make a new_ratio_string and put it object
	*/
	char new_ratio_string[512];
	char new_co_ratio[10];
	memset(new_ratio_string, 0x00, sizeof(new_ratio_string));

	for ( p=ll->head; p != NULL ; p = p->next ) {
		// fprintf(stdout, " Address of %s is \'%p\'.\n", p->key, p->value);
		ratio_node_t *tmp;
		tmp = (ratio_node_t *) p->value;
		// printf("value size =[%zu] tmp.initial=[%c], tmp.ratio=[%d]\n", p->value_sz, tmp->initial, tmp->ratio);
		sprintf(new_co_ratio, "%c:%d,", tmp->initial, tmp->ratio);
		strcat(new_ratio_string, new_co_ratio);
	}
	/* remove last comma */
	new_ratio_string[strlen(new_ratio_string)-1] = 0x00;

	/* change ratio_string of obj */
	free(obj->ratio_string);
	obj->ratio_string = strdup(new_ratio_string);
	return true;
}

float get_co_ratio_by_co_initial( fq_logger_t *l, ratio_obj_t *obj, char co_initial)
{
	ll_node_t *p = NULL;
	linkedlist_t *ll = obj->ll;

	/* Make down co ratio to zero */
	for ( p=ll->head; p != NULL ; p = p->next ) {
		// fprintf(stdout, " Address of %s is \'%p\'.\n", p->key, p->value);
		ratio_node_t *tmp;
		tmp = (ratio_node_t *) p->value;
		// printf("value size =[%zu] tmp.initial=[%c], tmp.ratio=[%d]\n", p->value_sz, tmp->initial, tmp->ratio);
		if( tmp->initial == co_initial) { 
			fq_log(l, FQ_LOG_INFO,  "current ratio of [%c] is [%f].\n", co_initial, tmp->ratio);
			return(tmp->ratio);
		}
	}
	fq_log(l, FQ_LOG_INFO,  "[%c] is not found in ratio object.\n", co_initial);
	return(0.0); /* not found */
}
