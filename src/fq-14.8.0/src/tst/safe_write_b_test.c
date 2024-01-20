#include "config.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "stdbool.h"

#include "fq_defs.h"
#include "fq_logger.h"
#include "fq_types.h"
#include "fq_common.h"
#include "fq_flock.h"

#include "fq_safe_write.h"

int main()
{
	FILE *fp=NULL;
	int rc;
	fq_logger_t *l=NULL;
	safe_file_obj_t *obj=NULL;

	rc = fq_open_file_logger(&l, "/tmp/safe_write_b.log", get_log_level("trace"));
	CHECK(rc==TRUE);

	rc = open_file_safe(l, "/tmp/msg.dat", true,  &obj);
	if(rc != TRUE ) {
		printf("erorr: open_file_safe().\n");
		return(-1);
	}

	while(1) {
		char buf[4096];
		char msg[4096];

		memset(buf, 0x00, sizeof(buf));
		sprintf(msg, "%s", get_seed_ratio_rand_str( buf, 32, "0123456789ABCDEFGHIJ"));
		msg[10] = 0x00;

		rc = obj->on_write_b( obj, msg, 32);
		if( rc != TRUE ) {
			printf("on_write_b() error.\n");
			break;
		}
		usleep(1000);
	}

	if(obj) close_file_safe(&obj);
	if(l) fq_close_file_logger(&l);

	return(0);
}
