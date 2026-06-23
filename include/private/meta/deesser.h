/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-deesser
 * Created on: 16 июн 2026 г.
 *
 * lsp-plugins-deesser is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-deesser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-deesser. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PRIVATE_META_DEESSER_H_
#define PRIVATE_META_DEESSER_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>

namespace lsp
{
    //-------------------------------------------------------------------------
    // Plugin metadata
    namespace meta
    {
        typedef struct deesser
        {
            static constexpr float  THRESHOLD_MIN           = GAIN_AMP_M_60_DB;
            static constexpr float  THRESHOLD_MAX           = GAIN_AMP_0_DB;
            static constexpr float  THRESHOLD_DFL           = GAIN_AMP_M_12_DB;
            static constexpr float  THRESHOLD_STEP          = 0.05f;

            static constexpr float  ATTACK_MIN              = 0.0f;
            static constexpr float  ATTACK_MAX              = 400.0f;
            static constexpr float  ATTACK_DFL              = 10.0f;
            static constexpr float  ATTACK_STEP             = 0.0025f;

            static constexpr float  RELEASE_MIN             = 0.0f;
            static constexpr float  RELEASE_MAX             = 1000.0f;
            static constexpr float  RELEASE_DFL             = 50.0f;
            static constexpr float  RELEASE_STEP            = 0.0025f;

            static constexpr float  HOLD_MIN                = 0.0f;
            static constexpr float  HOLD_MAX                = 100.0f;
            static constexpr float  HOLD_DFL                = 0.0f;
            static constexpr float  HOLD_STEP               = 0.1f;

            static constexpr float  KNEE_MIN                = GAIN_AMP_M_24_DB;
            static constexpr float  KNEE_MAX                = GAIN_AMP_0_DB;
            static constexpr float  KNEE_DFL                = GAIN_AMP_M_6_DB;
            static constexpr float  KNEE_STEP               = 0.01f;

            static constexpr float  RATIO_MIN               = 1.0f;
            static constexpr float  RATIO_MAX               = 100.0f;
            static constexpr float  RATIO_DFL               = 6.0f;
            static constexpr float  RATIO_STEP              = 0.0025f;

            static constexpr float  ZOOM_MIN                = GAIN_AMP_M_18_DB;
            static constexpr float  ZOOM_MAX                = GAIN_AMP_0_DB;
            static constexpr float  ZOOM_DFL                = GAIN_AMP_0_DB;
            static constexpr float  ZOOM_STEP               = 0.0125f;

            static constexpr float  LPF_FREQ_MIN            = 1000.0f;
            static constexpr float  LPF_FREQ_MAX            = SPEC_FREQ_MAX;
            static constexpr float  LPF_FREQ_DFL            = 10000.0f;
            static constexpr float  LPF_FREQ_STEP           = 0.002f;

            static constexpr float  HPF_FREQ_MIN            = 10.0f;
            static constexpr float  HPF_FREQ_MAX            = 4000.0f;
            static constexpr float  HPF_FREQ_DFL            = 2000.0f;
            static constexpr float  HPF_FREQ_STEP           = 0.002f;

            static constexpr float  PF_Q_MIN                = 0.0f;
            static constexpr float  PF_Q_MAX                = 10.0f;
            static constexpr float  PF_Q_DFL                = 0.0f;
            static constexpr float  PF_Q_STEP               = 0.0125f;

            static constexpr float  PEAK1_FREQ_MIN          = 1000.0f;
            static constexpr float  PEAK1_FREQ_MAX          = 8000.0f;
            static constexpr float  PEAK1_FREQ_DFL          = 4000.0f;
            static constexpr float  PEAK1_FREQ_STEP         = 0.002f;

            static constexpr float  PEAK2_FREQ_MIN          = 1000.0f;
            static constexpr float  PEAK2_FREQ_MAX          = 8000.0f;
            static constexpr float  PEAK2_FREQ_DFL          = 6000.0f;
            static constexpr float  PEAK2_FREQ_STEP         = 0.002f;

            static constexpr float  PEAK_Q_MIN              = 0.0f;
            static constexpr float  PEAK_Q_MAX              = 100.0f;
            static constexpr float  PEAK_Q_DFL              = 4.0f;
            static constexpr float  PEAK_Q_STEP             = 0.025f;

            static constexpr float  PEAK_GAIN_MIN           = GAIN_AMP_0_DB;
            static constexpr float  PEAK_GAIN_MAX           = GAIN_AMP_P_24_DB;
            static constexpr float  PEAK_GAIN_DFL           = GAIN_AMP_P_6_DB;
            static constexpr float  PEAK_GAIN_STEP          = 0.0025f;

            static constexpr float  REACT_TIME_MIN          = 0.000f;
            static constexpr float  REACT_TIME_MAX          = 1.000f;
            static constexpr float  REACT_TIME_DFL          = 0.200f;
            static constexpr float  REACT_TIME_STEP         = 0.001f;

            static constexpr float  SPLIT_FREQ_MIN          = 500.0f;
            static constexpr float  SPLIT_FREQ_MAX          = 4000.0f;
            static constexpr float  SPLIT_FREQ_DFL          = 1000.0f;
            static constexpr float  SPLIT_FREQ_STEP         = 0.002f;

            static constexpr float  LINKING_MIN             = 0;
            static constexpr float  LINKING_MAX             = 100.0f;
            static constexpr float  LINKING_DFL             = 100.0f;
            static constexpr float  LINKING_STEP            = 0.01f;

            static constexpr float  SC_LOOKAHEAD_MIN        = 0.0f;
            static constexpr float  SC_LOOKAHEAD_MAX        = 20.0f;
            static constexpr float  SC_LOOKAHEAD_DFL        = 0.0f;
            static constexpr float  SC_LOOKAHEAD_STEP       = 0.01f;

            static constexpr float  SC_REACTIVITY_MIN       = 0.000;
            static constexpr float  SC_REACTIVITY_MAX       = 50;
            static constexpr float  SC_REACTIVITY_DFL       = 10;
            static constexpr float  SC_REACTIVITY_STEP      = 0.0125;

            static constexpr size_t SC_MODE_DFL             = 1;
            static constexpr size_t SC_SOURCE_DFL           = 0;
            static constexpr size_t SC_SOURCE_L_DFL         = 2;
            static constexpr size_t SC_SOURCE_R_DFL         = 3;
            static constexpr size_t SC_TYPE_DFL             = 0;

            static constexpr size_t FFT_MESH_POINTS         = 640;
            static constexpr size_t CURVE_MESH_POINTS       = 256;
            static constexpr size_t FFT_ANALYSIS_RANK       = 13;
            static constexpr size_t FFT_ANALYSIS_ITEMS      = 1 << FFT_ANALYSIS_RANK;
            static constexpr size_t REFRESH_RATE            = 20;
            static constexpr size_t FFT_XOVER_RANK_MIN      = 12;
            static constexpr size_t FFT_XOVER_FREQ_MIN      = 44100;
            static constexpr float  CURVE_DB_MIN            = -72;
            static constexpr float  CURVE_DB_MAX            = +24;
        } deesser;

        // Plugin type metadata
        extern const plugin_t deesser_mono;
        extern const plugin_t deesser_stereo;
        extern const plugin_t sc_deesser_mono;
        extern const plugin_t sc_deesser_stereo;

    } /* namespace meta */
} /* namespace lsp */

#endif /* PRIVATE_META_DEESSER_H_ */
