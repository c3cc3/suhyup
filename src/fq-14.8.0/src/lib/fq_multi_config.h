#ifndef __SZ_DEFINE_H
#define __SZ_DEFINE_H

/* Copyright (C) KANG SHIN-SUK (symlink@hanmail.net)
 *     2002/11/27			Version 1.0
 */
#include "fq_typedef.h"
#include <stdarg.h>

#define MAX_GROUPSIZ		80	
#define CONFIG_BUFSIZ		16384

typedef struct SZ_toValue {
	unsigned char *pDefine;
	unsigned char *pValue;					/* ��: NULL�� ��� ������ ó�� */
} SZ_toValue_t;

typedef struct {
	int           numUser;			/* ��ϵ� �׸��� �� */
	int           numWait;			/* ���ĵ��� �ʴ� �׸��� �� */
	SZ_toValue_t *pUser;
	unsigned char        *pGroup;
	struct {
		int   numOfs;				/* ����� ������ �� */

		/* toBuffer: ������ ������ �����Ѵ�.
		 *    +---------- ~~~ -----------+
		 *    |                          |
		 *    +---------- ~~~ -----------+
		 *  numOfs                     numUser
		 *
		 *  toBuffer                                   	���� ���� ����
		 *  (toBuffer + BUFSIZ) - sizeof(SZ_toValue_t) 	���� �迭
		 */
		unsigned char toBuffer[CONFIG_BUFSIZ];
		unsigned char toGroup[MAX_GROUPSIZ];
	} SP;
} SZ_toDefine_t;

#define SZ_toAt( __pSet, __numAt)			&((__pSet)->pUser[__numAt])
#define SZ_maxUser( __pSet)				 	(__pSet)->numUser

#define SZ_toInit( __pSet)							\
	(__pSet)->numUser =								\
		(__pSet)->SP.numOfs = 0,					\
		(__pSet)->pGroup = (__pSet)->SP.toGroup,	\
		(__pSet)->pUser = (SZ_toValue_t *)((__pSet)->SP.toBuffer + CONFIG_BUFSIZ)
		
#define SZ_toExit( __pSet)
#define SZ_toStart( __pSet)					\
	(__pSet)->numUser =						\
		(__pSet)->SP.numOfs = 0,			\
		(__pSet)->pUser = (SZ_toValue_t *)((__pSet)->SP.toBuffer + CONFIG_BUFSIZ)


/* 
 * SZ_toDefine: �����Ѵ�.
 */
EXTRN int    SZ_toDefine( SZ_toDefine_t *, unsigned char *, unsigned char *pValue, ...);

/* SZ_toCommit: ������ �����Ѵ�.
 *   - ��) ����, �˻����� ���� ���� (SZ_toDefine �Լ�) �Ͽ��ٸ�,
 *         �ݵ�� ������ �� �־�� �Ѵ�.
 */
EXTRN int    SZ_toCommit( SZ_toDefine_t *);


/* SZ_toGroup: �⺻ �׷��� �����Ѵ�.
 *   - pGroup�� ��� ó���� ������ ��ġ�� �ȴ�.
 */
EXTRN int    SZ_toGroup( SZ_toDefine_t *pSet, unsigned char *pGroup, ...);

/* SZ_toValue: �׸��� �˻��Ѵ�. 
 *     �̶�, ���� �׸���� ���� �׸��� �ִٸ�, �ش� �׸��� ó�� ��ġ�� 
 *     �̵��� �ϰ� �ȴ�.
 *
 *     ** ��) ����, ���ĵ��� ���� �׸��� �����Ѵٸ� �˻��� ���ĵ� �׸���
 *            �������� ���� �˻��� �� ���� �����Ѵٸ�, �˻� ������ ����
 *            �׸��� ó����ġ�� ��ȯ�ϸ�, ���ĵ��� �ʴ� �׸��� �� �̻�
 *            �˻��� ���� �ʴ´�. �̶�, ���� �������� ���� �߰����� ��
 *            �Ͽ��ٸ�, �� ���Ŀ����� �˻��Ͽ� ���� �׸��� ó�� ��ġ��
 *            ��ȯ�Ѵ�.
 *
 *            �̴�, ���� �׸��� ���ĵ��� �ʴ� �׸� ���� ��� ã�� ���Ҽ�
 *            �ִ� ���ɼ��� �ִ�. (SZ_toCommit() �Ŀ��� �������� ��� ��ȯ)
 *
 *     �׷����� �˻��� �� ���, pDefine�� �������� �׷����� ǥ���ϴ� '.'��
 *     �����Ͽ� �˻��� �ϸ�ȴ�. (EX. "Group.")
 */
EXTRN SZ_toValue_t *SZ_toValue( SZ_toDefine_t *, const unsigned char *pDefine);

#define SZ_toFirst						SZ_toValue

/* SZ_toNext: pCur�� �׸��� ��ġ�ϴ� ���� �׸��� ��´�.
 *   - ��) �˻� �׸��� ���� �˻��� ���� �׸��� ����Ѵ�.
 *         �׷��Ƿ�, SZ_toNext() �Լ��� ȣ���� ����, ���� �˻� �׸���
 *         ��� �����Ͽ��� �Ѵ�. (�˻��� �ϰ� �Ǹ�, ���� �˻� �׸��� ����ȴ�)
 *
 * SZ_toValue_t *pCur;
 *
 * if ((pCur = SZ_toFirst( &toDefine, "Item")) != NULL)
 *    do {
 *       ...
 *    } while ((pCur = SZ_toNext( &toDefine, pCur)) != NULL);
 */
EXTRN SZ_toValue_t *SZ_toNext( SZ_toDefine_t *, SZ_toValue_t *pCur);


/* SZ_toCompact: ���۸� �����Ѵ�.
 *   - ���ŵ� ������ ������ ���ְ�, ���ĵ� ������� ���ۿ� ������ �ٽ�
 *     ����Ѵ�.
 */
EXTRN int           SZ_toCompact( SZ_toDefine_t *pSet);


/* SZ_fromBuffer: ���۷� ����, ���� ������ �о� ���δ�.(�� �׸��� ����)
 *    pDelim	�׸�� ���� �����ϴ� ������ ���
 *
 * SZ_fromStream: ���Ϸ� ����, ���� ������ �о� ���δ�.(���� �׸��� ����)
 *    ó�� ��Ģ:
 *       1) ����				";", "#'
 *       2) �׸� ������			":", "=", " ", "\t"
 *       	�̶�, ó���� �׸� �����ڰ� ���� ��� �� �׸� ���� �߰��Ѵ�.
 *
 *       3) �׷��� ����			"{"
 *       	�ݵ��, �׸� ���� ������ �������� �����Ͽ��� �Ѵ�.
 *
 *       	��) Group=1 {
 *
 *       4) �׷��� ��ħ			"}"
 *       	�ݵ��, �׸�� ��ġ�� ���� �Ͽ��� �Ѵ�.
 *
 *       5) ���� ����
 *       	- ������ ������ �� ����
 *       	  ���� ���۰� ���� "'", "\"", "`"�� �����ϰ� ������ ���� ����
 *
 *       	  ������ ������ �������� ������ ��Ÿ���� ���ڴ� ���õǸ�,
 *       	  ������ �Ǵ� ó���� �����ϴ� ���� ���ڵ��� ���ȴ�.
 *
 *       	  ��) " ���� ����"		=> " ���� ����"
 *       	      '���� ���� '		=> "���� ���� "
 *       	      ` ���� ���� `		=> " ���� ���� "
 *
 *			- �Ϲ����� ������ �� ����
 *			  �̶��� ������ ǥ���ϴ� ���� ������ ���� ������ ���ϰ� �Ǹ�,
 *			  �������� �����ϴ� ����(" ", "\t")���ڴ� ���� �ȴ�.
 * 		
 * 			- �� �׸� ���ӵǴ� ���� ����
 * 			  �׸�� ���� �κп� ������ ������ ":", "="���� ������ ���� ����
 * 			  �����Ѵ�.
 *
 * 			  ��) : " ���� ����"
 * 			  
 * Sample.conf
 * ------------------------------------------------------
 *   Group {
 *     Default "Default Value" {
 *     	  Value=10
 *   	  Test		CFG toConfig	# Test String
 *   	  Sample: test
 *   	  : " Sample �׸� �߰� �˴ϴ�"
 *     }
 *
 *     String Type;
 *   }
 *
 * ------------------------------------------------------
 *   �ش� �׸� ����
 *   	
 *             	  SZ_toGroup( NULL)     SZ_toGroup( "Group")
 *            	---------------------	------------------------
 * SZ_toValue 	"Group.Default.Value" 	 "Default.Value"
 *   	      	"Group.Default.Test" 	 "Default.Test"
 *
 *            	"Group.Type"         	 "Type"
 *            	"Group.Default"     	 "Default"
 *
 */
EXTRN int    SZ_fromBuffer( SZ_toDefine_t *, const char *pDelim, unsigned char *);
EXTRN int    SZ_fromStream( SZ_toDefine_t *, FILE *fpSet);
EXTRN int    SZ_fromFile( SZ_toDefine_t *, const char *filename);


/* pDefine�� �׸��� ���� �������� ��´�. ����, �������� ���ٸ� pDefault��
 * �������� ��ȯ�Ѵ�.
 */
EXTRN unsigned char         *SZ_toExist( SZ_toDefine_t *, const unsigned char *pDefine, unsigned char *pDefault);

#define SZ_toString( __pSet, __pDefine)				\
		SZ_toExist( __pSet, __pDefine, NULL)
#define SZ_toInteger( __pSet, __pDefine)			\
		atoi( (char *)SZ_toExist( __pSet, __pDefine, (unsigned char *)"0"))
#define SZ_toDouble( __pSet, __pDefine)			\
		atof( (char *)SZ_toExist( __pSet, __pDefine, (unsigned char *)"0.0"))
#define SZ_toFloat( __pSet, __pDefine)			\
		atof( (char *)SZ_toExist( __pSet, __pDefine, (unsigned char *)"0.0"))


/* SZ_toDelete: �׸��� �����Ѵ�. 
 *   - �̴� SZ_toValue()�� �ش� �׸��� ã��, pValue = NULL�� �����ϴ� �Ͱ�
 *     ���� ȿ���� ���´�.
 *
 *   ��ȯ: ���ŵ� �׸��� ��
 *
 * int SZ_toDelete( SZ_toDefine_t *pSet, const char *pDefine)
 * {
 *     SZ_toValue_t *pCur;
 *
 *     if ((pCur = SZ_toFirst( pSet, pDefine)) != NULL)
 *        do {
 *           pCur->pValue = NULL;
 *        } while ((pCur = SZ_toNext( pSet, pCur)) != NULL);
 *
 *     return 0;
 * }
 */
EXTRN int            SZ_toDelete( SZ_toDefine_t *, const unsigned char *);

EXTRN int            SZ_toSave( SZ_toDefine_t *pSet, int fpSet);
EXTRN int            SZ_toLoad( int fpSet, SZ_toDefine_t *pSet);


#if 0
typedef struct SZ_toSetup {
	char  *pDefine;							/* �׸�� */

	/* fromBuffer:
	 *    int		����� ���� ����
	 *    char *	����� ��
	 *    void *	������ ���� (pValue)
	 */
	int  (*fromBuffer)( int, char *, void *);
	void  *pValue;							/* �� ���� ���� */
	struct SZ_toSetup *pGroup;				/* �׷� �׸� ����Ʈ */
} SZ_toSetup_t;

/* fromBuffer */
EXTRN int SZ_PTR   ( int, char *, void *);	/* �������� ��´�. */

#if 0
EXTRN int SZ_STRING( int, char *, void *);	/* ���ڿ��� ��´�. */
#endif

EXTRN int SZ_INT8  ( int, char *, void *);	/* char �� ������ ��´�. */
EXTRN int SZ_INT16 ( int, char *, void *);	/* short�� ������ ��´�. */
EXTRN int SZ_INT32 ( int, char *, void *);	/* int�� ������ ��´�. */

EXTRN int SZ_FLOAT ( int, char *, void *);	/* double������ ��´�. */


EXTRN int           SZ_toSetup( SZ_toDefine_t *pSet, SZ_toSetup_t *pSetup);
#endif

#endif
