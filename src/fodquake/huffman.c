/*
Copyright (C) 2006-2007 Mark Olsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <string.h>

#include "huffmantable.h"
#include "huffmantable_q3.h"
#include "bitswap.h"
#include "common.h"
#include "huffman.h"

static void huffencbyte(struct huffenctable_s *huffenctable, unsigned char inbyte, unsigned char *outbuf, unsigned int *len)
{
	unsigned char *o;
	unsigned char bitsleft;
	unsigned int l;
	unsigned char size; 
	unsigned short code;
	unsigned char bitstocopy;

	l = *len;
	o = outbuf+l/8;
	bitsleft = 8-(l%8);

	if (bitsleft == 8)
		*o = 0;

	size = huffenctable[inbyte].len;
	code = huffenctable[inbyte].code;
	while(size)
	{
		bitstocopy = size<bitsleft?size:bitsleft;
		*o|= bitswaptable[code>>(16-bitsleft)];
		code<<= (bitstocopy);
		size-= bitstocopy;
		o++;
		*o = 0;
		l+= bitstocopy;
		bitsleft = 8;
	}

	*len = l;
}

static unsigned char huffdecbyte(struct huffdectable_s *huffdectable, const unsigned char *inbuf, unsigned int *len)
{
	const unsigned char *o;
	unsigned short index;
	unsigned int l;
	
	l = *len;

	o = inbuf+l/8;
	index = bitswaptable[*o++]<<((l%8)+8);
	index|= bitswaptable[*o++]<<(l%8);
	index|= bitswaptable[*o]>>(8-(l%8));

	index>>= 16-11;

	*len+= huffdectable[index].len;

	return huffdectable[index].value;
}

struct HuffContext *Huff_Init(unsigned int tablecrc)
{
	/* The first value is from FTE with broken CRC generation */
	if (tablecrc == 0x5ed5c4e4 || tablecrc == 0x286f2e8d)
		return (struct HuffContext *)&hufftables_q3;
	else
		return 0;
}

unsigned int Huff_CompressPacket(struct HuffContext *huffcontext, const void *inbuf, unsigned int inbuflen, void *outbuf, unsigned int outbuflen)
{
	struct hufftables *ht = (struct hufftables *)huffcontext;
	const unsigned char *decmsg;
	unsigned char *buffer;
	unsigned int outlen;
	int i;

	if (!(outbuflen > inbuflen))
		return 0;

	decmsg = inbuf;
	buffer = outbuf + 1;

	outlen = 0;
	for(i=0;i<inbuflen&&outlen/8<inbuflen;i++)
		huffencbyte(ht->huffenctable, decmsg[i], buffer, &outlen);

	if (outlen/8 >= inbuflen)
	{
		memcpy(buffer, inbuf, inbuflen);
		buffer[-1] = 0x80;
		return inbuflen + 1;
	}

	/* Wasting 1 byte, but we must be compatible... */
	buffer[-1] = 8-(outlen%8);
	outlen+= 8;
	outlen/= 8;

	return outlen + 1;
}

unsigned int Huff_DecompressPacket(struct HuffContext *huffcontext, const void *inbuf, unsigned int inbuflen, void *outbuf, unsigned int outbuflen)
{
	struct hufftables *ht = (struct hufftables *)huffcontext;
	const unsigned char *encmsg;
	unsigned char *buffer;
	unsigned int outlen;
	int i;

	if (outbuflen < inbuflen)
		return 0;

	encmsg = inbuf;
	buffer = outbuf;

	if (encmsg[0] == 0x80)
	{
		memcpy(outbuf, inbuf + 1, inbuflen - 1);
		return inbuflen - 1;
	}

	encmsg++;
	inbuflen--;
	inbuflen*= 8;
	inbuflen-= encmsg[-1];

	outlen = 0;
	i = 0;
	while(outlen < inbuflen && i < MAX_MSGLEN)
	{
		buffer[i++] = huffdecbyte(ht->huffdectable, encmsg, &outlen);
	}

	return i;
}

