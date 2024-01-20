#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>

#include "fq_multi_config.h"

#ifndef isblank
#define isblank( __c)				strchr( " \t", __c)
#endif

#define isabout( __c)				strchr( "#;\r\n$", __c)

LOCAL int __strcasecmp( const unsigned char *pLeft, const unsigned char *pRight)
{
	register int numOfs = 0, numDelta;

_gCompar:
	numDelta = (toupper( *(pLeft + numOfs)) - toupper( *(pRight + numOfs)));
	if (numDelta != 0)
	{
		if (numOfs && 
				((*(pLeft + numOfs) == 0x00) &&
				 (*(pLeft + (numOfs - 1)) == '.')))
			goto _gEqual;
	}
	else {
		if (*(pLeft + numOfs) == 0)
		{
_gEqual:
			return 0;
		}
		else {
			++numOfs;
			goto _gCompar;
		}
	}

	return numDelta;
}

int SZ_toDefine( SZ_toDefine_t *pSet, unsigned char *pDefine, unsigned char *pValue, ...)
{
	register int  numOfs;
	SZ_toValue_t *pUser;

	if (pDefine && *pDefine) {
		SZ_toValue_t *pCur;

		pUser = pSet->pUser - 1;	/* SZ_toValue()에서 pSet->pUser를 사용하게
									 * 되므로, 포인터 위치 변화를 주지 않는다.
									 */
		{
			if ((pCur = SZ_toValue( pSet, pDefine)) != NULL)
				pUser->pDefine = pCur->pDefine;
			else {
/*				strcpy( pSet->pGroup, pDefine); */
				pUser->pDefine = pSet->SP.toBuffer + (++pSet->SP.numOfs);
				for ( numOfs = 0; *(pSet->SP.toGroup + numOfs); numOfs++)
					*(pUser->pDefine + numOfs) = *(pSet->SP.toGroup + numOfs);

				*(pUser->pDefine + numOfs) = 0,
					pSet->SP.numOfs += numOfs;
			}
		} 
	    --pSet->pUser;		/* 배열 항목의 추가 */

		pSet->numUser++,		/* 항목 추가 */
			pSet->numWait++;	/* 정렬되지 않은 항목 */
		*(pUser->pValue = pSet->SP.toBuffer + (++pSet->SP.numOfs)) = 0;
	}
	else {
		if (pSet->numUser == 0)
			goto _gDone;
		else {
			pUser = pSet->pUser;
		}
	}

	if (pValue == NULL)
		goto _gDone;
	else {
		va_list         args;
		register unsigned char *pOfs;
		
		va_start( args, pValue);
		if ((numOfs = 
					vsnprintf( (char *)(pOfs = pSet->SP.toBuffer + pSet->SP.numOfs), 
						CONFIG_BUFSIZ - pSet->SP.numOfs, (const char *)pValue, args)) < 0)

			return -1;
		else {
			*(pOfs + numOfs) = 0,
				pSet->SP.numOfs += numOfs;
		}
#ifdef __DEBUG__
		fprintf( __stderr, 
				" [SZ_toDefine]: '%s' => '%s'\n", pUser->pDefine, pOfs);
#endif
		va_end( args);
	}
_gDone:
	return pSet->numUser;
}


LOCAL void __SWAP( SZ_toValue_t *pCommit, int numLeft, int numRight)
{
	SZ_toValue_t toTmp = pCommit[numLeft];


	pCommit[numLeft] = pCommit[numRight],
		pCommit[numRight] = toTmp;
}


LOCAL void __SZ_toCommit( SZ_toValue_t *pCommit, int numLeft, int numRight)
{
	register int numOfs,
	             numLast,
				 numDelta;


	if (numLeft >= numRight)
		return;
	else {
		__SWAP( pCommit, numLeft, (numLeft + numRight) >> 1);
		for ( numOfs = (numLast = numLeft) + 1; 
				numOfs <= numRight; numOfs++)
		{
			SZ_toValue_t *pLeft = &pCommit[numOfs],
			             *pRight = &pCommit[numLeft];


			if ((pLeft->pValue == NULL) || 
					(pRight->pValue && 
					 (((numDelta = 
						strcasecmp( (char *)pLeft->pDefine, (char *)pRight->pDefine)) < 0) ||

					  ((numDelta == 0) && 
					   (pLeft->pValue < pRight->pValue)))))
				__SWAP( pCommit, ++numLast, numOfs);
		}
	}

	__SWAP( pCommit, numLeft, numLast);
	if (numLast > 1)
		__SZ_toCommit( pCommit, numLeft, numLast - 1);
	__SZ_toCommit( pCommit, numLast + 1, numRight);
}


int SZ_toCommit( SZ_toDefine_t *pSet)
{
#ifdef __DEBUG__
	fprintf( __stderr, " [SZ_toCommit]: %d\n", pSet->numUser);
#endif
	__SZ_toCommit( pSet->pUser, 0, pSet->numUser - 1);
	while (pSet->pUser->pValue == NULL)
	{
		pSet->pUser++;
		if ((--pSet->numUser) <= 0)
			break;
	}

#ifdef __DEBUG__
	fprintf( __stderr, "\tCommit\n");
#endif
	pSet->numWait = 0;		/* 모든 항목 정렬 완료 */
	return pSet->numUser;
}


int SZ_toGroup( SZ_toDefine_t *pSet, unsigned char *pGroup, ...)
{
	if (pGroup == NULL)
		pSet->pGroup = pSet->SP.toGroup;
	else {
		va_list args;
		int     numStore;

		
		va_start( args, pGroup);
		if (((numStore = vsprintf( (char *)pSet->SP.toGroup, (const char *)pGroup, args)) > 0) && 
				(*(pSet->SP.toGroup + (numStore - 1)) != '.')) 
			*(pSet->SP.toGroup + numStore++) = '.';

		pSet->pGroup = pSet->SP.toGroup + numStore;
#ifdef __DEBUG__
		*(pSet->pGroup + 0) = 0;
		fprintf( __stderr, " [SZ_toGroup]: => '%s', %d Bytes\n", 
				pSet->SP.toGroup, numStore);
#endif
		return numStore;
	}
#if 0
	pSet->pGroup = pSet->SP.toGroup;
	if ((pGroup != NULL) &&
			(*(pGroup + 0) != 0x00))
	{
_gCopy:
		*pSet->pGroup++ = *pGroup;
		if ((pSet->pGroup - pSet->SP.toGroup) >= MAX_GROUPSIZ)
			return -1;
		else {
			if (*(++pGroup) != 0x00)
				goto _gCopy;
		}
		
		if (*(pGroup - 1) != '.') *pSet->pGroup++ = '.';
	}
#endif
	return 0;
}


SZ_toValue_t *SZ_toValue( SZ_toDefine_t *pSet, const unsigned char *pDefine)
{
	register int  numHigh = pSet->numUser - 1,
	              numLow = pSet->numWait;
	SZ_toValue_t *pUser;


	strcpy( (char *)pSet->pGroup, (char *)pDefine);
#ifdef __DEBUG__
	fprintf( __stderr, " [SZ_toValue]: SEARCH('%s')\n", pSet->SP.toGroup);
#endif
	while (numLow <= numHigh)
	{
		register int  numMid = (numHigh + numLow) >> 1,
		              numDelta;
		


		if ((numDelta = __strcasecmp( pSet->SP.toGroup, 
						(pUser = &pSet->pUser[numMid])->pDefine)) == 0)
		{
			if (numMid > 0)
			{ /* 첫번째 항목의 위치를 찾는다. */
				SZ_toValue_t *pBack = pUser;


#ifdef __DEBUG__
				fprintf( __stderr, " [SZ_toValue]: '%s'\n", pUser->pDefine);
#endif
				while ((__strcasecmp( 
								pSet->SP.toGroup, (--pBack)->pDefine) == 0) &&
						((pUser = pBack) > pSet->pUser))
					;
			}

			return pUser;
		}
		else {
			if (numDelta < 0) 
				numHigh = numMid - 1;
			else {
				numLow = numMid + 1;
			}
		}
	}

	/* 정렬되지 않는 항목을 찾는다. */
	pUser = NULL;
	{
		SZ_toValue_t *pLow = &pSet->pUser[pSet->numWait];



		while ((--pLow) >= pSet->pUser)
			if (__strcasecmp( pSet->SP.toGroup, pLow->pDefine) == 0)
				pUser = pLow;
	}

	return pUser;
}



SZ_toValue_t *SZ_toNext( SZ_toDefine_t *pSet, SZ_toValue_t *pCur)
{
	if (((++pCur) != (SZ_toValue_t *)(pSet->SP.toBuffer + CONFIG_BUFSIZ)) && 
			(__strcasecmp( pSet->SP.toGroup, pCur->pDefine) == 0))
		return pCur;

	return NULL;
}


int SZ_toCompact( SZ_toDefine_t *pSet)
{
	LOCAL struct {
		struct SZ_toArray {
			short numDefine,
			      numValue;
		} *pUser;
		int    numOfs,
		       numUser;
		unsigned char  toBuffer[CONFIG_BUFSIZ];	/* 최적화 버퍼 */
	} S;
	SZ_toValue_t      *pCur = (SZ_toValue_t *)(pSet->SP.toBuffer + CONFIG_BUFSIZ);
	struct SZ_toArray *pLast = NULL;
	register int       numCopy;


	if (pSet->numUser == 0)
		return 0;
	else {
		S.pUser = (struct SZ_toArray *)(S.toBuffer + CONFIG_BUFSIZ),
			S.numOfs = S.numUser = 0;
_gStrip:
		while ((--pCur) >= pSet->pUser)
			if (pCur->pValue != NULL)
				goto _gCopy;
	}

#ifdef __DEBUG__
	fprintf( __stderr, " [SZ_toCompact]: BEGIN ( %d Bytes, %d Defines)\n", 
			S.numOfs, S.numUser);
#endif
	numCopy = 
		pSet->SP.numOfs = S.numOfs;
	do {		/* 재배치된 버퍼에 내용을 복사한다. */
		*(pSet->SP.toBuffer + numCopy) = *(S.toBuffer + numCopy);
	} while ((--numCopy) > 0);

	for ( pSet->pUser = 	/* 항목의 위치 정보에 따라, 값을 설정한다. */
				(SZ_toValue_t *)(pSet->SP.toBuffer + CONFIG_BUFSIZ), 
			numCopy = S.numUser; (--numCopy) >= 0; )
	{	/* 데이터의 위치를 복사한다. */
		pLast = &S.pUser[numCopy];

		(pCur = --pSet->pUser)->pDefine = 
				pSet->SP.toBuffer + pLast->numDefine,
			pCur->pValue = pSet->SP.toBuffer + pLast->numValue;
#ifdef __DEBUG__
		fprintf( __stderr, "   %3d: '%s' => '%s'\n", 
				numCopy, pCur->pDefine, pCur->pValue);
#endif
	}
#ifdef __DEBUG__
	fprintf( __stderr, " [SZ_toCompact]: DONE\n");
#endif
	return (pSet->numUser = S.numUser);
_gCopy:
	--S.pUser,
	  ++S.numUser;
	if (pLast &&	/* 이전 항목명과 일치한다면.. */
			(strcasecmp( (const char*)(pLast->numDefine + S.toBuffer), (const char*)pCur->pDefine) == 0))
		S.pUser->numDefine = pLast->numDefine;
	else {
		S.pUser->numDefine = ++S.numOfs;
		for ( numCopy = 0; *(pCur->pDefine + numCopy); )	/* 항목명 복사 */
			*(S.toBuffer + S.numOfs++) = *(pCur->pDefine + numCopy++);
		*(S.toBuffer + S.numOfs) = 0x00;
	}

	S.pUser->numValue = ++S.numOfs;
	for ( numCopy = 0,			/* 항목 설정값 복사 */
			pLast = S.pUser; *(pCur->pValue + numCopy); )
		*(S.toBuffer + S.numOfs++) = *(pCur->pValue + numCopy++);
	*(S.toBuffer + S.numOfs) = 0x00;
#ifdef __DEBUG__
	fprintf( __stderr, " %4d %4d [%4d]\t'%s' => '%s'\n", 
			S.pUser->numDefine, S.pUser->numValue, S.numOfs,
			pCur->pDefine, pCur->pValue);
#endif
	goto _gStrip;
}



unsigned char *SZ_toExist( SZ_toDefine_t *pSet, const unsigned char *pDefine, unsigned char *pDefault)
{
	SZ_toValue_t *pUser;


	if ((pUser = SZ_toValue( pSet, pDefine)) != NULL)
		do {
			if (pUser->pValue != NULL)
				return pUser->pValue;
		} while ((pUser = SZ_toNext( pSet, pUser)) != NULL);

	return pDefault;
}


int  SZ_toDelete( SZ_toDefine_t *pSet, const unsigned char *pDefine)
{
	SZ_toValue_t *pCur;
	register int   numDelete = 0;



#ifdef __DEBUG__
	fprintf( __stderr, " [SZ_toDelete]: BEGIN ('%s')\n", pDefine);
#endif
	if ((pCur = SZ_toFirst( pSet, pDefine)) != NULL)
		do {
#ifdef __DEBUG__
			fprintf( __stderr, "   %3d: '%s'\n", numDelete, pCur->pDefine);
#endif
			pCur->pValue = NULL,
				numDelete++;
		} while ((pCur = SZ_toNext( pSet, pCur)) != NULL);
#ifdef __DEBUG__
	fprintf( __stderr, " [SZ_toDelete]: %d Deletes\n", numDelete);
#endif

	return numDelete;
}










int SZ_fromBuffer( SZ_toDefine_t *pSet, const char *pDelim, unsigned char *pDefine)
{
	register unsigned char *pValue = pDefine;


_gStrip:
	if (strchr( pDelim, *pValue) != NULL)
		*pValue++ = 0;
	else {
		if (*(++pValue) != 0x00)
			goto _gStrip;
		else {
			pValue = pDefine,
				pDefine = NULL;
		}
	}

	while (isblank(*(pValue + 0)) != 0)/* 내용의 공백을 제거한다. */
		pValue++;

	if (*pValue == 0x00)
		return 0;

	return SZ_toDefine( pSet, pDefine, pValue);
}


int SZ_fromStream( SZ_toDefine_t *pSet, FILE *fpSet)
{
#define MAX_CFGSIZ					256
	LOCAL char     toBuffer[MAX_CFGSIZ];
	LOCAL struct {
#define MAX_STACK					16
		short numStack,
		      numOfs,

			  toBreak[MAX_STACK];
		unsigned char toBuffer[MAX_GROUPSIZ];
	} S;

	register int   numCommit = 0,
	               numCopy;
	register char *pBuffer,
	              *pValue,
				  *pCopy,

				   numBlock;


_gWhile:
	if ((pBuffer = fgets( toBuffer, sizeof(toBuffer), fpSet)) == NULL)
		return SZ_toCommit( pSet);
	else {
		numCommit++;

		/* 앞 부분의 공백을 제거한다. */
		while (isblank( *pBuffer)) pBuffer++;
		if ((*pBuffer == 0) ||
				(isabout( *pBuffer) != 0))
			goto _gWhile;
		else {
			if (*(pValue = pBuffer) == '}')
			{
				if ((--S.numStack) < 0)
					goto _gBreak;
				else {
					S.numOfs = S.toBreak[S.numStack];
				}


				goto _gWhile;
			}
		}
#ifdef __DEBUG__
		fprintf( stderr, "\tBREAK: '%s'\n", pValue);
#endif
		do {
			/* 항목명의 구분을 찾는다. */
			switch (*pValue)
			{
				case ' ': case '=': case '\t': case ':':
				{
					*pValue++ = 0; 
				} goto _gStart;
				/* 항목명이 존재하지 않는다면. */
				case '#' : case ';' :	/* 주석 */
				case '\r': case '\n': case 0x00: goto _gWhile;
				default  : pValue++;
			}
		} while (True);
_gStart:
		while (isblank( *pValue) != 0) pValue++;
#ifdef __DEBUG__
		fprintf( stderr, "\tDEFINE: '%s' -> '%s'\n", pBuffer, pValue);
#endif
	}

	numBlock = 0;
	switch (*pValue)
	{
		case '`' : case '"' : case '\'': numCopy = *pValue;
		{
			for ( pCopy = ++pValue; *pCopy != numCopy; pCopy++)
				if (*pCopy == 0x00)
					goto _gDefine;
			
			*pCopy++ = 0;
			while (isblank( *pCopy) != 0) pCopy++;
			numBlock = (*pCopy == '{');		/* 그룹의 시작 여부 */
		} break;
		case '{':
		{
_gOpen:
			S.toBreak[S.numStack] = S.numOfs;
			if ((++S.numStack) >= MAX_STACK)
				goto _gBreak;
			else {
				while (*pBuffer)
					*(S.toBuffer + S.numOfs++) = *pBuffer++;

				*(S.toBuffer + S.numOfs++) = '.';
			}
		} goto _gWhile;
		case '#' : case ';' :	/* 주석 */
		case '\r': case '\n': case 0x00: goto _gWhile;	/* 설정값 없음 */
		default :
		{
			for ( pCopy = pValue; *pCopy; pCopy++)
				switch (*pCopy)
				{
					case '{' : numBlock = *pCopy;	/* 그룹의 시작 여부 */
					case '#' : case ';' :	/* 설명문 */
					case '\r': case '\n':	/* 마지막 단어 */
					{
						while ((--pCopy) > pValue)
							if (isblank( *pCopy) == 0)
								break;
						*(++pCopy) = 0;
					} goto _gDefine;
				}
		} break;
	}

_gDefine:
	if (*(pBuffer + 0) == 0x00)
		pCopy = NULL;
	else {
		register char *pDefine = pBuffer;


		for ( numCopy = S.numOfs, 
				pCopy = (char *)S.toBuffer; *(pDefine + 0) != 0x00; )
			*(pCopy + numCopy++) = *pDefine++;

		*(pCopy + numCopy) = 0;
	}

	if (SZ_toDefine( pSet, (unsigned char *)pCopy, (unsigned char *)pValue) < 0)
	{
_gBreak:
		return -(numCommit);
	}
	else {
		if (numBlock)	/* 그룹이 시작된다면.. */
			goto _gOpen;
	}

	goto _gWhile;
}

int SZ_fromFile( SZ_toDefine_t *pSet, const char *filename)
{
	FILE *fpSet=NULL;
#define MAX_CFGSIZ					256
	LOCAL char     toBuffer[MAX_CFGSIZ];
	LOCAL struct {
#define MAX_STACK					16
		short numStack,
		      numOfs,

			  toBreak[MAX_STACK];
		unsigned char toBuffer[MAX_GROUPSIZ];
	} S;

	register int   numCommit = 0,
	               numCopy;
	register char *pBuffer,
	              *pValue,
				  *pCopy,

				   numBlock;

	if( (fpSet = fopen( filename, "r")) == NULL) {
#ifdef __DEBUG__
        fprintf(stderr, "[%s][%d]: fopen() error.\n", __FILE__, __LINE__);
#endif
        return(-1);
    }

_gWhile:
	if ((pBuffer = fgets( toBuffer, sizeof(toBuffer), fpSet)) == NULL) {
		fclose(fpSet);
		return(SZ_toCommit(pSet));
	}
	else {
		numCommit++;

		/* 앞 부분의 공백을 제거한다. */
		while (isblank( *pBuffer)) pBuffer++;
		if ((*pBuffer == 0) ||
				(isabout( *pBuffer) != 0))
			goto _gWhile;
		else {
			if (*(pValue = pBuffer) == '}')
			{
				if ((--S.numStack) < 0)
					goto _gBreak;
				else {
					S.numOfs = S.toBreak[S.numStack];
				}


				goto _gWhile;
			}
		}
#ifdef __DEBUG__
		fprintf( stderr, "\tBREAK: '%s'\n", pValue);
#endif
		do {
			/* 항목명의 구분을 찾는다. */
			switch (*pValue)
			{
				case ' ': case '=': case '\t': case ':':
				{
					*pValue++ = 0; 
				} goto _gStart;
				/* 항목명이 존재하지 않는다면. */
				case '#' : case ';' :	/* 주석 */
				case '\r': case '\n': case 0x00: goto _gWhile;
				default  : pValue++;
			}
		} while (True);
_gStart:
		while (isblank( *pValue) != 0) pValue++;
#ifdef __DEBUG__
		fprintf( stderr, "\tDEFINE: '%s' -> '%s'\n", pBuffer, pValue);
#endif
	}

	numBlock = 0;
	switch (*pValue)
	{
		case '`' : case '"' : case '\'': numCopy = *pValue;
		{
			for ( pCopy = ++pValue; *pCopy != numCopy; pCopy++)
				if (*pCopy == 0x00)
					goto _gDefine;
			
			*pCopy++ = 0;
			while (isblank( *pCopy) != 0) pCopy++;
			numBlock = (*pCopy == '{');		/* 그룹의 시작 여부 */
		} break;
		case '{':
		{
_gOpen:
			S.toBreak[S.numStack] = S.numOfs;
			if ((++S.numStack) >= MAX_STACK)
				goto _gBreak;
			else {
				while (*pBuffer)
					*(S.toBuffer + S.numOfs++) = *pBuffer++;

				*(S.toBuffer + S.numOfs++) = '.';
			}
		} goto _gWhile;
		case '#' : case ';' :	/* 주석 */
		case '\r': case '\n': case 0x00: goto _gWhile;	/* 설정값 없음 */
		default :
		{
			for ( pCopy = pValue; *pCopy; pCopy++)
				switch (*pCopy)
				{
					case '{' : numBlock = *pCopy;	/* 그룹의 시작 여부 */
					case '#' : case ';' :	/* 설명문 */
					case '\r': case '\n':	/* 마지막 단어 */
					{
						while ((--pCopy) > pValue)
							if (isblank( *pCopy) == 0)
								break;
						*(++pCopy) = 0;
					} goto _gDefine;
				}
		} break;
	}

_gDefine:
	if (*(pBuffer + 0) == 0x00)
		pCopy = NULL;
	else {
		register char *pDefine = pBuffer;


		for ( numCopy = S.numOfs, 
				pCopy = (char *)S.toBuffer; *(pDefine + 0) != 0x00; )
			*(pCopy + numCopy++) = *pDefine++;

		*(pCopy + numCopy) = 0;
	}

	if (SZ_toDefine( pSet, (unsigned char *)pCopy, (unsigned char *)pValue) < 0)
	{
_gBreak:
		fclose(fpSet);
		return -(numCommit);
	}
	else {
		if (numBlock)	/* 그룹이 시작된다면.. */
			goto _gOpen;
	}

	goto _gWhile;
}



#include <unistd.h>

#define MAX_SIG					6

LOCAL unsigned char __SZ_SIG[MAX_SIG] = { 0x7F, 'S', '.', 'Z', 0x01, 0x00 };


int SZ_toSave( SZ_toDefine_t *pSet, int fpSet)
{
	if (write( fpSet, __SZ_SIG, sizeof(__SZ_SIG)) < 0)
	{
_gExit:
		return -1;
	}
	else {
		short numDelta = pSet->numUser;


		if (write( fpSet, &numDelta, sizeof(numDelta)) < 0)
			goto _gExit;
		else {
			if (numDelta == 0)
				goto _gBreak;
		}
	}

	{
		register int  numUser = 0;
		short        *pBegin,
		             *pSize;
		SZ_toValue_t *pUser = pSet->pUser;



		pSize =
			pBegin = (short *)(pSet->SP.toBuffer + (pSet->SP.numOfs + 1));
		do {
			if (pUser->pValue == NULL)
				goto _gNext;
			else {
				*(pSize + 0) = (pUser->pValue - pUser->pDefine);
				for ( *(pSize + 1) = 0; 
						*(pUser->pValue + *(pSize + 1)); *(pSize + 1) += 1);
				*(pSize + 1) += 1;
			}

#ifdef __DEBUG__
			fprintf( __stderr, " <<< '%s': '%s' (%d, %d)\n", 
					pUser->pDefine, pUser->pValue, *(pSize + 0), *(pSize + 1));
#endif
			if (write( fpSet, pUser->pDefine, *(pSize + 0) + *(pSize + 1)) < 0)
				goto _gExit;
			pSize += 2;
_gNext:
			++pUser;
		} while ((++numUser) < pSet->numUser);
#ifdef __DEBUG__
		fprintf( __stderr, " <<< %d User\n\n", (pSize - pBegin) / 2);
#endif
		if (write( fpSet, pBegin, (pSize - pBegin) * sizeof(short)) < 0)
			goto _gExit;
	}
_gBreak:
	return pSet->numUser;
}


int SZ_toLoad( int fpSet, SZ_toDefine_t *pSet)
{
	if ((read( fpSet, pSet->SP.toBuffer, 
					sizeof(__SZ_SIG) + sizeof(short)) < 0) || 
			(memcmp( pSet->SP.toBuffer, __SZ_SIG, sizeof(__SZ_SIG)) != 0))
	{
_gExit:
		return -1;
	}
	else {
		if ((pSet->numUser = 
					*((short *)(pSet->SP.toBuffer + sizeof(__SZ_SIG)))) == 0)
			return 0;
		else {
#ifdef __DEBUG__
			fprintf( __stderr, " >>> %d User\n", pSet->numUser);
#endif
			pSet->pUser = (SZ_toValue_t *)(pSet->SP.toBuffer + CONFIG_BUFSIZ),
				pSet->pUser -= pSet->numUser;
		}
	}


	if ((pSet->SP.numOfs = read( fpSet, pSet->SP.toBuffer, CONFIG_BUFSIZ)) < 0)
		goto _gExit;
	else {
		SZ_toValue_t   *pUser;
		register unsigned char *pBuffer;
		short          *pSize = (short *)(pSet->SP.toBuffer + pSet->SP.numOfs);


#ifdef __DEBUG__
		fprintf( __stderr, " >>> %d Bytes\n", pSet->SP.numOfs);
#endif
		pSize -= (pSet->numUser * 2), 
			pSet->SP.numOfs = 
				((unsigned char *)pSize) - (pBuffer = pSet->SP.toBuffer);
		pUser = pSet->pUser;
#ifdef __DEBUG__
		{
			unsigned char *pOfs = (unsigned char *)pSize,
			      *pEof = (unsigned char *)(pSize + (pSet->numUser * 2));


			fprintf( __stderr, " >>>");
			while (pOfs != pEof)
				fprintf( __stderr, " %02X", *pOfs++);
			fprintf( __stderr, "\n");
		}
#endif
		do {
			pUser->pDefine = pBuffer,
				pUser->pValue = (pBuffer += *(pSize + 0));
			pBuffer += *(pSize + 1);

#ifdef __DEBUG__
			fprintf( __stderr, " >>> '%s': '%s' (%d, %d)\n", 
					pUser->pDefine, pUser->pValue, *(pSize + 0), *(pSize + 1));
#endif
			pSize += 2,
				pUser++;
		} while ((pUser - pSet->pUser) < pSet->numUser);
	}

	return pSet->numUser;
}


#if 0
int SZ_toSetup( SZ_toDefine_t *pSet, SZ_toSetup_t *pSetup)
{
	SZ_toValue_t *pCur;
	register int   numStep;
	char          *pBreak = pSet->pGroup;


_gWhile:
	if (pSetup->pGroup)
	{
		register char *pCopy = pSetup->pDefine;


		while (*pCopy) *pSet->pGroup++ = *pCopy++;
		if (*(pCopy - 1) != '.') *pSet->pGroup++ = '.';

		if (pSetup->pGroup &&
				(SZ_toSetup( pSet, pSetup->pGroup) < 0))
		{
_gExit:
			pSet->pGroup = pBreak;
			return -1;
		}
	}

	if (pSetup->fromBuffer && 
			((pCur = SZ_toFirst( pSet, pSetup->pDefine)) != NULL))
	{
		numStep = 0;
		do {
			if (pCur->pValue &&		/* 제거된 데이터가 아니면.. */
					(pSetup->fromBuffer( numStep++, 
										 pCur->pValue, pSetup->pValue) < 0))
				goto _gExit;
		} while ((pCur = SZ_toNext( pSet, pCur)) != NULL);
	}

	if ((++pSetup)->pDefine != NULL)
		goto _gWhile;
	else {
		pSet->pGroup = pBreak;
	}

	return 0;
}



#include <stdlib.h>

int SZ_PTR( int numStep, char *pBuffer, void *pValue)
{
	*((char **)pValue) = pBuffer;
	return sizeof(void *);
}


#if 0
int SZ_STRING( int numStep, char *pBuffer, void *pValue)
{
	char *pTo = (char *)pValue;


	while (*pBuffer)
		*pTo++ = *pBuffer++;
	*pTo = 0;
	return pTo - ((char *)pValue);
}
#endif

LOCAL int __SZ_toInt32( char *pBuffer)
{
	if (*(pBuffer + 0) != 'x')
		return atoi( pBuffer);
	else {
		register int numValue = 0;

		
		pBuffer++;
		while (*pBuffer)
			if ((*pBuffer >= 'A') && (*pBuffer <= 'F'))
				numValue = (numValue * 16) + ((*pBuffer++ - 'A') + 10);
			else {
				if ((*pBuffer >= 'a') && (*pBuffer <= 'f'))
					numValue = (numValue * 16) + ((*pBuffer++ - 'a') + 10);
				else {
					if (isdigit( *pBuffer))
						numValue = (numValue * 16) + (*pBuffer++ - '0');
					else {
						break;
					}
				}
			}

		return numValue;
	}
}


int SZ_INT32( int numStep, char *pBuffer, void *pValue)
{
	*((int *)pValue) = __SZ_toInt32( pBuffer);
	return sizeof(int);
}


int SZ_INT16( int numStep, char *pBuffer, void *pValue)
{
	*((short *)pValue) = (short)__SZ_toInt32( pBuffer);
	return sizeof(short);
}


int SZ_INT8( int numStep, char *pBuffer, void *pValue)
{
	*((char *)pValue) = (char)__SZ_toInt32( pBuffer);
	return sizeof(char);
}


int SZ_FLOAT( int numStep, char *pBuffer, void *pValue)
{
	*((double *)pValue) = (double)atof( pBuffer);
	return sizeof(double);
}
#endif

#if 0
#include <fcntl.h>

int main( int argc, char *argv[])
{
	SZ_toDefine_t toDefine;

	SZ_toInit( &toDefine);
	if (SZ_fromStream( &toDefine, stdin) > 0)
	{
		SZ_toCompact( &toDefine);
		{
			SZ_toValue_t *pCur = 
				(SZ_toValue_t *)(toDefine.SP.toBuffer + CONFIG_BUFSIZ);

			
			while ((--pCur) >= toDefine.pUser)
			{
				fprintf( stderr, "'%s',%p => '%s',%p\n",
						pCur->pDefine, pCur->pDefine,
						pCur->pValue, pCur->pValue);
			}
		}

		SZ_toSave( &toDefine, open( argv[1], O_CREAT | O_RDWR, 0644));
	}

	return 0;
}
#endif
