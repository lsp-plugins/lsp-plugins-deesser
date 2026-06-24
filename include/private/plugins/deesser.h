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
#include <lsp-plug.in/dsp-units/util/Sidechain.h>
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

                enum an_channel_t
                {
                    CH_INPUT,
                    CH_SIDECHAIN,
                    CH_OUTPUT,

                    CH_TOTAL
                };

                enum sidechain_type_t
                {
                    SCT_INTERNAL,
                    SCT_EXTERNAL,
                    SCT_LINK
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

                typedef struct sidechain_t
                {
                    uint8_t                 nType;              // Sidechain lookahead
                    uint8_t                 nLookahead;         // Sidechain lookahead
                    bool                    bListen;            // Sidechain listen

                    plug::IPort            *pType;              // Sidechain type
                    plug::IPort            *pMode;              // Sidechain mode
                    plug::IPort            *pSource;            // Sidechain source
                    plug::IPort            *pSplitScSource[2];  // Sidechain source in split mode
                    plug::IPort            *pLookahead;         // Sidechain lookahead
                    plug::IPort            *pListen;            // Sidechain listen
                    plug::IPort            *pReactivity;        // Sidechain reactivity
                    plug::IPort            *pPreamp;            // Sidechain pre-amplification
                } sidechain_t;

                typedef struct crossover_t
                {
                    uint8_t                 nMode;              // Work mode
                    uint8_t                 nSlope;             // Slope
                    float                   fFreq;              // Split frequency
                    float                   fLink;              // Link between low and high band

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

                    float                  *vIn[CH_TOTAL*2];    // Analysis input

                    plug::IPort            *pReactivity;        // Reactivity
                    plug::IPort            *pShiftGain;         // Shift gain port
                    plug::IPort            *pOn[CH_TOTAL*2];    // EnableFFT analysis data
                    plug::IPort            *pMesh;              // FFT analysis data
                } analysis_t;

                typedef struct reduction_t
                {
                    bool                    bSync;              // Sync mesh
                    float                   fEnv[2];            // Output envelope

                    float                  *vPoints;            // Compression points
                    float                  *vCurve;             // Curve

                    plug::IPort            *pThreshold;         // Threshold
                    plug::IPort            *pAttack;            // Attack
                    plug::IPort            *pRelease;           // Release
                    plug::IPort            *pHold;              // Hold time
                    plug::IPort            *pRatio;             // Reduction ratio
                    plug::IPort            *pKnee;              // Knee
                    plug::IPort            *pMesh;              // Curve mesh

                    plug::IPort            *pEnv[2];            // Envelope output
                    plug::IPort            *pRed[2];            // Gain reduction output
                    plug::IPort            *pCurve[2];          // Curve output
                } reduction_t;

                typedef struct channel_t
                {
                    dspu::Bypass            sBypass;            // Bypass
                    dspu::Sidechain         sSC;                // Sidechain
                    dspu::Equalizer         sSCEq;              // Sidechain equalizer
                    dspu::Crossover         sXOver;             // Crossover
                    dspu::FFTCrossover      sFFTXOver;          // FFT crossover
                    dspu::Compressor        sCompressor;        // Compressor for gain reduction
                    dspu::Delay             sDryDelay;          // Non-processed (dry) signal delay
                    dspu::Delay             sInDelay;           // Input signal delay
                    dspu::Delay             sScDelay;           // Sidechain signal delay

                    float                  *vIn;                // Input signal
                    float                  *vOut;               // Output signal
                    float                  *vScIn;              // Sidechain signal
                    float                  *vShmIn;             // Shared memory link signal

                    float                  *vScBuffer;          // Sidechain input buffer
                    float                  *vEnvBuffer;         // Envelope buffer
                    float                  *vHiBuffer;          // High-frequency buffer
                    float                  *vBuffer;            // Buffer for data

                    float                   fLoGain;            // Gain of the lower frequency band
                    float                   fHiGain;            // Gain of the higher frequency band
                    float                   fMeterIn;           // Input signal level
                    float                   fMeterOut;          // Output signal level

                    plug::IPort            *pIn;                // Input port
                    plug::IPort            *pOut;               // Output port
                    plug::IPort            *pScIn;              // Sidechain port
                    plug::IPort            *pShmIn;             // Shared memory link input
                    plug::IPort            *pMeterIn;           // Input signal meter
                    plug::IPort            *pMeterOut;          // Output signal meter
                } channel_t;

            protected:
                size_t                  nChannels;          // Number of channels
                channel_t              *vChannels;          // Delay channels
                float                  *vEmptyBuffer;       // Empty buffer filled with zeros
                float                  *vBuffer;            // Temporary buffer for audio processing

                float                   fStereoLink;        // Stereo linking
                float                   fInGain;            // Input gain
                float                   fOutGain;           // Output gain
                bool                    bSidechain;         // Sidechain version
                bool                    bStereoSplit;       // Stereo split

                dspu::DynamicFilters    sFilters;           // Dynamic filters
                dspu::Analyzer          sAnalyzer;          // Analyzer

                premix_t                sPremix;            // Premix settings
                sidechain_t             sSC;                // Sidechain setup
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
                static dspu::sidechain_source_t decode_sidechain_source(plug::IPort * src);

            protected:
                void                    do_destroy();
                void                    update_common();
                void                    update_premix();
                void                    update_sidechain();
                void                    update_analyzer();
                void                    update_preeq();
                void                    update_xover();
                void                    update_reduction();
                void                    bind_input_channels();
                void                    premix_channel(uint32_t channel, size_t count);
                void                    output_preeq_meshes();
                void                    output_xover_meshes();
                void                    output_reduction_meshes();
                void                    output_analysis_meshes();
                void                    output_meters();
                void                    clear_meters();
                sidechain_type_t        decode_sidechain_type(float value) const;
                void                    process_xover(size_t id, size_t samples);
                inline float           *select_buffer(channel_t & c);

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

