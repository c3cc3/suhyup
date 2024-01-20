#include <stdio.h>
#include <stdbool.h>
#include <libgen.h>

#include "fq_common.h"
char *read_file(const char *filename)
{
	FILE *fp = fopen(filename, "r");
	size_t size_to_read = 0;
	size_t	size_read = 0;

	long pos;
	char *file_contents = NULL;

	if(!fp) {
		return NULL;
	}
	fseek(fp, 0L, SEEK_END);

	pos = ftell(fp);
	if(pos<0) {
		fclose(fp);
		return NULL;
	}
	size_to_read = pos;
	rewind(fp);
	// file_contents = (char *)parson_malloc(sizeof(char *) * (size_to_read + 1));
	file_contents = (char *)malloc(sizeof(char *) * (size_to_read + 1));
	if( !file_contents ) {
		fclose(fp);
		return NULL;
	}

	size_read = fread(file_contents, 1, size_to_read, fp);
	if( size_read == 0 || ferror(fp)) {
		fclose(fp);
		// parson_free(file_contents);
		if(file_contents ) free(file_contents);
		return NULL;
	}

	fclose(fp);
	file_contents[size_read] = '\0';

	return file_contents;
}
