#include <stdio.h>
//#include <malloc.h>
#include <string.h>

#include "engine.h"

uint32_t GetKey( uint32_t *pdwKey )	// key generator
{
	uint32_t tmp1 = 20021 * (*pdwKey & 0xFFFF);
	uint32_t tmp2 = 20021 * (*pdwKey >> 16);
	uint32_t tmp  = 346 * (*pdwKey) + tmp2 + (tmp1 >> 16);

	*pdwKey = (tmp << 16) + (tmp1 & 0xFFFF) + 1;

	return	(tmp & 0x7FFF);
}

uint32_t	GetVariableData(
	const uint8_t*			pbtSrc,							// 入力データ
	uint32_t*				pdwDstOfReadLength				// 読み込んだ長さの格納先
	)
{
	uint32_t				dwData = 0;
	uint32_t				dwSrcPtr = 0;
	uint32_t				dwShift = 0;
	uint8_t				btCurrentSrc;

	do
	{
		btCurrentSrc = pbtSrc[dwSrcPtr++];

		dwData |= (btCurrentSrc & 0x7F) << dwShift;

		dwShift += 7;
	}
	while( btCurrentSrc & 0x80 );

	if( pdwDstOfReadLength != NULL )
	{
		*pdwDstOfReadLength = dwSrcPtr;
	}

	return	dwData;
}

void dsc_prepare_buffers( FILE *fd, uint32_t key, uint32_t *buf1, uint32_t *buf2, uint8_t *buf3 )
{
	uint32_t tmp;
	int bufsize;
	int i, j;
	int nMin = 0;
	int nMax = 1;
	uint32_t dwBufferPtr, dwBufferPtr2, dwBufferPtrPrev2, dwCode, dwIndex;

	//------------------------------------------------------------------
	// fill buffer 1
	memset( buf1, 0, 512*4 );
	bufsize = 0;
	for ( i = 0 ; i < 512 ; i++ )
	{
		tmp = ( fgetc( fd ) - GetKey( &key )) & 0xFF;
		if( tmp != 0 )
		{
			buf1[bufsize++] = (tmp << 16) + i;
		}
	}

	// sort buffer 1 (bubble ^__^)
	for ( i = 0; i < (bufsize - 1); i++ )
		for ( j = i + 1; j < bufsize; j++ )
			if( buf1[i] > buf1[j] )
			{
				int32_t tmp;
				tmp = buf1[i];
				buf1[i] = buf1[j];
				buf1[j] = tmp;
			}

	// fill buffer 2 and 3
	memset( buf2, 0, 1024*4 );
	memset( buf3, 0, 0x3FF0 );

	dwBufferPtr = 0;
	dwBufferPtr2 = 0;
	dwBufferPtrPrev2 = 0x200;
	dwCode = 1;

	for( dwIndex = 0 ; dwBufferPtr < bufsize ; dwIndex++ )
	{
		if( dwIndex & 1 )
		{
			dwBufferPtr2 = 0;
			dwBufferPtrPrev2 = 512;
		}
		else
		{
			dwBufferPtr2 = 512;
			dwBufferPtrPrev2 = 0;
		}

		nMin = 0;

		while( (buf1[dwBufferPtr] >> 16) == dwIndex )
		{
			int32_t *pdwBuffer3 = (int32_t*)(buf3 + (buf2[dwBufferPtrPrev2] << 4) );

			pdwBuffer3[0] = 0;
			pdwBuffer3[1] = buf1[dwBufferPtr] & 0x1FF;

			dwBufferPtr++;
			dwBufferPtrPrev2++;

			nMin++;
		}

		for( i = nMin ; i < nMax ; i++ )
		{
			int32_t *pdwBuffer3 = (int32_t*)(buf3 + (buf2[dwBufferPtrPrev2] << 4) );
			int32_t dwBufferPtr3 = 0;

			pdwBuffer3[dwBufferPtr3] = 1;
			dwBufferPtr3 += 2;

			buf2[dwBufferPtr2] = dwCode;
			pdwBuffer3[dwBufferPtr3] = dwCode;
			dwBufferPtr2++;
			dwBufferPtr3++;
			dwCode++;

			buf2[dwBufferPtr2] = dwCode;
			pdwBuffer3[dwBufferPtr3] = dwCode;
			dwBufferPtr2++;
			dwBufferPtr3++;
			dwCode++;

			dwBufferPtrPrev2++;
		}

		nMax = (nMax - nMin) * 2;
	}
}

void *dsc_decompile( FILE *fd, uint32_t srcsize )
{
    struct
    {
        char magic[0x10];
        uint32_t key;
        uint32_t dstsize;
        uint32_t count;
        uint32_t unkn;
    } header;
	uint32_t	buffer1[512];
	uint32_t	buffer2[1024];
	uint8_t	    buffer3[0x3FF0];
	uint8_t     *dstdata;

	int srcptr, dstptr;
	int i, j, tmp;
	int32_t dwCount, dwIndex, dwSrc;
	uint8_t btWork;

	fread( &header, sizeof(header), 1, fd );
	dstdata = malloc( header.dstsize );

	dsc_prepare_buffers( fd, header.key, buffer1, buffer2, buffer3 );

	dstptr = 0;
	dwCount = 0;
	srcptr = 512;
	for( i = 0 ; (i < header.count) && (srcptr < srcsize) && (dstptr < header.dstsize) ; i++ )
	{
		dwIndex = 0;

		do
		{
			if( dwCount == 0 )
			{
				dwSrc = fgetc( fd ); srcptr++;
				dwCount = 8;
			}

			dwIndex = dwIndex * 4 + ((dwSrc & 0xFF) >> 7);
			dwSrc <<= 1;
			dwCount--;

			dwIndex = *(int32_t*) &buffer3[4 * dwIndex + 8];
		}
		while( *(int32_t*) &buffer3[dwIndex << 4] != 0 );

		dwIndex <<= 4;
		btWork = buffer3[dwIndex + 4];

		if( buffer3[dwIndex + 5] == 1 )
		{
			int32_t dwBitBuffer = (dwSrc & 0xFF) >> (8 - dwCount);
			int32_t dwBitCount = dwCount;
			int32_t dwBack;
			int32_t dwLength;

			if( dwCount < 12 )
			{
				tmp = (19 - dwCount) >> 3;
				dwBitCount = dwCount + 8 * tmp;

				for( j = 0 ; j < tmp ; j++ )
				{
					dwBitBuffer = (dwBitBuffer << 8) + fgetc( fd );
					srcptr++;
				}
			}

			dwCount = dwBitCount - 12;
			dwSrc = (dwBitBuffer << (8 - dwCount));
			dwBack = ((dwBitBuffer >> dwCount) & 0xFFFF) + 2;
			dwLength = btWork + 2;

			if( (dwBack > dstptr) || (dstptr >= header.dstsize) )
				break;

			for( j = 0 ; j < dwLength ; j++ )
				dstdata[dstptr + j] = dstdata[dstptr + j - dwBack];

			dstptr += dwLength;
		}
		else
			dstdata[dstptr++] = btWork;
	}

	return dstdata;
}

image_res_t *cbg_decompile( FILE *fd )
{
    struct
    {
        char magic[0x10];
        uint16_t width;
        uint16_t height;
        uint16_t bpp;
        uint8_t unkn0[10];
        uint32_t huffmansize;
        uint32_t key;
        uint32_t decryptsize;
        uint32_t unkn1;
    } header;
    image_res_t *img;

	uint32_t				dwDstPtr = 0;
	//uint32_t				dwSrcPtr = 0;
	uint32_t				dwKey;
	uint32_t				dwWork;
	uint8_t                 btWork;

	int i, j;
	long lX, lY;

	uint32_t				adwFreq[256];

    fread( &header, sizeof( header ), 1, fd );

    img = malloc( sizeof( image_res_t ) );
	img->width = header.width;
	img->height = header.height;
	img->bpp = header.bpp;
	img->data = malloc( header.width * header.height * (header.bpp / 8) );

	memset( adwFreq, 0, sizeof(adwFreq) );
	dwKey = header.key;

	for( i = 0 ; i < 256 ; i++ )
	{
		uint32_t			dwShift = 0;
		uint8_t				btCurrentSrc;

		do
		{
			//printf( "%d %d\n", dwSrcPtr++, header.decryptsize );
            btCurrentSrc = fgetc( fd ) - (uint8_t) GetKey( &dwKey );

			adwFreq[i] |= (btCurrentSrc & 0x7F) << dwShift;

			dwShift += 7;
		}
		while( btCurrentSrc & 0x80 );
	}

	// 葉ノードの登録

	struct
    {
        int			bValidity;					// 有効性
        uint32_t	dwFreq;						// 頻度
        uint32_t	dwLeft;						// 左のノード
        uint32_t	dwRight;					// 右のノード
    } astNodeInfo[511];
	uint32_t				dwFreqTotal = 0;

	for( i = 0 ; i < 256 ; i++ )
	{
		astNodeInfo[i].bValidity = (adwFreq[i] > 0);
		astNodeInfo[i].dwFreq = adwFreq[i];
		astNodeInfo[i].dwLeft = i;
		astNodeInfo[i].dwRight = i;

		dwFreqTotal += adwFreq[i];
	}

	// 枝ノードの初期化

	for( i = 256 ; i < 511 ; i++ )
	{
		astNodeInfo[i].bValidity = 0;
		astNodeInfo[i].dwFreq = 0;
		astNodeInfo[i].dwLeft = -1;
		astNodeInfo[i].dwRight = -1;
	}

	// 枝ノードの登録

	uint32_t				dwNodes;

	for( dwNodes = 256 ; dwNodes < 511 ; dwNodes++ )
	{
		// 最小の二値を得る

		uint32_t				dwMin;
		uint32_t				dwFreq = 0;
		uint32_t				adwChild[2];

		for( i = 0 ; i < 2 ; i++ )
		{
			dwMin = 0xFFFFFFFF;
			adwChild[i] = (uint32_t) -1;

			for( j = 0 ; j < dwNodes ; j++ )
			{
				if( astNodeInfo[j].bValidity && (astNodeInfo[j].dwFreq < dwMin) )
				{
					dwMin = astNodeInfo[j].dwFreq;
					adwChild[i] = j;
				}
			}

			if( adwChild[i] != (uint32_t) -1 )
			{
				astNodeInfo[adwChild[i]].bValidity = 0;

				dwFreq += astNodeInfo[adwChild[i]].dwFreq;
			}
		}

		// 枝ノードの登録

		astNodeInfo[dwNodes].bValidity = 1;
		astNodeInfo[dwNodes].dwFreq = dwFreq;
		astNodeInfo[dwNodes].dwLeft = adwChild[0];
		astNodeInfo[dwNodes].dwRight = adwChild[1];

		if( dwFreq == dwFreqTotal )
		{
			// 終了

			break;
		}
	}

	// ハフマンの解凍

	uint32_t				dwRoot = dwNodes;
	uint32_t				dwMask = 0x80;

	uint8_t *clmbtDstOfHuffman = malloc( header.huffmansize );

	btWork = fgetc( fd );
	for( i = 0 ; i < header.huffmansize ; i++ )
	{
		uint32_t				dwNode = dwRoot;

		while( dwNode >= 256 )
		{
			if( btWork & dwMask )
			{
				dwNode = astNodeInfo[dwNode].dwRight;
			}
			else
			{
				dwNode = astNodeInfo[dwNode].dwLeft;
			}

			dwMask >>= 1;

			if( dwMask == 0 )
			{
				btWork = fgetc( fd );
				//dwSrcPtr++;
				dwMask = 0x80;
			}
		}

		clmbtDstOfHuffman[i] = (uint8_t) dwNode;
	}

	// RLEの解凍

	uint32_t				dwDstPtrOfHuffman = 0;
	uint8_t				btZeroFlag = 0;

	while( dwDstPtrOfHuffman < header.huffmansize )
	{
		uint32_t dwLength = GetVariableData( &clmbtDstOfHuffman[dwDstPtrOfHuffman], &dwWork );

		dwDstPtrOfHuffman += dwWork;

		if( btZeroFlag )
		{
			memset( &img->data[dwDstPtr], 0, dwLength );

			dwDstPtr += dwLength;
		}
		else
		{
			memcpy( &img->data[dwDstPtr], &clmbtDstOfHuffman[dwDstPtrOfHuffman], dwLength );

			dwDstPtr += dwLength;
			dwDstPtrOfHuffman += dwLength;
		}

		btZeroFlag ^= 1;
	}

	//

	uint16_t				wColors = (header.bpp >> 3);
	long				lLine = header.width * wColors;

	dwDstPtr = 0;

	for( lY = 0 ; lY < header.height ; lY++ )
	{
		for( lX = 0 ; lX < header.width ; lX++ )
		{
			for( i = 0 ; i < wColors ; i++ )
			{
				if( (lY == 0) && (lX == 0) )
				{
					// 左上隅

					btWork = 0;
				}
				else if( lY == 0 )
				{
					// 上隅
					// 左のピクセルを取得

					btWork = img->data[dwDstPtr - wColors];
				}
				else if( lX == 0 )
				{
					// 左隅
					// 上のピクセルを取得

					btWork = img->data[dwDstPtr - lLine];
				}
				else
				{
					// その他
					// 左と上のピクセルの平均を取得

					btWork = (img->data[dwDstPtr - wColors] + img->data[dwDstPtr - lLine]) >> 1;
				}

				img->data[dwDstPtr++] += btWork;
			}
		}
	}

	free( clmbtDstOfHuffman );

	return img;
}

resource_arc_t *open_resource( const char *fname )
{
    resource_arc_t *res = malloc( sizeof(resource_arc_t) );

    res->fd = fopen( fname, "rb" );
    fseek( res->fd, 0xC, SEEK_SET );
    fread( &res->num, 4, 1, res->fd );

    res->res = malloc( 0x20 * res->num );
    fread( res->res, 0x20, res->num, res->fd );

    return res;
}

void close_resource( resource_arc_t *res )
{
    fclose( res->fd );
    free( res );
}

void *get_resource( resource_arc_t *res, const char *name )
{
    unsigned i;
    void *data;
    char head[0x10];
    //FILE *fd;

    for ( i = 0; i < res->num; i ++ )
    {
        if ( strcasecmp( name, res->res[i].name ) == 0 )
            break;
    }
    if ( i == res->num )
        return NULL;

    fseek( res->fd, 0xC + 0x4 + res->num * 0x20 + res->res[i].off, SEEK_SET );
    fread( head, 0x10, 1, res->fd );
    fseek( res->fd, 0xC + 0x4 + res->num * 0x20 + res->res[i].off, SEEK_SET );

    if ( memcmp( head, "DSC FORMAT 1.00\0", 0x10 ) == 0 )
        data = dsc_decompile( res->fd, res->res[i].size );
    else if ( memcmp( head, "CompressedBG___\0", 0x10 ) == 0 )
    {
        /*void *tmp = malloc( res->res[i].size );
        fd = fopen( "tmp", "wb" );
        fread( tmp, res->res[i].size, 1, res->fd );
        fwrite( tmp, res->res[i].size, 1, fd );
        fclose( fd );*/
        data = cbg_decompile( res->fd );
}
    else
    {
        data = malloc( res->res[i].size );
        fread( data, res->res[i].size, 1, res->fd );
    }

    return data;
}
