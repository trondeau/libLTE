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

#include "lte/common/base.h"

const int lte_symbol_sz(int nof_prb) {
	if (nof_prb<=0) {
		return -1;
	}
	if (nof_prb<=6) {
		return 128;
	} else if (nof_prb<=15) {
		return 256;
	} else if (nof_prb<=25) {
		return 512;
	} else if (nof_prb<=50) {
		return 1024;
	} else if (nof_prb<=75) {
		return 1536;
	} else if (nof_prb<=100) {
		return 2048;
	}
	return -1;
}

int lte_voffset(int symbol_id, int cell_id, int nof_ports) {
	if (nof_ports == 1 && symbol_id==0) {
		return (cell_id+3) % 6;
	} else {
		return cell_id % 6;
	}
}

/* Returns the number of available RE per PRB */
int lte_re_x_prb(int ns, int symbol, int nof_ports, int nof_symbols) {
	if (symbol == 0) {
		if (((ns % 2) == 0) || (ns == 1)) {
			return RE_X_RB - 4;
		} else {
			if (nof_ports == 1) {
				return RE_X_RB - 2;
			} else {
				return RE_X_RB - 4;
			}
		}
	} else if (symbol == 1) {
		if (ns == 1) {
			return RE_X_RB - 4;
		} else if (nof_ports == 4) {
			return RE_X_RB - 4;
		} else {
			return RE_X_RB;
		}
	} else if (symbol == nof_symbols - 3) {
		if (nof_ports == 1) {
			return RE_X_RB - 2;
		} else {
			return RE_X_RB - 4;
		}
	} else {
		return RE_X_RB;
	}
}


struct lte_band {
	int band;
	float fd_low_mhz;
	int earfcn_offset;
	int earfcn_max;
	enum band_geographical_area area;
};

struct lte_band lte_bands[NOF_LTE_BANDS] = {
		{1, 2110, 0, 599, ALL},
		{2, 1930, 600, 1199, NAR},
		{3, 1805, 1200, 1949, ALL},
		{4, 2110, 1950, 2399, NAR},
		{5, 869, 2400, 2649, NAR},
		{6, 875, 2650, 2749, APAC},
		{7, 2620, 2750, 3449, EMEA},
		{8, 925, 3450, 3799, ALL},
		{9, 1844.9, 3800, 4149, APAC},
		{10, 2110, 4150, 4749, NAR},
		{11, 1475.9, 4750, 4949, JAPAN},
		{12, 729, 5010, 5179, NAR},
		{13, 746, 5180, 5279, NAR},
		{14, 758, 5280, 5379, NAR},
		{17, 734, 5730, 5849, NAR},
		{18, 860, 5850, 5999, JAPAN},
		{19, 875, 6000, 6149, JAPAN},
		{20, 791, 6150, 6449, EMEA},
		{21, 1495.9, 6450, 6599, JAPAN},
		{22, 3500, 6600, 7399, NA},
		{23, 2180, 7500, 7699, NAR},
		{24, 1525, 7700, 8039, NAR},
		{25, 1930, 8040, 8689, NAR},
		{26, 859, 8690, 9039, NAR},
		{27, 852, 9040, 9209, NAR},
		{28, 758, 9210, 9659, APAC},
		{29, 717, 9660, 9769, NAR},
		{30, 2350, 9770, 9869, NAR},
		{31, 462.5, 9870, 9919, CALA}
};
#define EOF_BAND 9919

int lte_str2mimotype(char *mimo_type_str, mimo_type_t *type) {
	if (!strcmp(mimo_type_str, "single")) {
		*type = SINGLE_ANTENNA;
	} else if (!strcmp(mimo_type_str, "diversity")) {
		*type = TX_DIVERSITY;
	} else if (!strcmp(mimo_type_str, "multiplex")) {
		*type = SPATIAL_MULTIPLEX;
	} else {
		return -1;
	}
	return 0;
}

char *lte_mimotype2str(mimo_type_t type) {
	switch(type) {
	case SINGLE_ANTENNA:
		return "single";
	case TX_DIVERSITY:
		return "diversity";
	case SPATIAL_MULTIPLEX:
		return "multiplex";
	}
	return NULL;
}

float get_fd(struct lte_band *band, int earfcn) {
	return band->fd_low_mhz + 0.1*(earfcn - band->earfcn_offset);
}

float lte_band_fd(int earfcn) {
	int i;
	i=0;
	while(i < NOF_LTE_BANDS && lte_bands[i].earfcn_offset<earfcn) {
		i++;
	}
	if (i == NOF_LTE_BANDS) {
		fprintf(stderr, "Error: EARFCN %d not found\n", earfcn);
		return -1.0;
	}
	return get_fd(&lte_bands[i], earfcn);
}

int lte_band_get_fd_band_all(int band, lte_earfcn_t *earfcn, int max_elems) {
	return lte_band_get_fd_band(band, earfcn, -1, -1, max_elems);
}

int lte_band_get_fd_band(int band, lte_earfcn_t *earfcn, int start_earfcn, int end_earfcn, int max_elems) {
	int i, j;
	int nof_earfcn;
	i=0;
	while(i < NOF_LTE_BANDS && lte_bands[i].band != band) {
		i++;
	}
	if (i == NOF_LTE_BANDS) {
		fprintf(stderr, "Error: Invalid band %d\n", band);
		return -1;
	}
	if (end_earfcn == -1) {
		end_earfcn = lte_bands[i].earfcn_max;
	} else {
		if (end_earfcn > lte_bands[i].earfcn_max) {
			fprintf(stderr, "Error: Invalid end earfcn %d. Max is %d\n", end_earfcn, lte_bands[i].earfcn_max);
			return -1;
		}
	}
	if (start_earfcn == -1) {
		start_earfcn = lte_bands[i].earfcn_offset;
	} else {
		if (start_earfcn < lte_bands[i].earfcn_offset) {
			fprintf(stderr, "Error: Invalid start earfcn %d. Min is %d\n", start_earfcn, lte_bands[i].earfcn_offset);
			return -1;
		}
	}
	nof_earfcn = end_earfcn - start_earfcn;

	if (nof_earfcn > max_elems) {
		nof_earfcn = max_elems;
	}
	for (j=0;j<nof_earfcn;j++) {
		earfcn[j].id = j + start_earfcn;
		earfcn[j].fd = get_fd(&lte_bands[i], earfcn[j].id);
	}
	return j;
}

int lte_band_get_fd_region(enum band_geographical_area region, lte_earfcn_t *earfcn, int max_elems) {
	int i;
	int n;
	int nof_fd = 0;
	for (i=0;i<NOF_LTE_BANDS && max_elems > 0;i++) {
		if (lte_bands[i].area == region) {
			n = lte_band_get_fd_band(i, &earfcn[nof_fd], -1, -1, max_elems);
			if (n != -1) {
				nof_fd += n;
				max_elems -= n;
			} else {
				return -1;
			}
		}
	}
	return nof_fd;
}

