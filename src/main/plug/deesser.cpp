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

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/bits.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/misc/envelope.h>
#include <lsp-plug.in/dsp-units/misc/windows.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/plug-fw/core/AudioBuffer.h>
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/shared/debug.h>
#include <lsp-plug.in/shared/id_colors.h>

#include <private/plugins/deesser.h>

namespace lsp
{
    namespace plugins
    {
        // The size of temporary buffer for audio processing
        static constexpr size_t BUFFER_SIZE             = 0x200;

        //---------------------------------------------------------------------
        // Plugin factory
        static const meta::plugin_t *plugins[] =
        {
            &meta::deesser_mono,
            &meta::deesser_stereo,
            &meta::sc_deesser_mono,
            &meta::sc_deesser_stereo
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            return new deesser(meta);
        }

        static plug::Factory factory(plugin_factory, plugins, 4);

        //---------------------------------------------------------------------
        // Implementation
        deesser::deesser(const meta::plugin_t *meta):
            Module(meta)
        {
            // Compute the number of audio channels by the number of inputs
            nChannels               =
                ((strcmp(meta->uid, meta::deesser_stereo.uid) == 0) ||
                (strcmp(meta->uid, meta::sc_deesser_stereo.uid) == 0)) ?
                2 : 1;
            // Initialize other parameters
            vChannels               = NULL;
            vEmptyBuffer            = NULL;
            vBuffer                 = NULL;

            fStereoLink             = 0.0f;
            fInGain                 = GAIN_AMP_0_DB;
            fOutGain                = GAIN_AMP_0_DB;
            bSidechain              =
                (strcmp(meta->uid, meta::sc_deesser_mono.uid) == 0) ||
                (strcmp(meta->uid, meta::sc_deesser_stereo.uid) == 0);
            bStereoSplit            = false;

            // Init premix settings
            sPremix.fInToSc         = GAIN_AMP_M_INF_DB;
            sPremix.fInToLink       = GAIN_AMP_M_INF_DB;
            sPremix.fLinkToIn       = GAIN_AMP_M_INF_DB;
            sPremix.fLinkToSc       = GAIN_AMP_M_INF_DB;
            sPremix.fScToIn         = GAIN_AMP_M_INF_DB;
            sPremix.fScToLink       = GAIN_AMP_M_INF_DB;

            for (size_t i=0; i<2; ++i)
            {
                sPremix.vIn[i]          = NULL;
                sPremix.vOut[i]         = NULL;
                sPremix.vSc[i]          = NULL;
                sPremix.vLink[i]        = NULL;
                sPremix.vTmpIn[i]       = NULL;
                sPremix.vTmpSc[i]       = NULL;
                sPremix.vTmpLink[i]     = NULL;
            }

            sPremix.pInToSc         = NULL;
            sPremix.pInToLink       = NULL;
            sPremix.pLinkToIn       = NULL;
            sPremix.pLinkToSc       = NULL;
            sPremix.pScToIn         = NULL;
            sPremix.pScToLink       = NULL;

            // Init sidechain
            sSC.bListen             = false;

            sSC.pType               = NULL;
            sSC.pMode               = NULL;
            sSC.pSource             = NULL;
            sSC.pSplitScSource[0]   = NULL;
            sSC.pSplitScSource[1]   = NULL;
            sSC.pLookahead          = NULL;
            sSC.pListen             = NULL;
            sSC.pReactivity         = NULL;
            sSC.pPreamp             = NULL;

            // Init analysis settings
            sAnalysis.vFreqs        = NULL;
            sAnalysis.vIndexes      = NULL;
            for (size_t i=0; i<CH_TOTAL*2; ++i)
                sAnalysis.vIn[i]        = NULL;

            sAnalysis.pReactivity   = NULL;
            sAnalysis.pShiftGain    = NULL;
            for (size_t i=0; i<CH_TOTAL*2; ++i)
                sAnalysis.pOn[i]        = NULL;
            sAnalysis.pMesh         = NULL;

            for (size_t i=0; i<CH_TOTAL * 2; ++i)
                sAnalysis.pOn[i]        = NULL;

            // Init crossover settings
            sXOver.nMode            = XOVER_NONE;
            sXOver.nSlope           = 1;
            sXOver.fFreq            = 0.0f;
            sXOver.fLink            = 0.0f;

            sXOver.vLoBand          = NULL;
            sXOver.vHiBand          = NULL;

            sXOver.pMode            = NULL;
            sXOver.pSlope           = NULL;
            sXOver.pFreq            = NULL;
            sXOver.pLink            = NULL;
            sXOver.pMesh            = NULL;

            // Init pre-equalization settings
            sPreEq.nSyncMesh    = SCM_SYNC_ALL;
            for (size_t i=0; i<=SCF_TOTAL; ++i)
                sPreEq.vMeshData[i] = NULL;

            sPreEq.pHpfSlope        = NULL;
            sPreEq.pHpfFreq         = NULL;
            sPreEq.pHpfQ            = NULL;
            sPreEq.pLpfSlope        = NULL;
            sPreEq.pLpfFreq         = NULL;
            sPreEq.pLpfQ            = NULL;
            sPreEq.pPeak1On         = NULL;
            sPreEq.pPeak1Freq       = NULL;
            sPreEq.pPeak1Gain       = NULL;
            sPreEq.pPeak1Q          = NULL;
            sPreEq.pPeak2On         = NULL;
            sPreEq.pPeak2Freq       = NULL;
            sPreEq.pPeak2Gain       = NULL;
            sPreEq.pPeak2Q          = NULL;
            sPreEq.pMesh            = NULL;

            // Init reductoin settings
            sReduction.bSync        = true;

            sReduction.fEnv[0]      = GAIN_AMP_M_INF_DB;
            sReduction.fEnv[1]      = GAIN_AMP_M_INF_DB;
            sReduction.fOutEnv[0]   = GAIN_AMP_M_INF_DB;
            sReduction.fOutEnv[1]   = GAIN_AMP_M_INF_DB;
            sReduction.fOutGain[0]  = GAIN_AMP_M_INF_DB;
            sReduction.fOutGain[1]  = GAIN_AMP_M_INF_DB;

            sReduction.vPoints      = NULL;
            sReduction.vCurve       = NULL;

            sReduction.pThreshold   = NULL;
            sReduction.pAttack      = NULL;
            sReduction.pRelease     = NULL;
            sReduction.pHold        = NULL;
            sReduction.pRatio       = NULL;
            sReduction.pKnee        = NULL;
            sReduction.pMesh        = NULL;

            sReduction.pEnv[0]      = NULL;
            sReduction.pEnv[1]      = NULL;
            sReduction.pRed[0]      = NULL;
            sReduction.pRed[1]      = NULL;
            sReduction.pCurve[0]    = NULL;
            sReduction.pCurve[1]    = NULL;

            // Init common settings
            pBypass                 = NULL;
            pGainIn                 = NULL;
            pGainOut                = NULL;
            pStereoSplit            = NULL;
            pStereoLink             = NULL;

            pIDisplay               = NULL;
            pData                   = NULL;
        }

        deesser::~deesser()
        {
            do_destroy();
        }

        void deesser::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialization
            Module::init(wrapper, ports);

            // Estimate the number of bytes to allocate
            const size_t szof_channels      = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);
            const size_t buf_sz             = BUFFER_SIZE * sizeof(float);
            const size_t tmp_buf_sz         = lsp_max(buf_sz, meta::deesser::FFT_MESH_POINTS * 2 * sizeof(float));
            const size_t points_sz          = meta::deesser::CURVE_MESH_POINTS * sizeof(float);
            const size_t freqs_sz           = align_size((meta::deesser::FFT_MESH_POINTS) * sizeof(float), OPTIMAL_ALIGN);
            const size_t idx_sz             = align_size((meta::deesser::FFT_MESH_POINTS) * sizeof(uint32_t), OPTIMAL_ALIGN);
            const size_t mesh_sz            = align_size((meta::deesser::FFT_MESH_POINTS + 4) * sizeof(float), OPTIMAL_ALIGN);
            const size_t alloc              =
                szof_channels +                 // vChannels
                buf_sz +                        // vEmptyBuffer
                tmp_buf_sz +                    // vBuffer
                idx_sz +                        // vIndexes
                mesh_sz +                       // vFreqs
                freqs_sz * 4 +                  // sXOver.vLoBand + sXOver.vHiBand
                mesh_sz * (SCF_TOTAL + 1) +     // sPreEq.vMeshData
                points_sz * 2 +                 // sReduction.vPoints + sReduction.vCurve
                nChannels * buf_sz * 3 +        // sPremix.vTmpIn + sPremix.vTmpLink + sPremix.vTmpSc
                nChannels * (
                    buf_sz +                    // vChannels.vScBuffer
                    buf_sz +                    // vChannels.vEnvBuffer
                    buf_sz +                    // vChannels.vHiBuffer
                    tmp_buf_sz                  // vChannels.vBuffer
                );

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Initialize pointers to channels and temporary buffer
            vChannels               = advance_ptr_bytes<channel_t>(ptr, szof_channels);
            vEmptyBuffer            = advance_ptr_bytes<float>(ptr, buf_sz);
            vBuffer                 = advance_ptr_bytes<float>(ptr, tmp_buf_sz);

            sAnalysis.vFreqs        = advance_ptr_bytes<float>(ptr, freqs_sz);
            sAnalysis.vIndexes      = advance_ptr_bytes<uint32_t>(ptr, idx_sz);

            for (size_t i=0; i <= SCF_TOTAL; ++i)
                sPreEq.vMeshData[i]     = advance_ptr_bytes<float>(ptr, mesh_sz);
            sXOver.vLoBand          = advance_ptr_bytes<float>(ptr, freqs_sz * 2);
            sXOver.vHiBand          = advance_ptr_bytes<float>(ptr, freqs_sz * 2);

            sReduction.vPoints      = advance_ptr_bytes<float>(ptr, points_sz);
            sReduction.vCurve       = advance_ptr_bytes<float>(ptr, points_sz);

            if (sFilters.init(4) != STATUS_OK)
                return;
            for (size_t i=0; i<4; ++i)
                sFilters.set_filter_active(i, true);

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];

                // Construct in-place DSP processors
                c->sBypass.construct();
                c->sSC.construct();
                c->sSCEq.construct();
                c->sXOver.construct();
                c->sLPXOver.construct();
                c->sCompressor.construct();
                c->sDryDelay.construct();
                c->sInDelay.construct();
                c->sScDelay.construct();

                if (!c->sSC.init(nChannels, meta::deesser::SC_REACTIVITY_MAX))
                    return;
                c->sSC.set_stereo_mode(dspu::SCSM_STEREO);

                if (!c->sSCEq.init(SCF_TOTAL, 0))
                    return;
                c->sSCEq.set_mode(dspu::EQM_IIR);

                if (!c->sXOver.init(2, BUFFER_SIZE))
                    return;
                for (size_t j=0; j<2; ++j)
                    c->sXOver.set_handler(j, process_band, this, c);                // Bind channel as a handler
                c->sXOver.set_mode(0, dspu::CROSS_MODE_BT);

                c->sCompressor.set_mode(dspu::CM_DOWNWARD);

                c->vScBuffer            = advance_ptr_bytes<float>(ptr, buf_sz);
                c->vEnvBuffer           = advance_ptr_bytes<float>(ptr, buf_sz);
                c->vHiBuffer            = advance_ptr_bytes<float>(ptr, buf_sz);
                c->vBuffer              = advance_ptr_bytes<float>(ptr, tmp_buf_sz);
                c->fLoGain              = (i == 0) ? GAIN_AMP_0_DB : GAIN_AMP_M_12_DB;      // DBG
                c->fHiGain              = (i == 0) ? GAIN_AMP_M_24_DB : GAIN_AMP_M_36_DB;   // DBG
                c->fMeterIn             = GAIN_AMP_M_INF_DB;
                c->fMeterOut            = GAIN_AMP_M_INF_DB;

                // Initialize fields
                c->vRawIn               = NULL;
                c->vIn                  = NULL;
                c->vOut                 = NULL;
                c->vScIn                = NULL;
                c->vShmIn               = NULL;

                // Initialize ports
                c->pIn                  = NULL;
                c->pOut                 = NULL;
                c->pScIn                = NULL;
                c->pShmIn               = NULL;

                c->pMeterIn             = NULL;
                c->pMeterOut            = NULL;

                // Bind premix buffers
                sPremix.vTmpIn[i]       = advance_ptr_bytes<float>(ptr, buf_sz);
                sPremix.vTmpLink[i]     = advance_ptr_bytes<float>(ptr, buf_sz);
                sPremix.vTmpSc[i]       = advance_ptr_bytes<float>(ptr, buf_sz);
            }

            // Bind ports
            lsp_trace("Binding ports");
            size_t port_id      = 0;

            // Bind input audio ports
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pIn);

            // Bind output audio ports
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pOut);

            // Bind sidechain audio ports
            if (bSidechain)
            {
                for (size_t i=0; i<nChannels; ++i)
                    BIND_PORT(vChannels[i].pScIn);
            }

            // Shared memory link
            lsp_trace("Binding shared memory link");
            SKIP_PORT("Shared memory link name");
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pShmIn);

            // Bind common parameters
            lsp_trace("Binding common ports");
            BIND_PORT(pBypass);
            BIND_PORT(pGainIn);
            BIND_PORT(pGainOut);
            SKIP_PORT("Zoom");
            if (nChannels > 1)
            {
                BIND_PORT(pStereoSplit);
                BIND_PORT(pStereoLink);
            }
            for (size_t i=0; i<nChannels; ++i)
            {
                BIND_PORT(vChannels[i].pMeterIn);
                BIND_PORT(vChannels[i].pMeterOut);
            }

            // Pre-mixing ports
            lsp_trace("Binding pre-mix ports");
            SKIP_PORT("Show pre-mix overlay");
            BIND_PORT(sPremix.pInToLink);
            BIND_PORT(sPremix.pLinkToIn);
            BIND_PORT(sPremix.pLinkToSc);
            if (bSidechain)
            {
                BIND_PORT(sPremix.pInToSc);
                BIND_PORT(sPremix.pScToIn);
                BIND_PORT(sPremix.pScToLink);
            }

            // Sidechain ports
            lsp_trace("Binding sidechain ports");
            SKIP_PORT("Show sidechain overlay");
            BIND_PORT(sSC.pType);
            BIND_PORT(sSC.pMode);
            if (nChannels > 1)
            {
                BIND_PORT(sSC.pSource);
                for (size_t i=0; i<nChannels; ++i)
                    BIND_PORT(sSC.pSplitScSource[i]);
            }
            BIND_PORT(sSC.pLookahead);
            BIND_PORT(sSC.pListen);
            BIND_PORT(sSC.pReactivity);
            BIND_PORT(sSC.pPreamp);

            // FFT analysis ports
            lsp_trace("Binding FFT analysis ports");
            BIND_PORT(sAnalysis.pReactivity);
            BIND_PORT(sAnalysis.pShiftGain);
            for (size_t i=0; i<nChannels * CH_TOTAL; ++i)
                BIND_PORT(sAnalysis.pOn[i]);
            BIND_PORT(sAnalysis.pMesh);

            // Crossover ports
            lsp_trace("Binding Crossover ports");
            BIND_PORT(sXOver.pMode);
            BIND_PORT(sXOver.pSlope);
            BIND_PORT(sXOver.pFreq);
            BIND_PORT(sXOver.pLink);
            BIND_PORT(sXOver.pMesh);

            // Pre-equalization ports
            lsp_trace("Binding Pre-Eq ports");
            BIND_PORT(sPreEq.pHpfSlope);
            BIND_PORT(sPreEq.pHpfFreq);
            BIND_PORT(sPreEq.pHpfQ);
            BIND_PORT(sPreEq.pLpfSlope);
            BIND_PORT(sPreEq.pLpfFreq);
            BIND_PORT(sPreEq.pLpfQ);
            BIND_PORT(sPreEq.pPeak1On);
            BIND_PORT(sPreEq.pPeak1Freq);
            BIND_PORT(sPreEq.pPeak1Gain);
            BIND_PORT(sPreEq.pPeak1Q);
            BIND_PORT(sPreEq.pPeak2On);
            BIND_PORT(sPreEq.pPeak2Freq);
            BIND_PORT(sPreEq.pPeak2Gain);
            BIND_PORT(sPreEq.pPeak2Q);
            BIND_PORT(sPreEq.pMesh);

            // Reduction parameters
            lsp_trace("Binding Reduction ports");
            BIND_PORT(sReduction.pThreshold);
            BIND_PORT(sReduction.pAttack);
            BIND_PORT(sReduction.pRelease);
            BIND_PORT(sReduction.pHold);
            BIND_PORT(sReduction.pRatio);
            BIND_PORT(sReduction.pKnee);
            BIND_PORT(sReduction.pMesh);
            for (size_t i=0; i<nChannels; ++i)
            {
                BIND_PORT(sReduction.pEnv[i]);
                BIND_PORT(sReduction.pRed[i]);
                BIND_PORT(sReduction.pCurve[i]);
            }

            // Initialize curve (logarithmic) in range of -72 .. +24 db
            const float delta   = (meta::deesser::CURVE_DB_MAX - meta::deesser::CURVE_DB_MIN) / (meta::deesser::CURVE_MESH_POINTS - 1);
            for (size_t i=0; i<meta::deesser::CURVE_MESH_POINTS; ++i)
                sReduction.vPoints[i]   = dspu::db_to_gain(meta::deesser::CURVE_DB_MIN + delta * i);
        }

        void deesser::destroy()
        {
            Module::destroy();
            do_destroy();
        }

        void deesser::do_destroy()
        {
            // Destroy channels
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t * const c    = &vChannels[i];
                    c->sBypass.destroy();
                    c->sSC.destroy();
                    c->sSCEq.destroy();
                    c->sXOver.destroy();
                    c->sLPXOver.destroy();
                    c->sCompressor.destroy();
                    c->sDryDelay.destroy();
                    c->sInDelay.destroy();
                    c->sScDelay.destroy();
                }
                vChannels   = NULL;
            }

            // Destroy global processors
            sFilters.destroy();
            sAnalyzer.destroy();

            // Destroy inline display if present
            if (pIDisplay != NULL)
            {
                pIDisplay->destroy();
                pIDisplay   = NULL;
            }

            // Free previously allocated data chunk
            if (pData != NULL)
            {
                free_aligned(pData);
                pData       = NULL;
            }
        }

        size_t deesser::select_fft_rank(size_t sample_rate)
        {
            const size_t k = (sample_rate + meta::deesser::FFT_XOVER_FREQ_MIN/2) / meta::deesser::FFT_XOVER_FREQ_MIN;
            const size_t n = int_log2(k);
            return meta::deesser::FFT_XOVER_RANK_MIN + n;
        }

        void deesser::update_sample_rate(long sr)
        {
            const size_t xover_fft_rank     = select_fft_rank(sr);
            const size_t lkahead_latency    = dspu::millis_to_samples(sr, meta::deesser::SC_LOOKAHEAD_MAX);
            const size_t max_fft_latency    = (1 << xover_fft_rank);
            const size_t max_latency        = max_fft_latency + lkahead_latency;

            // Update analyzer's sample rate
            sAnalyzer.init(
                CH_TOTAL * nChannels,
                meta::deesser::FFT_ANALYSIS_RANK,
                MAX_SAMPLE_RATE,
                meta::deesser::REFRESH_RATE,
                max_latency);
            sAnalyzer.set_sample_rate(sr);
            sAnalyzer.set_rank(meta::deesser::FFT_ANALYSIS_RANK);
            sAnalyzer.set_activity(false);
            sAnalyzer.set_envelope(dspu::envelope::PINK_NOISE);
            sAnalyzer.set_window(dspu::windows::HANN);
            sAnalyzer.set_rate(meta::deesser::REFRESH_RATE);

            sFilters.set_sample_rate(sr);

            // Update sample rate for the bypass processors
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];
                c->sBypass.init(sr);
                c->sSC.set_sample_rate(sr);
                c->sSCEq.set_sample_rate(sr);
                c->sXOver.set_sample_rate(sr);
                c->sCompressor.set_sample_rate(sr);
                c->sDryDelay.init(max_latency);
                c->sInDelay.init(max_latency);
                c->sScDelay.init(max_fft_latency);

                // Need to re-initialize FFT crossover?
                if (xover_fft_rank != c->sLPXOver.rank())
                {
                    c->sLPXOver.init(xover_fft_rank, 2);
                    for (size_t j=0; j<2; ++j)
                        c->sLPXOver.set_handler(j, process_band, this, c);
                    c->sLPXOver.set_rank(xover_fft_rank);
                    c->sLPXOver.set_phase(float(i) / float(nChannels));
                }
                c->sLPXOver.set_sample_rate(sr);
            }
        }

        void deesser::update_common()
        {
            const bool bypass       = pBypass->value() >= 0.5f;
            bStereoSplit            = (pStereoSplit != NULL) ? pStereoSplit->value() >= 0.5f : false;
            fStereoLink             = (pStereoLink != NULL) ? pStereoLink->value() * 0.01f : 0.0f;
            fInGain                 = pGainIn->value();
            fOutGain                = pGainOut->value();

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];
                c->sBypass.set_bypass(bypass);
            }
        }

        void deesser::update_premix()
        {
            sPremix.fInToSc     = (sPremix.pInToSc != NULL)     ? sPremix.pInToSc->value()      : GAIN_AMP_M_INF_DB;
            sPremix.fInToLink   = (sPremix.pInToLink != NULL)   ? sPremix.pInToLink->value()    : GAIN_AMP_M_INF_DB;
            sPremix.fLinkToIn   = (sPremix.pLinkToIn != NULL)   ? sPremix.pLinkToIn->value()    : GAIN_AMP_M_INF_DB;
            sPremix.fLinkToSc   = (sPremix.pLinkToSc != NULL)   ? sPremix.pLinkToSc->value()    : GAIN_AMP_M_INF_DB;
            sPremix.fScToIn     = (sPremix.pScToIn != NULL)     ? sPremix.pScToIn->value()      : GAIN_AMP_M_INF_DB;
            sPremix.fScToLink   = (sPremix.pScToLink != NULL)   ? sPremix.pScToLink->value()    : GAIN_AMP_M_INF_DB;
        }

        deesser::sidechain_type_t deesser::decode_sidechain_type(float value) const
        {
            const uint32_t key = uint32_t(value);
            if (bSidechain)
                return sidechain_type_t(key);
            return (key == 0) ? SCT_INTERNAL : SCT_LINK;
        }

        dspu::sidechain_source_t deesser::decode_sidechain_source(plug::IPort * src)
        {
            return (src != NULL) ? dspu::sidechain_source_t(uint32_t(src->value())) : dspu::SCS_MIDDLE;
        }

        void deesser::update_sidechain()
        {
            sSC.nType                   = decode_sidechain_type(sSC.pType->value());
            sSC.nLookahead              = dspu::millis_to_samples(fSampleRate, sSC.pLookahead->value());
            sSC.bListen                 = sSC.pListen->value() >= 0.5f;

            const float preamp          = sSC.pPreamp->value();
            const dspu::sidechain_mode_t mode = dspu::sidechain_mode_t(sSC.pMode->value());
            const float react           = sSC.pReactivity->value();

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];
                plug::IPort * const src = ((nChannels > 1) && (bStereoSplit)) ? sSC.pSplitScSource[i] : sSC.pSource;

                c->sSC.set_gain(preamp);
                c->sSC.set_mode(mode);
                c->sSC.set_reactivity(react);
                c->sSC.set_source(decode_sidechain_source(src));
            }
        }

        bool deesser::set_filter_params(dspu::Equalizer * eq, uint32_t index, const dspu::filter_params_t * fp)
        {
            dspu::filter_params_t old_fp;
            if (!eq->get_params(index, &old_fp))
                return false;

            if (old_fp.nType == fp->nType)
            {
                if (fp->nType == dspu::FLT_NONE)
                    return false;

                if ((old_fp.nSlope == fp->nSlope) &&
                    (old_fp.fFreq == fp->fFreq) &&
                    (old_fp.fGain == fp->fGain) &&
                    (old_fp.fQuality == fp->fQuality))
                    return false;
            }

            return eq->set_params(index, fp);
        }

        void deesser::process_band(void *object, void *subject, size_t band, const float *data, size_t sample, size_t count)
        {
//            deesser * const self    = static_cast<deesser *>(object);
            channel_t * const c     = static_cast<channel_t *>(subject);

            // destination: c->vBuffer
            // lo gain: c->vEnvBuffer
            // hi gain: c->vHiBuffer
            // input: data

            if (band == 0)
                dsp::fmadd3(&c->vBuffer[sample], data, &c->vEnvBuffer[sample], count);
            else
                dsp::fmadd3(&c->vBuffer[sample], data, &c->vHiBuffer[sample], count);
        }

        void deesser::update_preeq()
        {
            uint32_t slope;
            dspu::filter_params_t fp;

            // Apply filter changes
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c         = &vChannels[i];
                dspu::Equalizer * const eq  = &c->sSCEq;

                // HPF filter
                slope               = uint32_t(sPreEq.pHpfSlope->value()) << 1;
                fp.nType            = (slope > 0) ? dspu::FLT_BT_BWC_HIPASS : dspu::FLT_NONE;
                fp.nSlope           = slope;
                fp.fFreq            = sPreEq.pHpfFreq->value();
                fp.fFreq2           = fp.fFreq;
                fp.fGain            = GAIN_AMP_0_DB;
                fp.fQuality         = sPreEq.pHpfQ->value();
                if (set_filter_params(eq, SCF_LOWPASS, &fp))
                    sPreEq.nSyncMesh   |= (1 << SCF_LOWPASS);

                // LPF filter
                slope               = uint32_t(sPreEq.pLpfSlope->value()) << 1;
                fp.nType            = (slope > 0) ? dspu::FLT_BT_BWC_LOPASS : dspu::FLT_NONE;
                fp.nSlope           = slope;
                fp.fFreq            = sPreEq.pLpfFreq->value();
                fp.fFreq2           = fp.fFreq;
                fp.fGain            = GAIN_AMP_0_DB;
                fp.fQuality         = sPreEq.pLpfQ->value();
                if (set_filter_params(eq, SCF_HIPASS, &fp))
                    sPreEq.nSyncMesh   |= (1 << SCF_HIPASS);

                // Peak 1 filter
                slope               = uint32_t(sPreEq.pPeak1On->value());
                fp.nType            = (slope > 0) ? dspu::FLT_BT_RLC_BELL: dspu::FLT_NONE;
                fp.nSlope           = 0;
                fp.fFreq            = sPreEq.pPeak1Freq->value();
                fp.fFreq2           = fp.fFreq;
                fp.fGain            = sPreEq.pPeak1Gain->value();
                fp.fQuality         = sPreEq.pPeak1Q->value();
                if (set_filter_params(eq, SCF_PEAK1, &fp))
                    sPreEq.nSyncMesh   |= (1 << SCF_PEAK1);

                // Peak 2 filter
                slope               = uint32_t(sPreEq.pPeak2On->value());
                fp.nType            = (slope > 0) ? dspu::FLT_BT_RLC_BELL: dspu::FLT_NONE;
                fp.nSlope           = 0;
                fp.fFreq            = sPreEq.pPeak2Freq->value();
                fp.fFreq2           = fp.fFreq;
                fp.fGain            = sPreEq.pPeak2Gain->value();
                fp.fQuality         = sPreEq.pPeak2Q->value();
                if (set_filter_params(eq, SCF_PEAK2, &fp))
                    sPreEq.nSyncMesh   |= (1 << SCF_PEAK2);
            }

            if (sPreEq.nSyncMesh & SCM_ALL_FILTERS)
                sPreEq.nSyncMesh       |= SCM_CURVE;
            if (sPreEq.nSyncMesh)
                sPreEq.nSyncMesh       |= SCM_OUT_MESH;
        }

        void deesser::update_xover()
        {
            const uint32_t mode     = uint32_t(sXOver.pMode->value());
            const uint32_t slope    = uint32_t(sXOver.pSlope->value());
            const float freq        = sXOver.pFreq->value();
            sXOver.fLink            = sXOver.pLink->value() * 0.01f;
            if ((mode == sXOver.nMode) &&
                (slope == sXOver.nSlope) &&
                (freq == sXOver.fFreq))
                return;

            const bool mode_changed = (mode != sXOver.nMode);

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c         = &vChannels[i];

                switch (mode)
                {
                    case XOVER_CLASSIC:
                    {
                        dspu::Crossover * const xc     = &c->sXOver;

                        // Upate crossover parameters
                        size_t xslope;
                        switch (slope)
                        {
                            case meta::deesser::SLOPE_6DBO:     xslope = dspu::CROSS_SLOPE_6DBO; break;
                            case meta::deesser::SLOPE_18DBO:    xslope = dspu::CROSS_SLOPE_18DBO; break;
                            case meta::deesser::SLOPE_24DBO:    xslope = dspu::CROSS_SLOPE_24DBO; break;
                            case meta::deesser::SLOPE_48DBO:    xslope = dspu::CROSS_SLOPE_48DBO; break;
                            case meta::deesser::SLOPE_12DBO:
                            default:
                                xslope = dspu::CROSS_SLOPE_12DBO;
                                break;
                        }

                        xc->set_frequency(0, freq);
                        xc->set_slope(0, xslope);

                        // Reconfigure the crossover if needed
                        if (mode_changed)
                            xc->reset();
                        if (xc->needs_reconfiguration())
                            xc->reconfigure();

                        // Update curve graphs
                        xc->freq_chart(0, sXOver.vLoBand, sAnalysis.vFreqs, meta::deesser::FFT_MESH_POINTS);
                        xc->freq_chart(1, sXOver.vHiBand, sAnalysis.vFreqs, meta::deesser::FFT_MESH_POINTS);
                    }
                    break;

                    case XOVER_MODERN:
                    {
                        const uint32_t f_base           = i * 2;
                        dspu::filter_params_t fp;

                        // Clear internal memory if state has changed
                        if (mode_changed)
                            sFilters.reset();

                        uint32_t xlsf, xhsf, xslope;

                        switch (slope)
                        {
                            case meta::deesser::SLOPE_6DBO:
                                xlsf        = dspu::FLT_BT_RLC_LOSHELF;
                                xhsf        = dspu::FLT_BT_RLC_HISHELF;
                                xslope      = 1;
                                break;
                            case meta::deesser::SLOPE_18DBO:
                                xlsf        = dspu::FLT_BT_RLC_LOSHELF;
                                xhsf        = dspu::FLT_BT_RLC_HISHELF;
                                xslope      = 3;
                                break;
                            case meta::deesser::SLOPE_24DBO:
                                xlsf        = dspu::FLT_BT_BWC_LOSHELF;
                                xhsf        = dspu::FLT_BT_BWC_HISHELF;
                                xslope      = 2;
                                break;
                            case meta::deesser::SLOPE_48DBO:
                                xlsf        = dspu::FLT_BT_BWC_LOSHELF;
                                xhsf        = dspu::FLT_BT_BWC_HISHELF;
                                xslope      = 4;
                                break;
                            case meta::deesser::SLOPE_12DBO:
                            default:
                                xlsf        = dspu::FLT_BT_BWC_LOSHELF;
                                xhsf        = dspu::FLT_BT_BWC_HISHELF;
                                xslope      = 1;
                                break;
                        }

                        // Update filter parameters
                        fp.nType                        = xlsf;
                        fp.nSlope                       = xslope;
                        fp.fFreq                        = freq;
                        fp.fFreq2                       = freq;
                        fp.fGain                        = GAIN_AMP_0_DB;
                        fp.fQuality                     = 0.0f;
                        sFilters.set_params(f_base + 0, &fp);

                        fp.nType                        = xhsf;
                        sFilters.set_params(f_base + 1, &fp);
                    }
                    break;

                    case XOVER_LINEAR_PHASE:
                    {
                        dspu::LPCrossover * const xf    = &c->sLPXOver;

                        // Upate crossover parameters
                        float xslope;
                        switch (slope)
                        {
                            case meta::deesser::SLOPE_6DBO:     xslope = -6.0f; break;
                            case meta::deesser::SLOPE_18DBO:    xslope = -18.0f; break;
                            case meta::deesser::SLOPE_24DBO:    xslope = -24.0f; break;
                            case meta::deesser::SLOPE_48DBO:    xslope = -48.0f; break;
                            case meta::deesser::SLOPE_12DBO:
                            default:
                                xslope = -12.0f;
                                break;
                        }

                        xf->set_frequency(0, freq);
                        xf->set_slope(0, xslope);

                        // Reconfigure the crossover if needed
                        if (mode_changed)
                            xf->reset();
                        if (xf->needs_update())
                            xf->update_settings();

                        // Update curve graphs
                        xf->freq_chart(0, sXOver.vLoBand, sAnalysis.vFreqs, meta::deesser::FFT_MESH_POINTS);
                        xf->freq_chart(1, sXOver.vHiBand, sAnalysis.vFreqs, meta::deesser::FFT_MESH_POINTS);
                    }
                    break;

                    default:
                        break;
                }
            } // for

            // Update crossover parameters
            sXOver.nMode        = mode;
            sXOver.nSlope       = slope;
            sXOver.fFreq        = freq;
        }

        void deesser::update_reduction()
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                dspu::Compressor * const comp = &vChannels[i].sCompressor;

                const float threshold = comp->attack_threshold();
                const float ratio = comp->ratio();
                const float knee = comp->knee();

                comp->set_threshold(
                    sReduction.pThreshold->value(),
                    GAIN_AMP_M_INF_DB);
                comp->set_timings(
                    sReduction.pAttack->value(),
                    sReduction.pRelease->value());
                comp->set_hold(sReduction.pHold->value());
                comp->set_ratio(sReduction.pRatio->value());
                comp->set_knee(sReduction.pKnee->value());

                if (comp->modified())
                {
                    comp->update_settings();

                    // Obtain the curve data if compressor's curve has changed
                    if ((i == 0) &&
                        ((threshold != comp->attack_threshold()) ||
                        (ratio != comp->ratio()) ||
                        (knee != comp->knee())))
                    {
                        comp->curve(sReduction.vCurve, sReduction.vPoints, meta::deesser::CURVE_MESH_POINTS);
                        sReduction.bSync    = true;
                    }
                }
            }
        }

        void deesser::update_analyzer()
        {
            // Update analyzer parameters
            sAnalyzer.set_reactivity(sAnalysis.pReactivity->value());
            if (sAnalysis.pShiftGain != NULL)
                sAnalyzer.set_shift(sAnalysis.pShiftGain->value() * 100.0f);
//            sAnalyzer.set_activity(active_channels > 0);

            for (size_t i=0; i<CH_TOTAL * nChannels; ++i)
            {
                plug::IPort * const sw  = sAnalysis.pOn[i];
                const bool on = (sw != NULL) ? sw->value() >= 0.5f : false;
                sAnalyzer.enable_channel(i, on);
            }

            // Update analyzer
            if (sAnalyzer.needs_reconfiguration())
            {
                sAnalyzer.reconfigure();
                sAnalyzer.get_frequencies(
                    sAnalysis.vFreqs, sAnalysis.vIndexes,
                    SPEC_FREQ_MIN, SPEC_FREQ_MAX,
                    meta::deesser::FFT_MESH_POINTS);
            }
        }

        void deesser::update_latency()
        {
            size_t latency      = 0;
            size_t in_delay     = 0;
            size_t sc_delay     = 0;

            // Compute the latency depending on the mode
            if (sXOver.nMode == XOVER_LINEAR_PHASE)
            {
                const size_t xover_latency  = vChannels[0].sLPXOver.latency();
                if (xover_latency >= sSC.nLookahead)
                {
                    latency         = xover_latency;
                    sc_delay        = xover_latency - sSC.nLookahead;
                }
                else
                {
                    latency         = xover_latency + sSC.nLookahead;
                    sc_delay        = xover_latency;
                }
            }
            else
            {
                in_delay        = sSC.nLookahead;
                latency         = in_delay;
            }

            // Apply latency to delay lines
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c = &vChannels[i];
                const size_t a_base = i * CH_TOTAL;

                c->sDryDelay.set_delay(latency);
                c->sInDelay.set_delay(in_delay);
                c->sScDelay.set_delay(sc_delay);

                sAnalyzer.set_channel_delay(a_base + CH_INPUT, latency);
            }

            lsp_trace("latency=%d, sc_delay=%d, in_delay=%d", int(latency), int(sc_delay), int(in_delay));

            set_latency(latency);
        }

        void deesser::update_settings()
        {
            update_common();
            update_premix();
            update_sidechain();
            update_analyzer();
            update_preeq();
            update_xover();
            update_reduction();
            update_latency();
        }

        void deesser::premix_channel(uint32_t channel, size_t count)
        {
            // Get pointers to buffers and advance position
            channel_t * const c     = &vChannels[channel];
            float * const in_buf    = sPremix.vIn[channel];
            float * const out_buf   = sPremix.vOut[channel];
            float * const sc_buf    = sPremix.vSc[channel];
            float * const link_buf  = sPremix.vLink[channel];

            c->vRawIn               = in_buf;
            c->vIn                  = in_buf;
            c->vOut                 = out_buf;
            c->vScIn                = sc_buf;
            c->vShmIn               = link_buf;

            // Update pointers
            sPremix.vIn[channel]   += count;
            sPremix.vOut[channel]  += count;
            if (sPremix.vSc[channel] != NULL)
                sPremix.vSc[channel]   += count;
            if (sPremix.vLink[channel] != NULL)
                sPremix.vLink[channel] += count;

            // Perform transformation
            if (bSidechain)
            {
                // (Sc, Link) -> In
                if ((sc_buf != NULL) && (sPremix.fScToIn > GAIN_AMP_M_INF_DB))
                {
                    c->vIn              = sPremix.vTmpIn[channel];
                    dsp::fmadd_k4(c->vIn, in_buf, sc_buf, sPremix.fScToIn, count);

                    if ((link_buf != NULL) && (sPremix.fLinkToIn > GAIN_AMP_M_INF_DB))
                        dsp::fmadd_k3(c->vIn, link_buf, sPremix.fLinkToIn, count);
                }
                else if ((link_buf != NULL) && (sPremix.fLinkToIn > GAIN_AMP_M_INF_DB))
                {
                    c->vIn              = sPremix.vTmpIn[channel];
                    dsp::fmadd_k4(c->vIn, in_buf, link_buf, sPremix.fLinkToIn, count);
                }

                // (In, Link) -> Sc
                if (sPremix.fInToSc > GAIN_AMP_M_INF_DB)
                {
                    c->vScIn            = sPremix.vTmpSc[channel];
                    if (sc_buf != NULL)
                        dsp::fmadd_k4(c->vScIn, sc_buf, in_buf, sPremix.fInToSc, count);
                    else
                        dsp::mul_k3(c->vScIn, in_buf, sPremix.fInToSc, count);

                    if ((link_buf != NULL) && (sPremix.fLinkToSc > GAIN_AMP_M_INF_DB))
                        dsp::fmadd_k3(c->vScIn, link_buf, sPremix.fLinkToSc, count);
                }
                else if ((link_buf != NULL) && (sPremix.fLinkToSc > GAIN_AMP_M_INF_DB))
                {
                    c->vScIn            = sPremix.vTmpSc[channel];
                    if (sc_buf != NULL)
                        dsp::fmadd_k4(c->vScIn, sc_buf, link_buf, sPremix.fLinkToSc, count);
                    else
                        dsp::mul_k3(c->vScIn, link_buf, sPremix.fLinkToSc, count);
                }

                // (In, Sc) -> Link
                if (sPremix.fInToLink > GAIN_AMP_M_INF_DB)
                {
                    c->vShmIn           = sPremix.vTmpLink[channel];
                    if (link_buf != NULL)
                        dsp::fmadd_k4(c->vShmIn, link_buf, in_buf, sPremix.fInToLink, count);
                    else
                        dsp::mul_k3(c->vShmIn, in_buf, sPremix.fInToLink, count);

                    if ((sc_buf != NULL) && (sPremix.fScToLink > GAIN_AMP_M_INF_DB))
                        dsp::fmadd_k3(c->vShmIn, sc_buf, sPremix.fScToLink, count);
                }
                else if ((sc_buf != NULL) && (sPremix.fScToLink > GAIN_AMP_M_INF_DB))
                {
                    c->vShmIn           = sPremix.vTmpLink[channel];
                    if (link_buf != NULL)
                        dsp::fmadd_k4(c->vShmIn, link_buf, sc_buf, sPremix.fScToLink, count);
                    else
                        dsp::mul_k3(c->vShmIn, sc_buf, sPremix.fScToLink, count);
                }
            }
            else
            {
                // Link -> (In, Sc)
                if (link_buf != NULL)
                {
                    // Link -> In
                    if (sPremix.fLinkToIn > GAIN_AMP_M_INF_DB)
                    {
                        c->vIn          = sPremix.vTmpIn[channel];
                        dsp::fmadd_k4(c->vIn, in_buf, link_buf, sPremix.fLinkToIn, count);
                    }
                    // Link -> Sc
                    if (sPremix.fLinkToSc > GAIN_AMP_M_INF_DB)
                    {
                        c->vScIn        = sPremix.vTmpSc[channel];
                        if (sc_buf != NULL)
                            dsp::fmadd_k4(c->vScIn, sc_buf, link_buf, sPremix.fLinkToSc, count);
                        else
                            dsp::mul_k3(c->vScIn, link_buf, sPremix.fLinkToSc, count);
                    }
                }

                // In -> Link
                if (sPremix.fInToLink > GAIN_AMP_M_INF_DB)
                {
                    c->vShmIn       = sPremix.vTmpLink[channel];
                    if (link_buf != NULL)
                        dsp::fmadd_k4(c->vShmIn, link_buf, in_buf, sPremix.fInToLink, count);
                    else
                        dsp::mul_k3(c->vShmIn, in_buf, sPremix.fInToLink, count);
                }
            }
        }

        void deesser::bind_input_channels()
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c = &vChannels[i];
                core::AudioBuffer * const shm_buf   = (c->pShmIn != NULL) ? c->pShmIn->buffer<core::AudioBuffer>() : NULL;

                sPremix.vIn[i]      = c->pIn->buffer<float>();
                sPremix.vOut[i]     = c->pOut->buffer<float>();
                sPremix.vSc[i]      = (c->pScIn != NULL) ? c->pScIn->buffer<float>() : sPremix.vIn[i];
                sPremix.vLink[i]    = ((shm_buf != NULL) && (shm_buf->active())) ? shm_buf->buffer() : NULL;
            }
        }

        void deesser::output_preeq_meshes()
        {
            if (sPreEq.nSyncMesh == 0)
                return;

            plug::mesh_t * const mesh   = sPreEq.pMesh->buffer<plug::mesh_t>();
            if ((mesh == NULL) || (!mesh->isEmpty()))
                return;

            // Ready to sync
            size_t idx      = 0;

            // Fill frequencies
            float *p        = mesh->pvData[idx++];
            dsp::copy(&p[2], sAnalysis.vFreqs, meta::deesser::FFT_MESH_POINTS);
            p[0]            = SPEC_FREQ_MIN * 0.5f;
            p[1]            = p[0];
            p              += meta::deesser::FFT_MESH_POINTS + 2;
            p[0]            = SPEC_FREQ_MAX * 2.0f;
            p[1]            = p[0];

            // Fill filters
            dspu::Equalizer * const eq  = &vChannels[0].sSCEq;
            for (size_t i=0; i<=SCF_TOTAL; ++i)
            {
                // Need to update frequency?
                p               = mesh->pvData[idx++];
                if (sPreEq.nSyncMesh & (1 << i))
                {
                    if (i < SCF_TOTAL)
                    {
                        eq->freq_chart(i, vBuffer, sAnalysis.vFreqs, meta::deesser::FFT_MESH_POINTS);
                        dsp::pcomplex_mod(sPreEq.vMeshData[i], vBuffer, meta::deesser::FFT_MESH_POINTS);
                    }
                    else
                    {
                        dsp::mul3(sPreEq.vMeshData[i], sPreEq.vMeshData[0], sPreEq.vMeshData[1], meta::deesser::FFT_MESH_POINTS);
                        for (size_t j=2; j<SCF_TOTAL; ++j)
                            dsp::mul2(sPreEq.vMeshData[i], sPreEq.vMeshData[j], meta::deesser::FFT_MESH_POINTS);
                    }
                }

                // Store data to mesh
                dsp::copy(&p[2], sPreEq.vMeshData[i], meta::deesser::FFT_MESH_POINTS);
                p[0]            = GAIN_AMP_0_DB;
                p[1]            = p[2];
                p              += meta::deesser::FFT_MESH_POINTS + 2;
                p[0]            = p[-1];
                p[1]            = GAIN_AMP_0_DB;
            }
            mesh->data(idx, meta::deesser::FFT_MESH_POINTS + 4);

            // Cleanup sync flag
            sPreEq.nSyncMesh = 0;
        }

        void deesser::output_xover_meshes()
        {
            // Obtain the mesh of the crossover
            plug::mesh_t * const mesh   = sXOver.pMesh->buffer<plug::mesh_t>();
            if ((mesh == NULL) || (!mesh->isEmpty()))
                return;

            // Ready to sync
            size_t idx      = 0;

            // Fill frequencies
            float *p        = mesh->pvData[idx++];
            dsp::copy(&p[2], sAnalysis.vFreqs, meta::deesser::FFT_MESH_POINTS);
            p[0]            = SPEC_FREQ_MIN * 0.5f;
            p[1]            = p[0];
            p              += meta::deesser::FFT_MESH_POINTS + 2;
            p[0]            = SPEC_FREQ_MAX * 2.0f;
            p[1]            = p[0];

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *const c  = &vChannels[i];
                p                   = mesh->pvData[idx++];

                switch (sXOver.nMode)
                {
                    case XOVER_CLASSIC:
                        dsp::mix_copy2(
                            vBuffer,
                            sXOver.vLoBand, sXOver.vHiBand,
                            c->fLoGain, c->fHiGain,
                            meta::deesser::FFT_MESH_POINTS * 2);
                        dsp::pcomplex_mod(&p[2], vBuffer, meta::deesser::FFT_MESH_POINTS);
                        break;
                    case XOVER_LINEAR_PHASE:
                        dsp::mix_copy2(
                            &p[2],
                            sXOver.vLoBand, sXOver.vHiBand,
                            c->fLoGain, c->fHiGain,
                            meta::deesser::FFT_MESH_POINTS);
                        break;
                    case XOVER_MODERN:
                    {
                        const size_t f_base     = i * 2;
                        sFilters.freq_chart(f_base + 0, vBuffer, sAnalysis.vFreqs, c->fLoGain, meta::deesser::FFT_MESH_POINTS);
                        sFilters.freq_chart(f_base + 1, c->vBuffer, sAnalysis.vFreqs, c->fHiGain, meta::deesser::FFT_MESH_POINTS);
                        dsp::pcomplex_mul2(vBuffer, c->vBuffer, meta::deesser::FFT_MESH_POINTS);
                        dsp::pcomplex_mod(&p[2], vBuffer, meta::deesser::FFT_MESH_POINTS);
                        break;
                    }

                    case XOVER_NONE:
                    default:
                        dsp::fill(&p[2], c->fLoGain, meta::deesser::FFT_MESH_POINTS);
                        break;
                }

                // Store data to mesh
                p[0]            = GAIN_AMP_0_DB;
                p[1]            = p[2];
                p              += meta::deesser::FFT_MESH_POINTS + 2;
                p[0]            = p[-1];
                p[1]            = GAIN_AMP_0_DB;
            }

            mesh->data(idx, meta::deesser::FFT_MESH_POINTS + 4);
        }

        void deesser::output_reduction_meshes()
        {
            if (!sReduction.bSync)
                return;

            plug::mesh_t * const mesh   = sReduction.pMesh->buffer<plug::mesh_t>();
            if ((mesh == NULL) || (!mesh->isEmpty()))
                return;

            // Fill data
            dsp::copy(mesh->pvData[0], sReduction.vPoints, meta::deesser::CURVE_MESH_POINTS);
            dsp::copy(mesh->pvData[1], sReduction.vCurve, meta::deesser::CURVE_MESH_POINTS);
            mesh->data(2, meta::deesser::CURVE_MESH_POINTS);

            // Cleanup sync flag
            sReduction.bSync    = 0;
        }

        void deesser::output_analysis_meshes()
        {
            // Obtain the mesh of the crossover
            plug::mesh_t * const mesh   = sAnalysis.pMesh->buffer<plug::mesh_t>();
            if ((mesh == NULL) || (!mesh->isEmpty()))
                return;

            size_t idx      = 0;

            // Fill frequencies
            float *p        = mesh->pvData[idx++];
            dsp::copy(&p[2], sAnalysis.vFreqs, meta::deesser::FFT_MESH_POINTS);
            p[0]            = SPEC_FREQ_MIN * 0.5f;
            p[1]            = p[0];
            p              += meta::deesser::FFT_MESH_POINTS + 2;
            p[0]            = SPEC_FREQ_MAX * 2.0f;
            p[1]            = p[0];

            for (size_t i=0; i<nChannels * CH_TOTAL; ++i)
            {
                p               = mesh->pvData[idx++];

                sAnalyzer.get_spectrum(i, &p[2], sAnalysis.vIndexes, meta::deesser::FFT_MESH_POINTS);

                // Store data to mesh
                p[0]            = GAIN_AMP_M_INF_DB;
                p[1]            = p[2];
                p              += meta::deesser::FFT_MESH_POINTS + 2;
                p[0]            = p[-1];
                p[1]            = GAIN_AMP_M_INF_DB;
            }

            mesh->data(idx, meta::deesser::FFT_MESH_POINTS + 4);
        }

        void deesser::output_meters()
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];

                // Commit compressor meters
                const float env         = sReduction.fEnv[i];
                const float red         = c->sCompressor.reduction(env);
                const float curve       = c->sCompressor.curve(env);

                sReduction.fOutEnv[i]   = env;
                sReduction.fOutGain[i]  = curve;

                sReduction.pEnv[i]->set_value(env);
                sReduction.pRed[i]->set_value(red);
                sReduction.pCurve[i]->set_value(curve);

                c->pMeterIn->set_value(c->fMeterIn);
                c->pMeterOut->set_value(c->fMeterOut);
            }
        }

        inline float *deesser::select_buffer(channel_t & c)
        {
            switch (sSC.nType)
            {
                case SCT_EXTERNAL: return (c.vScIn != NULL) ? c.vScIn : vEmptyBuffer;
                case SCT_LINK: return (c.vShmIn != NULL) ? c.vShmIn : vEmptyBuffer;
                default: break;
            }

            return c.vIn;
        }

        void deesser::clear_meters()
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];

                sReduction.fEnv[i]      = GAIN_AMP_M_INF_DB;
                c->fHiGain              = GAIN_AMP_0_DB;
                c->fLoGain              = GAIN_AMP_0_DB;
                c->fMeterIn             = GAIN_AMP_M_INF_DB;
                c->fMeterOut            = GAIN_AMP_M_INF_DB;
            }
        }

        void deesser::process(size_t samples)
        {
            bind_input_channels();
            clear_meters();

            float *sc_in[2];

            // Do processing
            for (size_t offset = 0; offset < samples; )
            {
                // Determine buffer size for processing
                const size_t to_process = lsp_min(BUFFER_SIZE, samples - offset);

                // Pre-mix, apply gain and measure input signal level
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t * const c     = &vChannels[i];

                    // Pre-mix data
                    premix_channel(i, to_process);

                    // Apply input gain and measure input level
                    if (fInGain != GAIN_AMP_0_DB)
                    {
                        dsp::mul_k3(sPremix.vTmpIn[i], c->vIn, fInGain, to_process);
                        c->vIn                  = sPremix.vTmpIn[i];
                    }
                    c->fMeterIn             = lsp_max(c->fMeterIn, dsp::abs_max(c->vIn, to_process));

                    // Select input buffer for the sidechain
                    float * const src       = select_buffer(vChannels[i]);
                    c->sSCEq.process(c->vScBuffer, src, to_process);
                    sc_in[i]                = c->vScBuffer;
                }

                // Apply sidechain
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t * const c     = &vChannels[i];
                    c->sSC.process(c->vBuffer, sc_in, to_process);
//                    const float level   = dsp::abs_max(c->vIn, to_process) * fInGain;
//                    c->pInLvl->set_value(level);
                }

                // Apply stereo linking between sidechain buffers
                if ((nChannels > 1) && (fStereoLink > 0.0f))
                {
                    dsp::lr_to_mid(
                        vBuffer,
                        vChannels[0].vBuffer, vChannels[1].vBuffer,
                        to_process);
                    dsp::mix2(
                        vChannels[0].vBuffer, vBuffer,
                        1.0f - fStereoLink, fStereoLink,
                        to_process);
                    dsp::mix2(
                        vChannels[1].vBuffer, vBuffer,
                        1.0f - fStereoLink, fStereoLink,
                        to_process);
                }

                // Compute compression gain and envelope
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t * const c     = &vChannels[i];
                    c->sCompressor.process(c->vBuffer, c->vEnvBuffer, c->vBuffer, to_process);
                    sReduction.fEnv[i]      = lsp_max(sReduction.fEnv[i], dsp::max(c->vEnvBuffer, to_process));
                }

                // Do main logic
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t * const c     = &vChannels[i];
                    const size_t a_base     = i * CH_TOTAL;

                    sAnalysis.vIn[a_base + CH_INPUT]    = c->vIn;
                    sAnalysis.vIn[a_base + CH_SIDECHAIN]= sc_in[i];
                    sAnalysis.vIn[a_base + CH_OUTPUT]   = c->vBuffer;

                    // Apply processing
                    process_xover(i, to_process);
                    c->sDryDelay.process(vBuffer, c->vRawIn, to_process);
                    dsp::mul_k2(c->vBuffer, fOutGain, to_process);

                    // Measure output level and apply bypass
                    c->fMeterOut            = lsp_max(c->fMeterOut, dsp::abs_max(c->vBuffer, to_process));
                    const float * const src = (sSC.bListen) ? sc_in[i] : c->vBuffer;
                    c->sBypass.process(c->vOut, vBuffer, src, to_process);
                }

                // Perform analysis
                sAnalyzer.process(sAnalysis.vIn, to_process);

                offset     += to_process;
            }

            output_preeq_meshes();
            output_xover_meshes();
            output_reduction_meshes();
            output_analysis_meshes();
            output_meters();

            // Request inline display for redraw
            if (pWrapper != NULL)
                pWrapper->query_display_draw();
        }

        void deesser::process_xover(size_t id, size_t samples)
        {
            channel_t * const c = &vChannels[id];

            // Apply delay to the sidechain (control) signal and input signal
            c->sScDelay.process(c->vHiBuffer, c->vBuffer, samples);
            dsp::lerp_kvk(c->vEnvBuffer, GAIN_AMP_0_DB, c->vHiBuffer, sXOver.fLink, samples);
            c->sInDelay.process(vBuffer, c->vIn, samples);

            c->fHiGain      = lsp_min(c->fHiGain, dsp::min(c->vHiBuffer, samples));

            switch (sXOver.nMode)
            {
                case XOVER_CLASSIC:
                    c->fLoGain      = GAIN_AMP_0_DB + (c->fHiGain - GAIN_AMP_0_DB) * sXOver.fLink;

                    dsp::fill_zero(c->vBuffer, samples);
                    c->sXOver.process(vBuffer, samples);
                    break;

                case XOVER_LINEAR_PHASE:
                    c->fLoGain      = GAIN_AMP_0_DB + (c->fHiGain - GAIN_AMP_0_DB) * sXOver.fLink;

                    dsp::fill_zero(c->vBuffer, samples);
                    c->sLPXOver.process(vBuffer, samples);
                    break;

                case XOVER_MODERN:
                {
                    c->fLoGain      = GAIN_AMP_0_DB + (c->fHiGain - GAIN_AMP_0_DB) * sXOver.fLink;

                    const size_t flt_base = id * 2;
                    sFilters.process(flt_base + 1, vBuffer, vBuffer, c->vHiBuffer, samples);        // Hi-shelving filter
                    sFilters.process(flt_base + 0, c->vBuffer, vBuffer, c->vEnvBuffer, samples);    // Lo-shelving filter
                    break;
                }

                case XOVER_NONE:
                default:
                    c->fLoGain      = c->fHiGain;
                    dsp::mul3(c->vBuffer, vBuffer, c->vHiBuffer, samples);
                    break;
            }
        }

        void deesser::ui_activated()
        {
            sAnalyzer.set_activity(true);
            sPreEq.nSyncMesh   |= SCM_OUT_MESH;
            sReduction.bSync    = true;
        }

        void deesser::ui_deactivated()
        {
            sAnalyzer.set_activity(false);
        }

        bool deesser::inline_display(plug::ICanvas *cv, size_t width, size_t height)
        {
            // Check proportions
            if (height > width)
                height  = width;

            // Init canvas
            if (!cv->init(width, height))
                return false;
            width               = cv->width();
            height              = cv->height();

            // Clear background
            const bool bypassing = vChannels[0].sBypass.bypassing();
            cv->set_color_rgb((bypassing) ? CV_DISABLED : CV_BACKGROUND);
            cv->paint();

            const float zx      = 1.0f/GAIN_AMP_M_72_DB;
            const float zy      = 1.0f/GAIN_AMP_M_72_DB;
            const float dx      = width/(logf(GAIN_AMP_P_24_DB / GAIN_AMP_M_72_DB));
            const float dy      = height/(logf(GAIN_AMP_M_72_DB / GAIN_AMP_P_24_DB));

            // Draw horizontal and vertical lines
            cv->set_line_width(1.0);
            cv->set_color_rgb((bypassing) ? CV_SILVER: CV_YELLOW, 0.5f);
            for (float i=GAIN_AMP_M_72_DB; i<GAIN_AMP_P_24_DB; i *= GAIN_AMP_P_24_DB)
            {
                float ax = dx*(logf(i*zx));
                float ay = height + dy*(logf(i*zy));
                cv->line(ax, 0, ax, height);
                cv->line(0, ay, width, ay);
            }

            // Draw 1:1 line
            cv->set_line_width(2.0);
            cv->set_color_rgb(CV_GRAY);
            {
                float ax1 = dx*(logf(GAIN_AMP_M_72_DB*zx));
                float ax2 = dx*(logf(GAIN_AMP_P_24_DB*zx));
                float ay1 = height + dy*(logf(GAIN_AMP_M_72_DB*zy));
                float ay2 = height + dy*(logf(GAIN_AMP_P_24_DB*zy));
                cv->line(ax1, ay1, ax2, ay2);
            }

            // Draw axis
            cv->set_color_rgb((bypassing) ? CV_SILVER : CV_WHITE);
            {
                float ax = dx*(logf(GAIN_AMP_0_DB*zx));
                float ay = height + dy*(logf(GAIN_AMP_0_DB*zy));
                cv->line(ax, 0, ax, height);
                cv->line(0, ay, width, ay);
            }

            // Reuse display
            pIDisplay           = core::IDBuffer::reuse(pIDisplay, 4, width);
            core::IDBuffer *b   = pIDisplay;
            if (b == NULL)
                return false;

            const bool aa = cv->set_anti_aliasing(true);
            lsp_finally { cv->set_anti_aliasing(aa); };
            cv->set_line_width(2);

            // Colors
            static const uint32_t c_colors[] =
            {
                CV_MIDDLE_CHANNEL, CV_LEFT_CHANNEL, CV_RIGHT_CHANNEL
            };

            // Prepare mesh data and draw mesh
            for (size_t j=0; j<width; ++j)
            {
                const size_t k      = (j*meta::deesser::CURVE_MESH_POINTS)/width;
                b->v[0][j]          = sReduction.vPoints[k];
                b->v[1][j]          = sReduction.vCurve[k];
            }

            dsp::fill(b->v[2], 0.0f, width);
            dsp::fill(b->v[3], height, width);
            dsp::axis_apply_log1(b->v[2], b->v[0], zx, dx, width);
            dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width);

            cv->set_color_rgb((bypassing || !(active())) ? CV_SILVER : CV_MIDDLE_CHANNEL);
            cv->draw_lines(b->v[2], b->v[3], width);

            // Draw dot
            if (active())
            {
                const size_t channels       = ((nChannels > 1) && (!bStereoSplit)) ? 2 : 1;
                const uint32_t * const vd   = (channels == 1) ? &c_colors[0] : &c_colors[1];

                for (size_t i=0; i<channels; ++i)
                {
                    uint32_t color  = (bypassing) ? CV_SILVER : vd[i];
                    Color c1(color), c2(color);
                    c2.alpha(0.9);

                    float ax = dx*(logf(sReduction.fOutEnv[i]*zx));
                    float ay = height + dy*(logf(sReduction.fOutGain[i]*zy));

                    cv->radial_gradient(ax, ay, c1, c2, 12);
                    cv->set_color_rgb(0);
                    cv->circle(ax, ay, 4);
                    cv->set_color_rgb(color);
                    cv->circle(ax, ay, 3);
                }
            }

            return true;
        }

        void deesser::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            v->write("nChannels", nChannels);
            v->begin_array("vChannels", vChannels, nChannels);
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c         = &vChannels[i];

                v->begin_object(c, sizeof(channel_t));
                {
                    v->write_object("sBypass", &c->sBypass);
                    v->write_object("sSC", &c->sSC);
                    v->write_object("sSCEq", &c->sSCEq);
                    v->write_object("sXOver", &c->sXOver);
                    v->write_object("sLPXOver", &c->sLPXOver);
                    v->write_object("sCompressor", &c->sCompressor);
                    v->write_object("sDryDelay", &c->sDryDelay);
                    v->write_object("sInDelay", &c->sInDelay);
                    v->write_object("sScDelay", &c->sScDelay);

                    v->write("vRawIn", c->vRawIn);
                    v->write("vIn", c->vIn);
                    v->write("vOut", c->vOut);
                    v->write("vScIn", c->vScIn);
                    v->write("vShmIn", c->vShmIn);

                    v->write("vScBuffer", c->vScBuffer);
                    v->write("vEnvBuffer", c->vEnvBuffer);
                    v->write("vHiBuffer", c->vHiBuffer);
                    v->write("vBuffer", c->vBuffer);

                    v->write("fLoGain", c->fLoGain);
                    v->write("fHiGain", c->fHiGain);
                    v->write("fMeterIn", c->fMeterIn);
                    v->write("fMeterOut", c->fMeterOut);

                    v->write("pIn", c->pIn);
                    v->write("pOut", c->pOut);
                    v->write("pScIn", c->pScIn);
                    v->write("pShmIn", c->pShmIn);
                    v->write("pMeterIn", c->pMeterIn);
                    v->write("pMeterOut", c->pMeterOut);
                }
                v->end_object();
            }
            v->end_array();

            v->write("vEmptyBuffer", vEmptyBuffer);
            v->write("vBuffer", vBuffer);

            v->write("fStereoLink", fStereoLink);
            v->write("fInGain", fInGain);
            v->write("fOutGain", fOutGain);
            v->write("bSidechain", bSidechain);
            v->write("bStereoSplit", bStereoSplit);

            v->write_object("sFilters", &sFilters);
            v->write_object("sAnalyzer", &sAnalyzer);

            v->begin_object("sPremix", &sPremix, sizeof(premix_t));
            {
                v->write("fInToSc", sPremix.fInToSc);
                v->write("fInToLink", sPremix.fInToLink);
                v->write("fLinkToIn", sPremix.fLinkToIn);
                v->write("fLinkToSc", sPremix.fLinkToSc);
                v->write("fScToIn", sPremix.fScToIn);
                v->write("fScToLink", sPremix.fScToLink);

                v->writev("vIn", sPremix.vIn, 2);
                v->writev("vOut", sPremix.vOut, 2);
                v->writev("vSc", sPremix.vSc, 2);
                v->writev("vLink", sPremix.vLink, 2);
                v->writev("vTmpIn", sPremix.vTmpIn, 2);
                v->writev("vTmpLink", sPremix.vTmpLink, 2);
                v->writev("vTmpSc", sPremix.vTmpSc, 2);

                v->write("pInToSc", sPremix.pInToSc);
                v->write("pInToLink", sPremix.pInToLink);
                v->write("pLinkToIn", sPremix.pLinkToIn);
                v->write("pLinkToSc", sPremix.pLinkToSc);
                v->write("pScToIn", sPremix.pScToIn);
                v->write("pScToLink", sPremix.pScToLink);
            }
            v->end_object();

            v->begin_object("sSC", &sSC, sizeof(sidechain_t));
            {
                v->write("nType", sSC.nType);
                v->write("nLookahead", sSC.nLookahead);
                v->write("bListen", sSC.bListen);

                v->write("pType", sSC.pType);
                v->write("pMode", sSC.pMode);
                v->write("pSource", sSC.pSource);
                v->writev("pSplitScSource", sSC.pSplitScSource, 2);
                v->write("pLookahead", sSC.pLookahead);
                v->write("pListen", sSC.pListen);
                v->write("pReactivity", sSC.pReactivity);
                v->write("pPreamp", sSC.pPreamp);
            }
            v->end_object();

            v->begin_object("sAnalysis", &sAnalysis, sizeof(analysis_t));
            {
                v->write("vFreqs", sAnalysis.vFreqs);
                v->write("vIndexes", sAnalysis.vIndexes);

                v->writev("vIn", sAnalysis.vIn, CH_TOTAL*2);

                v->write("pReactivity", sAnalysis.pReactivity);
                v->write("pShiftGain", sAnalysis.pShiftGain);
                v->writev("pOn", sAnalysis.pOn, CH_TOTAL*2);
                v->write("pMesh", sAnalysis.pMesh);
            }
            v->end_object();

            v->begin_object("sXOver", &sXOver, sizeof(crossover_t));
            {
                v->write("nMode", sXOver.nMode);
                v->write("nSlope", sXOver.nSlope);
                v->write("fFreq", sXOver.fFreq);
                v->write("fLink", sXOver.fLink);

                v->write("vLoBand", sXOver.vLoBand);
                v->write("vHiBand", sXOver.vHiBand);

                v->write("pMode", sXOver.pMode);
                v->write("pSlope", sXOver.pSlope);
                v->write("pFreq", sXOver.pFreq);
                v->write("pLink", sXOver.pLink);
                v->write("pMesh", sXOver.pMesh);
            }
            v->end_object();

            v->begin_object("sPreEq", &sPreEq, sizeof(preeq_t));
            {
                v->write("nSyncMesh", sPreEq.nSyncMesh);
                v->writev("vMeshData", sPreEq.vMeshData, SCF_TOTAL+1);

                v->write("pHpfSlope", sPreEq.pHpfSlope);
                v->write("pHpfFreq", sPreEq.pHpfFreq);
                v->write("pHpfQ", sPreEq.pHpfQ);
                v->write("pLpfSlope", sPreEq.pLpfSlope);
                v->write("pLpfFreq", sPreEq.pLpfFreq);
                v->write("pLpfQ", sPreEq.pLpfQ);
                v->write("pPeak1On", sPreEq.pPeak1On);
                v->write("pPeak1Freq", sPreEq.pPeak1Freq);
                v->write("pPeak1Gain", sPreEq.pPeak1Gain);
                v->write("pPeak1Q", sPreEq.pPeak1Q);
                v->write("pPeak2On", sPreEq.pPeak2On);
                v->write("pPeak2Freq", sPreEq.pPeak2Freq);
                v->write("pPeak2Gain", sPreEq.pPeak2Gain);
                v->write("pPeak2Q", sPreEq.pPeak2Q);
                v->write("pMesh", sPreEq.pMesh);
            }
            v->end_object();

            v->begin_object("sReduction", &sReduction, sizeof(reduction_t));
            {
                v->write("bSync", sReduction.bSync);
                v->writev("fEnv", sReduction.fEnv, 2);
                v->writev("fOutEnv", sReduction.fOutEnv, 2);
                v->writev("fOutGain", sReduction.fOutGain, 2);

                v->write("vPoints", sReduction.vPoints);
                v->write("vCurve", sReduction.vCurve);

                v->write("pThreshold", sReduction.pThreshold);
                v->write("pAttack", sReduction.pAttack);
                v->write("pRelease", sReduction.pRelease);
                v->write("pHold", sReduction.pHold);
                v->write("pRatio", sReduction.pRatio);
                v->write("pKnee", sReduction.pKnee);
                v->write("pMesh", sReduction.pMesh);

                v->writev("pEnv", sReduction.pEnv, 2);
                v->writev("pRed", sReduction.pRed, 2);
                v->writev("pCurve", sReduction.pCurve, 2);
            }
            v->end_object();

            v->write("pBypass", pBypass);
            v->write("pGainIn", pGainIn);
            v->write("pGainOut", pGainOut);
            v->write("pStereoSplit", pStereoSplit);
            v->write("pStereoLink", pStereoLink);

            v->write("pIDisplay", pIDisplay);

            v->write("pData", pData);
        }

    } /* namespace plugins */
} /* namespace lsp */


