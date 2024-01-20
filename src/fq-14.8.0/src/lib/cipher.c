#include <string.h>

#include "cipher.h"

static unsigned char *PADDING[] = {
	(unsigned char *) "",
	(unsigned char *) "\x01",
	(unsigned char *) "\x02\x02",
	(unsigned char *) "\x03\x03\x03",
	(unsigned char *) "\x04\x04\x04\x04",
	(unsigned char *) "\x05\x05\x05\x05\x05",
	(unsigned char *) "\x06\x06\x06\x06\x06\x06",
	(unsigned char *) "\x07\x07\x07\x07\x07\x07\x07",
	(unsigned char *) "\x08\x08\x08\x08\x08\x08\x08\x08",
	(unsigned char *) "\x09\x09\x09\x09\x09\x09\x09\x09\x09",
	(unsigned char *) "\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a",
	(unsigned char *) "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
	(unsigned char *) "\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c",
	(unsigned char *) "\x0d\x0d\x0d\x0d\x0d\x0d\x0d\x0d\x0d\x0d\x0d\x0d\x0d",
	(unsigned char *) "\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e\x0e",
	(unsigned char *) "\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f\x0f",
	(unsigned char *) "\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10",
};

void CipherInit (
	CIPHER_CTX *context,
	const CIPHER_ALGORITHM *cipher,
	void *cipher_ctx,
	const unsigned char *iv,
	const unsigned char *key,
	int keyLength,
	int encrypt
)
{
	context->cipher = cipher;
	context->cipher_ctx = cipher_ctx;
	(*cipher->init)(context->cipher_ctx, iv, key, keyLength, encrypt);
	context->bufferLen = 0;
	context->encrypt = encrypt;
}

int CipherUpdate (
	CIPHER_CTX *context,
	unsigned char *partOut,
	const unsigned char *partIn,
	unsigned int partInLen
)
{
	const CIPHER_ALGORITHM *cipher;
	unsigned int tempLen, partOutLen;
	int sizeOfBlock;

	cipher = context->cipher;
	sizeOfBlock = cipher->sizeOfBlock;
	tempLen = sizeOfBlock - context->bufferLen;

	if (partInLen < tempLen || (partInLen == tempLen && context->encrypt == CIPHER_DECRYPT)) {
		memcpy(context->buffer + context->bufferLen, partIn, partInLen);
		context->bufferLen += partInLen;
		return 0;
	}

	memcpy(context->buffer + context->bufferLen, partIn, tempLen);

	(*cipher->update)(context->cipher_ctx, partOut, context->buffer, sizeOfBlock);
	partOut += sizeOfBlock;
	partOutLen = sizeOfBlock;
	partIn += tempLen;
	partInLen -= tempLen;

	if (context->encrypt == CIPHER_ENCRYPT)
		tempLen = sizeOfBlock * (partInLen / sizeOfBlock);
	else
		tempLen = sizeOfBlock * ((partInLen - 1) / sizeOfBlock);
	(*cipher->update)(context->cipher_ctx, partOut, partIn, tempLen);
	partOutLen += tempLen;
	partIn += tempLen;
	partInLen -= tempLen;

	memcpy(context->buffer, partIn, context->bufferLen = partInLen);

	return partOutLen;
}

int CipherFinal(CIPHER_CTX *context, unsigned char *partOut)
{
	const CIPHER_ALGORITHM *cipher;
	unsigned int partOutLen;
	unsigned int padLen;
	int sizeOfBlock;

	cipher = context->cipher;
	sizeOfBlock = cipher->sizeOfBlock;
	partOutLen = 0;

	if (context->encrypt == CIPHER_ENCRYPT) {
		padLen = sizeOfBlock - context->bufferLen;
		memset(context->buffer + context->bufferLen, padLen, padLen);
		(*cipher->update)(context->cipher_ctx, partOut, context->buffer, sizeOfBlock);
		partOutLen = sizeOfBlock;
		(*cipher->restart)(context->cipher_ctx);
		context->bufferLen = 0;

	} else {
		unsigned char lastPart[16];

		if (context->bufferLen == 0) {
			partOutLen = 0;
		} else {
			if (context->bufferLen != sizeOfBlock) {
				partOutLen = -1;

			} else {
				(*cipher->update)(context->cipher_ctx, lastPart, context->buffer, sizeOfBlock);
				padLen = lastPart[sizeOfBlock - 1];
				if (padLen == 0 || padLen > sizeOfBlock) {
					partOutLen = -1;
				} else {
					if (memcmp(&lastPart[sizeOfBlock - padLen], PADDING[padLen], padLen) != 0)
						partOutLen = -1;
					else
						memcpy(partOut, lastPart, partOutLen = sizeOfBlock - padLen);
				}
				if (partOutLen == 0) {
					(*cipher->restart)(context->cipher_ctx);
					context->bufferLen = 0;
				}
			}
		}
		memset(lastPart, 0, sizeof(lastPart));
	}

	return partOutLen;
}

#if 0
int SealInit (
	CIPHER_CTX *context,
	const CIPHER_ALGORITHM *cipher,
	void *cipher_ctx,
	int keyLength,
	unsigned char *iv,
	unsigned char encryptedKey,
	unsigned char publicKey,
	R_RANDOM_STRUCT *randomStruct
)
{
	unsigned char key[512];
	int i;
	if (R_GenerateBytes(iv, cipher->sizeOfBlock, randomStruct) < 0)
		return -1;
	if (R_GenerateBytes(key, keyLength, randomStruct) < 0)
		return -1;
	CipherInit(context, cipher, iv, key, keyLength, CIPHER_ENCRYPT);
}
#endif
