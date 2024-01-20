/* 
** fq_common.c
*/
#define FQ_COMMON_C_VERSION "1.0.4"

/* 1.0.1 :2013/09/02  get_seq() -> fq_get_seq() */
/* 1.0.3 :2013/11/13  add: str_trim_char() */
/* 1.0.4 :2014/03/25  add: int is_running_time(char *from, char *to); */

#include <sys/mman.h>

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif

#include <sys/utsname.h>
#include <signal.h>
#include <assert.h>

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <sys/vfs.h>
#include <stdbool.h>


#include "fq_defs.h"
#include "fq_common.h"
#include "fq_cache.h"
#include "fq_logger.h"

#define MAX_SUB_FOLDERS 1000 /* for big files */

/*
** ½Ã½ºÅÛÀÇ endianÀ» °Ë»çÇØ¼­ LittleÀÌ¸é 1À» BigÀÌ¸é 0À» ¸®ÅÏÇÑ´Ù.
*/
/* #define THIS_BIG_ENDIAN     0 */ /* solaris */
/* #define THIS_LITTLE_ENDIAN  1 */ /* x86 */

int get_endian(void)
{
    int i = 0x00000001;
    if ( ((char *)&i)[0] )
        return THIS_LITTLE_ENDIAN;
    else
        return THIS_BIG_ENDIAN;
}

/* After using, free memory */
char *asc_time( time_t tval )
{
	char    tmp[128];
	char    *ptr;
	struct tm t;

	localtime_r(&tval, &t);

	sprintf(tmp, "%04d%02d%02d %02d%02d%02d" ,
			t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

	ptr = strdup(tmp);
	return(ptr);
}

/*
** input string format: yyyymmdd HHMMSS
**						0123456789ABCDE
*/
time_t bin_time( char *src )
{
	struct tm when;
	time_t	bin_time;
	/* char *weekday[]={ "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", NULL}; */


	char yyyy[5], mm[3], dd[3], HH[3], MM[3], SSSS[3];
    long i_yyyy, i_mm, i_dd, i_HH, i_MM, i_SS;


    memcpy(yyyy, &src[0], 4); i_yyyy = atoi(yyyy);
    memcpy(mm, &src[4], 2); i_mm = atoi(mm);
    memcpy(dd, &src[6], 2); i_dd = atoi(dd);

    memcpy(HH, &src[9], 2); i_HH = atoi(HH);
    memcpy(MM, &src[11], 2);i_MM = atoi(MM);
    memcpy(SSSS, &src[13], 2);i_SS = atoi(SSSS);
	
	memset(&when, 0, sizeof(when));

	when.tm_year = i_yyyy-1900;
	when.tm_mon = i_mm-1;
	when.tm_mday = i_dd;

	when.tm_hour = i_HH;
	when.tm_min = i_MM;
	when.tm_sec = i_SS;

	bin_time =  mktime(&when);

	/* printf ("That day is a %s.\n", weekday[when.tm_wday]); */

	return(bin_time);

}

static int makeInt(const char *p, int size)
{
    const char *endp;
    int intval = 0;

    endp = p + size;
    while (p < endp)
    {
        intval = intval * 10 + *p - '0';
        p++;
    }
    return intval;
}

/* input 20131210:132837 or 20131210 132837 */

time_t ConvertToSecSince1970(char *szYYYYMMDDHHMMSS)
{
	struct tm    Tm;    

    memset(&Tm, 0, sizeof(Tm));
    Tm.tm_year = makeInt(szYYYYMMDDHHMMSS +  0, 4) - 1900;
    Tm.tm_mon  = makeInt(szYYYYMMDDHHMMSS +  4, 2) - 1;
    Tm.tm_mday = makeInt(szYYYYMMDDHHMMSS +  6, 2);
    Tm.tm_hour = makeInt(szYYYYMMDDHHMMSS +  9, 2);
    Tm.tm_min  = makeInt(szYYYYMMDDHHMMSS + 11, 2);
    Tm.tm_sec  = makeInt(szYYYYMMDDHHMMSS + 13, 2);
    return (mktime(&Tm));
}

long time_diff( char *old_str1, char *new_str2)
{
	long old, new, diff;
	
	old  = bin_time(old_str1);
	new  = bin_time(new_str2);

	/* printf("new=[%ld] old=[%ld]\n", new, old); */
	diff = new - old;

	return(diff);
}

static char *_tty_color[] = {
    "\033[0;40;30m",       /* 0: black on black */
    "\033[1;40;37m",       /* 1: white */
    "\033[0;40;31m",       /* 2: red */
    "\033[0;40;32m",       /* 3: green */
    "\033[0;40;33m",       /* 4: brown */
    "\033[0;40;34m",       /* 5: blue */
    "\033[1;40;33m",       /* 6: yellow */
    "\033[0;40;35m",       /* 7: magenta */
    "\033[0;40;36m",       /* 8: cyan */
    "\033[0;40;37m",       /* 9: light gray */
    "\033[1;40;30m",       /* 10: gray */
    "\033[1;40;31m",       /* 11: brightred */
    "\033[1;40;32m",       /* 12: brightgreen */
    "\033[1;40;34m",       /* 13: brightblue */
    "\033[1;40;35m",       /* 14: brighmagenta */
    "\033[1;40;36m",       /* 15: brightcyan */
};
void printc(int color, const char *fmt, ...)
{
		if( color < 0 || color > 16 ) {
			color = 1;
		}
			
        va_list ap;
        char buf[4096];

        va_start(ap, fmt);
        vsprintf(buf, fmt, ap);
        va_end(ap);

        fprintf(stdout, "%s%s\033[0;40;0m", _tty_color[color], buf);
        fflush(stdout);
}
/*
** unit: ÇÑ ¶óÀÎ¿¡ ¸î ¹®ÀÚ¾¿À» º¸ÀÏ°ÍÀÎÁö °áÁ¤
** output sample
[   0] 49 6e 20 74 68 65 20 55-4e 49 58 20 65 6e 76 69   In the UNIX envi
[   1] 72 6f 6e 6d 65 6e 74 20-61 20 74 68 72 65 61 64   ronment a thread
[   2] 3a 20 0d 0a 45 78 69 73-74 73 20 77 69 74 68 69   : ..Exists withi
*/
void packet_dump(const char* buf, int size, int unit, const char *departure)
{
     char     tmp[150];
     int      i, j, lno;

     j = 7;
     lno = 0;

     memset((tmp+0), 0x00, 150);

     for (i = 0; i < size; i++) {
          if((i%unit) == 0)
               memset((tmp+0), 0x20, ((unit*3)+unit+1+7+3));
          if(j == 7)
               sprintf((tmp+0), "[%4d] ", lno);
          sprintf((tmp+j), "%02x", (unsigned char)*(buf+i));
          tmp[j+2] = 0x20;
          if((unsigned char)*(buf+i) >= (unsigned char)0x20)
               tmp[(j/3)+(unit*3)+1+6] = *(buf+i);
          else
               tmp[(j/3)+(unit*3)+1+6] = '.'; /* Á¦¾î¹®ÀÚ´Â  '.'À¸·Î Ç¥±âÇÑ´Ù */
          j += 3;
          if((i%unit) == (unit-1)) {
               printf("%s [%d~%d]\n", (tmp+0), (lno*unit), ((lno*unit)+(unit-1)) );
               j = 7;
               ++lno;
          }
          else if((i%unit) == ((unit/2)-1))
               tmp[j-1] = '-';
     }

     if ((i % unit) != 0) {
             printf("%s\n", (tmp+0));
     }
        printf("---------------------from: [%s]: Total length: [%d]--------------------\n", departure, size);
}

int deFQ_packet_dump(fq_logger_t *l, const unsigned char* buf, int size, long seq, long run_time)
{
     char     tmp[150];
     char     tmp2[1024];
     int      i, j, lno;
	 int	  unit=16;
	 char	  departure[128];
	 fq_file_logger_t *p = l->logger_private;

     j = 7;
     lno = 0;

     memset((tmp+0), 0x00, 150);

	sprintf(departure, "SEQ:%ld-th, run_time:%ld msec", seq, run_time);

     for (i = 0; i < size; i++) {
          if((i%unit) == 0)
               memset((tmp+0), 0x20, ((unit*3)+unit+1+7+3));
          if(j == 7)
               sprintf((tmp+0), "[%4d] ", lno);
          sprintf((tmp+j), "%02x", (unsigned char)*(buf+i));
          tmp[j+2] = 0x20;
          if((unsigned char)*(buf+i) >= (unsigned char)0x20)
               tmp[(j/3)+(unit*3)+1+6] = *(buf+i);
          else
               tmp[(j/3)+(unit*3)+1+6] = '.'; /* Á¦¾î¹®ÀÚ´Â  '.'À¸·Î Ç¥±âÇÑ´Ù */
          j += 3;
          if((i%unit) == (unit-1)) {
               // printf("%s\n", (tmp+0));
               fputs(tmp, p->logfile);
			   fputs("\n",p->logfile);
               fflush(p->logfile);
               j = 7;
               ++lno;
          }
          else if((i%unit) == ((unit/2)-1))
               tmp[j-1] = '-';
     }

     if ((i % unit) != 0) {
           // printf("%s\n", (tmp+0));
			
		   fputs(tmp, p->logfile);
		   fputs("\n",p->logfile);
		   fflush(p->logfile);
     }
	sprintf(tmp2, "--------------------deFQ result: [%s]: Total length: [%d]--------------------\n", departure, size);
	fputs(tmp2, p->logfile);
	fputs("\n",p->logfile);
	fflush(p->logfile);
	return(0);
}

int queue_data_file_dump(fq_logger_t *l, char *qname, long seq,  const unsigned char* buf, int datasize)
{
    char     tmp[150];
    char     tmp2[1024];
    int      i, j, lno;
	int	  unit=16;
	char	  departure[128];
	FILE *wfp= NULL;
	char	filename[256];

	FQ_TRACE_ENTER(l);
	sprintf( filename, "/tmp/%s_%ld.dump", qname, seq);
	wfp = fopen(  filename, "w+");
	if( wfp == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "fopen('%s') error. reason=[%s].", filename, strerror(errno));
		FQ_TRACE_EXIT(l);
		return(-1);
	}
	if( fwrite(buf, 1, datasize, wfp) != datasize ) {
		fq_log(l, FQ_LOG_ERROR, "fwrite() error, reason=[%s].", strerror(errno));
		fclose(wfp);
		FQ_TRACE_EXIT(l);
		return(-2);
	}
	FQ_TRACE_EXIT(l);
	return(datasize); /* success */
}

#if 0
static int FQ_METHOD log_to_file(fq_logger_t *l, int level, int used, char *what)
{
    if (l &&
        (l->level <= level || level == FQ_LOG_REQUEST_LEVEL) &&
        l->logger_private && what) {
        fq_file_logger_t *p = l->logger_private;
        if (p->logfile) {
            what[used++] = '\n';
            what[used] = '\0';
#if defined(FQ_LOG_LOCKING)
#if defined(WIN32) && defined(_MSC_VER)
            LockFile((HANDLE)_get_osfhandle(_fileno(p->logfile)),
                     0, 0, 1, 0);
#endif
#endif
            fputs(what, p->logfile);
            /* [V] Flush the dam' thing! */
            fflush(p->logfile);
#if defined(FQ_LOG_LOCKING)
#if defined(WIN32) && defined(_MSC_VER)
            UnlockFile((HANDLE)_get_osfhandle(_fileno(p->logfile)),
                       0, 0, 1, 0);
#endif
#endif
        }
        return FQ_TRUE;
    }
    return FQ_FALSE;
}
#endif

int STRCCMP(const void* s1, const void* s2)
{
        unsigned char* p = (unsigned char*)s1;
        unsigned char* q = (unsigned char*)s2;

        if ( !s1 || !s2 )
                return(-1);

        while ( *p && *q ) {
                if ( TOLOWER(*p) != TOLOWER(*q) )
                        return(-1);
                p++; q++;
        }
        if ( !(*p) && !(*q) )
                return(0);
        return(-1);
}

int skip_comment(FILE *fp)
{
        char ch;
        int  k = 0, skip;


        if ( feof(fp) )
                return (EOF);

        ch = getc(fp);

        // if ( ch == EOF ) return (EOF); /* This is always false */

        if ( ch == C_COMMENT )
                skip=1;
        else {
                if ( ch == '\n' ) {
                        skip=0; k++;
                }
                else {
                        ungetc(ch, fp);
                        return (k);
                }
        }
        while(!feof(fp)) {
                if( (ch = getc(fp)) == '\n') {
                        skip=0; k++;
                        if( (ch = getc(fp)) == C_COMMENT)
                                skip=1;
                        else if ( ch == '\n' ) {
                                skip=0; k++;
                        }
                        else {
                                ungetc(ch, fp);
                                return (k);
                        }
                } else if ( !skip ) {
                        if ( ch == C_COMMENT )
                                skip = 1;
						/* This is always false */
						/*
                        else if ( ch == EOF ) return (k);
						*/
                        else {
                                ungetc(ch, fp);
                                return (k);
                        }
                }
        }
        return (EOF);
}

/*
**  Description:
**  return parameter
** 		dst: value of token
**		end_token_flag
**			1: end column in a line
**			2: is not end column.
**  return
**		0: found token
**		EOF(-1) : End of file.
*/
int get_token1(FILE * fp, char *dst, int *end_token_flag)
{
	char *s = dst;
	int k=0;

	*s = '\0';
	*end_token_flag = 0;

	/* skip preceding spaces and comment lines */
	while ( !feof(fp) ) {
		*s = getc(fp);
		if ( *s == ' ' || *s == '\t' )
			continue;
		else if ( *s == '\n' ) {
			k += (1+skip_comment(fp));
			continue;
		}
		else {
			break;
		}
	}
	if ( feof(fp) )         /* no token */
			return (EOF);

	/* token found */
	if ( *s == '\"' ) {                     /* quote opened */
		while ( !feof(fp) ) {
			*s = getc(fp);
			if ( *s == '\t' ) {
				*s = ' ';
			} else if ( *s == '\n' ) {
				*s = '\0';
				*end_token_flag = 1+skip_comment(fp);
				return (k);     /* quote not closed */
			} else if ( *s == '\"' ) { /* quote closed */
				*s = '\0';
				while  ( !feof(fp) ) {
					*s = getc(fp);
					if ( *s == ' ' || *s == '\t' )
						continue;
					else if ( *s == '\n' ) {
						*s = '\0';
						*end_token_flag = 1+skip_comment(fp);
						return (k);
					}
					else {
						ungetc(*s, fp);
						*s = '\0';
						return (k);
					}
				}
				return (k);
			}
			s++;
		}
	} else {
		while ( !feof(fp) ) {
			*(++s) = getc(fp);
			if ( *s == ' ' || *s == '\t' ) {
				while  ( !feof(fp) ) {
					*s = getc(fp);
					if ( *s == ' ' || *s == '\t' )
						continue;
					else if ( *s == '\n' ) {
						*s = '\0';
						*end_token_flag = 1+skip_comment(fp);
						return (k);
					}
					else {
						ungetc(*s, fp);
						*s = '\0';
						return (k);
					}
				}
				if ( *s ) {
					*(++s) = '\0';
					return (k);
				}
			} else if ( *s == '\n' ) {
				*s = '\0';
				*end_token_flag = 1+skip_comment(fp);
				return (k);
			}
		}
		if ( *s ) {
			*(++s) = '\0';
			return (k);
		}
	}
	if ( *s ) {
		*(++s) = '\0';
		return (k);
	}
	return (EOF);
}
	
int get_token(FILE* fp, char *dst, int* line)
{
        char *s = dst;
        int k = 0;


        *s = '\0';
        *line = 0;

        /* skip preceding spaces and comment lines */
        while ( !feof(fp) ) {
                *s = getc(fp);
                if ( *s == ' ' || *s == '\t' )
                        continue;
                else if ( *s == '\n' ) {
                        k += (1+skip_comment(fp));
                        continue;
                }
                else
                        break;
        }
        if ( feof(fp) )         /* no token */
                return (EOF);

        /* token found */
        if ( *s == '\"' ) {                     /* quote opened */
                while ( !feof(fp) ) {
                        *s = getc(fp);
                        if ( *s == '\t' ) {
                                *s = ' ';
                        } else if ( *s == '\n' ) {
                                *s = '\0';
                                *line = 1+skip_comment(fp);
                                return (k);     /* quote not closed */

                        } else if ( *s == '\"' ) { /* quote closed */
                                *s = '\0';
                                while  ( !feof(fp) ) {
                                    *s = getc(fp);
                                    if ( *s == ' ' || *s == '\t' )
                                        continue;
                                    else if ( *s == '\n' ) {
                                        *s = '\0';
                                        *line = 1+skip_comment(fp);
                                        return (k);
                                    }
                                    else {
                                        ungetc(*s, fp);
                                        *s = '\0';
                                        return (k);
                                    }
                                }
                                return (k);
                        }
                        s++;
                }
        } else {
                while ( !feof(fp) ) {
                        *(++s) = getc(fp);
                        if ( *s == ' ' || *s == '\t' ) {
                                while  ( !feof(fp) ) {
                                    *s = getc(fp);
                                    if ( *s == ' ' || *s == '\t' )
                                        continue;
                                    else if ( *s == '\n' ) {
                                        *s = '\0';
                                        *line = 1+skip_comment(fp);
                                        return (k);
                                    }
                                    else {
                                        ungetc(*s, fp);
                                        *s = '\0';
                                        return (k);
                                    }
                                }
                                if ( *s ) {
                                        *(++s) = '\0';
                                        return (k);
                                }
                        } else if ( *s == '\n' ) {
                                *s = '\0';
                                *line = 1+skip_comment(fp);
                                return (k);
                        }
                }
                if ( *s ) {
                        *(++s) = '\0';
                        return (k);
                }
        }
        if ( *s ) {
                *(++s) = '\0';
                return (k);
        }
        return (EOF);
}

/*-----------------------------------------------------------------------
** get_feol : Get data from the current position to the end of the line
** Argument : File* fp;  - file pointer
**            char* dst; - destination
** Return   : 0 if successful
**           -1 if EOF encountered
*/
int get_feol(FILE* fp, char *dst)
{
        char *s = dst;
        int in_quote = 0;

        /* ASSERT(fp); */
        /* ASSERT(dst); */

        assert(fp);
        assert(dst);

        *s = '\0';
        while ( !feof(fp) ) {
                *s = getc(fp);
                if ( feof(fp) ) {
                        *s = '\0';
                        return (0);
                }
                if ( *s == '\"' )
                        in_quote = (in_quote ? 0 : 1);
                else if ( !in_quote && (*s == ' ' || *s == '\t') )
                        continue;
                else if ( *s == '\n' ) {
                        *s = '\0';
                        return (0);
                } else
                        s++;
        }
        return (-1);
}

/************************************************************************
 ** get_assign - Get an assignment pair
 ** Arguments - str : source stream (no blank)
 **             key : key string
 **             val : value string
 ** Return value - 0 if key found, -1 o/w
 **/
int get_assign(const char *str, char *key, char *val)
{
        char *p;

        *key = '\0';
        *val = '\0';

        if ( !str )
                return (-1);

        p = (char *)strchr(str, '=');
        if ( !p )
                return (-1);
        if ( p == str )
                return (-1);
        strncpy(key, str, p-str);
        key[p-str] = '\0';
        (void)str_lrtrim(key);

        strcpy(val, p+1);
        unquote(val);
        return (0);
}

void str_trim_char(char *str, char Charactor)
{
    int i;

    if ( !(*str) ) return;

    for ( i = strlen(str)-1; i >= 0; i-- ) {
        if ( str[i] == Charactor )
            str[i] = '\0';
        else
            return;
    }
    return;
}

void str_trim(char *str)
{
        int i;

        if ( !(*str) )
                return;

        for ( i = strlen(str)-1; i >= 0; i-- )
                if ( str[i] == ' ' || str[i] == '\t' ||
                     str[i] == '\n'|| str[i] == '\r' )
                        str[i] = '\0';
                else
                        return;
}

void str_trim2(char *str)
{
        int i;

        if ( !(*str) )
                return;

        for ( i = strlen(str)-1; i >= 0; i-- )
                if ( str[i] == ' ' || str[i] == '\t' ||
                     str[i] == '\n'|| str[i] == '\r' )
                        str[i] = '\0';
                else if ( i > 0 && (unsigned char)str[i] == 0xa1 && (unsigned char)str[i-1] == 0xa1 ) {
                        str[i] = '\0'; i--;
                        str[i] = '\0';
                }
                else
                        return;
}

void str_lrtrim(char *str)
{
        char *p, *q;

        if ( !(*str) )
                return;

        str_trim(str);
        p = q = str;

        while ( *q && *q == ' ' )
                q++;

        while ( *q ) {
                *p = *q;
                q++; p++;
        }
        *p = '\0';
        return;
}

void unquote(char* str)
{
        int i, k;

        if ( !str || !(*str) )
                return;

        (void)str_lrtrim(str);

        k = strlen(str);
        if ( k > 1 && *str == '\"' && *(str+k-1) == '\"' ) {
                for (i=0; i < k-2; i++)
                        *(str+i) = *(str+i+1);
                *(str+i) = '\0';
        }
}

void on_stopwatch_micro(stopwatch_micro_t *p)
{
    gettimeofday((struct timeval *)&p->tp, NULL);
	p->start_usec = p->tp.tv_usec; /* and microseconds */
	p->start_sec = p->tp.tv_sec;   /* seconds since Jan. 1, 1970 */
#if 0
	printf("start sec=[%ld] usec=[%ld]\n", p->start_sec, p->start_usec);
#endif
	return;
}

void off_stopwatch_micro(stopwatch_micro_t *p)
{
    gettimeofday((struct timeval *)&p->tp, NULL);
	p->end_usec = p->tp.tv_usec; /* and microseconds */
	p->end_sec = p->tp.tv_sec;   /* seconds since Jan. 1, 1970 */
#if 0
	printf("end sec=[%ld] usec=[%ld]\n", p->end_sec, p->end_usec);
#endif
	return;
}
long off_stopwatch_micro_result(stopwatch_micro_t *p)
{
	long total_micro_sec;
	long sec=0L;
	long usec=0L;

    gettimeofday((struct timeval *)&p->tp, NULL);
	p->end_usec = p->tp.tv_usec; /* and microseconds */
	p->end_sec = p->tp.tv_sec;   /* seconds since Jan. 1, 1970 */

	if( (p->end_usec < p->start_usec) ) {
		sec = (p->end_sec-1) - p->start_sec;
		usec = (1000000+p->end_usec) - p->start_usec;
    }
    else {
        sec = p->end_sec - p->start_sec;
        usec = p->end_usec - p->start_usec;
    }

	total_micro_sec = sec * 1000000;
	total_micro_sec =  total_micro_sec + usec;

	return total_micro_sec;
}

void get_stopwatch_micro(stopwatch_micro_t *p, long *sec, long *usec)
{

	if( (p->end_usec < p->start_usec) ) {
		*sec = (p->end_sec-1) - p->start_sec;
		*usec = (1000000+p->end_usec) - p->start_usec;
	}
	else {
		*sec = p->end_sec - p->start_sec;
		*usec = p->end_usec - p->start_usec;
	}

	return;		
}
void get_milisecond( char *timestr )
{
	time_t tval;
	struct tm t;
	struct timeval tp;
	
	time(&tval);
	localtime_r(&tval, &t);

	gettimeofday(&tp, NULL);

	if( timestr )
		sprintf(timestr, "%02d%02d%02d.%06ld", t.tm_hour, t.tm_min, t.tm_sec, tp.tv_usec);

	return;
}
void get_date_time_ms( char *str )
{
	time_t tval;
	struct tm t;
	struct timeval tp;
	
	time(&tval);
	localtime_r(&tval, &t);

	gettimeofday(&tp, NULL);

	if( str )
		sprintf(str, "%04d%02d%02d%02d%02d%02d%06ld", 
				t.tm_year+1900, t.tm_mon+1, t.tm_mday,
				t.tm_hour, t.tm_min, t.tm_sec, tp.tv_usec);

	return;
}
	
#ifdef HAVE_CLOCK_GETTIME
void on_stopwatch_nano(stopwatch_nano_t *p)
{
    clock_gettime(CLOCK_REALTIME, (struct timespec *)&p->ts);

	p->start_sec = p->ts.tv_sec;
	p->start_nsec = p->ts.tv_nsec;
	return;
}

void off_stopwatch_nano(stopwatch_nano_t *p)
{
    clock_gettime(CLOCK_REALTIME, (struct timespec *)&p->ts);

	p->end_sec = p->ts.tv_sec;   /* seconds since Jan. 1, 1970 */
	p->end_nsec = p->ts.tv_nsec; /* and nano seconds */
	return;
}

void get_stopwatch_nano(stopwatch_nano_t *p,  long *sec, long *nsec)
{
	*sec = p->end_sec - p->start_sec;
	*nsec = p->end_nsec - p->start_nsec;

    if( *nsec < 0 && (p->end_sec > p->start_sec) )  {
        *nsec = (1000000000+p->end_nsec) - p->start_nsec;
#if 0
        *sec--;
#endif
    }
    return;
}

void get_nano_time(long *sec, long *nsec)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
#if 0
    printf("%ld seconds and %ld nanoseconds\n", ts.tv_sec, ts.tv_nsec);
#endif
    *sec = ts.tv_sec;
    *nsec = ts.tv_nsec;
    return;
}

long get_random()
{
    long l_sec=0L, l_nsec=0L, l_tmp=0L, l_random=0L;

    l_tmp = random();
    get_nano_time(&l_sec, &l_nsec);

    l_random = l_tmp + l_sec + (l_nsec/1000);
    return(l_random);
}
#endif

void get_TPS(long count, long sec, long nsec, float *tps)
{
	float delay_time;
	float tmp;

	tmp = (float)nsec/(float)1000000000;
	delay_time = (float)sec + tmp;
	*tps = (float)count/delay_time;
}

/*
** proved that distribution Chart is good.
*/ 
char * get_seed_ratio_rand_str(
	char * buf, /* pointer to the buffer that will contain the garbage */
	size_t count, /* including the last null character. */
	const char * chrs /* character range used */
)
{
	char * eosbuf = buf;
	const char * eoschrs = NULL;
	time_t timeval;

	/* static & 0 IMPORTANT! Thread-safe? */
	static unsigned int seed = 0;


	/* (A)
	* If the seed is 0 (not created yet), we generate a seed.
	*/
	if (seed == 0)
	{
		timeval = time((time_t*)NULL);

		/*
		* check if time is available on system
		*/
		if (timeval == (time_t)-1)
		return (NULL);
		seed = (unsigned) timeval;

		/*
		* Seed the random number generation algo, so we can call rand()
		*/
		srand (seed);
	}


	/* (B)
	* if chrs is an empty string, choose our own range
	* of characters from ASCII 32 to ASCII 126
	*/
	if (NULL == chrs || '\0' == *chrs)
	{
		/* generate only the required number of characters */
		while (count-- > 1)
			*eosbuf++ = (char) (rand () % RANDSTR_RANGE);
	}
	/* we are using a user-specified range. */
	else
	{
		eoschrs = chrs;

		/* get a pointer to point to the last character '\0' of the range.
		*/
		while (*eoschrs)
			++eoschrs;

		/* generate the random string. MAX == length of range. */
		while (count-- > 1)
			*eosbuf++ = *(chrs + (rand () % (size_t)(eoschrs - chrs)));
	}

	/* (C)
	* Important: terminate the string!
	*/
	*eosbuf = (char) '\0';


	/* (D)
	* Return pointer to the buffer.
	*/
	return (buf);
}

void *file_mmapping(int fd, size_t space_size, off_t from_offset, char **pt_errmsg)
{

    void *p;
    int prot = PROT_WRITE|PROT_READ;
#if 0
    int  flag = MAP_SHARED; /*  MAP_SHARED, MAP_PRIVATE, MAP_NORESERVE, MAP_FIXED, MAP_INITDATA */
    int  flag = MAP_SHARED|MAP_NORESERVE; /*  MAP_SHARED, MAP_PRIVATE, MAP_NORESERVE, MAP_FIXED, MAP_INITDATA. */
    int flag = MAP_PRIVATE|MAP_NORESERVE;
#endif
	int flag;
    char    szErrMsg[128];


#ifdef MAP_NORESERVE
    flag = MAP_SHARED; /* header file must be share */
#else
    flag = MAP_SHARED|MAP_NORESERVE; /* header file must be share */
#endif
    prot = PROT_WRITE|PROT_READ;

    p = mmap(NULL, space_size, prot,  flag, fd, from_offset);
    if( p == MAP_FAILED ) {
        sprintf(szErrMsg, "mmap() error. [%s]", strerror(errno));
        *pt_errmsg = strdup(szErrMsg);
        return(NULL);
    }

    return(p);
}

int load_file_mmap(char *filename, char**ptr_dst, int *flen, char** ptr_errmsg)
{
	int fdin, rc=-1;
	char	*src=NULL;
	struct stat statbuf;
	off_t len=0;

	if(( fdin = open(filename ,O_RDONLY)) < 0 ) { /* option : O_RDWR */
		*ptr_errmsg = strdup("open() error.");
		goto L0;
	}

	if(( fstat(fdin, &statbuf)) < 0 ) {
		*ptr_errmsg = strdup("stat() error.");
		goto L1;
	}

	len = statbuf.st_size;
	*flen = len;

	/* option: PROT_READ | PROT_WRITE */
	if(( src = mmap(0,len, PROT_READ, MAP_SHARED, fdin, 0 )) == (void *)-1) {
		*ptr_errmsg = strdup("mmap(PROT_READ) error.");
		goto L1;
	}

#ifdef  _MMAP_WRITE_TEST
	*(src+4) = 'C';
	if( msync(src, len, MS_ASYNC) < 0 ) {
		fprintf(stderr, "msync() error.\n");
		exit(0);
	}
#endif
	*ptr_dst = strdup(src);

	rc = 0;
L1:
	SAFE_FD_CLOSE(fdin);
	
L0:
	munmap(src, len);

	return(rc);
}

int LoadFileToBuffer(char *filename, unsigned char **ptr_dst, int *flen, char** ptr_errmsg)
{
	int fdin, rc=-1;
	struct stat statbuf;
	off_t len=0;
	char	*p=NULL;
	int	n;

	if(( fdin = open(filename ,O_RDONLY)) < 0 ) { /* option : O_RDWR */
		*ptr_errmsg = strdup("open() error.");
		goto L0;
	}

	if(( fstat(fdin, &statbuf)) < 0 ) {
		*ptr_errmsg = strdup("stat() error.");
		goto L1;
	}

	len = statbuf.st_size;
	*flen = len;

	p = calloc(len+1, sizeof(char));
	if(!p) {
		*ptr_errmsg = strdup("memory leak!! Check your free memory!");	
		goto L1;
	}

	if(( n = read( fdin, p, len)) != len ) {
		*ptr_errmsg = strdup("read() error.");
		goto L1;
    }

	*ptr_dst = p;

	rc = *flen;
L1:
	SAFE_FD_CLOSE(fdin);
L0:
	return(rc);
}

int load_file_buf(char *path, char *qname, off_t de_sum, char**ptr_dst, int *flen, char** ptr_errmsg)
{
	int fdin, rc=-1;
	struct stat statbuf;
	off_t len=0;
	char	*p=NULL;
	int	n;
	char	filename[128];
	long	sub_folder;

	
	sub_folder = de_sum % MAX_SUB_FOLDERS;

	sprintf(filename, "%s/BIG_%s/%04ld/%ld", path, qname, sub_folder, de_sum);

	if(( fdin = open(filename ,O_RDONLY)) < 0 ) { /* option : O_RDWR */
		*ptr_errmsg = strdup("open() error.");
		goto L0;
	}

	if(( fstat(fdin, &statbuf)) < 0 ) {
		*ptr_errmsg = strdup("stat() error.");
		goto L1;
	}

	len = statbuf.st_size;
	*flen = len;

	p = calloc(len+1, sizeof(char));
	if(!p) {
		*ptr_errmsg = strdup("memory leak!! Check your free memory!");	
		goto L1;
	}

	if(( n = read( fdin, p, len)) != len ) {
		*ptr_errmsg = strdup("read() error.");
		goto L1;
    }

	*ptr_dst = p;

	rc = 0;
L1:
	SAFE_FD_CLOSE(fdin);
L0:
	/* We don't unlink for high speed. */
	/* unlink(filename); */

	return(rc);
}

int save_big_file(fq_logger_t *l, const char *data, size_t len, const char* path, const char *qname,  off_t sum )
{
	int fd=-1, n;
	char	filename[128];
	long	sub_folder;

	if(!data || !path || !qname ) {
		fq_log(l, FQ_LOG_ERROR, "illegal function request. Check parameters.");
		return(-3);
	}
		
	sub_folder = sum % MAX_SUB_FOLDERS;

	sprintf(filename, "%s/BIG_%s/%04ld/%ld", path, qname, sub_folder, sum);
	
	/* We don't unlink for speed, so We open to not be O_EXCL. */
	/* if( (fd=open(filename, O_RDWR|O_CREAT|O_EXCL, 0666)) < 0 ) { */
	if( (fd=open(filename, O_RDWR|O_CREAT, 0666)) < 0 ) {
		fq_log(l, FQ_LOG_ERROR, "file open error. reason=[%s] file=[%s]", strerror(errno), filename);
		return(-1);
	}
	if( (n=write(fd, data, len)) != len ) {
		fq_log(l, FQ_LOG_ERROR, "file write error. reason=[%s]", strerror(errno));
		SAFE_FD_CLOSE(fd);
		return(-2);
	}

	SAFE_FD_CLOSE(fd);
	fd = -1;

	return(n);
}

int save_file_mmap(char *data, size_t len, char *qfilename, off_t sum )
{
    int fd=-1, n=0;
    char    filename[128];
    char    *p=NULL;
    char    *errmsg=NULL;

    sprintf(filename, "%s.queue.%ld", qfilename, sum);

    if( (fd=open(filename, O_RDWR|O_CREAT|O_EXCL, 0666)) < 0 ) {
        fprintf(stderr, "file open error. reason=[%s]\n", strerror(errno));
        return(-1);
    }

    if( ftruncate(fd, len) < 0 ) {
        fprintf(stderr, "ftruncate() error. [%s]\n", strerror(errno));
        SAFE_FD_CLOSE(fd);
		fd = -1;
        return(-2);
    }

    if( (p = file_mmapping(fd, len, 0, &errmsg)) == NULL) {
        fprintf(stderr, "mmapping() error. [%s]\n", errmsg);
        SAFE_FD_CLOSE(fd);
		fd = -1;
        return(-3);
    }

    memcpy(p, data, len);

    SAFE_FD_CLOSE(fd);
	fd = -1;

	SAFE_FREE(errmsg);

    return(n);
}

int get_item(const char *str, const char *key, char *dst, int len)
{
	int i,j=0,length,len_key;
	char *p, *q;

	assert(dst);

	dst[0] = '\0';
	if ( !str || !key )
			return (-1);

	length  = strlen(str);
	len_key = strlen(key);

	for(i = 0 ; i < length+1 ; i++) {
	  if(str[i] == '&' || str[i] == 0 ) {
		if(!strncasecmp(str+j, key, len_key) && str[j+len_key] == '='){
			p = (char *)strchr(str+j, '=');
			if(p == NULL) return (-1);
			q = (char *)strchr(p+1,'&');
			if(q == NULL){
				strncpy(dst, p+1, FQ_MIN(str+length-p, len-1));
				dst[FQ_MIN(str+length-p,len-1)] = '\0';;
			}
			else {
				strncpy(dst,p+1,FQ_MIN(q-p-1, len-1));
				dst[FQ_MIN(q-p-1, len-1)] = '\0';;
			}
			return (0);
		}
		j = i+1;
	  }
	}
	return (-1);
}
int str_gethexa(const char* src, char* dst)
{
        char *ptr;
        int ch, n = 0;

        ptr = (char*)src;
        *dst = '\0';
        while ( *ptr && sscanf(ptr, "%x", &ch) > 0 ) {
                *dst = ch;
                dst++; n++;
                while ( *ptr && *ptr != ' ' )
                        ptr++;
                while ( *ptr && *ptr == ' ' )
                        ptr++;
        }
        *dst = '\0';

        return (n);
}

int getitem(const char *str, const char *key, char *dst)
{
        int i,j=0,length,len_key;
        char *p, *q;

        assert(str);
        assert(key);
        assert(dst);

        dst[0] = 0;
        length = strlen(str);  len_key = strlen(key);
        for(i = 0 ; i < length+1 ; i++)
          if(str[i] == '&' || str[i] == 0 ) {
                if(!strncasecmp(str+j, key, len_key) && str[j+len_key] == '='){
                        p = (char *)strchr(str+j,'=');
                        if(p == NULL) return (-1);
                        q = (char *)strchr(p+1,'&');
                        if(q == NULL){
                                strncpy(dst, p+1, str+length-p);
                                dst[str+length-p] = 0;
                        }
                        else {
                                strncpy(dst,p+1,q-p-1);
                                dst[q-p-1] = 0;
                        }
                        return (0);
                }
                j = i+1;
          }
        return (-1);
}

/*
** Àü¿ëÅ¬¶óÀÌ¾ðÆ®¿¡¼­ XMLÀ» ÆÄ½ÌÇÏ±âÀ§ÇÑ ÇÔ¼ö
** PFM_VERSION
*/
int get_item2(const char *str, const char *key, char *dst, int len)
{
	int i,j=0,length,len_key;
	char *p, *q;
	char buf[1024];


	dst[0] = '\0';
	if ( !str || !key )
			return (-1);

	length  = strlen(str);
	len_key = strlen(key);

	for(i = 0 ; i < length+1 ; i++) {
		if(str[i] == '<' || str[i] == 0 ) {
			if(!strncasecmp(str+j, key, len_key) && str[j+len_key] == '>') {
				p = strchr(str+j, '>');
				if(p == NULL) return (-1);
				sprintf(buf, "</%s>", key);
				q = strstr(p+1,buf);
				if(q == NULL){
					return -1;
				}
				else {
					strncpy(dst, p+1, FQ_MIN(q-p-1, len-1));
					dst[FQ_MIN(q-p-1, len-1)] = '\0';
				}

				str_lrtrim2(dst);
				return (0);
			}
			j = i+1;
		}
	}

	return (-1);
}
void str_lrtrim2(char *str)
{
        char *p, *q;

        if ( !(*str) )
                return;

        str_trim(str);
        p = q = str;

        while ( *q && (*q == ' ' || *q == '\t' || *q == '\n'|| *q == '\r') )
                q++;

        while ( *q ) {
                *p = *q;
                q++; p++;
        }
        *p = '\0';
        return;
}
int get_arrayindex(const char* src, char* name, int* index)
{
        int len;
        char *p, *q, buf[80];

        assert(src);

        p = (char *)strchr(src, '[');
        if ( !p )
                return (-1);
        q = (char *)strchr(p, ']');
        if ( !q )
                return (-1);

        len = p - src;
        if ( len == 0 )
                return(-1);
        strncpy(name, src, len);
        *(name + len) = '\0';

        len = q - p;
        if ( len == 0 )
                return(-1);
        strncpy(buf, p+1, len);
        buf[len] = '\0';

        *index = atoi(buf);

        return (0);
}
int get_item_XML(const char *str, const char *key, char *dst, int len)
{
	int i,j=0,length,len_key;
	char *p, *q;
	char buf[1024*20];


	dst[0] = '\0';
	if ( !str || !key )
			return (-1);

	length  = strlen(str);
	len_key = strlen(key);

	for(i = 0 ; i < length+1 ; i++) {
		if(str[i] == '<' || str[i] == 0 ) {
			if(!strncasecmp(str+j, key, len_key) && str[j+len_key] == '>') {
				p = strchr(str+j, '>');
				if(p == NULL) return (-1);
				sprintf(buf, "</%s>", key);
				q = strstr(p+1,buf);
				if(q == NULL){
						return -1;
				}
				else {
						strncpy(dst, p+1, FQ_MIN(q-p-1, len-1));
						dst[FQ_MIN(q-p-1, len-1)] = '\0';
				}

				str_lrtrim2(dst);
				return (0);
			}
			j = i+1;
		}
	}
	return (-1);
}


#if 0
char* getprompt(const char* prompt, char *buf, int size)
{
        char *ptr;
        sigset_t sig, sigsave;
        struct termios term, termsave;
        FILE *fp=NULL;
        int c;

        if ( (fp = fopen(ctermid(NULL), "r+")) == NULL)
                return (NULL);
        setbuf(fp, NULL);

        sigemptyset(&sig);
        sigaddset(&sig, SIGINT);
        sigaddset(&sig, SIGTSTP);
        sigprocmask(SIG_BLOCK, &sig, &sigsave);

        tcgetattr(fileno(fp), &termsave);
        term = termsave;
        tcsetattr(fileno(fp), TCSAFLUSH, &term);

        fputs(prompt, fp);
        ptr = buf;

        while ( (c=getc(fp)) != EOF && c != '\n') {
                if ( ptr < &buf[size] )
                        *ptr++ = c;
        }

        *ptr = 0;

        tcsetattr(fileno(fp), TCSAFLUSH, &termsave);

        sigprocmask(SIG_SETMASK, &sigsave, NULL);

		SAFE_FP_CLOSE(fp);

        return(buf);
}

char* getprompt2(const char* prompt, char *buf, int size)
{
	char *ptr;
	int c;

	fputs(prompt, stdout);
	ptr = buf;

	while ( (c=getc(stdin)) != EOF && c != '\n') {
			if ( ptr < &buf[size] )
					*ptr++ = c;
	}
	*ptr = 0;

	return(buf);
}

#else
char* getprompt(const char* prompt, char *buf, int size)
{
        char *ptr;
        sigset_t sig, sigsave;

        FILE *fp=NULL;
        int c;

        if ( (fp = fopen(ctermid(NULL), "r+")) == NULL)
                return (NULL);

        setbuf(fp, NULL);

        sigemptyset(&sig);
        sigaddset(&sig, SIGINT);
        sigaddset(&sig, SIGTSTP);
        sigprocmask(SIG_BLOCK, &sig, &sigsave);

        fputs(prompt, fp);
        ptr = buf;

        while ( (c=getc(fp)) != EOF && c != '\n') {
                if ( ptr < &buf[size] ) /* prevent overflow */
                        *ptr++ = c;
        }

        *ptr = 0;

        sigprocmask(SIG_SETMASK, &sigsave, NULL);

		SAFE_FP_CLOSE(fp);

        return(buf);
}

char* getprompt_str(const char* prompt, char *buf, int size)
{
	char *ptr;
	int c;

	fputs(prompt, stdout);
	ptr = buf;

	while ( (c=getc(stdin)) != EOF && c != '\n') {
			if ( ptr < &buf[size] )
					*ptr++ = c;
	}
	*ptr = 0;

	return(buf);
}
int getprompt_int(const char* prompt, int *i, int size)
{
	char *buf=NULL;
	char *ptr;
	int c;

	buf = calloc(size+1, sizeof(char));

	fputs(prompt, stdout);
	ptr = buf;

	while ( (c=getc(stdin)) != EOF && c != '\n') {
			if ( ptr < &buf[size] )
					*ptr++ = c;
	}
	*ptr = 0;
	*i = atoi(buf);

	if(buf) free(buf);

	return(*i);
}
long getprompt_long(const char* prompt, long *l, int size)
{
	char *buf=NULL;
	char *ptr;
	int c;

	buf = calloc(size+1, sizeof(char));

	fputs(prompt, stdout);
	ptr = buf;

	while ( (c=getc(stdin)) != EOF && c != '\n') {
			if ( ptr < &buf[size] )
					*ptr++ = c;
	}
	*ptr = 0;
	*l = atol(buf);

	if(buf) free(buf);

	return(*l);
}

float getprompt_float(const char* prompt, float *f, int size)
{
	char *buf=NULL;
	char *ptr;
	int c;

	buf = calloc(size+1, sizeof(char));

	fputs(prompt, stdout);
	ptr = buf;

	while ( (c=getc(stdin)) != EOF && c != '\n') {
			if ( ptr < &buf[size] )
					*ptr++ = c;
	}
	*ptr = 0;
	*f = atof(buf);

	if(buf) free(buf);

	return(*f);
}

#endif

void getstdin( char *prompt, char input_string_buf[], int str_length)
{
	char buf[BUFSIZ];
	
	if( HASVALUE(prompt) ) 
		fputs(prompt, stdout);

	setbuf(stdin, buf);
	getc(stdin);

	strncpy(input_string_buf, buf, str_length-1);

	return;
}

int yesno()
{
	int rc = _KEY_NO;
	char keybuf[16];

	getprompt("continue?(y/n/c)", keybuf, 1);
	switch(keybuf[0]) {
		case 'y':
		case 'Y':
			rc = _KEY_YES;
			break;
		case 'c':
		case 'C':
			rc = _KEY_CONTINUE;
			break;
		default:
			rc = _KEY_NO;
			break;
	}
	return(rc);
}

double unix_getnowtime()
{
        struct timeval  timelong;

        gettimeofday(&timelong, NULL);

        return ((double)timelong.tv_sec + (double)timelong.tv_usec/1000000.0);
}

#if 0
int _checklicense( const char *path )
{
        char            date[64], hostname[64], filename[256], license[32], tmp[32];
        char            dumy[32];
        int             i, hostlen, lastdate;
        double          nowtime, licensetime;
        struct stat statbuff;
        int                     fd=-1;
		char    *fq_home_path=NULL;
		
        gethostname(hostname, 64);

		fq_home_path = getenv("FQ_HOME");
		if( fq_home_path == NULL ) {
			
			fq_home_path = getenv("FQ_DATA_HOME");
			if( fq_home_path == NULL ) {
				sprintf(filename, "%s/%s.license", path, hostname);
			}
			else {
				sprintf(filename, "%s/%s.license", fq_home_path, hostname );
			}
		}
		else {
			sprintf(filename, "%s/%s.license", fq_home_path, hostname );
		}

        if (stat(filename, &statbuff) < 0)
        {
                sprintf(filename, "%s/trial", path);
                if (stat(filename, &statbuff) < 0) return -1;
        }

        fd = open(filename, O_RDONLY, 0);
        if (fd < 0) return -1;
        if( read(fd, tmp, 28) < 0 ) {
			fprintf(stderr, "read('%s') error. (%s)", filename, strerror(errno));
			return(-1);
		}
        SAFE_FD_CLOSE(fd);
		fd = -1;

        for (i = 0; i < 28; i++) dumy[i] = tmp[i] + 31;

        dumy[4] = tmp[4] + 48;
        dumy[7] = tmp[7] + 35;
        dumy[9] = tmp[9] + 42;
        dumy[10] = tmp[10] + 40;
        dumy[13] = tmp[13] + 38;
        dumy[28] = 0;

        memcpy(license, dumy+3, 25); license[25] = 0;

        if (memcmp(license, "test__", 6) == 0)
        {
                nowtime = unix_getnowtime();
                sprintf(date, "%.10s", license+6);
                licensetime = atof(date);

                lastdate = (int)(nowtime - licensetime) / (24 * 3600);
                if (lastdate < 0) return -1;

                lastdate = 30 - lastdate;

                if (lastdate <= 0) return -1;
                return lastdate;
        }

        gethostname(hostname, 32);
        hostlen = strlen(hostname);

        for (i = 0; i < hostlen; i++)
        {
                if (hostname[i] != license[i]) return -1;
        }

        return 0;
}
#else
int _checklicense( const char *path )
{
        char            date[64], hostname[64], filename[256], license[32], tmp[32];
        char            dumy[32];
        int             i, hostlen, lastdate;
        double          nowtime, licensetime;
        struct stat statbuff;
        int                     fd=-1;
		char    *fq_home_path=NULL;

        gethostname(hostname, 64);

		fq_home_path = getenv("FQ_HOME");
		if( fq_home_path == NULL ) {
			
			fq_home_path = getenv("FQ_DATA_HOME");
			if( fq_home_path == NULL ) {
				sprintf(filename, "%s/%s.license", path, hostname);
			}
			else {
				sprintf(filename, "%s/%s.license", fq_home_path, hostname );
			}
		}
		else {
			sprintf(filename, "%s/%s.license", fq_home_path, hostname );
		}

        if (stat(filename, &statbuff) < 0)
        {
            sprintf(filename, "%s/site.samsung.license", path);
			if( stat(filename, &statbuff) <  0 ) { // 
				sprintf(filename, "%s/trial", path);
				if (stat(filename, &statbuff) < 0) return -1;
			}
        }

        fd = open(filename, O_RDONLY, 0);
        if (fd < 0) return -1;
        if( read(fd, tmp, 28) < 0 ) {
			fprintf(stderr, "read('%s') error. (%s)", filename, strerror(errno));
			return(-1);
		}
        SAFE_FD_CLOSE(fd);
		fd = -1;

        for (i = 0; i < 28; i++) dumy[i] = tmp[i] + 31;

        dumy[4] = tmp[4] + 48;
        dumy[7] = tmp[7] + 35;
        dumy[9] = tmp[9] + 42;
        dumy[10] = tmp[10] + 40;
        dumy[13] = tmp[13] + 38;
        dumy[28] = 0;

        memcpy(license, dumy+3, 25); license[25] = 0;

        if (memcmp(license, "test__", 6) == 0)
        {
                nowtime = unix_getnowtime();
                sprintf(date, "%.10s", license+6);
                licensetime = atof(date);

                lastdate = (int)(nowtime - licensetime) / (24 * 3600);
                if (lastdate < 0) return -1;

                lastdate = 30 - lastdate;

                if (lastdate <= 0) return -1;
                return lastdate;
        }
		else if( memcmp(license, "site__", 6) == 0)
		{
                nowtime = unix_getnowtime();
                sprintf(date, "%.10s", license+6);
                licensetime = atof(date);

                lastdate = (int)(nowtime - licensetime) / (24 * 3600);
                if (lastdate < 0) return -1;

                lastdate = (365*10) - lastdate; // 10 yesrs

                if (lastdate <= 0) return -1;
                return lastdate;
		}
        gethostname(hostname, 32);
        hostlen = strlen(hostname);

        for (i = 0; i < hostlen; i++)
        {
                if (hostname[i] != license[i]) return -1;
        }

        return 0;
}
#endif


#if defined(OS_SOLARIS)
/*
** [root@Entrust01 ~]# lsof -i @192.168.0.201:22
** COMMAND   PID   USER   FD   TYPE   DEVICE SIZE/OFF NODE NAME
** sshd    28560   root    3u  IPv4 11483009      0t0  TCP 192.168.0.201:ssh->58.87.61.251:44850 (ESTABLISHED)
** sshd    28565 korean    3u  IPv4 11483009      0t0  TCP 192.168.0.201:ssh->58.87.61.251:44850 (ESTABLISHED)
** sshd    28615   root    3u  IPv4 11483406      0t0  TCP 192.168.0.201:ssh->58.87.61.251:41262 (ESTABLISHED)
** sshd    28620 korean    3u  IPv4 11483406      0t0  TCP 192.168.0.201:ssh->58.87.61.251:41262 (ESTABLISHED)
** sshd    28698   root    3u  IPv4 11484412      0t0  TCP 192.168.0.201:ssh->58.87.61.251:26204 (ESTABLISHED)
** sshd    28703 korean    3u  IPv4 11484412      0t0  TCP 192.168.0.201:ssh->58.87.61.251:26204 (ESTABLISHED)
*/
#define MAXLINE	512
int get_lsof(char *path, char *filename, tlist_t *p, char** ptr_errmsg)
{

	FILE 	*fp=NULL;
	char	buff[1024];
	int		rc = -1;
	char	CMD[128];
	char	cmd[128];
	char	pid[8];
	int		count=0;

	sprintf(CMD, "lsof %s/%s", path, filename); 

	if( (fp=popen(CMD, "r")) == NULL) {
		*ptr_errmsg = strdup("popen() error [lsof]");
		goto L0;
	}

	while(fgets(buff, MAXLINE, fp) != NULL) {
		if( strstr(buff, "COMMAND") != NULL ) continue;
		if( strstr(buff, "PID") != NULL ) continue;

		sscanf(buff, "%s %s %*s %*s %*s %*s %*s %*s %*s", 
			cmd,
			pid);

		list_Add(p, pid, cmd, 0);
		count++;
	}

	if( count == 0 ) {
		*ptr_errmsg = strdup("No data.");
		if(fp!=NULL) {
			pclose(fp);
			fp = NULL;
		}
		return(-1);
	}

#if 0
	list_print(p, stdout);
#endif

	if(fp!=NULL) {
		pclose(fp);
		fp = NULL;
	}
	
	rc = count;
L0:
	return(rc);
}
#endif

int str_isblank(char* str)
{
        char* p;

        p = str;
        while ( *p ) {
                if ( *p != ' ' )
                        return(0);
                p++;
        }
        return(1);
}

#define	ACCESSERRFMT	"WARNING: access %s: %s\n"
/*
 * is_readable() -- is file readable
 */
int
is_readable(path, msg)
	char *path;			/* file path */
	int msg;			/* issue warning message if 1 */
{
	if (access(path, R_OK) < 0) {
	    /* if (!Fwarn && msg == 1) */
	    if ( msg == 1 )
			(void) fprintf(stderr, ACCESSERRFMT, path, strerror(errno));
	    return(0);
	}
	return(1);
}

#ifdef HAVE_UUID_GENERATE
int get_uuid( char *dst, int dstlen )	
{
	uuid_t out ;

	if( dstlen < 37 ) {
		return(-1);
	}
	uuid_generate(out);
	uuid_unparse(out, dst);
	uuid_clear(out);

	return 0;
}
#else
int get_uuid( char *dst, int dstlen )
{
    return(0);
}
#endif

unsigned long conv_endian(unsigned long x)
{
	unsigned long t = x;

	x = ( (x >> 8) | ( x & 0x000000ff ) << 24 ) & 0xff00ff00;
	t = ( (x << 8) | ( t & 0xff000000 ) >> 24 ) & 0x00ff00ff;

	return(x | t);
}

char *get_hostid( char *buf )
{
	struct  utsname ut;

	if( uname(&ut) < 0 ) {
		return( (char *)NULL);
	}
	sprintf(buf, "%s", ut.nodename);

	return(buf);
}

/* #ifndef _LINUX_COMPILE_ */
#if 0
#include <kstat.h>
float get_cpu_idle(void)
{
	kstat_ctl_t *fp=NULL;
	kstat_t	*ksp;
	kstat_named_t   *kname;
	int idle=0, sum=0;
	float	cpu_idle_percent=0.0;
	char	szErrMsg[128];

	sysconf(_SC_CLK_TCK);

	if( (fp = kstat_open()) == NULL) {
		sprintf(szErrMsg, "[%s][%d]reason=[%s]\n",
				__FILE__,  __LINE__, strerror(errno));
		goto L0;
	}

	if ((ksp = kstat_lookup(fp,"cpu", 0 ,NULL)) == NULL) {
		sprintf(szErrMsg, "[%s][%d]reason=[%s]\n",
				__FILE__,__LINE__, strerror(errno));
		goto L1;
	}

	if( kstat_read(fp, ksp, NULL) < 0 ) {
		sprintf(szErrMsg, "[%s][%d]reason=[%s]\n",
				__FILE__, __LINE__, strerror(errno));
		goto L1;
	}

	if (!(kname=kstat_data_lookup(ksp, "cpu_ticks_idle")) ) {
		sprintf(szErrMsg, "[%s][%d]reason=[%s]\n",
				__FILE__,__LINE__, strerror(errno));
		goto L1;
	}
#if 0
	fprintf(stdout, "cpu_ticks_idle=[%d]\n", kname->value.i64);
#endif
	idle = kname->value.i64;
	sum = sum + kname->value.i64;

	if (!(kname=kstat_data_lookup(ksp, "cpu_ticks_kernel")) ) {
		sprintf(szErrMsg, "[%s][%d]reason=[%s]\n",
				__FILE__, __LINE__, strerror(errno));
		goto L1;
	}
#if 0
	fprintf(stdout, "cpu_ticks_idle=[%d]\n", kname->value.i64);
#endif
	sum = sum + kname->value.i64;

	if (!(kname=kstat_data_lookup(ksp, "cpu_ticks_user")) ) {
		sprintf(szErrMsg, "[%s][%d]reason=[%s]\n",
				__FILE__, __LINE__, strerror(errno));
		goto L1;
	}
	sum = sum + kname->value.i64;

#if 0
	cpu_idle_percent = (float)((((float)idle*(float)100)/(float)sum));
#else
	cpu_idle_percent = (float)idle;
#endif
L1:
	kstat_close(fp);
#if 0
	fprintf(stdout, "cpu_idle_percent:[%5.2f]\n", cpu_idle_percent);
#endif
L0:
	return(cpu_idle_percent);
}
#endif

/*
** in Linux, vmstat index for used_CPU is 15.
** in Solaris , index is 22.
** in IBM_AIX used_CPU is 15.
*/
int get_used_CPU(char *buf, int vmstat_index)
{
	FILE *fp=NULL;
	char cpu[512];
	char cmd[512];
	int	n;

	memset(cmd, 0x00, sizeof(cmd));

	/* tot ÀÇ ÀÇ¹Ì: vmstat 1 3 À¸·Î µ¹¸®¸é 3¶óÀÎÀÌ µÇ°í ±×µé °á°ú¸¦ Æò±Õ³»±â À§ÇØ¼­ */
	/*
	sprintf(cmd, "vmstat | awk '/0/ {tot=tot+1; id=id+$%d} END {print 100 - id/tot}'", vmstat_index);
	*/

	sprintf(cmd, "vmstat 1 2| awk 'NR <= 3 {next } { id=$%d } END {print 100 - id}'", vmstat_index);
	
	fp = popen(cmd, "r");

	if( !fp ) {
		return(-1);
	}

	memset(cpu, 0, sizeof(cpu));

	n = fread(cpu, sizeof(cpu), 1, fp);
	if(n < 0) {
		return(-2);
	}

	str_lrtrim(cpu);
	sprintf(buf, "%s", cpu);

	if(fp!=NULL) {
		pclose(fp);
		fp = NULL;
	}
	
	return(strlen(cpu));
}

/*
** in Linux, vmstat index for free_memory is 4.
** in Solaris , index is 4.
** in IBM_AIX free_MEM is 4.
*/
int get_free_MEM(char *buf, int vmstat_index)
{
	FILE *fp=NULL;
	char mem[512];
	char cmd[512];
	int	n;

	memset(cmd, 0x00, sizeof(cmd));

	/*
	sprintf(cmd, "vmstat | awk '/0/ {id=id+$%d} END {print id}'", vmstat_index);
	*/
	sprintf(cmd, "vmstat 1 2| awk 'NR <= 3 {next } { id=$%d } END {print id}'", vmstat_index);
	
	fp = popen(cmd, "r");

	if( !fp ) {
		return(-1);
	}

	memset(mem, 0, sizeof(mem));

	n = fread(mem, sizeof(mem), 1, fp);
	if(n < 0) {
		return(-2);
	}

	str_lrtrim(mem);
	sprintf(buf, "%s", mem);

	if(fp!=NULL) {
		pclose(fp);
		fp = NULL;
	}
	
	return(strlen(mem));
}
int get_used_DISK(char *filesystem_name, char *buf, int vmstat_index)
{
	FILE *fp=NULL;
	char disk[512];
	char cmd[512];
	int	found  = 0;

	memset(cmd, 0x00, sizeof(cmd));

	sprintf(cmd, "df -k %s", filesystem_name);
	
	fp = popen(cmd, "r");

	if( !fp ) {
		return(-1);
	}
	while(fgets(disk, 512, fp) != NULL) {
		char *p=NULL;

		p = strstr(disk, filesystem_name);
		if(p) {
			str_lrtrim2(disk);
			sscanf(disk, "%*s%*s%*s%*s%s", buf);
			found = 1;
			break;
		}
	}
	if(fp != NULL ) {
		pclose(fp);
		fp = NULL;
	}
	
	return(found);
}

int get_cpu_mem_idle(char *CPU_idle_Percent, char *MEM_free_kilo )
{
        FILE *fp = NULL;
        char buf[500];
        int rc =-1, line=0;

        char IdlePercent[10];
        char MemFreeKilo[10];

        fp = popen("vmstat 1 2", "r");
        if(fp == NULL)
        {
                printf("get_cpu_mem_idle() - popen \277\241\267\257!\n");
                sprintf(CPU_idle_Percent, "%s", "err");
                sprintf(MEM_free_kilo, "%s", "err");
                return(rc);
        }

        while(fgets(buf, 500, fp) != NULL)
        {
                if( strstr(buf, "memory") != NULL ) continue;
                if( strstr(buf, "free") != NULL ) continue;
                if( strstr(buf, "disk") != NULL ) continue;
                if( strstr(buf, "swap") != NULL ) continue;

        		line++;

				if(line == 1) continue;

#if 0
                sscanf(buf, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s  %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %s", 
#endif
                sscanf(buf, "%*s %*s %*s %s %*s %*s %*s %*s %*s %*s %*s %*s %s", 
						MemFreeKilo, IdlePercent);

                sprintf(CPU_idle_Percent, "%s", IdlePercent);
                sprintf(MEM_free_kilo, "%s", MemFreeKilo);
				pclose(fp);
				fp = NULL;
                return(0);
        }
		if(fp != NULL ) {
			pclose(fp);
			fp = NULL;
		}
        sprintf(CPU_idle_Percent, "%s", "err");
        sprintf(MEM_free_kilo, "%s", "err");
        return(rc);
}

/* #ifndef _LINUX_COMPILE_ */
#if 0
int kernel_version(void)
{
    struct utsname uts;
    int major, minor, patch;

    if (uname(&uts) < 0)
        return -1;
    if (sscanf(uts.release, "%d.%d.%d", &major, &minor, &patch) != 3)
        return -1;
    return KRELEASE(major, minor, patch);
}
int is_alive_pid( int mypid )
{
	DIR		*dirp;
	struct dirent	*dp;
	psinfo_t	ps;
	char		fullpath[256];
	int		n, fd;

	dirp = opendir("/proc");
	if (dirp == NULL) return(-1);

	while(1)
	{
		dp = readdir(dirp);
		if (dp == NULL) break;
		if (memcmp(dp->d_name, ".", 1) == 0) continue;
		if (memcmp(dp->d_name, "..", 2) == 0) continue;

		sprintf(fullpath, "/proc/%s/psinfo", dp->d_name);
		fd = open(fullpath, O_RDONLY);
		if (fd < 0) continue;

		n = read(fd, (void *)&ps, sizeof(ps));

		if (n <= 0) 
		{
			close(fd);
			fd = -1;
			continue;
		}
		if ( ps.pr_pid == mypid) {
			close(fd);
			fd = -1;
			closedir(dirp);
			return(1);
		}
		close(fd);
		fd = -1;
	}
	closedir(dirp);

	return(0);
}

const char *username(void)
{
    uid_t   uid;
    struct passwd *pwd;

    uid = getuid();
    if ((pwd = getpwuid(uid)) == 0)
	return (0);
    return (pwd->pw_name);
}
#endif

#ifdef HAVE_SYS_RESOURCE_H
int syslimit_print()
{

    int		  resource;
    struct rlimit rlp;

    /*
     *      Redirect stdout and stdin
     */
    FILE *fp1 =  freopen( "/dev/tty", "a", stdout );
    setvbuf( stdout, NULL, _IONBF, 0 );
    FILE *fp2 =  freopen( "/dev/tty", "a", stderr );
    setvbuf( stderr, NULL, _IONBF, 0 );
	
    printf( "Number of limits: %d\n", RLIM_NLIMITS );

    for ( resource = 0; resource < 256; resource++ ) {

    	if ( getrlimit( resource, &rlp )  < 0 ) {
		perror( "getrlimit" );
		return( errno );
	}

	switch( resource ) {

	  case RLIMIT_CPU:
	    printf( "Maximum amount of CPU time in seconds\n" );
	    break;

	  case RLIMIT_DATA:
	    printf( "Maximum size of a process' data segment in bytes\n" );
	    break;

	  case RLIMIT_STACK:
	    printf( "Maximum size of a process' stack in bytes\n" );
	    break;
	    
#ifdef _HPUX_SOURCE
	  case RLIMIT_RSS:
	    printf( "Maximum resident set size in bytes\n" );
	    break;
#endif

	  case RLIMIT_FSIZE:
            printf( "Maximum size of a file in bytes that may be created by "
	        "a process\n" );
	    break;

	  case RLIMIT_CORE:
	    printf( "Maximum size of a core file in bytes\n" );
	    break;

	  case RLIMIT_NOFILE:
	    printf( "Maximum value that the system may assign to a "
		"newly-created descriptor\n" );
	    break;

	  case RLIMIT_AS:
	    printf( "Maximum size of a process' total available memory, "
		"in bytes\n" );
	    break;

#ifdef _HPUX_SOURCE
	  case RLIMIT_TCACHE:
	    printf( "Maximum number of cached threads\n" );
	    break;
#endif

#ifdef _HPUX_SOURCE
	  case RLIMIT_AIO_OPS:
	    printf( "Maximum number of POSIX AIO ops\n" );
	    break;
#endif

#ifdef _HPUX_SOURCE
	  case RLIMIT_AIO_MEM:
	    printf( "Maximum bytes locked for POSIX AIO\n" );
	    break;
#endif

	  case RLIM_NLIMITS:
	    printf( "Number of resource limits\n" );
	    break;

	  default:
	    printf( "Unknown resource: %d\n", resource );
	    break;

	}

	printf( "\tCurrent limit: %lu\n", rlp.rlim_cur );
	printf( "\tHard limit   : %lu\n\n", rlp.rlim_max );

    }
    
    return 0;
}
#endif

void SetLimit(int value)
{
	struct rlimit rlb;
	int	current;

	if ((current = getrlimit(RLIMIT_NOFILE, &rlb)) < 0) {
		printf("warning: getrlimit\n");
		rlb.rlim_cur = 64;
	}
	else {
		if( current < value ) {
			rlb.rlim_cur = value;
		}
		if (setrlimit(RLIMIT_NOFILE, &rlb) < 0) {
			printf("warning: setrlimit\n");
		}
	}
}

#if 0
#include <dirent.h>

static int my_select(const struct dirent *s_dirent)
{ /* filter */
	int g_my_select = DT_DIR;
	int g_my_select_mask = 0;

	if(s_dirent != ((struct dirent *)0)) {
		if((s_dirent->d_type & g_my_select) == g_my_select_mask)
			return(1);
	}
	return(0);
}

extern int errno;

int bt_scandir(const char *s_path)
{
	int s_check, s_index;
	struct dirent **s_dirlist;

#if 0
#define _INCLUDE_DIRECTORY
#endif

#ifndef _INCLUDE_DIRECTORY
	s_check = scandir(s_path, (struct dirent ***)(&s_dirlist), my_select, alphasort);
#else
	s_check = scandir(s_path, (struct dirent ***)(&s_dirlist), 0, alphasort);
#endif

	if(s_check >= 0) {
		(void)fprintf(stdout, "scandir result=%d\n", s_check);
		for(s_index = 0;s_index < s_check;s_index++) {
			struct stat fbuf;
			char fullname[128];

			sprintf(fullname, "%s/%s", s_path, (char *)s_dirlist[s_index]->d_name);

			if( stat(fullname, &fbuf) < 0 ) {
				fprintf(stderr, "stat() error. resson=[%s]\n", strerror(errno));
				continue;
			}

			(void)fprintf(stdout, "[%s](%ld)bytes.\n", 
				(char *)s_dirlist[s_index]->d_name, fbuf.st_size);

			SAFE_FREE((void *)s_dirlist[s_index]);
		}
		
		if(s_dirlist != ((void *)0)) {
			SAFE_FREE((void *)s_dirlist);
		}
		(void)fprintf(stdout, "\x1b[1;33mTOTAL result\x1b[0m=%d\n", s_check);
	}
	return(s_check);
}
#endif

#if 0
int main(int s_argc, char **s_argv)
{
	int n=0;

	while ( 1 )  {
		char    noti_msg[40], syscmd[128];

		if( (n=bt_scandir("/home/bizcube/fnbclog")) > 0) {
			fprintf(stdout, "±¹¹ÎÀºÇà °Å·¡ ¹ÌÈ®ÀÎ ÆÄÀÏ [%d]°Ç ¹ß°ß!\n",  n);
			sprintf(noti_msg, "±¹¹ÎÀºÇà °Å·¡ ¹ÌÈ®ÀÎ ÆÄÀÏ [%d]°Ç ¹ß°ß!\n",  n);

			sprintf(syscmd, "/home/bizcube/install/smsc/bin/smsc -m \"%s\" -r  01692021516 &", noti_msg); 
			system(syscmd);

			sprintf(syscmd, "/home/bizcube/install/smsc/bin/smsc -m \"%s\" -r  0167291520 &", noti_msg); 
			system(syscmd);
		}
		else {
			fprintf(stdout, "±¹¹ÎÀºÇà Á¤»ó.\n");
		}
		sleep(30);
	}
	return(0);
}
#endif
/*
** decription:
**    reserved character : 
**		'!' : ÀÌ¹®ÀÚ ÀÌÈÄ´Â ºñ±³ ¾ÈÇÔ. ( ÀÌ ¹®ÀÚ°¡ ÀÖÀ¸¸é, ±æÀÌ ºñ±³µµ ÇÏÁö ¾ÊÀ½.)
**		'?" : don't care character. ÀÌ À§Ä¡¿¡ ¾î´À ¹®ÀÚ°¡ ¿Íµµ »ó°ü ¾øÀ½.
	sample)
	char	*str_form = "AAAA??BBB?CCCCE????DDDDDD!";
	char	*input="AAAA??BBB?CCCCD????DDDDDD-----------------------";

**	return value)
**		µ¿ÀÏÆÐÅÏ) 0
**		´Ù¸¥ÆÐÅÏ) < 0
**	warning) ÇÔ¼ö »ç¿ëÈÄ errmsg¸¦ ¹Ýµå½Ã SAFE_FREE()ÇÒ °Í.
*/
int pattern_match(char *form, char* input, char** ptr_errmesg)
{
	int  i, rc=-1, form_len, input_len;
	char	szErrMsg[1024];

	form_len = strlen(form);
	input_len = strlen(input);

	if( form[form_len-1] != '!' &&  form_len != input_len ) {
		sprintf(szErrMsg, "form ±æÀÌ[%d] ¿Í input ±æ¸®[%d]°¡ ´Ù¸§.", form_len, input_len);
		goto L0;
	}

	for(i=0; i<form_len; i++) {
		if( form[i] == '?' ) continue; /* Don't care */
		if( form[i] == '!' ) break; /* end mark */

		if( form[i] != input[i]) {
			sprintf(szErrMsg, "[%d]-th ¹®ÀÚ°¡ ´Ù¸§", i );
			goto L0;
		}
	}
	rc = 0;
L0:
	if( rc < 0 ) {
		*ptr_errmesg = strdup(szErrMsg);
	}

	return(rc);
}

static int get_random_fd(void)
{
	struct timeval	tv;
	static int	fd = -2;
	int		i;

	if (fd == -2) {
		gettimeofday(&tv, 0);
#ifndef _WIN32
		fd = open("/dev/urandom", O_RDONLY);
		if (fd == -1)
			fd = open("/dev/random", O_RDONLY | O_NONBLOCK);
		if (fd >= 0) {
			i = fcntl(fd, F_GETFD);
			if (i >= 0)
				fcntl(fd, F_SETFD, i | FD_CLOEXEC);
		}
#endif
		srand((getpid() << 16) ^ getuid() ^ tv.tv_sec ^ tv.tv_usec);
#ifdef DO_JRAND_MIX
		jrand_seed[0] = getpid() ^ (tv.tv_sec & 0xFFFF);
		jrand_seed[1] = getppid() ^ (tv.tv_usec & 0xFFFF);
		jrand_seed[2] = (tv.tv_sec ^ tv.tv_usec) >> 16;
#endif
	}
	/* Crank the random number generator a few times */
	gettimeofday(&tv, 0);
	for (i = (tv.tv_sec ^ tv.tv_usec) & 0x1F; i > 0; i--)
		rand();
	return fd;
}
/*
 * Generate a series of random bytes.  Use /dev/urandom if possible,
 * and if not, use srandom/random.
 */
void get_random_bytes(void *buf, int nbytes)
{
	int i, n = nbytes, fd = get_random_fd();
	int lose_counter = 0;
	unsigned char *cp = (unsigned char *) buf;

#ifdef DO_JRAND_MIX
	unsigned short tmp_seed[3];
#endif

	if (fd >= 0) {
		while (n > 0) {
			i = read(fd, cp, n);
			if (i <= 0) {
				if (lose_counter++ > 16)
					break;
				continue;
			}
			n -= i;
			cp += i;
			lose_counter = 0;
		}
	}

	/*
	 * We do this all the time, but this is the only source of
	 * randomness if /dev/random/urandom is out to lunch.
	 */
	for (cp = buf, i = 0; i < nbytes; i++)
		*cp++ ^= (rand() >> 7) & 0xFF;
#ifdef DO_JRAND_MIX
	memcpy(tmp_seed, jrand_seed, sizeof(tmp_seed));
	jrand_seed[2] = jrand_seed[2] ^ syscall(__NR_gettid);

	for (cp = buf, i = 0; i < nbytes; i++)
		*cp++ ^= (jrand48(tmp_seed) >> 7) & 0xFF;

	memcpy(jrand_seed, tmp_seed, sizeof(jrand_seed)-sizeof(unsigned short));
#endif

	return;
}

#define FILE_LOG_ON		1
#define FILE_LOG_OFF 	0

void get_time(char *todaystr, char* timestr)
{
        time_t tval;
        struct tm t;

        time(&tval);
        localtime_r(&tval, &t);
        if( todaystr)
                sprintf(todaystr,"%04d%02d%02d",
                t.tm_year+1900, t.tm_mon+1, t.tm_mday);

        if( timestr )
                sprintf(timestr, "%02d%02d%02d",
                t.tm_hour, t.tm_min, t.tm_sec);
}

void get_time2(char *todaystr, char* timestr)
{
	time_t tval;
	struct tm t;

    tval = time(NULL);
	localtime_r(&tval, &t);

	strftime( todaystr, 8, "%Y%m%d", &t);
	strftime( timestr, 6, "%H%M%S", &t);

	return;
}

void get_time3(char *date_time)
{
	time_t tval;
	struct tm t;

    tval = time(NULL);
	localtime_r(&tval, &t);

	strftime( date_time, 14, "%Y%m%d%H%M%S", &t);
	return;
}

void get_time_ms(char *todaystr, char* timestr, char *ms)
{
        time_t tval;
        struct tm t;
		struct timeval tp;

        time(&tval);
        localtime_r(&tval, &t);
		gettimeofday(&tp, NULL);

        if( todaystr)
                sprintf(todaystr,"%04d%02d%02d",
                t.tm_year+1900, t.tm_mon+1, t.tm_mday);

        if( timestr )
                sprintf(timestr, "%02d%02d%02d",
                t.tm_hour, t.tm_min, t.tm_sec);
		if( ms )
				sprintf(ms, "%06ld", tp.tv_usec);
}

int errf(char *fmt, ...)
{
        int k;
        va_list ap;
        char buf[256];

        sprintf(buf, "ERR %d: ", (int)pthread_self());
        k = strlen(buf);

        va_start(ap, fmt);
        vsprintf(&buf[k], fmt, ap);
        va_end(ap);

        k = fputs(buf, stderr);
        fflush(stderr);

        return (k);
}

int oserrf(char *fmt, ...)
{
        int k;
        va_list ap;
        char buf[256];

        sprintf(buf, "ERR %d: ", (int)pthread_self());
        k = strlen(buf);

        va_start(ap, fmt);
        vsprintf(&buf[k], fmt, ap);
        va_end(ap);

        k = fputs(buf, stderr);
        fflush(stderr);

        exit(1);
}

void assertf(char* exp, char* srcfile, int lineno)
{
        errf("ASSERTION failed: %s, file %s, line %d\n", exp,
                        srcfile, lineno);
        abort();
}

#if 0
main()
{
	int	rc = -1;
	char	*str_form = "AAAA??BBB?CCCCE????DDDDDD!";
	char	*input="AAAA??BBB?CCCCD????DDDDDD-----------------------";
	char	*errmsg=NULL;
	
	rc = pattern_match(str_form, input, &errmsg);
	printf("%s\n", rc==0?"µ¿ÀÏ":errmsg);
	
	SAFE_FREE(errmsg);

}
#endif
#define FORCE_DIRC_BUFF     1024

void  force_directories( const char *a_dirc )
{

    char    buff[FORCE_DIRC_BUFF+1];
    char   *p_dirc  = buff;

    /* ÀÎ¼ö a_dirc°¡ ³»¿ë ¼öÁ¤ÀÌ ºÒ°¡´ÉÇÑ »ó¼öÀÏ ¼ö ÀÖÀ¸¹Ç·Î ¹öÆÛ¸¦ ÀÌ¿ë */
    memcpy( buff, a_dirc, FORCE_DIRC_BUFF);
    buff[FORCE_DIRC_BUFF]   = '\0';

    while( *p_dirc){
        if ( '/' == *p_dirc){
            *p_dirc = '\0';
            if ( 0 != access( buff, F_OK)){
                mkdir( buff, 0777);
            }
            *p_dirc = '/';
        }
        p_dirc++;
    }
    if ( 0 != access( buff, F_OK)){
        mkdir( buff, 0777);
    }
}

int can_access_file( fq_logger_t *l, const char *fname)
{
	int status;

	FQ_TRACE_ENTER(l);

	if( !HASVALUE(fname) ) {
		fq_log(l, FQ_LOG_ERROR, "filename is null.");
		FQ_TRACE_EXIT(l);
		return(0);
	}

	status = access(fname, F_OK); /* whether the file exists. */
	if( status == 0 ) {
		status = access(fname, R_OK|W_OK); /* whether the file grants read, write. */

		if(status == 0) {
			fq_log(l, FQ_LOG_DEBUG, "[%s] access OK.", fname);
			FQ_TRACE_EXIT(l);
			return(1);
		}
		else {
			fq_log(l, FQ_LOG_ERROR, "[%s] can't access. reason=[%s]", fname, strerror(errno));
			FQ_TRACE_EXIT(l);
			return(0);
		}
	}
	else {
		fq_log(l, FQ_LOG_ERROR, "[%s] does not exist.", fname);
		FQ_TRACE_EXIT(l);
		return(0);
	}
}

int getFileSize(fq_logger_t *l, const char *filename)
{
	int 	fd=-1; 
	struct stat statbuf;
	off_t len=0;

	FQ_TRACE_ENTER(l);

	if( !can_access_file(l, filename) ) {
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if(( fd = open(filename ,O_RDONLY)) < 0 ) { /* option : O_RDWR */
		FQ_TRACE_EXIT(l);
		return(-1);
	}

	if(( fstat(fd, &statbuf)) < 0 ) {
		SAFE_FD_CLOSE(fd);
		FQ_TRACE_EXIT(l);
		return(-2);
	}

	len = statbuf.st_size;
	SAFE_FD_CLOSE(fd);
	FQ_TRACE_EXIT(l);
	return(len);
}
int getProgName(const char *ArgZero, char *ProgName)
{
	int i;
	int	arglen;

	arglen = strlen(ArgZero);
	for(i=arglen; i>0; i--) {
		if( ArgZero[i] == '/') {
			memcpy(ProgName, &ArgZero[i+1], arglen-i);
			return(strlen(ProgName));
		}
	}
	sprintf(ProgName, "%s", ArgZero);
	return(strlen(ProgName));
}
int MicroTime(long *sec, long *msec, char *asctime)
{ 
    struct timeval tp; 
	struct tm t;
	char    buf[32], dst[10];
 
    if(gettimeofday((struct timeval *)&tp, NULL)==0){ 
        (*msec)=tp.tv_usec; /* and microseconds */
        (*sec)=tp.tv_sec;   /* seconds since Jan. 1, 1970 */
 
		localtime_r(sec, &t);

		sprintf(buf, "%06ld", *msec);
		buf[19]=0x00;
		memcpy(dst, buf, 9);
		dst[9]=0x00;

		sprintf(asctime, "%04d/%02d/%02d %02d:%02d:%02d %s",
			t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, dst);
			
        return(0); 
 
    } else { 
        return(-1); 
    } 
} 

/**
 * Redirect the standard file descriptors to /dev/null and route any
 * error messages to the log file.
 */
void redirect_stdfd() {

  int i;
  
  for(i= 0; i < 3; i++) {
    if(close(i) == -1 || open("/dev/null", O_RDWR) != i) {
#if 0
      log("Cannot reopen standard file descriptor (%d) -- %s\n", i, STRERROR);
#endif
      return;
    }
  }
}

/*
 * DESCRIPTION
 * split_line() splits buf loaded from buf by line.
 * argument char *p[] : array pointer.
 * buf: loaded buffer pointer from a file.
 */
int split_line(char *buf, char *p[])
{
	int index=0;
	int	i=0, j=0;
	char tmpbuf[8192];

	memset(tmpbuf, 0x00, sizeof(tmpbuf));

	while (buf[i]) {
		if( buf[i] == '\n') {
			p[index] = strdup(tmpbuf);
			memset(tmpbuf, 0x00, sizeof(tmpbuf));
			index++;
			j=0;
			i++;
		}
		tmpbuf[j++] = buf[i++];
	}

	return(index);
}

int is_file( char *fname)
{
    struct stat stbuf;

    if ( stat(fname, &stbuf) != 0 ) {
        return(0); /* doesn't exist */
    }
    else {
                return(1); /* exist */
    }
}

/*
** This function save a sequence value in the /tmp/.seq_lock with a file."
** so If the server is rebooted, sequece value reset by 0
*/
long fq_get_seq()
{
        long ret=-1;
        int lfd=0, sfd=0;
        char lock_fname[128], seq_fname[128];
        void *lp=NULL, *sp=NULL;
        long x;
        int status;

        lp = calloc(1, sizeof(char));
        sp = calloc(1, sizeof(long));

        sprintf(lock_fname, "%s", "/tmp/.seq_lock");

        if( can_access_file( NULL, lock_fname ) == 0 ) {
				int n;
                if( (lfd = open(lock_fname, O_CREAT|O_EXCL, 0666)) < 0){
                        fprintf(stderr, "lock_file create|open error. reason=[%s]\n", strerror(errno));
                        return(-1);
                }
				SAFE_FD_CLOSE(lfd);

                if( (lfd=open( lock_fname, O_RDWR)) < 0 ) {
                        fprintf(stderr, "lock_file open error. reason=[%s]\n", strerror(errno));
                        return(-1);
                }

                memset(lp, 0x00, 1);
                n = write(lfd, lp, 1);
				if(n != 1) {
					fprintf(stderr, "write() error.");
					return(-1);
				}
				SAFE_FD_CLOSE(lfd);

                printf("new lock_file create and  open OK.\n");
        }

        if( (lfd=open( lock_fname, O_RDWR)) < 0 ) {
                fprintf(stderr, "lock_file open error. reason=[%s]\n", strerror(errno));
				SAFE_FREE(lp);
				SAFE_FREE(sp);
                return(-1);
        }

        if( lseek(lfd, 0, SEEK_SET) < 0 ) {
                fprintf(stderr, "lseek() error. reason=[%s]\n", strerror(errno));
                goto L0;
        }

        status = lockf(lfd, F_LOCK, 1);

        if(status < 0 ) {
                fprintf(stderr, "lockf() failed. reason=[%s]\n", strerror(errno));
				SAFE_FREE(lp);
				SAFE_FREE(sp);
				SAFE_FD_CLOSE(lfd);
                return(-1);
        }

        sprintf(seq_fname, "%s", "/tmp/.seq_num");

		if( can_access_file( NULL, seq_fname ) == 0 ) {
                if( (sfd = open(seq_fname, O_CREAT|O_EXCL, 0666)) < 0){
                        fprintf(stderr, "seq file create|open error. reason=[%s]\n", strerror(errno));
                        goto L0;
                }
                printf("seq file create OK.\n");
                
                if( (sfd=open( seq_fname, O_RDWR)) < 0 ) {
                        fprintf(stderr, "seq file open failed. reason=[%s]\n", strerror(errno));
                        goto L0;
                }
                memset(sp, 0x00, sizeof(long));
                x = 0L;
                memcpy(sp, &x, sizeof(long));
                if( write(sfd, sp, sizeof(long)) < 0 ) {
                        fprintf(stderr, "seq file write error. reason=[%s]\n", strerror(errno));
                        goto L0;
                }
                SAFE_FD_CLOSE(sfd);
        }

        if( (sfd=open( seq_fname, O_RDWR)) < 0 ) {
                fprintf(stderr, "seq file open failed. reason=[%s]\n", strerror(errno));
                goto L0;
        }

        if( lseek(sfd, 0, SEEK_SET) < 0 ) {
                fprintf(stderr, "lseek() error. reason=[%s]\n", strerror(errno));
                goto L0;
        }

        memset(sp, 0x00, sizeof(long));

        if( read( sfd, sp, sizeof(long)) < 0 ) {
                fprintf(stderr, "reason=[%s]\n", strerror(errno));
                goto L0;
        }

        memcpy(&x, sp, sizeof(long));
        x++;

        if( lseek(sfd, 0, SEEK_SET) < 0 ) {
                fprintf(stderr, "lseek() error. reason=[%s]\n", strerror(errno));
                goto L0;
        }

        memcpy(sp, &x, sizeof(long));

        if( write(sfd, sp, sizeof(long)) < 0 ) {
                fprintf(stderr, "reason=[%s]\n", strerror(errno));
                goto L0;
        }
        ret = x;
L0:
		SAFE_FREE(lp);
		SAFE_FREE(sp);

        lockf(lfd, F_ULOCK, 1);

		SAFE_FD_CLOSE(lfd);
		SAFE_FD_CLOSE(sfd);
        return(ret);    
}

/*
** return: TRUE -> success
**         FALSE-> failure
*/
int GetSequeuce( fq_logger_t *l, char *prefix, int total_length, char *buf, size_t buf_size)
{
	long l_seq;
	int no_len;
	char	fmt[40];
	int prefix_len;

	if( buf_size < (total_length + 1 ) ) {
		fq_log(l, FQ_LOG_ERROR, "buffer size(%d) is less than total_length(%d)+1. ", buf_size,  total_length);
		return(FALSE);
	}
	
	prefix_len = strlen(prefix);
	if( prefix ) {
		l_seq = fq_get_seq();
		no_len = total_length  - prefix_len;
		sprintf(fmt, "%s%c0%d%s", prefix, '%', no_len, "ld");
	}
	else {
		sprintf(fmt, "%c0%d%s", '%', total_length, "ld");
	}
	sprintf( buf, fmt, l_seq);

	fq_log(l, FQ_LOG_DEBUG, "SEQ = '%s'", buf);

	return(TRUE);
}

int is_busy_file(char *path, char *filename)
{

	FILE 	*fp=NULL;
	char	buff[1024];
	int		rc = -1;
	char	CMD[128];
	char	cmd[128];
	char	pid[8];
	int		count=0;

	sprintf(CMD, "lsof %s/%s", path, filename); 

	if( (fp=popen(CMD, "r")) == NULL) {
		fprintf(stderr, "popen() error [lsof]\n.");
		goto L0;
	}

	while(fgets(buff, 512, fp) != NULL) {
		if( strstr(buff, "COMMAND") != NULL ) continue;
		if( strstr(buff, "PID") != NULL ) continue;

		sscanf(buff, "%s %s %*s %*s %*s %*s %*s %*s %*s", 
			cmd,
			pid);
		count++;
	}

	if( count == 0 ) {
		if(fp!=NULL) {
			pclose(fp);
			fp = NULL;
		}
		return(0);
	}

	if(fp!=NULL) {
		pclose(fp);
		fp = NULL;
	}
	
	rc = count;
L0:
	return(rc);
}

#ifdef HAVE_SIGNAL_H
/**
 * Set a collective thread signal block for signals honored by monit
 * @param new The signal mask to use for the block
 * @param old The signal mask used to save the previous mask
 */
void set_signal_block(sigset_t *new, sigset_t *old) 
{
  sigemptyset(new);
  sigaddset(new, SIGHUP);
  sigaddset(new, SIGQUIT);
  sigaddset(new, SIGINT);
  sigaddset(new, SIGTERM);
  sigaddset(new, SIGPIPE);
  sigaddset(new, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, new, old);
}

/**
 * Set the thread signal mask back to the old mask
 * @param old The signal mask to restore
 */
void unset_signal_block(sigset_t *old) 
{
  pthread_sigmask(SIG_SETMASK, old, NULL);
}
#endif

#ifdef HAVE_DIV
int is_multiple_int(int number, int multiple) 
{ 
	int ret;
	div_t   div_value;

	div_value   = div(number, multiple);

	if( number == 1 || div_value.rem == 0 ) {
	  ret = FQ_TRUE; 
	} else { 
	  ret = FQ_FALSE; 
	} 

	return ret; 
} 
#endif

#ifdef HAVE_LDIV
int is_multiple_long(long number, long multiple) 
{ 
	int ret;
	ldiv_t   ldiv_value;

	ldiv_value   = ldiv(number, multiple);

	if( number == 1 || ldiv_value.rem == 0 ) {
	  ret = FQ_TRUE; 
	} else { 
	  ret = FQ_FALSE; 
	} 

	return ret; 
} 
#endif

#ifdef OS_LINUX
int is_using_file(char *filename) 
{ 
    int fd = open(filename, O_RDONLY); 

    if (fd < 0) { 
        perror("open"); 
        return(-1); 
    } 
 
    if (fcntl(fd, F_SETLEASE, F_WRLCK) && EAGAIN == errno) { 
		close(fd); 
		return(FQ_TRUE);
    } 
    else { 
        fcntl(fd, F_SETLEASE, F_UNLCK); 
		close(fd); 
		return(FQ_FALSE);
    } 
} 
#endif

void 
step_print( char *front, char *result)
{
	char    buf[80];

	memset(buf, '.', sizeof(buf));
	buf[sizeof(buf)-1] = 0x00;
	memcpy(buf, front, strlen(front));
	memcpy(buf+74, "[XXX]", 5);
	fprintf(stdout, "%s\r", buf);
	fflush(stdout);
	usleep(10000);
	memcpy(buf+75, "   ", 3);
	memcpy(buf+75, result, strlen(result));
	fprintf(stdout, "%s\n", buf);

	return;
}
/* filecopy.c
  filecopy.c,v 1.1 2000/03/02 11:46:36 gwiley Exp
  Glen Wiley, <gwiley@ieee.org>

	Copyright (c)2000, Glen Wiley
   
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.

  this function will reliably copy a file, detecting
  whether source/destination are the same file
  returns 0 on success
*/

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

/* size of the buffer used for copying the file */
static const int COPYBUFFSZ = 4096;

/*-------------------------------------- filecopy
 copy file fn_src to a separate file, fn_dst
 if setown_flag != 0 then we will chown/chgrp to uid/gid otherwise the owner and
 group of the source file will be preserved
 if setmode_flag != 0 then we will chmod to match mode otherwise we use the mode
 on the source file
 return 0 on success, errno on failure
*/
int
filecopy(const char *fn_src, const char *fn_dst, int setown_flag, uid_t uid, gid_t gid, int setmode_flag, mode_t mode)
{
	int   bytesr;
	int   retval  = 0;
	FILE  *fh_src = NULL;
	FILE  *fh_dst = NULL;
	char  *buff   = NULL;
	struct stat stat_s;
	struct stat stat_d;

	buff = (char *) calloc(COPYBUFFSZ, sizeof(char));
	if(buff == NULL)
		return errno;

	if(stat(fn_src, &stat_s) != 0)
		retval = errno;

	/* if we can stat the dest file then make sure it is not the same as src */
	if(stat(fn_dst, &stat_d) == 0)
	{
		if(stat_s.st_dev == stat_d.st_dev && stat_s.st_ino == stat_d.st_ino)
			retval = EEXIST;
	}

	if(!retval)
	{
		fh_src = fopen(fn_src, "r");
		if(fh_src == NULL)
			retval = errno;
	}

	if(!retval)
	{
	 	fh_dst = fopen(fn_dst, "w+");

	 	if(fh_dst == NULL || stat(fn_dst, &stat_d) != 0)
			retval = errno;
		else
		{
			int n;
			/* we make a best effort to change the user/group, if called
				by non-root user this will fail but they should expect that */
			if(setown_flag) {
				n = fchown(fileno(fh_dst), uid, gid);
			} else {
				n = fchown(fileno(fh_dst), stat_s.st_uid, stat_s.st_gid);
			}

			/* we make a best effort to change the mode, this should
				probably not end up failing */
			if(setmode_flag)
				fchmod(fileno(fh_dst), mode);
			else
				fchmod(fileno(fh_dst), stat_s.st_mode);

			while(!retval)
			{
				bytesr = fread(buff, 1, COPYBUFFSZ, fh_src);
				if(bytesr < 1)
				{
					if(feof(fh_src))
						break;
					else
						retval = ferror(fh_src);
				}
				else
				{
					if(fwrite(buff, 1, bytesr, fh_dst) != bytesr)
					{
						retval = ferror(fh_dst);
						break;
					}
				}
			}
		} /* if fh_dst */

		if(fh_dst)
			fclose(fh_dst);

	} /* if !retval */

	if(fh_src)
		fclose(fh_src);

	if(buff)
		free(buff);

	return retval;
} /* filecopy */

/* filecopy.c end.*/

/*
 * subseconds sleeps for System V - or anything that has poll()
 */
#include <poll.h>
int fq_usleep(unsigned int usec)
{
    long subtotal = 0;
    int msec;
    struct pollfd foo;

    subtotal += usec;
    if (subtotal < 1000)
         return(0);

    msec = subtotal/1000;
    subtotal = subtotal%1000;

    return poll(&foo,(unsigned long)0,msec);
}

/*
 * support routine for 4.2BSD system call emulations
 */
int fd_usleep(long usec)
{
    static struct /* `timeval' */ 
    {
         long tv_sec;  /* seconds */ 
         long tv_usec; /* microsecs */ 
    } delay;
     
    delay.tv_sec = usec / 1000000L; 
    delay.tv_usec = usec % 1000000L; 
 
    return select( 0, (fd_set *)0, (fd_set *)0, (fd_set *)0, (struct timeval *)&delay ); 
}

void get_FileLastModifiedTime(char *path, char *todaystr, char* timestr)
{
    struct tm t;
    struct stat attr;

    stat(path, &attr);

    printf("Last modified time: %s", ctime(&attr.st_mtime));

    localtime_r(&attr.st_mtime, &t);
    if( todaystr)
        sprintf(todaystr,"%04d%02d%02d",
        t.tm_year+1900, t.tm_mon+1, t.tm_mday);

    if( timestr )
        sprintf(timestr, "%02d%02d%02d",
        t.tm_hour, t.tm_min, t.tm_sec);

    return;
}

#if 0
       #include <libgen.h>
EXAMPLE
           char *dirc, *basec, *bname, *dname;
           char *path = "/etc/passwd";

           dirc = strdup(path);
           basec = strdup(path);
           dname = dirname(dirc);
           bname = basename(basec);
           printf("dirname=%s, basename=%s\n", dname, bname);
#endif

#if 0
#include <locale.h>
#include <stdio.h>

int main()
{
    float f = 12345.67;
	int	i=123456;

    // obtain the existing locale name for numbers    
    char *oldLocale = setlocale(LC_NUMERIC, NULL);

    // inherit locale from environment
    setlocale(LC_NUMERIC, "");

    // print number
    printf("%'.2f\n", f);
	printf("%'d\n", i);
	printf("%d\n", i);

    // set the locale back
    setlocale(LC_NUMERIC, oldLocale);
}
#endif

#include <stdio.h>
#include <string.h>

#define DOT     '.'
#define COMMA   ','
#define C_MAX     50

static char commas[C_MAX]; /* Where the result is */

char *addcommas(float f) {
  char tmp[C_MAX];            /* temp area */
  sprintf(tmp, "%f", f);    /* refine %f if you need */
  char *dot = strchr(tmp, DOT); /* do we have a DOT? */
  char *src,*dst; /* source, dest */

  if (dot) {            /* Yes */
    dst = commas+C_MAX-strlen(dot)-1; /* set dest to allow the fractional part to fit */
    strcpy(dst, dot);               /* copy that part */
    *dot = 0;       /* 'cut' that frac part in tmp */
    src = --dot;    /* point to last non frac char in tmp */
    dst--;          /* point to previous 'free' char in dest */
  }
  else {                /* No */
    src = tmp+strlen(tmp)-1;    /* src is last char of our float string */
    dst = commas+C_MAX-1;         /* dst is last char of commas */
  }

  int len = strlen(tmp);        /* len is the mantissa size */
  int cnt = 0;                  /* char counter */

  do {
    if ( *src<='9' && *src>='0' ) {  /* add comma is we added 3 digits already */
      if (cnt && !(cnt % 3)) *dst-- = COMMA;
      cnt++; /* mantissa digit count increment */
    }
    *dst-- = *src--;
  } while (--len);

  return dst+1; /* return pointer to result */
}

/*
** ¼³¸í: ÀÌ ÇÁ·Î±×·¥Àº ¾Æ½Ã¾Æ³ª¿¡¼­ EDIFACT¿øº»¿¡ ¼¼¹ÌÄÝ·Ð(;)ÀÌ Æ÷ÇÔµÈ °æ¿ì ÀÌ¸¦ ½ºÆäÀÌ½º·Î
**       º¯È¯ÇÏ±â À§ÇØ Á¦ÀÛÇÑ ÇÔ¼öÀÌ´Ù.
**		 mmap()À¸·Î ÆÄÀÏÀ» ÀÐ°í write() ÇÔ¼ö¸¦ ½á¼­ »õ·Î¿î ÆÄÀÏÀ» »ý¼ºÇÑ´Ù.
**		 mmap()¿¡¼­ Á÷Á¢ º¯¼ö°ªÀ» º¯°æÇÏ¿´À» ¶§, ¹Ý¿µµÇÁö ¾Ê¾Æ¼­ ÀÌ ÇÔ¼ö¸¦ ¸¸µé¾ú´Ù.
**		 ¿Ö mmap()¿¡¼­ ¸Þ¸ð¸®°ª º¯°æÀ¸·Î ÆÄÀÏ º¯°æÀÌ µÇÁö ¾Ê´ÂÁö´Â ²À È®ÀÎÇÒ »çÇ×ÀÌ´Ù.
** ÀÛ¾÷ÀÏ: 2013/12/18
*/
int ReplaceCharactorsInFile(char *in_filename, char *out_filename,  char OldChar, char NewChar, char** ptr_errmsg)
{
	int i;

    int fdin, rc=-1;
    char    *src=NULL;
    struct stat statbuf;
    off_t len=0;
    char szErrMsg[1024];
	int	fdout;
	int	wn;

    if(( fdin = open(in_filename ,O_RDWR)) < 0 ) {	/* option : O_RDWR , O_RDONLY*/
        *ptr_errmsg = strdup("input file open() error.");
        goto L0;
    }

	 if( (fdout=open(out_filename, O_RDWR|O_CREAT|O_EXCL, 0666)) < 0 ) {
		*ptr_errmsg = strdup("output file open() error.");
		goto L0;
    }

    if(( fstat(fdin, &statbuf)) < 0 ) {
        *ptr_errmsg = strdup("stat() error.");
        goto L1;
    }

    len = statbuf.st_size;

    /* option: PROT_READ | PROT_WRITE */

    if(( src = mmap(0,len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fdin, 0 )) == (void *)-1) {
		sprintf(szErrMsg, "mmap(PROT_READ) error: len=%ld", len);
        *ptr_errmsg = strdup(szErrMsg);
        goto L1;
    }

	for(i=0;  i<len; i++ ) {
		if( src[i] == OldChar ) {
			src[i] = NewChar;
		}
	}

	wn = write(fdout, src, len);
	if( wn < 0 ) {
		*ptr_errmsg = strdup("write() error.");
		close(fdin);
		close(fdout);
		return(rc);
	}

	close(fdout);

	munmap(src, len);


    rc = 0;

L1:
    close(fdin);
L0:
    return(rc);
}
/*
** param format: 00:00 , 23:59
*/
int is_running_time(char *from, char *to)
{
    char    cur_date[9], cur_time[7];
    time_t  from_bintime;
    time_t  to_bintime;
	time_t	cur_bintime;
    char    from_hour[3], from_minute[3];
    char    to_hour[3], to_minute[3];
    char    from_asctime[16];
    char    to_asctime[16];

    get_time(cur_date, cur_time);

    memset(from_hour, 0x00, sizeof(from_hour));
    memset(from_minute, 0x00, sizeof(from_minute));
    strncpy(from_hour, from, 2);
    strncpy(from_minute, &from[3], 2);
	memset(from_asctime, 0x00, sizeof(from_asctime));
    sprintf(from_asctime, "%s %s%s00", cur_date, from_hour, from_minute);
    from_bintime =  bin_time(from_asctime);

    memset(to_hour, 0x00, sizeof(to_hour));
    memset(to_minute, 0x00, sizeof(to_minute));
    strncpy(to_hour, to, 2);
    strncpy(to_minute, &to[3], 2);
	memset(to_asctime, 0x00, sizeof(to_asctime));
    sprintf(to_asctime, "%s %s%s00", cur_date, to_hour, to_minute);
    to_bintime =  bin_time(to_asctime);

    cur_bintime = time(0);

	/* debugging */
	printf("from:%ld , [%s]\n", from_bintime, from_asctime);
	printf(" cur:%ld\n", cur_bintime);
	printf("  to:%ld , [%s]\n", to_bintime, to_asctime);


    if( cur_bintime >= from_bintime && cur_bintime <= to_bintime) {
        return(1);
    }
    else {
        return(0);
    }
}

#if 0
int main(int ac, char **av)
{
	int rc;

	while(1) {
		rc = is_running_time("12:05", "13:30");
		printf("rc = [%d]\n", rc);
		if(rc == 0) {
			sleep(5);
			continue;
		}
		else {
			break;
		}
	}
	return(0);
}
#endif
/*
 * ÀÌ ÇÔ¼ö´Â ÁÖ¾îÁø time_t ÀÇ °ª(t)ÀÌ 
 * Á¦ÇÑ½Ã°£(limit)À» ³Ñ¾ú´ÂÁö¸¦ Ã¼Å©ÇÑ´Ù.
 * ³Ñ¾úÀ¸¸é TRUE¸¦ ³ÑÁö ¾Ê¾ÒÀ¸¸é FALSE¸¦ ¸®ÅÏÇÑ´Ù.
 * ¿¹¸¦ µé¸é,
 * health check¸¦ À§ÇÏ¿© 1ÃÊ ÁÖ±â·Î time stamp¸¦ Âï´Â µ¥¸ó ÇÁ·Î¼¼½º°¡
 * »ì¾ÆÀÖ´ÂÁö¸¦ Ã¼Å©ÇÏ±â À§ÇÑ ÇÔ¼öÀÌ´Ù.
 * 5 seconds have passed since?
 *
 */
int is_exceeded_sec(time_t t, long limit)
{
	time_t  current_time, diff_sec;

	current_time = time(0);
	diff_sec = current_time-t;

	if( t != 0 && diff_sec > limit ) {
		return(1); /* dead */
	}
	else {
		return(0); /* live */
	}
}
/*
 * This is a function test code
 */
#if 0
#include "common.h"
#include <stdio.h>
#include <time.h>

#define TRUE 1
#define FALSE 0
int main(int ac, char **av)
{
	time_t	t;

	time(&t);

	sleep(4);

	if( is_exceeded_sec(t, 3) == TRUE) {
		printf("Áö³µ¾î¿ä.\n");
	}
	else {
		printf("¾ÈÁö³µ¾î¿ä.\n");
	}
	return(0);
}
#endif

#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int is_alive_pid_in_Linux( pid_t pid)
{
	struct stat sts;
	char	proc_path_file[256];
	int rc	= 0;
	extern int errno;

	sprintf(proc_path_file, "/proc/%d", (int)pid);

	if (stat(proc_path_file, &sts) == -1 && errno == ENOENT) {
		// fprintf( stderr, "process(%d) doesn't exist.\n", pid);
		rc = 0; /* false */
	}
	else {
		// fprintf( stderr, "process(%d) is alive.\n", pid);
		rc = 1; /* true */
	}
	return(rc);
}
int is_alive_pid_in_general( pid_t pid)
{
	int rc	= 0;
	extern int errno;

	if ( kill(pid, 0)== -1 && errno == ESRCH) {
		// fprintf( stderr, "process(%d) doesn't exist.\n", pid);
		rc = 0;
	}
	else {
		// fprintf( stderr, "process(%d) is alive.\n", pid);
		rc = 1;
	}
	return(rc);
}

#if 0
int main(int ac, char **av)
{
	is_alive_pid_in_Linux(getpid());
	return(0);
}
#endif

int delimiter_parse(char *src, char delimiter, int items, char *dst[])
{
	int i;
	char *p, *q;
	char *tmp=NULL;
	int total_len;
	int len;
	int delimiter_count=0;


	for(i=0; src[i]; i++ ) {
		if(src[i] == delimiter ) {
			delimiter_count++;
		}
	}
	if( (delimiter_count+1) != items ) {
		return(FALSE);
	}

	total_len = strlen(src);
	p = src;
	for(i=0; i<items; i++) {
		if( !p ) break;
		if( q=strchr(p, delimiter) ) {
			len = q-p;
			tmp = calloc(len+1, sizeof(char));
			strncpy(tmp, p, q-p);
			tmp[q-p] = 0x00;
			dst[i] = strdup(tmp);
			p = q + 1;
			free(tmp);
			tmp = NULL;
		}
		else { // last data
			len = total_len - strlen(p);
			tmp = calloc(len+1, sizeof(char));
			strcpy(tmp, p);
			dst[i] = strdup(tmp);
			if(tmp) free(tmp);
			p = NULL;
		}
	}
	return(TRUE);
}

void tps_usleep(int tps)
{
	int x;

	x = 1000000 / tps;
	usleep(x);
}

/*  Return the UNIX time in milliseconds.  You'll need a working
    gettimeofday(), so this won't work on Windows.  */
/*
** uint64_t	start;
** uint64_t end;
** 
** start = milliseconds();
** end = milliseconds();
** printf("took '%u' miliseconds.\n", (uint32_t)(end-start));
*/
uint64_t milliseconds (void)
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (((uint64_t)tv.tv_sec * 1000) + ((uint64_t)tv.tv_usec / 1000));
}

float get_percent(int total, int score)
{
	float percent;

	percent = (float) (score * 100) / total;

	return(percent);
}

int millisec2sec( int mseconds )
{
	int secs;

	secs = mseconds / 1000 + (mseconds % 1000 >= 500 ? 1 : 0);
	return(secs);
}

double microsec2sec( double microsec )
{
	double secs;

	secs =  microsec / 1000000;
	return(secs);
}

unsigned int* chartoint32(unsigned char *pbData, int inLen)
{
	unsigned int *data;
	int len,i;

	if(inLen % 4)
		len = (inLen/4)+1;
	else
		len = (inLen/4);

	data = malloc(sizeof(unsigned int) * len);

	for(i=0;i<len;i++)
	{
		data[i] = ((unsigned int*)pbData)[i];
	}

	return data;
}

unsigned char* int32tochar(unsigned int *pbData, int inLen)
{
	unsigned char *data;
	int i, rc;

	data = malloc(sizeof(unsigned char) * inLen);

	rc = get_endian();
	if( rc == THIS_LITTLE_ENDIAN ) {
		for(i=0;i<inLen;i++)
		{
			data[i] = (unsigned char)(pbData[i/4] >> ((i%4)*8));
		}
	}
	else {
		for(i=0;i<inLen;i++)
		{
			data[i] = (unsigned char)(pbData[i/4] >> ((3-(i%4))*8));
		}
	}

	return data;
}
/*
** Warning! After using, free a pointer
*/
char *decimal_to_binary_string(int n)
{
   int c, d, count;
   char *pointer;
 
   count = 0;
   pointer = (char*)malloc(32+1);
 
   if ( pointer == NULL )
      exit(EXIT_FAILURE);
 
   for ( c = 31 ; c >= 0 ; c-- )
   {
      d = n >> c;
 
      if ( d & 1 )
         *(pointer+count) = 1 + '0';
      else
         *(pointer+count) = 0 + '0';
 
      count++;
   }
   *(pointer+count) = '\0';
 
   return  pointer;
}

/*
** C program to find nCr and nPr: This code calculate nCr which is n!/(r!*(n-r)!) and nPr = n!/(n-r)!
** nPr: ¿¿¿¿: = n!/(n-r)! -> n¿¿ ¿¿¿¿ ¿¿¿¿ r¿¿¿¿ ¿¿¿¿. ¿,
** ¿¿:  ¿¿¿ ¿¿ : N¿ ¿¿¿ 3¿¿ ¿¿¿¿ ¿¿¿ ¿ ¿¿ ¿¿(¿¿¿ ¿¿¿ ¿¿¿¿¿ ¿¿¿¿)
** 9P4 -> 9*8*7*6 = 3024
** 
** nCr: ¿¿¿¿: nPr/r! = 
** ¿¿: ¿¿¿ ¿¿: N¿ ¿¿¿ 3¿¿ ¿¿¿¿ ¿¿¿ ¿ ¿¿ ¿¿(¿¿¿ ¿¿¿ ¿¿¿¿¿ ¿¿¿ ¿¿¿¿¿ ¿¿)
** 9C4: 3024/4! = 3024/24 = 126
*/
#if 0
long factorial(int);
long find_ncr(int, int);
long find_npr(int, int);
int main()
{
   int n, r;
   long ncr, npr;
 
   printf("Enter the value of n and r\n");
   scanf("%d%d",&n,&r);
 
   ncr = find_ncr(n, r);
   npr = find_npr(n, r);
 
   printf("%dC%d = %ld\n", n, r, ncr);
   printf("%dP%d = %ld\n", n, r, npr);
 
   return 0;
}
#endif

long nCr(int n, int r) 
{
   long result;
 
   result = factorial(n)/(factorial(r)*factorial(n-r));
 
   return result;
}
long nPr(int n, int r) 
{
   long result;
 
   result = factorial(n)/factorial(n-r);
 
   return result;
} 
long factorial(int n) 
{
   int c;
   long result = 1;
 
   for (c = 1; c <= n; c++)
      result = result*c;
 
   return result;
}
 
#if 0
int main() {
  long x, y, hcf, lcm;
 
  printf("Enter two integers\n");
  scanf("%ld%ld", &x, &y);
 
  hcf = gcd(x, y);

  lcm = (x*y)/hcf;
 
  printf("Greatest common divisor of %ld and %ld = %ld\n", x, y, hcf);
  printf("Least common multiple of %ld and %ld = %ld\n", x, y, lcm);
 
  return 0;
}
#endif
#if  1
/* ¿¿¿¿¿ ¿¿ */
long gcd(long x, long y) {
  if (x == 0) {
    return y;
  }
 
  while (y != 0) {
    if (x > y) {
      x = x - y;
    }
    else {
      y = y - x;
    }
  }
  return x;
}
#else
long gcd(long a, long b) {
  if (b == 0) {
    return a;
  }
  else {
    return gcd(b, a % b);
  }
}
#endif


long reverse_long(long n) 
{
   static long r = 0;

   if (n == 0)
      return 0;

   r = r * 10;
   r = r + n % 10;
   reverse_long(n/10);
   return r;
}
/* palindrome: ¿¿¿¿¿ ¿¿¿ ¿¿¿¿¿ ¿¿¿ ¿¿ ¿ */
int is_palindrome(int n)
{
	int temp;
	int reverse;

	while( temp != 0 )
	{
      reverse = reverse * 10;
      reverse = reverse + temp%10;
      temp = temp/10;
	}

	if ( n == reverse ) {
		return 1;
	}
	else {
		return 0;
	}
} 
int check_prime(int a)
{
   int c;
 
   for ( c = 2 ; c <= a - 1 ; c++ )
   { 
      if ( a%c == 0 )
	 return 0;
   }
   if ( c == a )
      return 1;
}

static long long power(int n, int r) 
{
   int c;
   long long p = 1;
 
   for (c = 1; c <= r; c++) 
      p = p*n;
 
   return p;   
}

/* ¿ ¿¿¿ ¿¿¿ ¿ ¿¿¿¿ ¿¿ ¿¿¿ ¿¿ ¿¿ ¿¿¿ ¿¿ ¿¿ ¿¿ ¿¿¿¿¿¿¿¿ ¿¿¿ */
int check_armstrong(long long n) 
{
   long long sum = 0, temp;
   int remainder, digits = 0;
 
   temp = n;
 
   while (temp != 0) {
      digits++;
      temp = temp/10;
   }
 
   temp = n;
 
   while (temp != 0) {
      remainder = temp%10;
      sum = sum + power(remainder, digits);
      temp = temp/10;
   }
 
   if (n == sum)
      return 1;
   else
      return 0;
}
/* anagram: ¿¿ ¿¿¿ ¿¿ ¿  */
/* ¿¿¿¿ ¿¿¿¿¿ ¿¿, ¿¿¿ ¿¿ ¿ */
int check_anagram(char a[], char b[])
{
   int first[26] = {0}, second[26] = {0}, c = 0;
 
   while (a[c] != '\0')
   {
      first[a[c]-'a']++;
      c++;
   }
 
   c = 0;
 
   while (b[c] != '\0')
   {
      second[b[c]-'a']++;
      c++;
   }
 
   for (c = 0; c < 26; c++)
   {
      if (first[c] != second[c])
         return 0;
   }
 
   return 1;
}


int countNumWords(const char *str) 
{
  // typedef enum { false=0, true } bool;
  bool inWord = false;
  int wordCount = 0;
  while (*str) {
    if (!inWord && isalpha(*str)) {
      inWord = true;
      wordCount++;
    }
    else if (inWord && *str == ' ') {
      inWord = false;
    }
    str++;
  }
  return wordCount;
}

typedef enum { BY=0, KB, MB, GB, TB } unit_type_t;

static double CalculateSquare(int number)
{
     return (power(number, 2));
}

static double CalculateCube(int number)
{
     return (power(number, 3));
}
static double CalculateTera(int number)
{
     return (power(number, 4));
}

double ConvertSize(double bytes, char* type)
{
    const int CONVERSION_VALUE = 1024;
	unit_type_t i_type;

	if( strncmp(type, "BY", 2)== 0 ) {
			i_type = BY;
	}
	else if( strncmp(type, "KB", 2)== 0 ) {
			i_type = KB;
	}
	else if( strncmp(type, "MB", 2)== 0 ) {
			i_type = MB;
	}
	else if( strncmp(type, "GB", 2)== 0 ) {
			i_type = GB;
	}
	else if( strncmp(type, "TB", 2)== 0 ) {
			i_type = TB;
	}
	else {
		fprintf(stderr, "[%s] is illegal type.[available types: BY/KB/MB/GB/TB]\n", type);
		return( 0 );
	}

	//determine what conversion they want
	switch (i_type)
	{
		case BY:
			 //convert to bytes (default)
			 return bytes;
		case KB:
			 //convert to kilobytes
			 return (bytes / CONVERSION_VALUE);
		case MB:
			 //convert to megabytes
			 return (bytes / CalculateSquare(CONVERSION_VALUE));
		case GB:
			 //convert to gigabytes
			 return (bytes / CalculateCube(CONVERSION_VALUE));
		case TB:
			 //convert to terabytes
			 return (bytes / CalculateTera(CONVERSION_VALUE));
		default:
			 return 0;
	 }
}

void print_current_time()
{
	// char todaystr[9];
	// char timestr[7];
	char todaystr[30];
	char timestr[30];

	memset(todaystr, 0x00, sizeof(todaystr));
	memset(timestr, 0x00, sizeof(timestr));

	time_t tval;
	struct tm t;

	time(&tval);
	localtime_r(&tval, &t);

	short year = t.tm_year+1900;
	short month  = t.tm_mon+1;
	short day = t.tm_mday;

	if( year < 1900 && year > 2050 ) {
		return;
	}
	if( month < 1 && month > 12 ) {
		return;
	}
	if( day < 1 && month > 31 ) {
		return;
	}

	if( todaystr)
			sprintf(todaystr,"%04d%02d%02d", year, month, day);

	short hour = t.tm_hour;
	short min = t.tm_min;
	short sec = t.tm_sec;
	if( hour < 0 && hour > 24 ) {
		return;
	}
	if( min < 0 && min > 60 ) {
		return;
	}
	if( sec < 0 && sec > 60 ) {
		return;
	}

	if( timestr )
			sprintf(timestr, "%02d%02d%02d", hour, min, sec);

	printf("Current : %s %s\n", todaystr, timestr);
}

size_t filesize(int fd)
{
    struct stat statbuf;
    off_t len=0;

    if(( fstat(fd, &statbuf)) < 0 ) {
		perror("fstat");
    }

    return(statbuf.st_size);
}

size_t get_filesize(char *data_filename)
{
	int fd;
    struct stat statbuf;
    off_t len=0;

	fd = open(data_filename, O_RDONLY);
	CHECK(fd >= 0 );

    if(( fstat(fd, &statbuf)) < 0 ) {
		perror("fstat");
    }
	close(fd);

    return(statbuf.st_size);
}

ssize_t block_size(int fd)
{
    struct statfs st;

    assert(fstatfs(fd, &st) != -1);
    return (ssize_t) st.f_bsize;
}

ssize_t mmap_copy_file(int in, int out)
{
    ssize_t w = 0, n;
    size_t len;
    char *p;

    len = filesize(in);
    p = mmap(NULL, len, PROT_READ, MAP_SHARED, in, 0);
    assert(p != NULL);

    while(w < len && (n = write(out, p + w, (len - w)))) {
        if(n == -1) { assert(errno == EINTR); continue; }
        w += n;
    }

    munmap(p, len);

    return w;
}
/*
** speed_level
** 0 ~ 10
** 0: Maximum fast
** 10: slow : about 100 sec
*/
int user_job(int speed_level)
{
	char *p_usleep_time=NULL;
	long  usleep_time;

	p_usleep_time = calloc( speed_level+1, sizeof(char));

	if( get_seed_ratio_rand_str( p_usleep_time,  speed_level, "0123456789") == NULL ) {
		SAFE_FREE(p_usleep_time);
		return(-1);
	}

	/* Max time is 1 sec */
	usleep_time = atol(p_usleep_time);

	printf("working time = [%ld] usec.\n", usleep_time);

	usleep( usleep_time);

	SAFE_FREE(p_usleep_time);
	return(0);
}
/**
 ****************************************************|
 * String replace Program                            |
 ****************************************************|
 * Takes three string input from the user
 * Replaces all the occurances of the second string
 * with the third string from the first string
 * @author Swashata
 */
/** Define the max char length */
#define MAX_REPLACE_LEN 8192
 
#if 0
int main(void) {
    char o_string[MAX_L], s_string[MAX_L], r_string[MAX_L]; //String storing variables
 
    printf("Please enter the original string (max length %d characters): ", MAX_L);
    fflush(stdin);
    gets(o_string);
 
    printf("\nPlease enter the string to search (max length %d characters): ", MAX_L);
    fflush(stdin);
    gets(s_string);
 
    printf("\nPlease enter the replace string (max length %d characters): ", MAX_L);
    fflush(stdin);
    gets(r_string);
 
    printf("\n\nThe Original string\n*************************************\n");
    puts(o_string);
 
    replace(o_string, s_string, r_string);
 
    printf("\n\nThe replaced string\n*************************************\n");
    puts(o_string);
 
    return 0;
}
#endif
 
/**
 * The replace function
 *
 * Searches all of the occurrences using recursion
 * and replaces with the given string
 * @param char * o_string The original string
 * @param char * s_string The string to search for
 * @param char * r_string The replace string
 * @return void The o_string passed is modified
 */
void replace(char * o_string, char * s_string, char * r_string) 
{
      /* a buffer variable to do all replace things */
      char buffer[MAX_REPLACE_LEN];
      /* to store the pointer returned from strstr */
      char * ch;
 
      /* first exit condition */
      if(!(ch = strstr(o_string, s_string)))
              return;
 
      /* copy all the content to buffer before the first occurrence of the search string */
      strncpy(buffer, o_string, ch-o_string);
 
      /* prepare the buffer for appending by adding a null to the end of it */
      buffer[ch-o_string] = 0;
 
      /* append using sprintf function *.
      sprintf(buffer+(ch - o_string), "%s%s", r_string, ch + strlen(s_string));
 
      /* empty o_string for copying */
      o_string[0] = 0;
      strcpy(o_string, buffer);

      /* pass recursively to replace other occurrences */
      return replace(o_string, s_string, r_string);
 }

// header for statvfs
#include <sys/statvfs.h>
#if 0
struct statvfs {
    unsigned long  f_bsize;    /* filesystem block size */
    unsigned long  f_frsize;   /* fragment size */
    fsblkcnt_t     f_blocks;   /* size of fs in f_frsize units */
    fsblkcnt_t     f_bfree;    /* # free blocks */
    fsblkcnt_t     f_bavail;   /* # free blocks for unprivileged users */
    fsfilcnt_t     f_files;    /* # inodes */
    fsfilcnt_t     f_ffree;    /* # free inodes */
    fsfilcnt_t     f_favail;   /* # free inodes for unprivileged users */
    unsigned long  f_fsid;     /* filesystem ID */
    unsigned long  f_flag;     /* mount flags */
    unsigned long  f_namemax;  /* maximum filename length */
};
#endif

long GetAvailableSpace(const char* path)
{
  struct statvfs stat;
  
  if (statvfs(path, &stat) != 0) {
    // error happens, just quits here
    return -1;
  }

  // the available size is f_bsize * f_bavail
  // return stat.f_bsize * stat.f_bavail;
  return stat.f_bavail;
}

/*
**  system command, fuser  
**
*/
int get_fuser(char *pathfile, char **out)
{
	FILE *fp=NULL;
	char cmd[512];
	char pid_list[1024];
	int	n;

	memset(cmd, 0x00, sizeof(cmd));

	sprintf(cmd, "fuser %s | awk '{print $0}'", pathfile);
	
	fp = popen(cmd, "r");

	if( !fp ) {
		return(-1);
	}

	n = fread(pid_list, sizeof(pid_list), 1, fp);
	if(n < 0) {
		if( fp ) pclose(fp);
		fp = NULL;
		return(-2);
	}

	*out = strdup(pid_list);

	if(fp!=NULL) {
		pclose(fp);
		fp = NULL;
	}
	
	return(strlen(*out));
}

char * os_readfile(const char *name, size_t *len)
{
	FILE *f;
	char *buf;
	long pos;

	f = fopen(name, "rb");
	if (f == NULL)
		return NULL;

	if (fseek(f, 0, SEEK_END) < 0 || (pos = ftell(f)) < 0) {
		fclose(f);
		return NULL;
	}
	*len = pos;
	if (fseek(f, 0, SEEK_SET) < 0) {
		fclose(f);
		return NULL;
	}

	buf = malloc(*len);
	
	if (buf == NULL) {
		fclose(f);
		return NULL;
	}

	if (fread(buf, 1, *len, f) != *len) {
		fclose(f);
		free(buf);
		return NULL;
	}

	fclose(f);

	return buf;
}
int make_daemon(void)
{
	pid_t pid;
	int rc;

	if ((pid = fork()) < 0)
		return -1;
	else if (pid != 0)
		exit(0); /* parent exit */

	close(0);
	close(1);
	close(2);

	setsid();
	rc = chdir("/");
	CHECK(rc == 0);
	umask(0);

	return 0;
}
#include <math.h>
/* Function to check if x is power of 2*/
int isPowerOfTwo(int n) 
{ 
   if(n==0) 
   return 0; 
  
   return (ceil(log2(n)) == floor(log2(n))); 
} 

void safe_fputs( char *src, FILE *stream)
{
	char *p;

	flockfile(stream);
	for(p = src; *p; p++) {
		putc_unlocked((int) *p, stream);
	}
	funlockfile(stream);
}

/*
** sanitize
** clear, remove something,
*/
char *sanitize(char *buf, char *s)
{
	char *t;

	ASSERT(buf);

	for (t=buf;*s; s++)
		if (*s == ' ') *t++ = '+';
		else if (isalnum(*s)) *t++ = *s;
		else {
			sprintf(t, "%%%02X", (unsigned char)*s);
			t += 3;
		}
	*t = '\0';
	return (buf);
}

/*
** replace A from B
*/
void subst(char *s, char from, char to) {
    while (*s == from)
    *s++ = to;
}

/*
** example
*/
#if 0
#include <stdio.h>
int width = 20;
char buf[4096];
int main() {
    sprintf(buf, "%0*d", width, 0); /* width:20, 0 padding, put value 0 : 00000000000000000000*/ 
    subst(buf, '0', '-');
    printf("%s\n", buf);
    return 0;
}
#endif

void make_segment_fault()
{
	*((unsigned long *)0) = 1234; /* make a segment fault ! */
	return;
}

bool is_working_time( fq_logger_t *l, char *from_time, char *to_time)
{
	time_t 	now_bin = time(0);
	struct tm now_st;
	localtime_r(&now_bin, &now_st);
	time_t	limit_start_bin;
	time_t	limit_end_bin;

	if( atoi(from_time) >= atoi(to_time) ) {
		fq_log(l, FQ_LOG_ERROR, "Time setting error. your settings from=[%s], to=[%s].",
			from_time, to_time);
		return true;
	}
	if( strlen(from_time) != 6 || strlen(to_time) != 6 ) {
		fq_log(l, FQ_LOG_ERROR, "Time length setting error. your settings from=[%s], to=[%s].",
			from_time, to_time);
		return true;
	}
	
	char date[9], time[7];
	get_time(date, time);

	char from_asc_time[16], to_asc_time[16];
	sprintf(from_asc_time, "%s %s", date, from_time);
	time_t from_bin;
	from_bin = bin_time( from_asc_time);

	sprintf(to_asc_time, "%s %s", date, to_time);
	time_t to_bin;
	to_bin = bin_time( to_asc_time);

	fq_log(l, FQ_LOG_DEBUG, "from: %ld, now: %ld, to: %ld", from_bin, now_bin, to_bin);

	if( (now_bin >= from_bin) && (now_bin < to_bin) ) {
		fq_log(l, FQ_LOG_DEBUG, "It is working time.");
		return true;
	}
	else {
		fq_log(l, FQ_LOG_DEBUG, "It is not working time.");
		return false;
	}
}

bool get_LR_value(char *src, char delimiter, char *left, char *right)
{
	char *p = NULL;
	char *start = src;
	char *end = src + strlen(src);
	int left_len;
	int right_len;


	p = strchr( src, delimiter);
	if( !p ) {
		return(false);
	}

	left_len = p - start;
	memcpy(left, src, left_len);
	left[left_len] = 0x00;

	right_len = (end-p) -1;
	memcpy(right, p+1, right_len);
	right[right_len] = 0x00;

	return(true);
}

char *alloc_reverse( char *quiz )
{
	char *p = quiz;
	int last_index = strlen(quiz)-1;

	char *dst = calloc( strlen(quiz)+1, sizeof(char));

	for(p=quiz; *p; p++) {
		dst[last_index--] = *p;
	}
	return dst;
}

bool chg_reverse( char *quiz )
{
	char *p = quiz;
	int last_index = strlen(quiz)-1;

	char *dst = calloc( strlen(quiz)+1, sizeof(char));

	for(p=quiz; *p; p++) {
		dst[last_index--] = *p;
	}
	memcpy(quiz, dst, strlen(dst));

	if(dst) free(dst);

	return true;
}

void exchg_reverse(char s[])
{
	char c;
	int  i, j;
	
	for(i=0, j=strlen(s)-1; i<j; i++, j--) {
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
	return;
}

#define FILENAME_SIZE 1024
#define MAX_LINE 2048
int  find_a_line_from_file( fq_logger_t *l, char *filename, char *line_string)
{
	FILE *file;
  	char buffer[MAX_LINE];

	FQ_TRACE_ENTER(l);

	file = fopen(filename, "r");
	if( file == NULL ) {
		fq_log(l, FQ_LOG_ERROR, "Error opening file(s) '%s'.", filename);
		FQ_TRACE_EXIT(l);

		return 1;
	}
	bool keep_reading = true;
	int current_line = 1;
	do 
	{
		// stores the next line from the file into the buffer
		fgets(buffer, MAX_LINE, file);
		if (feof(file)) keep_reading = false;

		buffer[strlen(buffer)-1] = 0x00; /* remove newline charactor */

		if( strcmp( line_string, buffer ) == 0 ) {
			fq_log(l, FQ_LOG_DEBUG, "found a line(%d) key='%s'.", current_line, line_string);
			return(current_line);
		}
		current_line++;

	} while (keep_reading);

	// close our access to the files
	fclose(file);
	return(-1);  // not found.
}

int delete_a_line_from_file( fq_logger_t *l, char *filename, int delete_line)
{
	// file handles for the original file and temp file
	FILE *file, *temp;
  	char temp_filename[FILENAME_SIZE];
  	// will store each line in the file, and the line to delete
  	char buffer[MAX_LINE];

	FQ_TRACE_ENTER(l);

	// creates a temp filename "temp___filename.txt" where filename.txt is the 
	// name of the file entered by the user by first copying the prefix temp____
	// to temp_filename and then appending the original filename
	sprintf(temp_filename, "%s,temp__", filename);

	// open the original file for reading and the temp file for writing
	file = fopen(filename, "r");
	temp = fopen(temp_filename, "w");

	// if there was a problem opening either file let the user know what the error
	// was and exit with a non-zero error status
	if (file == NULL)
	{
		fq_log(l, FQ_LOG_ERROR, "Error opening file(s). '%s'", filename);
		FQ_TRACE_EXIT(l);
		return 1;
	}
	if (temp == NULL)
	{
		fq_log(l, FQ_LOG_ERROR, "Error opening file(s). '%s'", temp);
		FQ_TRACE_EXIT(l);
		return 1;
	}

	// current_line will keep track of the current line number being read
	bool keep_reading = true;
	int current_line = 1;
	do 
	{
		// stores the next line from the file into the buffer
		fgets(buffer, MAX_LINE, file);

		// if we've reached the end of the file, stop reading from the file, 
		// otherwise so long as the current line is NOT the line we want to 
		// delete, write it to the file
		if (feof(file)) keep_reading = false;
		else if (current_line != delete_line)
		fputs(buffer, temp);

		// keeps track of the current line being read
		current_line++;

	} while (keep_reading);

	// close our access to the files
	fclose(file);
	fclose(temp);

	// delete the original file, give the temp file the name of the original file
	remove(filename);
	rename(temp_filename, filename);

	FQ_TRACE_EXIT(l);
	return 0;
}
int replace_a_line_from_file( fq_logger_t *l, char *filename, int replace_line, char *replace_string)
{

	// file handles for the original file and temp file
	FILE *file, *temp;
  	char temp_filename[FILENAME_SIZE];
  	// will store each line in the file, and the line to delete
  	char buffer[MAX_LINE];

	FQ_TRACE_ENTER(l);

	// creates a temp filename "temp___filename.txt" where filename.txt is the 
	// name of the file entered by the user by first copying the prefix temp____
	// to temp_filename and then appending the original filename
	sprintf(temp_filename, "%s,temp__", filename);

	// open the original file for reading and the temp file for writing
	file = fopen(filename, "r");
	temp = fopen(temp_filename, "w");

	// if there was a problem opening either file let the user know what the error
	// was and exit with a non-zero error status
	if (file == NULL)
	{
		fq_log(l, FQ_LOG_ERROR, "Error opening file(s). '%s'", filename);
		FQ_TRACE_EXIT(l);
		return 1;
	}
	if (temp == NULL)
	{
		fq_log(l, FQ_LOG_ERROR, "Error opening file(s). '%s'", temp);
		FQ_TRACE_EXIT(l);
		return 1;
	}

	// current_line will keep track of the current line number being read
	bool keep_reading = true;
	int current_line = 1;
	do 
	{
		// read the next line of the file into the buffer
		fgets(buffer, MAX_LINE, file);
		
		// if we've reached the end of the file, stop reading
		if (feof(file)) keep_reading = false;
		// if the line we've reached is the one we wish to replace, write the 
		// replacement text to this line of the temp file
		else if (current_line == replace_line) {
		  fputs(replace_string, temp);
		  fputs("\n", temp);
		}
		// otherwise write this line to the temp file
		else fputs(buffer, temp);
		
		// increment the current line as we will now be reading the next line
		current_line++;

	} while (keep_reading);

	// close our access to the files
	fclose(file);
	fclose(temp);

	// delete the original file, give the temp file the name of the original file
	remove(filename);
	rename(temp_filename, filename);

	FQ_TRACE_EXIT(l);
	return 0;
}
