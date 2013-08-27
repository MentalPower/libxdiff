/*
 *  LibXDiff by Davide Libenzi ( File Differential Library )
 *  Copyright (C) 2003	Davide Libenzi
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#include "xinclude.h"




static int xdl_copy_range(mmfile_t *mmf, long off, long size, xdemitcb_t *ecb);




static int xdl_copy_range(mmfile_t *mmf, long off, long size, xdemitcb_t *ecb) {
	long cpsize, rsize;
	mmbuffer_t mb;
	char buf[512];

	if (xdl_seek_mmfile(mmf, off) < 0) {

		return -1;
	}
	for (cpsize = 0; cpsize < size;) {
		rsize = XDL_MIN(size - cpsize, sizeof(buf));
		if (xdl_read_mmfile(mmf, buf, rsize) != rsize) {

			return -1;
		}

		mb.ptr = buf;
		mb.size = rsize;

		if (ecb->outf(ecb->priv, &mb, 1) < 0) {

			return -1;
		}

		cpsize += rsize;
	}

	return 0;
}


int xdl_bpatch(mmfile_t *mmf, mmfile_t *mmfp, xdemitcb_t *ecb) {
	long size, off, csize, osize;
	unsigned long fp, ofp;
	char const *blk;
	unsigned char const *data, *top;
	mmbuffer_t mb;

	if ((blk = (char const *) xdl_mmfile_first(mmfp, &size)) == NULL ||
	    size < XDL_BPATCH_HDR_SIZE) {

		return -1;
	}

	ofp = xdl_mmf_adler32(mmf);
	osize = xdl_mmfile_size(mmf);
	XDL_LE32_GET(blk, fp);
	XDL_LE32_GET(blk + 4, csize);
	if (fp != ofp || csize != osize) {

		return -1;
	}

	blk += XDL_BPATCH_HDR_SIZE;
	size -= XDL_BPATCH_HDR_SIZE;

	do {
		for (data = (unsigned char const *) blk, top = data + size;
		     data < top;) {
			if (*data == XDL_BDOP_INS) {
				data++;

				mb.size = (long) *data++;
				mb.ptr = (char *) data;
				data += mb.size;

				if (ecb->outf(ecb->priv, &mb, 1) < 0) {

					return -1;
				}
			} else if (*data == XDL_BDOP_CPY) {
				data++;
				XDL_LE32_GET(data, off);
				data += 4;
				XDL_LE32_GET(data, csize);
				data += 4;

				if (xdl_copy_range(mmf, off, csize, ecb) < 0) {

					return -1;
				}
			} else {

				return -1;
			}
		}
	} while ((blk = (char const *) xdl_mmfile_next(mmfp, &size)) != NULL);

	return 0;
}

