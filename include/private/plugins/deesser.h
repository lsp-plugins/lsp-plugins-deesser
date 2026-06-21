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

#ifndef PRIVATE_PLUGINS_DEESSER_H_
#define PRIVATE_PLUGINS_DEESSER_H_

#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
#include <lsp-plug.in/dsp-units/dynamics/Compressor.h>
#include <lsp-plug.in/dsp-units/filters/DynamicFilters.h>
#include <lsp-plug.in/dsp-units/filters/Equalizer.h>
#include <lsp-plug.in/dsp-units/util/Analyzer.h>
#include <lsp-plug.in/dsp-units/util/Crossover.h>
#include <lsp-plug.in/dsp-units/util/Delay.h>
#include <lsp-plug.in/dsp-units/util/FFTCrossover.h>
#include <lsp-plug.in/plug-fw/plug.h>
#include <private/meta/deesser.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * Base class for the latency compensation delay
         */
        class deesser: public plug::Module
        {
            protected:
                enum sc_filter_t
                {
                    SCF_PEAK1,
                    SCF_PEAK2,
                    SCF_LOWPASS,
                    SCF_HIPASS,
                    SCF_TOTAL,
                };

                enum sc_mask_t
                {
                    SCM_ALL_FILTERS     = (1 << SCF_PEAK1) | (1 << SCF_PEAK2) | (1 << SCF_LOWPASS) | (1 << SCF_HIPASS),
                    SCM_CURVE           = 1 << SCF_TOTAL,
                    SCM_OUT_MESH        = SCM_CURVE << 1,
                    SCM_SYNC_ALL        = (SCM_OUT_MESH << 1) - 1
                };

                enum sc_xover_mode_t
                {
                    XOVER_NONE,
                    XOVER_CLASSIC,
                    XOVER_MODERN,
                    XOVER_LINEAR_PHASE
                };

                typedef struct premix_t
                {
                    float                   fInToSc;            // Input -> Sidechain mix
                    float                   fInToLink;          // Input -> Link mix
                    float                   fLinkToIn;          // Link -> Input mix
                    float                   fLinkToSc;          // Link -> Sidechain mix
                    float                   fScToIn;            // Sidechain -> Input mix
                    float                   fScToLink;          // Sidechain -> Link mix

                    float                  *vIn[2];             // Input buffer
                    float                  *vOut[2];            // Output buffer
                    float                  *vSc[2];             // Sidechain buffer
                    float                  *vLink[2];           // Link buffer

                    float                  *vTmpIn[2];          // Replacement buffer for input
                    float                  *vTmpLink[2];        // Replacement buffer for link
                    float                  *vTmpSc[2];          // Replacement buffer for sidechain

                    plug::IPort            *pInToSc;            // Input -> Sidechain mix
                    plug::IPort            *pInToLink;          // Input -> Link mix
                    plug::IPort            *pLinkToIn;          // Link -> Input mix
                    plug::IPort            *pLinkToSc;          // Link -> Sidechain mix
                    plug::IPort            *pScToIn;            // Sidechain -> Input mix
                    plug::IPort            *pScToLink;          // Sidechain -> Link mix
                } premix_t;

                typedef struct crossover_t
                {
                    uint32_t                nMode;              // Work mode
                    uint32_t                nSlope;             // Slope
                    float                   fFreq;              // Split frequency

                    float                  *vLoBand;            // Characteristics of the low band
                    float                  *vHiBand;            // Characteristics of the high band

                    plug::IPort            *pMode;              // Crossover mode
                    plug::IPort            *pSlope;             // Crossover slope
                    plug::IPort            *pFreq;              // Crossover frequency
                    plug::IPort            *pLink;              // Crossover split linkage
                    plug::IPort            *pMesh;              // Transfer function
                } crossover_t;

                typedef struct preeq_t
                {
                    uint32_t                nSyncMesh;          // Synchronize mesh
                    float                  *vMeshData[SCF_TOTAL+1]; // Mesh data

                    plug::IPort            *pHpfSlope;          // Slope of the high-pass filter
                    plug::IPort            *pHpfFreq;           // Frequency of the high-pass filter
                    plug::IPort            *pHpfQ;              // Q factor of the high-pass filter
                    plug::IPort            *pLpfSlope;          // Slope of the low-pass filter
                    plug::IPort            *pLpfFreq;           // Frequency of the low-pass filter
                    plug::IPort            *pLpfQ;              // Q factor of the low-pass filter
                    plug::IPort            *pPeak1On;           // Peak filter 1 enable
                    plug::IPort            *pPeak1Freq;         // Peak filter 1 frequency
                    plug::IPort            *pPeak1Gain;         // Peak filter 1 gain
                    plug::IPort            *pPeak1Q;            // Peak filter 1 Q factor
                    plug::IPort            *pPeak2On;           // Peak filter 2 enable
                    plug::IPort            *pPeak2Freq;         // Peak filter 2 frequency
                    plug::IPort            *pPeak2Gain;         // Peak filter 2 gain
                    plug::IPort            *pPeak2Q;            // Peak filter 2 Q factor
                    plug::IPort            *pMesh;              // Filter mesh (overall + filters)
                } preeq_t;

                typedef struct analysis_t
                {
                    float                  *vFreqs;             // Analyzer FFT frequencies
                    uint32_t               *vIndexes;           // Analyzer FFT indexes

                    plug::IPort            *pReactivity;        // Reactivity
                    plug::IPort            *pShiftGain;         // Shift gain port
                    plug::IPort            *pMesh;              // FFT analysis data
                } analysis_t;

                typedef struct reduction_t
                {
                    bool                    bSync;              // Sync mesh

                    float                  *vPoints;            // Compression points
                    float                  *vCurve;             // Curve

                    plug::IPort            *pThreshold;         // Threshold
                    plug::IPort            *pAttack;            // Attack
                    plug::IPort            *pRelease;           // Release
                    plug::IPort            *pHold;              // Hold time
                    plug::IPort            *pRatio;             // Reduction ratio
                    plug::IPort            *pKnee;              // Knee
                    plug::IPort            *pMesh;              // Curve mesh
                } reduction_t;

                typedef struct channel_t
                {
                    // DSP processing modules
                    dspu::Bypass            sBypass;            // Bypass
                    dspu::Equalizer         sSCEq;              // Sidechain equalizer
                    dspu::Crossover         sXOver;             // Crossover
                    dspu::FFTCrossover      sFFTXOver;          // FFT crossover

                    float                  *vIn;                // Input signal
                    float                  *vOut;               // Output signal
                    float                  *vScIn;              // Sidechain signal
                    float                  *vShmIn;             // Shared memory link signal

                    float                  *vBuffer;            // Buffer for data

                    float                   fLoGain;            // Gain of the lower frequency band
                    float                   fHiGain;            // Gain of the higher frequency band

                    // Input ports
                    plug::IPort            *pIn;                // Input port
                    plug::IPort            *pOut;               // Output port
                    plug::IPort            *pScIn;              // Sidechain port
                    plug::IPort            *pShmIn;             // Shared memory link input
                } channel_t;

            protected:
                size_t                  nChannels;          // Number of channels
                channel_t              *vChannels;          // Delay channels
                float                  *vBuffer;            // Temporary buffer for audio processing

                float                   fStereoLink;        // Stereo linking
                bool                    bSidechain;         // Sidechain version
                bool                    bStereoSplit;       // Stereo split

                dspu::DynamicFilters    sFilters;           // Dynamic filters
                dspu::Analyzer          sAnalyzer;          // Analyzer
                dspu::Compressor        sCompressor;        // Compressor for gain reduction

                premix_t                sPremix;            // Premix settings
                analysis_t              sAnalysis;          // Analyzer parameters
                crossover_t             sXOver;             // Crossover settings
                preeq_t                 sPreEq;             // Pre-equalization settings
                reduction_t             sReduction;         // Reduction settings

                plug::IPort            *pBypass;            // Bypass
                plug::IPort            *pGainIn;            // Input gain
                plug::IPort            *pGainOut;           // Output gain
                plug::IPort            *pStereoSplit;       // Stereo split
                plug::IPort            *pStereoLink;        // Stereo linking

                uint8_t                *pData;              // Allocated data

            protected:
                static bool             set_filter_params(dspu::Equalizer * eq, uint32_t index, const dspu::filter_params_t * fp);
                static void             process_band(void *object, void *subject, size_t band, const float *data, size_t sample, size_t count);
                static size_t           select_fft_rank(size_t sample_rate);

            protected:
                void                    do_destroy();
                void                    update_premix();
                void                    update_analyzer();
                void                    update_preeq();
                void                    update_xover();
                void                    update_reduction();
                void                    bind_input_channels();
                void                    premix_channel(uint32_t channel, size_t count);
                void                    output_preeq_meshes();
                void                    output_xover_meshes();
                void                    output_reduction_meshes();

            public:
                explicit deesser(const meta::plugin_t *meta);
                deesser (const deesser &) = delete;
                deesser (deesser &&) = delete;
                virtual ~deesser() override;

                deesser & operator = (const deesser &) = delete;
                deesser & operator = (deesser &&) = delete;

                virtual void            init(plug::IWrapper *wrapper, plug::IPort **ports) override;
                virtual void            destroy() override;

            public:
                virtual void            update_sample_rate(long sr) override;
                virtual void            update_settings() override;
                virtual void            process(size_t samples) override;
                virtual void            ui_activated() override;
                virtual void            dump(dspu::IStateDumper *v) const override;
        };

    } /* namespace plugins */
} /* namespace lsp */


#endif /* PRIVATE_PLUGINS_DEESSER_H_ */

