#include "byteorder.h"

void decode16_r(unsigned short *out, const unsigned char *in)
{
	unsigned char *outb = (unsigned char *) out;
	outb[0] = in[1];
	outb[1] = in[0];
}

void decode16_rv(unsigned short *out, const unsigned char *in, int count)
{
	for (; count-- > 0; out++, in += sizeof(unsigned short))
		decode16_r(out, in);
}

void encode16_r(unsigned char *out, const unsigned short *in)
{
	unsigned char *inb = (unsigned char *) in;
	out[0] = inb[1];
	out[1] = inb[0];
}

void encode16_rv(unsigned char *out, const unsigned short *in, int count)
{
	for (; count-- > 0; out += sizeof(unsigned short), in++)
		encode16_r(out, in);
}

void decode32_r(unsigned long *out, const unsigned char *in)
{
	unsigned char *outb = (unsigned char *) out;
	outb[0] = in[3];
	outb[1] = in[2];
	outb[2] = in[1];
	outb[3] = in[0];
}

void decode32_rv(unsigned long *out, const unsigned char *in, int count)
{
	for (; count-- > 0; out++, in += sizeof(unsigned long))
		decode32_r(out, in);
}

void encode32_r(unsigned char *out, const unsigned long *in)
{
	unsigned char *inb = (unsigned char *) in;
	out[0] = inb[3];
	out[1] = inb[2];
	out[2] = inb[1];
	out[3] = inb[0];
}

void encode32_rv(unsigned char *out, const unsigned long *in, int count)
{
	for (; count-- > 0; out += sizeof(unsigned long), in++)
		encode32_r(out, in);
}
