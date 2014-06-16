/*
 * Copyright (C) 2011-2014 Swift Navigation Inc.
 * Contact: Fergus Noble <fergus@swift-nav.com>
 *          Colin Beighley <colin@swift-nav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "acq_channel.h"
#include "nap_common.h"

#include <libswiftnav/prns.h>

/** \addtogroup nap
 * \{ */

/** \defgroup acq_channel Acquisition Channel
 * Interface to the SwiftNAP acquisition channel.
 * \{ */

/** Number of acq. channel code phase taps.
 * Number of acquisition channel code phase taps that NAP configuration was
 * built with. Read from configuration flash at runtime in
 * nap_conf_rd_parameters().
 */
u16 nap_acq_n_taps;

/** Set the LOAD ENABLE bit of the NAP acquisition channel's LOAD register.
 * When the LOAD ENABLE bit is set, the acquisition channel will start loading
 * samples into its sample ram, starting at the first clock cycle after the
 * NAP's internal timing strobe goes low.
 */
void nap_acq_load_wr_enable_blocking(void)
{
  u8 temp[1] = { 0xFF };

  nap_xfer_blocking(NAP_REG_ACQ_LOAD, 1, 0, temp);
}

/** Pack data for writing to NAP acquisition channel INIT register.
 *
 * Swift NAP returns corrs corresponding to code phases from
 * code_phase_reg_value-nap_acq_n_taps-1 to code_phase_reg_value where
 * code_phase_reg_value is the raw value written into the code phase portion of
 * the init register.
 *
 * - corrs[0] -> code_phase_reg_value-nap_acq_n_taps+1
 * - corrs[AQC_N_TAPS-1] -> code_phase_reg_value
 *
 * Lets take account of this here by writing code_phase+nap_acq_n_taps-1
 * to the code phase register on the Swift NAP. This means the
 * correlations returned will be:
 *
 * - corrs[0] -> code_phase
 * - corrs[nap_acq_n_taps] -> code_phase-nap_acq_n_taps+1
 *
 * \param prn          PRN number - (0..31) (deprecated)
 * \param code_phase   Code phase of the first correlation returned
 *                     (see note above), in acquisition units.
 * \param carrier_freq Carrier frequency i.e. Doppler in acquisition
 *                     units.
 */
static void nap_acq_init_pack(u8 pack[], u16 code_phase, s16 carrier_freq)
{
  (void)code_phase;
  pack[0] = carrier_freq >> 8;
  pack[1] = carrier_freq;
}

/** Write acquisition parameters to NAP acquisition channel's INIT register.
 * If the channel is currently disabled, it will be enabled and start an
 * acquisition with these parameters. If it is currently enabled, another
 * acquisition will start with these parameters when the current acquisition
 * finishes.
 *
 * \note If searching more than one acquisition point for a particular PRN, the
 *       second set of acquisition parameters should be written into the
 *       channel as soon as possible after the first set, as they are pipelined
 *       and used immediately after the first acquisition finishes. If only
 *       searching one point, nap_acq_init_wr_disable_blocking should be called
 *       as soon as possible after the first set of acquisition parameters are
 *       written, and again after the ACQ_DONE interrupt occurs to clear the
 *       interrupt.
 *
 * \param PRN          C/A PRN to use for the acquisition (deprecated)
 * \param code_phase   Code phase of the first correlation returned
 * \param carrier_freq Carrier frequency i.e. Doppler in acquisition units.
 */
/* TODO : remove writing of PRN number to init register */
void nap_acq_init_wr_params_blocking(u8 prn, u16 code_phase, s16 carrier_freq)
{
  u8 temp[2];

  (void)prn;

  nap_acq_init_pack(temp, code_phase, carrier_freq);
  nap_xfer_blocking(NAP_REG_ACQ_INIT, sizeof(temp), 0, temp);
}

/** Disable NAP acquisition channel.
 * Write to the acquisition channel's INIT register to disable the correlations
 * (clears enable bit). This must be written once to pipeline disable the
 * correlations, and then a second time to clear the ACQ_DONE IRQ after the
 * last correlation has finished.
 */
void nap_acq_init_wr_disable_blocking()
{
  u8 temp[4] = { 0, 0, 0, 0 };

  nap_xfer_blocking(NAP_REG_ACQ_INIT, 4, 0, temp);
}

/** Unpack correlations read from acquisition channel.
 *
 * \param packed Array of u8 data read from NAP acq channel CORR register.
 * \param index  Index corresponding to the maximum tap correlation.
 * \param corr   Maximum tap correlation from last cycle.
 * \param acc    Accumulation of all tap final correlations from last cycle.
 */
static void nap_acq_corr_unpack(u8 packed[], u16 *index, u16 *max, u16 *ave)
{
  *max = (packed[0] << 8) | packed[1];
  *ave = (packed[2] << 8) | packed[3];
  *index = ((packed[4] << 8) | packed[5]) >> (16 - NAP_ACQ_FFT_INDEX_BITS);
}

/** Read correlations from acquisition channel.
 * Must be called after the NAP IRQ register IRQ_ACQ_DONE bit goes high, before
 * the next acquisition cycle is complete.
 *
 * \param index  Index corresponding to the maximum tap correlation.
 * \param corr   Maximum tap correlation from last cycle.
 * \param acc    Accumulation of all tap final correlations from last cycle.
 */
void nap_acq_corr_rd_blocking(u16 *index, u16 *max, u16 *ave)
{
  u8 temp[6];

  nap_xfer_blocking(NAP_REG_ACQ_CORR, sizeof(temp), temp, temp);
  nap_acq_corr_unpack(temp, index, max, ave);
}

/** Write CA code to acquisition channel's code ram.
 * CA Code for SV to be searched for must be written into channel's code ram
 * before acquisitions are started.
 *
 * \param prn PRN number (0-31) of CA code to be written.
 */
void nap_acq_code_wr_blocking(u8 prn)
{
  nap_xfer_blocking(NAP_REG_ACQ_CODE, 128, 0, ca_code(prn));
}

/** \} */

/** \} */

