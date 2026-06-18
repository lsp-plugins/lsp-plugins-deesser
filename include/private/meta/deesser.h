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
            static constexpr float  ZOOM_MIN                = GAIN_AMP_M_18_DB;
            static constexpr float  ZOOM_MAX                = GAIN_AMP_0_DB;
            static constexpr float  ZOOM_DFL                = GAIN_AMP_0_DB;
            static constexpr float  ZOOM_STEP               = 0.0125f;

            static constexpr float  LPF_FREQ_MIN            = 1000.0f;
            static constexpr float  LPF_FREQ_MAX            = SPEC_FREQ_MAX;
            static constexpr float  LPF_FREQ_DFL            = 10000.0f;
            static constexpr float  LPF_FREQ_STEP           = 0.025f;

            static constexpr float  HPF_FREQ_MIN            = 100.0f;
            static constexpr float  HPF_FREQ_MAX            = 4000.0f;
            static constexpr float  HPF_FREQ_DFL            = 2000.0f;
            static constexpr float  HPF_FREQ_STEP           = 0.025f;

            static constexpr float  PF_Q_MIN                = 0.0f;
            static constexpr float  PF_Q_MAX                = 1.0f;
            static constexpr float  PF_Q_DFL                = 0.0f;
            static constexpr float  PF_Q_STEP               = 0.0025f;

            static constexpr float  PEAK1_FREQ_MIN          = 1000.0f;
            static constexpr float  PEAK1_FREQ_MAX          = 8000.0f;
            static constexpr float  PEAK1_FREQ_DFL          = 4000.0f;
            static constexpr float  PEAK1_FREQ_STEP         = 0.025f;

            static constexpr float  PEAK2_FREQ_MIN          = 1000.0f;
            static constexpr float  PEAK2_FREQ_MAX          = 8000.0f;
            static constexpr float  PEAK2_FREQ_DFL          = 6000.0f;
            static constexpr float  PEAK2_FREQ_STEP         = 0.025f;

            static constexpr float  PEAK_Q_MIN              = 0.0f;
            static constexpr float  PEAK_Q_MAX              = 100.0f;
            static constexpr float  PEAK_Q_DFL              = 0.0f;
            static constexpr float  PEAK_Q_STEP             = 0.0025f;

            static constexpr float  PEAK_GAIN_MIN           = GAIN_AMP_0_DB;
            static constexpr float  PEAK_GAIN_MAX           = GAIN_AMP_P_24_DB;
            static constexpr float  PEAK_GAIN_DFL           = GAIN_AMP_P_6_DB;
            static constexpr float  PEAK_GAIN_STEP          = 0.0025f;

            static constexpr float  REACT_TIME_MIN          = 0.000f;
            static constexpr float  REACT_TIME_MAX          = 1.000f;
            static constexpr float  REACT_TIME_DFL          = 0.200f;
            static constexpr float  REACT_TIME_STEP         = 0.001f;

            static constexpr size_t MESH_POINTS             = 640;
            static constexpr size_t FFT_RANK                = 13;
            static constexpr size_t FFT_ITEMS               = 1 << FFT_RANK;
            static constexpr size_t REFRESH_RATE            = 20;
        } deesser;

        // Plugin type metadata
        extern const plugin_t deesser_mono;
        extern const plugin_t deesser_stereo;
        extern const plugin_t sc_deesser_mono;
        extern const plugin_t sc_deesser_stereo;

    } /* namespace meta */
} /* namespace lsp */

#endif /* PRIVATE_META_DEESSER_H_ */
