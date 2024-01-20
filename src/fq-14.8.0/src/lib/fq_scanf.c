/* fq_scanf.c */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "fq_atob.h"

#define MAXLN	256

int fq_scanf (int delimiter, const char *fmt, ...);
int fq_sscanf (int delimiter, const char *buf, const char *fmt, ...);
int fq_fscanf (int delimiter, FILE *fp, const char *fmt, ...);
static int fq_vfscanf (int delimiter, FILE *fp, const char *fmt, va_list ap);
static int fq_vsscanf (int delimiter, const char *buf, const char *s, va_list ap);

int isdelimiter(int c, int delimiter)
{
	if( delimiter == 0x20 ) {
		return ((c>=0x09 && c<=0x0D) || (c==0x20));
	}
	else {
		return (c==delimiter); 
	}
}

/*
** va_list: Varialbe Arguments Lists
** ap: Argument Pointer like "%s%s%s%d"
** count: number of arguments
*/
int 
fq_scanf (int delimiter, const char *fmt, ...)
{
    int             count;
    va_list ap;
    
    va_start (ap, fmt);
    count = fq_vfscanf (delimiter, stdin, fmt, ap);
    va_end (ap);
    return (count);
}

/*
 *  fscanf(fp,fmt,va_alist)
 */
int 
fq_fscanf (int delimiter, FILE *fp, const char *fmt, ...)
{
    int             count;
    va_list ap;

    va_start (ap, fmt);
    count = fq_vfscanf (delimiter, fp, fmt, ap);
    va_end (ap);
    return (count);
}

/*
 *  sscanf(buf,fmt,va_alist)
 */
int 
fq_sscanf (int delimiter, const char *buf, const char *fmt, ...)
{
    int             count;
    va_list ap;
    
    va_start (ap, fmt);
    count = fq_vsscanf (delimiter, buf, fmt, ap);
    va_end (ap);
    return (count);
}

/*
 *  vfscanf(fp,fmt,ap) 
 */
static int
fq_vfscanf (int delimiter, FILE *fp, const char *fmt, va_list ap)
{
    int             count;
    char            buf[MAXLN + 1];

    if (fgets (buf, MAXLN, fp) == 0)
	return (-1);
    count = fq_vsscanf (delimiter, buf, fmt, ap);
    return (count);
}


/*
 *  vsscanf(buf,fmt,ap)
 */
static int
fq_vsscanf (int delimiter, const char *buf, const char *s, va_list ap)
{
    int             count, noassign, width, base, lflag;
    const char     *tc;
    char           *t, tmp[MAXLN];
	char	*str_delimiters=NULL;

	switch(delimiter) {
		case 0x7C:
			str_delimiters = "|";
			break;
		case 0x2F:
			str_delimiters = "/";
			break;
		case 0x5E:
			str_delimiters = "^";
			break;
		case 0x7E:
			str_delimiters = "~";
			break;
		case 0x2C:
			str_delimiters = ",";
			break;
		case 0x2B:
			str_delimiters = "+";
			break;
		case 0x25:
			str_delimiters = "%";
			break;
		case 0x3A:
			str_delimiters = ":";
			break;
		case 0x3B:
			str_delimiters = ";";
			break;
		case 0x26:
			str_delimiters = "&";
			break;
		case 0x3D:
			str_delimiters = "=";
			break;
		case 0x20:
			str_delimiters = " \t\n\r\f\v";
			break;
		default:
			printf("[%d]-character is not supported as a delimiter.\n", delimiter);
			return(-1);
	}

    count = noassign = width = lflag = 0;

    while (*s && *buf) {
		while (isdelimiter (*s, delimiter))
			s++;
		if (*s == '%') {
			s++;
			for (; *s; s++) {
				if (strchr ("dibouxcsefg%", *s))
					break;
				if (*s == '*')
					noassign = 1;
				else if (*s == 'l' || *s == 'L')
					lflag = 1;
				else if (*s >= '1' && *s <= '9') {
					for (tc = s; isdigit (*s); s++);
					strncpy (tmp, tc, s - tc);
					tmp[s - tc] = '\0';
					atob ((unsigned int *)&width, tmp, 10);
					s--;
				}
			}
			if (*s == 's') {
				while (isdelimiter (*buf, delimiter))
					buf++;
				if (!width)
					width = strcspn (buf, str_delimiters);
				if (!noassign) {
					strncpy (t = va_arg (ap, char *), buf, width);
					t[width] = '\0';
				}
				buf += width;
			} else if (*s == 'c') {
				if (!width)
					width = 1;
				if (!noassign) {
					strncpy (t = va_arg (ap, char *), buf, width);
					t[width] = '\0';
				}
				buf += width;
			} else if (strchr ("dobxu", *s)) {
				while (isdelimiter (*buf, delimiter))
					buf++;

				if (*s == 'd' || *s == 'u')
					base = 10;
				else if (*s == 'x')
					base = 16;
				else if (*s == 'o')
					base = 8;
				else if (*s == 'b')
					base = 2;
				if (!width) {
					if (isdelimiter (*(s + 1), delimiter) || *(s + 1) == 0)
					width = strcspn (buf, str_delimiters);
					else
					width = strchr (buf, *(s + 1)) - buf;
				}
				strncpy (tmp, buf, width);
				tmp[width] = '\0';
				buf += width;
				if (!noassign)
					atob (va_arg (ap, u_int32_t *), tmp, base);
			}
			if (!noassign)
			count++;
			width = noassign = lflag = 0;
			s++;
		} else {
			while (isdelimiter (*buf, delimiter))
			buf++;
			if (*s != *buf)
			break;
			else
			s++, buf++;
		}
    }
    return (count);
}

#if 0
int main(int ac, char **av)
{
	int 	delimiter = 0x7C;
	char *p=NULL;
	char buf1[16];
	char buf2[16];
	char buf3[16];
	char buf4[16];

	p = "1^2^3^4";

	fq_sscanf(delimiter, p, "%s%s%s%s", buf1, buf2, buf3, buf4);

	printf("[%s] [%s] [%s] [%s] \n",  buf1, buf2, buf3, buf4);

	return(0);
} 
#endif
