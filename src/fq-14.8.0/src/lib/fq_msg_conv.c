/*
** fq_msg_conv.c
*/
#include "fq_defs.h"
#include "fq_common.h"
#include "fq_logger.h"
#include "fq_file_list.h"
#include "fq_codemap.h"
#include "fq_tokenizer.h"
#include "fq_msg_conv.h"
#include "fq_scanf.h"

static int add_string(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;
	char add_str[10];

	assert(src);
	assert(dst);

	*dst = '\0';
	switch ( code ) {
	case ADD_HAN_WON:	strcpy(add_str, "원"); break;
	case ADD_HAN_DOLLOR:	strcpy(add_str, "달러"); break;
	case ADD_CHAR_DOLLOR:	strcpy(add_str, "＄"); break;
	case ADD_CHAR_PERCENT:  strcpy(add_str, "%"); break;
#if 0
	case ADD_HAN_IL:	strcpy(add_str, "일"); break;
	case ADD_HAN_GUN:	strcpy(add_str, "건"); break;
	case ADD_HAN_HOI:	strcpy(add_str, "회"); break;
	case ADD_HAN_GOA:	strcpy(add_str, "좌"); break;
	case ADD_HAN_GYEWOL:	strcpy(add_str, "개월"); break;
	case ADD_HAN_NYEN:	strcpy(add_str, "년"); break;
	case ADD_HAN_KANG:	strcpy(add_str, "강"); break;
	case ADD_HAN_KA:	strcpy(add_str, "가"); break;
	case ADD_CHAR_YEN:	strcpy(add_str, "￥"); break;
	case ADD_CHAR_WON:	strcpy(add_str, "￦"); break;
	case ADD_HAN_MANWON:	strcpy(add_str, "만원"); break;
#endif
	}

	strcpy(dst, src);
	strcat(dst, add_str);
	return(0);
}
/*
** character type checking
*/
static int check_chars(int code, char* src, char* dst, int len)
{
	char *p = src;
	register int i;


	assert(src);
	assert(dst);

	strcpy(dst, src);
	switch ( code ) {
		case CHECK_NUMERIC_ONLY:
			while ( *p ) {
				if (!isdigit(*p)) 
					return (-code);
				p++;
			}
			return(0);

		case CHECK_ALPHA_ONLY:
			while ( *p ) {
				if ( !isalpha(*p) )
					return(-code);
				p++;
			}
			return(0);
		default:
			return(-code);
	}
}

int check_length(int code, char* src, char* dst, int len)
{
	assert(src);
	assert(dst);

	strcpy(dst, src);
	if ( code >= CHECK_LENGTH_FROM && code <= CHECK_LENGTH_TO ) {
		if ( strlen(src) == code ) 
			return(0);
		return (-code);
	}
	else if ( code == CHECK_FIELD_LENGTH ) {
		if ( strlen(src) == len ) 
			return(0);
		return (-code);
	}
	return(-code);
}

/*
** Fill the destination with given characters
*/
static int fill_chars(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;
	char ch;
	register int i, k;

	assert(src);
	assert(dst);

	switch ( code ) {
		
	case FILL_SPACE: 
		for (i=0; i < len; i++, q++)
			*q = ' ';
		*q = '\0';
		return(0);
	
	case FILL_SPACE_LEFT: 
		if ( (k = strlen(src)) > len )
			return(-code);
		for (i=0; i < len-k; i++, q++)
			*q = ' ';
		for (; i < len; i++, p++, q++)
			*q = *p;
		*q = '\0';
		return(0);
	
	case FILL_SPACE_RIGHT: 
		if ( (k = strlen(src)) > len )
			return(-code);
		for (i=0; i < k; i++, p++, q++)
			*q = *p;
		for (; i < len; i++, q++)
			*q = ' ';
		*q = '\0';
		return(0);

	case FILL_ZERO_LEFT: 
		if ( (k = strlen(src)) > len )
			return(-code);
		for (i=0; i < len-k; i++, q++)
			*q = '0';
		for (; i < len; i++, p++, q++)
			*q = *p;
		*q = '\0';
		return(0);

	default:
		return(-code);

	}
}
/*
** Delete characters
*/
static int del_chars(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;
	char ch;
	register int i, j, k;

	assert(src); 
	assert(dst); 

	switch ( code ) { 
	case DEL_SPACE: 
	case DEL_COMMA: 
		switch ( code ) {
			case DEL_SPACE:  ch = ' '; break;
			case DEL_COMMA:  ch = ','; break;
		}
		k = strlen(p);
		for (i=0; i < k; i++, p++) {
			if ( *p == ch )
				continue;
			else {
				*q = *p; q++;
			}
		}
		*q = '\0';
		return(0);
		k = strlen(p);
		p += 2;
		for( i=0; i < (k-2); i++ ) {
			*q = *p;
			q++;
			p++;
		}
		*q = '\0';
		return(0);
	default:
		return(-code);
	}
}

/*
** Change characters
*/
static int change_chars(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;
	char ch, ch2;
	register int i, j, k;

	ASSERT(src);
	ASSERT(dst);

	switch ( code ) {
		case CHG_PLUS_TO_SPACE:  	ch = '+'; ch2 = ' '; break;
		case CHG_ZERO_TO_SPACE:    	ch = '0'; ch2 = ' '; break;
	}

	k = strlen(p);
	for (i=0; i < k; i++, p++, q++) {
		if ( *p == ch )
			*q = ch2;
		else {
			*q = *p;
		}
	}
	*q = '\0';
	return(0);
}

static int ins_chars_to_date(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;
	char ch;
	register int i;

	assert(src);
	assert(dst);

	switch ( code ) {
		case INS_SLASH_TO_DATE: 	ch = '/'; break;
		case INS_HYPHEN_TO_DATE: 	ch = '-'; break;
	}

	strcpy(dst, src);

	if ( len > 4 ) {
		for (i=0; i < len-4; i++, p++, q++) 
			*q = *p;
		*q = ch; q++;
	}

	for (i=0; i < 2; i++, p++, q++) 
		*q = *p;
	*q = ch; q++;
	for (i=0; i < 2; i++, p++, q++) 
		*q = *p;
	*q = '\0';

	return(0);
}
static int to_upper(int code, char* src, char* dst, int len)
{
	char* p = src;
	char* q = dst;
	register int i;

	assert(src);
	assert(dst);

	for (i=0; i < strlen(src); i++, p++, q++)
		*q = toupper(*p);
	*q = '\0';
	return (0);
}

static int to_lower(int code, char* src, char* dst, int len)
{
	char* p = src;
	char* q = dst;
	register int i;

	assert(src);
	assert(dst);

	for (i=0; i < strlen(src); i++, p++, q++)
		*q = tolower(*p);
	*q = '\0';
	return (0);
}

static int to_number(int code, char* src, char* dst, int len)
{
	long long ll;

	assert(src);
	assert(dst);

	ll = strtoll(src, NULL, 10);
	sprintf(dst, "%lld", ll);
	return(0);
}

/*
** fit characters
*/
static int fit_chars(int code, char* src, char* dst, int len)
{
	char *p = src;
	char *q = dst;

	assert(src);
	assert(dst);

	switch(code) {
	case FIT_LEFT:
		strcpy(q, p);
		q[len] = '\0';
		return(0);
		
	default:
		return(-code);
	}
}

static int do_code_convert( fq_logger_t *l, int code, char *src, char *dst, int len)
{
	char* p = src;
	char* q = dst;
	int rc;


	FQ_TRACE_ENTER(l);

	assert(src);
	assert(dst);

	*q = '\0';

	if ( code >= CHECK_LENGTH_FROM && code <= CHECK_LENGTH_TO )
		return (check_length(code, p, q, len));

	switch ( code ) {
		case CHECK_FIELD_LENGTH:
			rc = check_length(code, p, q, len);
			FQ_TRACE_EXIT(l);
			return(rc);
		case CHECK_NUMERIC_ONLY:
		case CHECK_ALPHA_ONLY:
			rc = check_chars(code, p, q, len);
			FQ_TRACE_EXIT(l);
			return( rc );

		case FILL_SPACE:
		case FILL_SPACE_LEFT:
		case FILL_SPACE_RIGHT:
		case FILL_ZERO_LEFT:
			rc = fill_chars(code, p, q, len);
			FQ_TRACE_EXIT(l);
			return(rc);

		case DEL_SPACE:
			rc = del_chars(code, p, q, len);
			FQ_TRACE_EXIT(l);
			return(rc);

		case CHG_PLUS_TO_SPACE:
			rc = change_chars(code, p, q, len);
			FQ_TRACE_EXIT(l);
			return(rc);

		case ADD_HAN_WON:
		case ADD_HAN_DOLLOR:
		case ADD_CHAR_DOLLOR:
		case ADD_CHAR_PERCENT: 
			rc = add_string(code, p, q, len);
			FQ_TRACE_EXIT(l);
			return(rc);

		case INS_SLASH_TO_DATE:
		case INS_HYPHEN_TO_DATE:
			rc = ins_chars_to_date(code, p, q, len);
			FQ_TRACE_EXIT(l);
			return(rc);

		case TO_UPPER: 
			rc = to_upper(code, p, q, len);
			FQ_TRACE_EXIT(l);
			return(rc);
		case TO_LOWER: 
			rc = to_lower(code, p, q, len);
			FQ_TRACE_EXIT(l);
			return(rc);
		case TO_NUMBER: 
			rc = to_number(code, p, q, len);
			FQ_TRACE_EXIT(l);
			return(rc);

		case FIT_LEFT:
			rc = fit_chars(code, p, q, len);
			FQ_TRACE_EXIT(l);
			return(rc);

		default:
			strcpy(dst,p);
			FQ_TRACE_EXIT(l);
			return(-CODE_UNDEFINED);
	}
}	


int do_msg_conv(fq_logger_t *l, char *src_msg, char delimiter, char *src_codemap_file, char *dst_list_file, char **dst_msg)
{
	int rc;
	file_list_obj_t *obj=NULL;
	FILELIST *this_entry=NULL;
	codemap_t *src_c=NULL;
	fq_token_t t;
	char    dst_packet[MAX_CONV_MSG_LEN+1];
	int success_fields=0;
	int i, src_msg_len=0;

	FQ_TRACE_ENTER(l);

	src_msg_len = strlen(src_msg);
	if( src_msg_len > MAX_CONV_MSG_LEN ) {
		fq_log(l, FQ_LOG_ERROR, "Too long a message(%d).(max=%d)",  
				src_msg_len, MAX_CONV_MSG_LEN);
		goto return_MINUS;
    }

	rc = open_file_list(l, &obj, dst_list_file);
	if( rc != TRUE ) {
		fq_log(l, FQ_LOG_ERROR, "open_file_list('%s') error.\n", dst_list_file);
		goto return_MINUS;
	}

	src_c = new_codemap(l, src_codemap_file);
	if( !src_c ) {
		fq_log(l, FQ_LOG_ERROR, "new_codemap('%s') error.", src_codemap_file);
		goto return_MINUS;
	}

	rc = read_codemap(l, src_c, src_codemap_file);
	if( rc < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "read_codemap('%s') error.", src_codemap_file);
		goto return_MINUS;
    }

	if( Tokenize(l, src_msg, delimiter, &t) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "Tokenize('c') error.", delimiter);
		goto return_MINUS;
	}

	if( t.n_delimiters+1 != src_c->map->list->length) {
		fq_log(l, FQ_LOG_ERROR, "Check map file[%s] or message protocols. \n",  src_codemap_file);
		goto return_MINUS;
	}

	memset(dst_packet, 0x00, sizeof(dst_packet));

	/* Make dst message */
	this_entry = obj->head;
	for( i=0;  (this_entry->next && this_entry->value); i++ ) {
		char sz_key[3];
		char *dst=NULL;
		int r;
		char field_name[32];
		char conv_code[4];
		char dst2[4096];

		memset(field_name, 0x00, sizeof(field_name));
		memset(conv_code, 0x00, sizeof(conv_code));

		fq_sscanf( ' ', this_entry->value, "%s%s", field_name, conv_code);

		fq_log(l, FQ_LOG_DEBUG, "this_entry->value=[%s], field_name=[%s], conv_code=[%s]\n", this_entry->value, field_name, conv_code);

		/* find a key from src_c with this_entry->value */
		r = get_codekey(l, src_c, field_name, sz_key);
		if( r < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "Can't find a key with [%s]. Check codemap and listfile.\n", this_entry->value);
			goto return_MINUS;
		}

		/* find a value from list(t) with atoi(sz_key) */
		if( GetToken(l, &t, atoi(sz_key), &dst) < 0 ) {
			fq_log(l, FQ_LOG_ERROR, "GetToken('%s') error. Check a message.", sz_key);
			goto return_MINUS;
        }

		if( atoi( conv_code ) > 0 ) {
			int rc;
			rc = do_code_convert( l, atoi(conv_code), dst, dst2, strlen(dst));
			if( rc < 0 ) {
				fq_log(l, FQ_LOG_ERROR, "do_code_convert( '%s' ) error.\n", conv_code);
			}
		}
		
		fq_log(l, FQ_LOG_DEBUG, "[%s]->[%s]-th : conv:[%s]->[%s].", field_name,  sz_key, dst, dst2);

		strcat( dst_packet, dst2);
		strcat( dst_packet, "|");
		free(dst);

		success_fields++;

		this_entry = this_entry->next;
	}

	/* remove last delimiter */
	dst_packet[strlen(dst_packet)-1] = 0x00;

	*dst_msg = strdup(dst_packet);

	fq_log(l, FQ_LOG_DEBUG, "output packet = [%s].", dst_packet);

	if(src_c) free_codemap(l, &src_c);
	
	if(obj)  close_file_list( l, &obj);

	return success_fields;

return_MINUS:
	return(-1);	
}

