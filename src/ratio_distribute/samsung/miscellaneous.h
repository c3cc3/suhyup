/*
** micellaneous.c
*/
#ifndef _MICELLANEOUS_H
#define _MICELLANEOUS_H

#include <stdbool.h>
#include "fq_config.h"
#include "fq_linkedlist.h"
#include "fq_file_list.h"
#include "ratio_dist_conf.h"

#ifdef __cpluscplus
extern "C"
{
#endif

char *read_file(const char *filename);

#ifdef __cpluscplus
}
#endif
#endif

