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



#define XDL_INSBATCH_MAX 255
#define XDL_BDIFF_BIGMATCH 8
#define XDL_MIN_BLKSIZE 16



typedef struct s_bdrecord {
	struct s_bdrecord *next;
	unsigned long fp;
	char const *ptr;
} bdrecord_t;

typedef struct s_bdfile {
	char const *data, *top;
	chastore_t cha;
	unsigned int fphbits;
	bdrecord_t **fphash;
} bdfile_t;




static int xdl_prepare_bdfile(mmfile_t *mmf, long fpbsize, bdfile_t *bdf);
static void xdl_free_bdfile(bdfile_t *bdf);




static int xdl_prepare_bdfile(mmfile_t *mmf, long fpbsize, bdfile_t *bdf) {
	unsigned int fphbits;
	long i, fsize, size, hsize;
	char const *blk, *data, *top;
	bdrecord_t *brec;
	bdrecord_t **fphash;

	fsize = xdl_mmfile_size(mmf);
	fphbits = xdl_hashbits((unsigned int) (fsize / fpbsize) + 1);
	hsize = 1 << fphbits;
	if (!(fphash = (bdrecord_t **) malloc(hsize * sizeof(bdrecord_t *)))) {

		return -1;
	}
	for (i = 0; i < hsize; i++)
		fphash[i] = NULL;

	if (xdl_cha_init(&bdf->cha, sizeof(bdrecord_t), hsize / 4 + 1) < 0) {

		free(fphash);
		return -1;
	}

	if (!(blk = (char const *) xdl_mmfile_first(mmf, &size))) {
		bdf->data = bdf->top = NULL;
	} else {
		bdf->data = data = blk;
		bdf->top = top = blk + size;

		if ((data += (size / fpbsize) * fpbsize) == top)
			data -= fpbsize;

		for (; data >= blk; data -= fpbsize) {
			if (!(brec = (bdrecord_t *) xdl_cha_alloc(&bdf->cha))) {

				xdl_cha_free(&bdf->cha);
				free(fphash);
				return -1;
			}

			brec->fp = xdl_adler32(0, (unsigned char const *) data,
					       XDL_MIN(fpbsize, (long) (top - data)));
			brec->ptr = data;

			i = (long) XDL_HASHLONG(brec->fp, fphbits);
			brec->next = fphash[i];
			fphash[i] = brec;
		}
	}

	bdf->fphbits = fphbits;
	bdf->fphash = fphash;

	return 0;
}


static void xdl_free_bdfile(bdfile_t *bdf) {

	free(bdf->fphash);
	xdl_cha_free(&bdf->cha);
}


int xdl_bdiff(mmfile_t *mmf1, mmfile_t *mmf2, bdiffparam_t const *bdp, xdemitcb_t *ecb) {
	long i, rsize, size, bsize, ibufcnt, csize, off, msize, moff;
	unsigned long fp;
	char const *blk, *data, *top, *ptr1, *ptr2;
	bdrecord_t *brec;
	bdfile_t bdf;
	mmbuffer_t mb;
	unsigned char insbuf[XDL_INSBATCH_MAX + 2], cpybuf[32];

	if (!xdl_mmfile_iscompact(mmf1) || !xdl_mmfile_iscompact(mmf2)) {

		return -1;
	}
	if ((bsize = bdp->bsize) < XDL_MIN_BLKSIZE)
		bsize = XDL_MIN_BLKSIZE;
	if (xdl_prepare_bdfile(mmf1, bsize, &bdf) < 0) {

		return -1;
	}

	if ((blk = (char const *) xdl_mmfile_first(mmf2, &size)) != NULL) {
		for (ibufcnt = 0, data = blk, top = data + size; data < top;) {
			rsize = XDL_MIN(bsize, (long) (top - data));
			fp = xdl_adler32(0, (unsigned char const *) data, rsize);

			i = (long) XDL_HASHLONG(fp, bdf.fphbits);
			for (msize = 0, brec = bdf.fphash[i]; brec; brec = brec->next)
				if (brec->fp == fp) {
					csize = XDL_MIN((long) (top - data), (long) (bdf.top - brec->ptr));
					for (ptr1 = brec->ptr, ptr2 = data; csize && *ptr1 == *ptr2;
					     csize--, ptr1++, ptr2++);

					if ((csize = (long) (ptr1 - brec->ptr)) > msize) {
						moff = (long) (brec->ptr - bdf.data);
						if ((msize = csize) > XDL_BDIFF_BIGMATCH * bsize)
							break;
					}
				}

			if (msize < bsize) {
				insbuf[2 + ibufcnt++] = (unsigned char) *data++;
				if (ibufcnt == XDL_INSBATCH_MAX) {
					insbuf[0] = XDL_BDOP_INS;
					insbuf[1] = (unsigned char) ibufcnt;

					mb.ptr = (char *) insbuf;
					mb.size = 2 + ibufcnt;

					if (ecb->outf(ecb->priv, &mb, 1) < 0) {

						xdl_free_bdfile(&bdf);
						return -1;
					}
					ibufcnt = 0;
				}
			} else {
				if (ibufcnt) {
					insbuf[0] = XDL_BDOP_INS;
					insbuf[1] = (unsigned char) ibufcnt;

					mb.ptr = (char *) insbuf;
					mb.size = 2 + ibufcnt;

					if (ecb->outf(ecb->priv, &mb, 1) < 0) {

						xdl_free_bdfile(&bdf);
						return -1;
					}
					ibufcnt = 0;
				}

				fp = xdl_adler32(0, (unsigned char const *) data, msize);

				data += msize;

				cpybuf[0] = XDL_BDOP_CPY;
				XDL_LE32_PUT(cpybuf + 1, moff);
				XDL_LE32_PUT(cpybuf + 5, msize);
				XDL_LE32_PUT(cpybuf + 9, fp);

				mb.ptr = (char *) cpybuf;
				mb.size = 1 + 4 + 4 + 4;

				if (ecb->outf(ecb->priv, &mb, 1) < 0) {

					xdl_free_bdfile(&bdf);
					return -1;
				}
			}
		}
		if (ibufcnt) {
			insbuf[0] = XDL_BDOP_INS;
			insbuf[1] = (unsigned char) ibufcnt;

			mb.ptr = (char *) insbuf;
			mb.size = 2 + ibufcnt;

			if (ecb->outf(ecb->priv, &mb, 1) < 0) {

				xdl_free_bdfile(&bdf);
				return -1;
			}
		}
	}

	xdl_free_bdfile(&bdf);

	return 0;
}


long xdl_bdiff_tgsize(mmfile_t *mmfp) {
	long tgsize = 0, size, off, csize;
	unsigned long fp;
	char const *blk;
	unsigned char const *data, *top;

	if ((blk = (char const *) xdl_mmfile_first(mmfp, &size)) != NULL) {
		do {
			for (data = (unsigned char const *) blk, top = data + size;
			     data < top;) {
				if (*data == XDL_BDOP_INS) {
					data++;
					csize = (long) *data++;
					tgsize += csize;
					data += csize;
				} else if (*data == XDL_BDOP_CPY) {
					data++;
					XDL_LE32_GET(data, off);
					data += 4;
					XDL_LE32_GET(data, csize);
					data += 4;
					XDL_LE32_GET(data, fp);
					data += 4;
					tgsize += csize;
				} else {

					return -1;
				}
			}
		} while ((blk = (char const *) xdl_mmfile_next(mmfp, &size)) != NULL);
	}

	return tgsize;
}

