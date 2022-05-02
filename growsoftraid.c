/*	$OpenBSD$	*/
/*
 * Copyright (c) 2022 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2007, 2008, 2009 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2008 Chris Kuethe <ckuethe@openbsd.org>
 * Copyright (c) 2009 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2000 Christoph Herrmann, Thomas-Henning von Kamptz
 * Copyright (c) 1980, 1989, 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christoph Herrmann and Thomas-Henning von Kamptz, Munich and Frankfurt.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors, as well as Christoph
 *      Herrmann and Thomas-Henning von Kamptz.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $TSHeader: src/sbin/growfs/growfs.c,v 1.5 2000/12/12 19:31:00 tomsoft Exp $
 * $FreeBSD: src/sbin/growfs/growfs.c,v 1.25 2006/07/17 20:48:36 stefanf Exp $
 *
 */

#include <sys/types.h>
#include <sys/param.h>	/* DEV_BSIZE */
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#define _STANDALONE
#include <dev/softraidvar.h>
#undef _STANDALONE

static void
sr_checksum_print(const u_int8_t *md5)
{
	int i;

	for (i = 0; i < MD5_DIGEST_LENGTH; i++)
		printf("%02x", md5[i]);
}

static void
sr_uuid_print(const struct sr_uuid *uuid, int cr)
{
	printf("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
	    "%02x%02x%02x%02x%02x%02x%s",
	    uuid->sui_id[0], uuid->sui_id[1],
	    uuid->sui_id[2], uuid->sui_id[3],
	    uuid->sui_id[4], uuid->sui_id[5],
	    uuid->sui_id[6], uuid->sui_id[7],
	    uuid->sui_id[8], uuid->sui_id[9],
	    uuid->sui_id[10], uuid->sui_id[11],
	    uuid->sui_id[12], uuid->sui_id[13],
	    uuid->sui_id[14], uuid->sui_id[15],
	    cr ? "\n" : "");
}

static char *
sr_type_print(uint32_t type)
{
	static char buf[16];

	switch (type) {
	case SR_OPT_INVALID:
		return "INVALID";
	case SR_OPT_CRYPTO:
		return "CRYPTO";
	case SR_OPT_BOOT:
		return "BOOT";
	case SR_OPT_KEYDISK:
		return "KEYDISK";
	default:
		snprintf(buf, sizeof(buf), "%d", type);
		return buf;
	}
}

static void
sr_meta_print(const struct sr_metadata *m)
{
	const struct sr_meta_chunk *mc;
	const struct sr_meta_opt_hdr *omh;
	const struct sr_meta_crypto *smc;
	uint32_t i;

	printf("Softraid Metadata:\n");
	printf("\tssd_magic 0x%llx\n", m->ssdi.ssd_magic);
	printf("\tssd_version %d\n", m->ssdi.ssd_version);
	printf("\tssd_vol_flags 0x%x\n", m->ssdi.ssd_vol_flags);
	printf("\tssd_uuid ");
	sr_uuid_print(&m->ssdi.ssd_uuid, 1);
	printf("\tssd_chunk_no %d\n", m->ssdi.ssd_chunk_no);
	printf("\tssd_chunk_id %d\n", m->ssdi.ssd_chunk_id);
	printf("\tssd_opt_no %d\n", m->ssdi.ssd_opt_no);
	printf("\tssd_secsize %d\n", m->ssdi.ssd_secsize);
	printf("\tssd_volid %d\n", m->ssdi.ssd_volid);
	printf("\tssd_level %d\n", m->ssdi.ssd_level);
	printf("\tssd_size %lld\n", m->ssdi.ssd_size);
	printf("\tssd_vendor %s\n", m->ssdi.ssd_vendor);
	printf("\tssd_product %s\n", m->ssdi.ssd_product);
	printf("\tssd_revision %s\n", m->ssdi.ssd_revision);
	printf("\tssd_strip_size %d\n", m->ssdi.ssd_strip_size);
	printf("\tssd_checksum ");
	sr_checksum_print(m->ssd_checksum);
	printf("\n");
	printf("\tssd_devname %s\n", m->ssd_devname);
	printf("\tssd_meta_flags 0x%x\n", m->ssd_meta_flags);
	printf("\tssd_data_blkno %u\n", m->ssd_data_blkno);
	printf("\tssd_ondisk %llu\n", m->ssd_ondisk);
	printf("\tssd_rebuild %llu\n", m->ssd_rebuild);

	mc = (const struct sr_meta_chunk *)(m + 1);
	for (i = 0; i < m->ssdi.ssd_chunk_no; i++, mc++) {
		printf("\t\tscm_volid %d\n", mc->scmi.scm_volid);
		printf("\t\tscm_chunk_id %d\n", mc->scmi.scm_chunk_id);
		printf("\t\tscm_devname %s\n", mc->scmi.scm_devname);
		printf("\t\tscm_size %lld\n", mc->scmi.scm_size);
		printf("\t\tscm_coerced_size %lld\n",mc->scmi.scm_coerced_size);
		printf("\t\tscm_uuid ");
		sr_uuid_print(&mc->scmi.scm_uuid, 1);
		printf("\t\tscm_checksum ");
		sr_checksum_print(mc->scm_checksum);
		printf("\n");
		printf("\t\tscm_status %d\n", mc->scm_status);
	}

	omh = (const struct sr_meta_opt_hdr *)mc;
	for (i = 0; i < m->ssdi.ssd_opt_no; i++) {
		printf("\t\t\tsom_type %s\n", sr_type_print(omh->som_type));
		printf("\t\t\tsom_length %d\n", omh->som_length);
		printf("\t\t\tsom_checksum ");
		sr_checksum_print(omh->som_checksum);
		printf("\n");
		switch (omh->som_type) {
		case SR_OPT_CRYPTO:
			smc = (const struct sr_meta_crypto *)omh;
			printf("\t\t\tscm_alg %d\n", smc->scm_alg);
			printf("\t\t\tscm_ckeck_alg %d\n", smc->scm_check_alg);
			printf("\t\t\tscm_flags %x\n", smc->scm_flags);
			printf("\t\t\tscm_mask_alg %x\n", smc->scm_mask_alg);
		}
		omh = (const struct sr_meta_opt_hdr *)((const uint8_t *)omh +
		    omh->som_length);
	}
}

static void
sr_checksum(const void *src, void *md5, u_int32_t len)
{
        MD5_CTX                 ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, src, len);
	MD5Final(md5, &ctx);
}

static int
sr_meta_check(struct sr_metadata *m)
{
	struct sr_meta_chunk *mc;
	struct sr_meta_opt_hdr *omh;
	uint32_t i;
	uint8_t sum[MD5_DIGEST_LENGTH];

	sr_checksum(m, &sum, sizeof(struct sr_meta_invariant));
	if (memcmp(&sum, &m->ssd_checksum, sizeof(sum)) != 0) {
		warnx("bad sr_metadata checksum");
		return 0;
	}

	if (m->ssdi.ssd_version != 6) {
		warnx("only softraid version 6 supported, not %d",
		    m->ssdi.ssd_version);
		return 0;
	}
	if (m->ssdi.ssd_level != 'C') {
		warnx("only softraid level C supported");
		return 0;
	}
	if (m->ssdi.ssd_chunk_no != 1 || m->ssdi.ssd_chunk_id != 0) {
		warnx("bad number of chunks");
		return 0;
	}

	mc = (struct sr_meta_chunk *)(m + 1);
	for (i = 0; i < m->ssdi.ssd_chunk_no; i++, mc++) {
#if 0
		/*
		 * This is currently broken in softraid(4) so the
		 * checksum is never right. The problem is that the
		 * sum is calculated only over the first MD5_DIGEST_LENGTH
		 * bytes. It also happens in the wrong spot so double bad.
		 */
		sr_checksum(mc, &sum, sizeof(struct sr_meta_chunk_invariant));
		if (memcmp(&sum, &mc->scm_checksum, sizeof(sum)) != 0) {
			warnx("bad sr_meta_chunk checksum in chunk %d", i);
			return 0;
		}
#endif
	}
	omh = (struct sr_meta_opt_hdr *)mc;
	for (i = 0; i < m->ssdi.ssd_opt_no; i++) {
		memcpy(&sum, &omh->som_checksum, MD5_DIGEST_LENGTH);
		memset(&omh->som_checksum, 0, MD5_DIGEST_LENGTH);
		sr_checksum(omh, &omh->som_checksum, omh->som_length);
		if (memcmp(&sum, &omh->som_checksum, sizeof(sum)) != 0) {
			warnx("bad sr_meta_opt checksum in optioin %d", i);
			return 0;
		}
	}

	return 1;
}

static void
sr_meta_resize(struct sr_metadata *m, long long size)
{
	struct sr_meta_chunk *mc;
	uint32_t i;

	/* change disk size */
	m->ssdi.ssd_size = size;
	mc = (struct sr_meta_chunk *)(m + 1);
	for (i = 0; i < m->ssdi.ssd_chunk_no; i++, mc++) {
		mc->scmi.scm_size = size;
		mc->scmi.scm_coerced_size = size;
	}

	/* rebuild checksum */
	sr_checksum(m, &m->ssd_checksum, sizeof(struct sr_meta_invariant));

	mc = (struct sr_meta_chunk *)(m + 1);
	for (i = 0; i < m->ssdi.ssd_chunk_no; i++, mc++) {
		sr_checksum(mc, &mc->scm_checksum,
		    sizeof(struct sr_meta_chunk_invariant));
	}
}

/*
 * Read the disklabel from disk.
 */
static struct disklabel *
get_disklabel(int fd)
{
	static struct disklabel lab;

	if (ioctl(fd, DIOCGDINFO, (char *)&lab) != 0)
		err(1, "DIOCGDINFO");
	return &lab;
}

static void
usage(void)
{
	fprintf(stderr, "usage: swresize [-Nqv] [-s size] special\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	unsigned char buf[SR_META_SIZE * DEV_BSIZE];
	off_t skip = SR_META_OFFSET * DEV_BSIZE;	
	struct stat st;
	struct disklabel *lp;
	struct partition *pp;
	struct sr_metadata *m;
	long long size = 0;
	char *device;
	char reply[5];
	const char *errstr;
	int ch, fd, fdo, Nflag = 0, quiet = 0, verbose = 0;

	while ((ch = getopt(argc, argv, "Nqs:v")) != -1) {
		switch (ch) {
		case 'N':
			Nflag = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 's':
			size = strtonum(optarg, 1, LLONG_MAX, &errstr);
			if (errstr)
				usage();
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	fd = opendev(*argv, O_RDONLY, 0, &device);
	if (fd == -1)
		err(1, "%s", *argv);

	if (Nflag) {
		fdo = -1;
	} else {
		fdo = open(device, O_WRONLY);
		if (fdo == -1)
			err(1, "%s", device);
	}

	if (fstat(fd, &st) == -1)
		err(1, "%s: fstat()", device);

	/*
	 * Try to read a label from the disk. Then get the partition from the
	 * device minor number, using DISKPART(). Probably don't need to
	 * check against getmaxpartitions().
	 */
	lp = get_disklabel(fd);
	if ((int)DISKPART(st.st_rdev) < getmaxpartitions())
		pp = &lp->d_partitions[DISKPART(st.st_rdev)];
	else
		errx(1, "%s: invalid partition number %u",
		    device, DISKPART(st.st_rdev));
	/*
	 * Check if that partition is suitable for softraid
	 */
	if (DL_GETPSIZE(pp) < 1)
		errx(1, "partition is unavailable");
	if (pp->p_fstype != FS_RAID)
		errx(1, "can only alter softraid partitions");

	if (lseek(fd, skip, SEEK_SET) == -1)
		err(1, "lseek %llu", skip);
	if (read(fd, buf, sizeof(buf)) != sizeof(buf))
		err(1, "read");

	m = (void *)buf;
	if (verbose)
		sr_meta_print(m);

	if (!sr_meta_check(m))
		errx(1, "metadata check failed");

	if (size == 0)
		size = DL_SECTOBLK(lp, DL_GETPSIZE(pp)) - m->ssd_data_blkno;

	if (!quiet)
		printf("new softraid size is: %lld blocks from %lld blocks\n",
		    size, m->ssdi.ssd_size);

	if (Nflag)
		exit(0);

	printf("We strongly recommend you to make a backup "
	"before altering softraid settings\n\n"
	" Did you backup your data (Yes/No) ? ");
	if (fgets(reply, (int)sizeof(reply), stdin) == NULL ||
		strncasecmp(reply, "Yes", 3)) {
		printf("\n Nothing done \n");
		exit (0);
	}

	sr_meta_resize(m, size);

	if (!sr_meta_check(m))
		errx(1, "new metadata check failed");

	if (verbose) {
		printf("New ");
		sr_meta_print(m);
	}

	if (fdo != -1) {
		if (lseek(fdo, skip, SEEK_SET) == -1)
			err(1, "lseek %llu", skip);
		if (write(fdo, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write");
	}
	exit(0);
}
