/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2014 The libLTE Developers. See the
 * COPYRIGHT file at the top-level directory of this distribution.
 *
 * \section LICENSE
 *
 * This file is part of the libLTE library.
 *
 * libLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * A copy of the GNU Lesser General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "lte.h"

int cell_id = -1;
int nof_prb = 6;
int nof_ports = 1;

void usage(char *prog) {
	printf("Usage: %s [cpv]\n", prog);
	printf("\t-c cell id [Default %d]\n", cell_id);
	printf("\t-p nof_ports [Default %d]\n", nof_ports);
	printf("\t-n nof_prb [Default %d]\n", nof_prb);
	printf("\t-v [set verbose to debug, default none]\n");
}

void parse_args(int argc, char **argv) {
	int opt;
	while ((opt = getopt(argc, argv, "cpnv")) != -1) {
		switch(opt) {
		case 'p':
			nof_ports = atoi(argv[optind]);
			break;
		case 'n':
			nof_prb = atoi(argv[optind]);
			break;
		case 'c':
			cell_id = atoi(argv[optind]);
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage(argv[0]);
			exit(-1);
		}
	}
}


int main(int argc, char **argv) {
	pcfich_t pcfich;
	regs_t regs;
	int i, j;
	cf_t *ce[MAX_PORTS_CTRL];
	int nof_re;
	cf_t *slot_symbols[MAX_PORTS_CTRL];
	int cfi, cfi_rx, nsf, distance;
	int cid, max_cid;

	parse_args(argc,argv);

	nof_re = CPNORM_NSYMB * nof_prb * RE_X_RB;

	/* init memory */
	for (i=0;i<MAX_PORTS_CTRL;i++) {
		ce[i] = malloc(sizeof(cf_t) * nof_re);
		if (!ce[i]) {
			perror("malloc");
			exit(-1);
		}
		for (j=0;j<nof_re;j++) {
			ce[i][j] = 1;
		}
		slot_symbols[i] = malloc(sizeof(cf_t) * nof_re);
		if (!slot_symbols[i]) {
			perror("malloc");
			exit(-1);
		}
	}

	if (cell_id == -1) {
		cid = 0;
		max_cid = 503;
	} else {
		cid = cell_id;
		max_cid = cell_id;
	}
	while(cid <= max_cid) {

		printf("Testing CellID=%d...\n", cid);

		if (regs_init(&regs, cid, nof_prb, nof_ports, R_1, PHICH_NORM, CPNORM)) {
			fprintf(stderr, "Error initiating regs\n");
			exit(-1);
		}

		if (pcfich_init(&pcfich, &regs, cid, nof_prb, nof_ports, CPNORM)) {
			fprintf(stderr, "Error creating PBCH object\n");
			exit(-1);
		}

		for (nsf=0;nsf<10;nsf++) {
			for (cfi=1;cfi<4;cfi++) {
				pcfich_encode(&pcfich, cfi, slot_symbols, nsf);

				/* combine outputs */
				for (i=1;i<nof_ports;i++) {
					for (j=0;j<nof_re;j++) {
						slot_symbols[0][j] += slot_symbols[i][j];
					}
				}
				if (pcfich_decode(&pcfich, slot_symbols[0], ce, nsf, &cfi_rx, &distance)<0) {
					exit(-1);
				}
				INFO("cfi_tx: %d, cfi_rx: %d, ns: %d, distance: %d\n",
						cfi, cfi_rx, nsf, distance);

				if (distance) {
					printf("Error\n");
					exit(-1);
				}
			}
		}
		pcfich_free(&pcfich);
		regs_free(&regs);
		cid++;
	}

	for (i=0;i<MAX_PORTS_CTRL;i++) {
		free(ce[i]);
		free(slot_symbols[i]);
	}
	printf("OK\n");
	exit(0);
}
