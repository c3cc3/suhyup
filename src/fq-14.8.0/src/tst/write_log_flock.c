#include "config.h"
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "fq_defs.h"
#include "fq_logger.h"
#include "fq_types.h"
#include "fq_common.h"
#include "fq_flock.h"

static int write_to_file_with_flock( flock_obj_t *lock_obj, FILE *fp, char *what)
{
	if (fp) {
		lock_obj->on_flock(lock_obj);
		fputs(what, fp);
		fflush(fp);
		lock_obj->on_funlock(lock_obj);

		return(TRUE);
	}
	else 
		return(FALSE);
}

int main()
{
	FILE *fp=NULL;
	int rc;
	flock_obj_t *lock_obj=NULL;


	rc = open_flock_obj(NULL, "/tmp", "write_log", EN_FLOCK, &lock_obj);
	CHECK(rc==TRUE);

	fp = fopen("/tmp/msg.dat", "a");

	while(1) {
		char buf[4096];
		char newline_msg[4096];

		memset(buf, 0x00, sizeof(buf));
		sprintf(newline_msg, "%s\n", get_seed_ratio_rand_str( buf, 2048, "0123456789ABCDEFGHIJ"));

		rc = write_to_file_with_flock( lock_obj, fp, newline_msg);
		CHECK(rc==TRUE);
		usleep(1000);
	}

	if(lock_obj) close_flock_obj(NULL, &lock_obj);
}
