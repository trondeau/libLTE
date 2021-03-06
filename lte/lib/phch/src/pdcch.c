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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "lte/phch/dci.h"
#include "lte/phch/regs.h"
#include "lte/phch/pdcch.h"
#include "lte/common/base.h"
#include "lte/utils/bit.h"
#include "lte/utils/vector.h"
#include "lte/utils/debug.h"

#define PDCCH_NOF_FORMATS 			4
#define PDCCH_FORMAT_NOF_CCE(i)		(1<<i)
#define PDCCH_FORMAT_NOF_REGS(i) 	((1<<i)*9)
#define PDCCH_FORMAT_NOF_BITS(i) 	((1<<i)*72)

int pdcch_put(cf_t *pdcch, cf_t *slot1_data, int nsymbols) {
	memcpy(slot1_data, pdcch, sizeof(cf_t) * nsymbols);
	return nsymbols;
}

int pdcch_get(cf_t *slot1_data, cf_t *pdcch, int nsymbols) {
	memcpy(pdcch, slot1_data, sizeof(cf_t) * nsymbols);
	return nsymbols;
}

#define MIN(a,b) ((a>b)?b:a)

/**
 * 36.213 9.1
 */
int gen_common_search(dci_candidate_t *c, int nof_cce, int nof_bits, unsigned short rnti) {
	int i, L, k;
	k = 0;
	for (L = 2; L > 0; L--) {
		for (i = 0; i < MIN(nof_cce,16) / (4 * L); i++) {
			c[k].L = 4 * L;
			c[k].nof_bits = nof_bits;
			c[k].rnti = rnti;
			c[k].ncce = (4 * L) * (i % (nof_cce / (4 * L)));
			k++;
			INFO(
					"Common SS Candidate %d: RNTI: 0x%x, nCCE: %d, Nbits: %d, L: %d\n",
					k, c[k].rnti, c[k].ncce, c[k].nof_bits, c[k].L);
		}
	}
	return k;
}

/**
 * 36.213 9.1
 */
int gen_ue_search(dci_candidate_t *c, int nof_cce, int nof_bits, unsigned short rnti, int subframe) {
	int i, l, L, k, m;
	unsigned int Yk;
	const int S[4] = { 6, 12, 8, 16 };
	k = 0;
	for (l = 3; l >= 0; l--) {
		L = (1 << l);
		for (i = 0; i < MIN(nof_cce/L,16/S[l]); i++) {
			c[k].L = l;
			c[k].nof_bits = nof_bits;
			c[k].rnti = rnti;
			Yk = rnti;
			for (m = 0; m < subframe; m++) {
				Yk = (39827 * Yk) % 65537;
			}
			c[k].ncce = L * ((Yk + i) % (nof_cce / L));
			INFO("UE-specific SS Candidate %d: SF: %d, RNTI: 0x%x, nCCE: %d, Nbits: %d, L: %d\n",
					k, subframe, c[k].rnti, c[k].ncce, c[k].nof_bits, c[k].L);
			k++;
		}
	}
	return k;
}
void pdcch_init_common(pdcch_t *q, pdcch_search_t *s, unsigned short rnti) {
	int k;
	s->nof_candidates = 2*(MIN(q->nof_cce,16) / 4 + MIN(q->nof_cce,16) / 8);
	if (s->nof_candidates) {
		s->candidates[0] = malloc(sizeof(dci_candidate_t) * s->nof_candidates);
		dci_candidate_t *c = s->candidates[0];

		if (c) {
			// Format 1A and 1C L=4 and L=8, 4 and 2 candidates, only if nof_cce > 16
			k = 0;
			k += gen_common_search(&c[k], q->nof_cce,
					dci_format1A_sizeof(q->nof_prb, true), SIRNTI);
			k += gen_common_search(&c[k], q->nof_cce,
					dci_format1C_sizeof(q->nof_prb), SIRNTI);
		}
	}
}

/** 36.213 v9.3 Table 7.1-1: System Information DCI messages
 * Expect DCI formats 1C and 1A in the common search space
 */
void pdcch_init_search_si(pdcch_t *q) {
	pdcch_init_common(q, &q->search_mode[SEARCH_SI], SIRNTI);
	q->current_search_mode = SEARCH_SI;
}

/** 36.213 v9.3 Table 7.1-5
 * user-specific search space. Currently supported transmission Mode 1:
 * DCI Format 1A and 1 + PUSCH scheduling format 0
 */
void pdcch_init_search_ue(pdcch_t *q, unsigned short c_rnti) {
	int l, n, k;
	pdcch_search_t *s = &q->search_mode[SEARCH_UE];
	s->nof_candidates = 0;
	for (l=0;l<3;l++) {
		s->nof_candidates += 3*(MIN(q->nof_cce,16) / (1<<l));
	}
	INFO("Initiating %d candidate(s) in the UE-specific search space for C-RNTI: 0x%x\n", s->nof_candidates, c_rnti);
	if (s->nof_candidates) {
		for (n=0;n<NSUBFRAMES_X_FRAME;n++) {
			s->candidates[n] = malloc(sizeof(dci_candidate_t) * s->nof_candidates);
			dci_candidate_t *c = s->candidates[n];

			if (c) {
				// Expect Formats 1, 1A, 0
				k = 0;
				k += gen_ue_search(&c[k], q->nof_cce,
						dci_format0_sizeof(q->nof_prb), c_rnti, n);
				k += gen_ue_search(&c[k], q->nof_cce,
						dci_format1_sizeof(q->nof_prb, 1), c_rnti, n);
				k += gen_ue_search(&c[k], q->nof_cce,
						dci_format1A_sizeof(q->nof_prb, true), c_rnti, n);
			}
		}
	}
	q->current_search_mode = SEARCH_UE;
}

/** 36.213 v9.3 Table 7.1-3
 * Expect DCI formats 1C and 1A in the common search space
 */
void pdcch_init_search_ra(pdcch_t *q, unsigned short ra_rnti) {
	pdcch_init_common(q, &q->search_mode[SEARCH_RA], ra_rnti);
	q->current_search_mode = SEARCH_RA;
}

void pdcch_set_search_si(pdcch_t *q) {
	q->current_search_mode = SEARCH_SI;
}
void pdcch_set_search_ue(pdcch_t *q) {
	q->current_search_mode = SEARCH_UE;
}
void pdcch_set_search_ra(pdcch_t *q) {
	q->current_search_mode = SEARCH_RA;
}

/** Initializes the PDCCH transmitter and receiver */
int pdcch_init(pdcch_t *q, regs_t *regs, int nof_prb, int nof_ports,
		int cell_id, lte_cp_t cp) {
	int ret = -1;
	int i;

	if (cell_id < 0) {
		return -1;
	}
	if (nof_ports > MAX_PORTS_CTRL) {
		fprintf(stderr, "Invalid number of ports %d\n", nof_ports);
		return -1;
	}
	bzero(q, sizeof(pdcch_t));
	q->cell_id = cell_id;
	q->cp = cp;
	q->regs = regs;
	q->nof_ports = nof_ports;
	q->current_search_mode = SEARCH_NONE;

	q->nof_regs = regs_pdcch_nregs(q->regs);
	q->nof_cce = q->nof_regs / 9;
	q->nof_symbols = 4 * q->nof_regs;
	q->nof_bits = 2 * q->nof_symbols;

	INFO("Init PDCCH: %d REGs, %d bits, %d symbols, %d ports\n", q->nof_regs,
			q->nof_bits, q->nof_symbols, q->nof_ports);

	if (modem_table_std(&q->mod, LTE_QPSK, true)) {
		goto clean;
	}

	demod_soft_init(&q->demod);
	demod_soft_table_set(&q->demod, &q->mod);
	demod_soft_alg_set(&q->demod, APPROX);

	for (i = 0; i < NSUBFRAMES_X_FRAME; i++) {
		if (sequence_pdcch(&q->seq_pdcch[i], 2 * i, q->cell_id, q->nof_bits)) {
			goto clean;
		}
	}

	int poly[3] = { 0x6D, 0x4F, 0x57 };
	if (viterbi_init(&q->decoder, viterbi_37, poly, DCI_MAX_BITS + 16, true)) {
		goto clean;
	}

	q->pdcch_e = malloc(sizeof(char) * q->nof_bits);
	if (!q->pdcch_e) {
		goto clean;
	}

	q->pdcch_llr = malloc(sizeof(float) * q->nof_bits);
	if (!q->pdcch_llr) {
		goto clean;
	}

	q->pdcch_d = malloc(sizeof(cf_t) * q->nof_symbols);
	if (!q->pdcch_d) {
		goto clean;
	}

	for (i = 0; i < MAX_PORTS_CTRL; i++) {
		q->ce[i] = malloc(sizeof(cf_t) * q->nof_symbols);
		if (!q->ce[i]) {
			goto clean;
		}
		q->pdcch_x[i] = malloc(sizeof(cf_t) * q->nof_symbols);
		if (!q->pdcch_x[i]) {
			goto clean;
		}
		q->pdcch_symbols[i] = malloc(sizeof(cf_t) * q->nof_symbols);
		if (!q->pdcch_symbols[i]) {
			goto clean;
		}
	}
	ret = 0;
	clean: if (ret == -1) {
		pdcch_free(q);
	}
	return ret;
}

void pdcch_free(pdcch_t *q) {
	int i, j;

	for (i=0;i<PDCCH_NOF_SEARCH_MODES;i++) {
		for (j = 0; j < NSUBFRAMES_X_FRAME; j++) {
			if (q->search_mode[i].candidates[j]) {
				free(q->search_mode[i].candidates[j]);
			}
		}
	}
	if (q->pdcch_e) {
		free(q->pdcch_e);
	}
	if (q->pdcch_llr) {
		free(q->pdcch_llr);
	}
	if (q->pdcch_d) {
		free(q->pdcch_d);
	}
	for (i = 0; i < MAX_PORTS_CTRL; i++) {
		if (q->ce[i]) {
			free(q->ce[i]);
		}
		if (q->pdcch_x[i]) {
			free(q->pdcch_x[i]);
		}
		if (q->pdcch_symbols[i]) {
			free(q->pdcch_symbols[i]);
		}
	}

	for (i = 0; i < NSUBFRAMES_X_FRAME; i++) {
		sequence_free(&q->seq_pdcch[i]);
	}

	modem_table_free(&q->mod);
	viterbi_free(&q->decoder);
}

/** 36.212 5.3.3.2 to 5.3.3.4
 *
 * Returns XOR between parity and remainder bits
 *
 * TODO: UE transmit antenna selection CRC mask
 */
unsigned short dci_decode(viterbi_t *decoder, float *e, char *data, int E,
		int nof_bits) {

	float tmp[3 * (DCI_MAX_BITS + 16)];
	unsigned short p_bits;
	char *x;

	assert(nof_bits < DCI_MAX_BITS);

	/* unrate matching */
	rm_conv_rx(e, tmp, E, 3 * (nof_bits + 16));

	DEBUG("Viterbi input: ", 0);
	if (VERBOSE_ISDEBUG()) {
		vec_fprint_f(stdout, tmp, 3 * (nof_bits + 16));
	}

	/* viterbi decoder */
	viterbi_decode_f(decoder, tmp, data, nof_bits + 16);

	x = &data[nof_bits];
	p_bits = (unsigned short) bit_unpack(&x, 16);

	return (p_bits
			^ ((unsigned short) crc(0, data, nof_bits, 16, LTE_CRC16, 0)
					& 0xffff));
}

int pdcch_decode_candidate(pdcch_t *q, float *llr, dci_candidate_t *c,
		dci_msg_t *msg) {
	unsigned short crc_res;
	crc_res = dci_decode(&q->decoder, &llr[72 * c->ncce], msg->data,
			PDCCH_FORMAT_NOF_BITS(c->L), c->nof_bits);

	if (c->rnti == crc_res) {
		memcpy(&msg->location, c, sizeof(dci_candidate_t));
		INFO(
				"FOUND CAND: Nbits: %d, E: %d, nCCE: %d, L: %d, RNTI: 0x%x\n",
				c->nof_bits, PDCCH_FORMAT_NOF_BITS(c->L), c->ncce, c->L,
				c->rnti);
		return 1;
	}
	return 0;
}

int pdcch_extract_llr(pdcch_t *q, cf_t *slot1_symbols, cf_t *ce[MAX_PORTS_CTRL],
		float *llr, int nsubframe, float ebno) {

	/* Set pointers for layermapping & precoding */
	int i;
	cf_t *x[MAX_LAYERS];

	if (nsubframe < 0 || nsubframe > NSUBFRAMES_X_FRAME) {
		fprintf(stderr, "Invalid subframe %d\n", nsubframe);
		return -1;
	}

	if (ebno == 0.0) {
		fprintf(stderr, "EbNo is Zero\n");
		return -1;
	}

	/* number of layers equals number of ports */
	for (i = 0; i < q->nof_ports; i++) {
		x[i] = q->pdcch_x[i];
	}
	memset(&x[q->nof_ports], 0, sizeof(cf_t*) * (MAX_LAYERS - q->nof_ports));

	/* extract symbols */
	if (q->nof_symbols
			!= pdcch_get(slot1_symbols, q->pdcch_symbols[0], q->nof_symbols)) {
		fprintf(stderr, "There was an error getting the PDCCH symbols\n");
		return -1;
	}

	/* extract channel estimates */
	for (i = 0; i < q->nof_ports; i++) {
		if (q->nof_symbols != pdcch_get(ce[i], q->ce[i], q->nof_symbols)) {
			fprintf(stderr, "There was an error getting the PDCCH symbols\n");
			return -1;
		}
	}

	DEBUG("pdcch_symbols: ", 0);
	if (VERBOSE_ISDEBUG()) {
		vec_fprint_c(stdout, q->pdcch_symbols[0], q->nof_symbols);
	}

	/* in control channels, only diversity is supported */
	if (q->nof_ports == 1) {
		/* no need for layer demapping */
		predecoding_single_zf(q->pdcch_symbols[0], q->ce[0], q->pdcch_d,
				q->nof_symbols);
	} else {
		predecoding_diversity_zf(q->pdcch_symbols, q->ce, x, q->nof_ports,
				q->nof_symbols);
		layerdemap_diversity(x, q->pdcch_d, q->nof_ports,
				q->nof_symbols / q->nof_ports);
	}

	/* demodulate symbols */
	demod_soft_sigma_set(&q->demod, ebno);
	demod_soft_demodulate(&q->demod, q->pdcch_d, q->pdcch_llr, q->nof_symbols);

	DEBUG("llr: ", 0);
	if (VERBOSE_ISDEBUG()) {
		vec_fprint_f(stdout, q->pdcch_llr, q->nof_symbols);
	}

	/* descramble */
	scrambling_f_offset(&q->seq_pdcch[nsubframe], llr, 0, q->nof_bits);

	return 0;
}

int pdcch_decode_current_mode(pdcch_t *q, float *llr, dci_t *dci, int subframe) {
	int dci_cnt;
	int k, i;

	if (q->current_search_mode == SEARCH_UE) {
		k = subframe;
	} else {
		k = 0;
	}

	dci_cnt = 0;
	for (i = 0; i < q->search_mode[q->current_search_mode].nof_candidates
					&& dci_cnt < dci->nof_dcis; i++) {
		if (pdcch_decode_candidate(q, q->pdcch_llr,
				&q->search_mode[q->current_search_mode].candidates[k][i],
				&dci->msg[dci_cnt])) {
			dci_cnt++;
		}
	}
	return dci_cnt;
}

int pdcch_decode_si(pdcch_t *q, float *llr, dci_t *dci) {
	pdcch_set_search_si(q);
	return pdcch_decode_current_mode(q, llr, dci, 0);
}
int pdcch_decode_ra(pdcch_t *q, float *llr, dci_t *dci) {
	pdcch_set_search_ra(q);
	return pdcch_decode_current_mode(q, llr, dci, 0);
}
int pdcch_decode_ue(pdcch_t *q, float *llr, dci_t *dci, int nsubframe) {
	pdcch_set_search_ue(q);
	return pdcch_decode_current_mode(q, llr, dci, nsubframe);
}


/* Decodes PDCCH channels
 *
 * dci->nof_dcis is the size of the dci->msg buffer (ie max number of messages)
 *
 * Returns number of messages stored in dci
 */
int pdcch_decode(pdcch_t *q, cf_t *slot1_symbols, cf_t *ce[MAX_PORTS_CTRL],
		dci_t *dci, int nsubframe, float ebno) {

	if (pdcch_extract_llr(q, slot1_symbols, ce, q->pdcch_llr, nsubframe,
			ebno)) {
		return -1;
	}

	if (q->current_search_mode != SEARCH_NONE) {
		return pdcch_decode_current_mode(q, q->pdcch_llr, dci, nsubframe);
	}

	return 0;
}

void crc_set_mask_rnti(char *crc, unsigned short rnti) {
	int i;
	char mask[16];
	char *r = mask;

	INFO("Mask CRC with RNTI 0x%x\n", rnti);

	bit_pack(rnti, &r, 16);
	for (i = 0; i < 16; i++) {
		crc[i] = (crc[i] + mask[i]) % 2;
	}
}

/** 36.212 5.3.3.2 to 5.3.3.4
 * TODO: UE transmit antenna selection CRC mask
 */
void dci_encode(char *data, char *e, int nof_bits, int E, unsigned short rnti) {
	convcoder_t encoder;
	char tmp[3 * (DCI_MAX_BITS + 16)];

	assert(nof_bits < DCI_MAX_BITS);

	int poly[3] = { 0x6D, 0x4F, 0x57 };
	encoder.K = 7;
	encoder.R = 3;
	encoder.tail_biting = true;
	memcpy(encoder.poly, poly, 3 * sizeof(int));

	crc(0, data, nof_bits, 16, LTE_CRC16, 1);
	crc_set_mask_rnti(&data[nof_bits], rnti);

	convcoder_encode(&encoder, data, tmp, nof_bits + 16);

	DEBUG("CConv output: ", 0);
	if (VERBOSE_ISDEBUG()) {
		vec_fprint_b(stdout, tmp, 3 * (nof_bits + 16));
	}

	rm_conv_tx(tmp, e, 3 * (nof_bits + 16), E);
}

/** Converts the MIB message to symbols mapped to SLOT #1 ready for transmission
 */
int pdcch_encode(pdcch_t *q, dci_t *dci, cf_t *slot1_symbols[MAX_PORTS_CTRL],
		int nsubframe) {
	int i;
	/* Set pointers for layermapping & precoding */
	cf_t *x[MAX_LAYERS];

	if (nsubframe < 0 || nsubframe > NSUBFRAMES_X_FRAME) {
		fprintf(stderr, "Invalid subframe %d\n", nsubframe);
		return -1;
	}

	/* number of layers equals number of ports */
	for (i = 0; i < q->nof_ports; i++) {
		x[i] = q->pdcch_x[i];
	}
	memset(&x[q->nof_ports], 0, sizeof(cf_t*) * (MAX_LAYERS - q->nof_ports));

	/* should add <NIL> elements? Or maybe random bits to facilitate power estimation */
	bzero(q->pdcch_e, q->nof_bits);

	/* Encode DCIs */
	for (i = 0; i < dci->nof_dcis; i++) {
		/* do some sanity checks */
		if (dci->msg[i].location.ncce + PDCCH_FORMAT_NOF_CCE(dci->msg[i].location.L) > q->nof_cce
				|| dci->msg[i].location.L > 3
				|| dci->msg[i].location.nof_bits > DCI_MAX_BITS) {
			fprintf(stderr, "Illegal DCI message %d\n", i);
			return -1;
		}
		INFO("Encoding DCI %d: Nbits: %d, E: %d, nCCE: %d, L: %d, RNTI: 0x%x\n",
				i, dci->msg[i].location.nof_bits, PDCCH_FORMAT_NOF_BITS(dci->msg[i].location.L),
				dci->msg[i].location.ncce, dci->msg[i].location.L, dci->msg[i].location.rnti);

		dci_encode(dci->msg[i].data, &q->pdcch_e[72 * dci->msg[i].location.ncce],
				dci->msg[i].location.nof_bits, PDCCH_FORMAT_NOF_BITS(dci->msg[i].location.L),
				dci->msg[i].location.rnti);
	}

	scrambling_b_offset(&q->seq_pdcch[nsubframe], q->pdcch_e, 0, q->nof_bits);

	mod_modulate(&q->mod, q->pdcch_e, q->pdcch_d, q->nof_bits);

	/* layer mapping & precoding */
	if (q->nof_ports > 1) {
		layermap_diversity(q->pdcch_d, x, q->nof_ports, q->nof_symbols);
		precoding_diversity(x, q->pdcch_symbols, q->nof_ports,
				q->nof_symbols / q->nof_ports);
	} else {
		memcpy(q->pdcch_symbols[0], q->pdcch_d, q->nof_symbols * sizeof(cf_t));
	}

	/* mapping to resource elements */
	for (i = 0; i < q->nof_ports; i++) {
		pdcch_put(q->pdcch_symbols[i], slot1_symbols[i], q->nof_symbols);
	}
	return 0;
}

