/*
 *  LibXDiff by Davide Libenzi ( File Differential Library )
 *  Copyright (C) 2003  Davide Libenzi
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include "xdiff.h"
#include "xtestutils.h"





static int xdlt_outf(void *priv, mmbuffer_t *mb, int nbuf) {
	int i;

	for (i = 0; i < nbuf; i++)
		if (!fwrite(mb[i].ptr, mb[i].size, 1, (FILE *) priv))
			return -1;

	return 0;
}


int main(int argc, char *argv[]) {
	int do_diff, do_bdiff, do_bpatch;
	mmfile_t mf1, mf2;
	xpparam_t xpp;
	xdemitconf_t xecfg;
	bdiffparam_t bdp;
	xdemitcb_t ecb, rjecb;

	if (argc < 4) {
		fprintf(stderr, "use: %s {d,p} file1 file2\n", argv[0]);
		return 1;
	}

	do_diff = argv[1][0] == 'd' || argv[1][0] == 'D';
	do_bdiff = argv[1][0] == 'b' || argv[1][0] == 'B';
	do_bpatch = argv[1][0] == 'x' || argv[1][0] == 'X';

	xpp.flags = 0;
	xecfg.ctxlen = 3;
	bdp.bsize = 16;

	if (xdlt_load_mmfile(argv[2], &mf1, do_bdiff || do_bpatch) < 0) {

		return 2;
	}
	if (xdlt_load_mmfile(argv[3], &mf2, do_bdiff || do_bpatch) < 0) {

		xdl_free_mmfile(&mf1);
		return 2;
	}

	if (do_diff) {
		ecb.priv = stdout;
		ecb.outf = xdlt_outf;

		if (xdl_diff(&mf1, &mf2, &xpp, &xecfg, &ecb) < 0) {

			xdl_free_mmfile(&mf2);
			xdl_free_mmfile(&mf1);
			return 3;
		}
	} if (do_bdiff) {
		ecb.priv = stdout;
		ecb.outf = xdlt_outf;

		if (xdl_bdiff(&mf1, &mf2, &bdp, &ecb) < 0) {

			xdl_free_mmfile(&mf2);
			xdl_free_mmfile(&mf1);
			return 4;
		}
	} else if (do_bpatch) {
		ecb.priv = stdout;
		ecb.outf = xdlt_outf;

		if (xdl_bpatch(&mf1, &mf2, &ecb) < 0) {

			xdl_free_mmfile(&mf2);
			xdl_free_mmfile(&mf1);
			return 5;
		}

	} else {
		ecb.priv = stdout;
		ecb.outf = xdlt_outf;
		rjecb.priv = stderr;
		rjecb.outf = xdlt_outf;

		if (xdl_patch(&mf1, &mf2, XDL_PATCH_NORMAL, &ecb, &rjecb) < 0) {

			xdl_free_mmfile(&mf2);
			xdl_free_mmfile(&mf1);
			return 6;
		}
	}

	xdl_free_mmfile(&mf2);
	xdl_free_mmfile(&mf1);

	return 0;
}
