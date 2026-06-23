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
                freqs_sz * 2 +                  // sXOver.vLoBand + sXOver.vHiBand
                mesh_sz * (SCF_TOTAL + 1) +     // sPreEq.vMeshData
                points_sz * 2 +                 // sReduction.vPoints + sReduction.vCurve
                nChannels * buf_sz * 3 +        // sPremix.vTmpIn + sPremix.vTmpLink + sPremix.vTmpSc
                nChannels * (
                    buf_sz +                    // vChannels.vScBuffer
                    buf_sz +                    // vChannels.vEnvBuffer
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
            sXOver.vLoBand          = advance_ptr_bytes<float>(ptr, freqs_sz);
            sXOver.vHiBand          = advance_ptr_bytes<float>(ptr, freqs_sz);

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
                c->sFFTXOver.construct();
                c->sCompressor.construct();

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
                c->vBuffer              = advance_ptr_bytes<float>(ptr, tmp_buf_sz);
                c->fLoGain              = (i == 0) ? GAIN_AMP_0_DB : GAIN_AMP_M_12_DB;      // DBG
                c->fHiGain              = (i == 0) ? GAIN_AMP_M_24_DB : GAIN_AMP_M_36_DB;   // DBG

                // Initialize fields
                c->vIn                  = NULL;
                c->vOut                 = NULL;
                c->vScIn                = NULL;
                c->vShmIn               = NULL;

                // Initialize ports
                c->pIn                  = NULL;
                c->pOut                 = NULL;
                c->pScIn                = NULL;
                c->pShmIn               = NULL;

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
                    c->sFFTXOver.destroy();
                    c->sCompressor.destroy();
                }
                vChannels   = NULL;
            }

            // Destroy global processors
            sFilters.destroy();
            sAnalyzer.destroy();

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
            const size_t max_latency        = 0; // TODO
            const size_t xover_fft_rank     = select_fft_rank(sr);

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

                // Need to re-initialize FFT crossover?
                if (xover_fft_rank != c->sFFTXOver.rank())
                {
                    c->sFFTXOver.init(xover_fft_rank, 2);
                    for (size_t j=0; j<2; ++j)
                        c->sFFTXOver.set_handler(j, process_band, this, c);
                    c->sFFTXOver.set_rank(xover_fft_rank);
                    c->sFFTXOver.set_phase(float(i) / float(nChannels));

                    // Configure crossover
                    c->sFFTXOver.enable_filters(0, true, false);
                    c->sFFTXOver.enable_filters(1, false, true);
                    c->sFFTXOver.enable_band(0, true);
                    c->sFFTXOver.enable_band(1, true);
                }
                c->sFFTXOver.set_sample_rate(sr);
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
            // TODO
//            deesser * const self    = static_cast<deesser *>(object);
//            channel_t * const c     = static_cast<channel_t *>(subject);
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
            if ((mode == sXOver.nMode) &&
                (slope == sXOver.nSlope) &&
                (freq == sXOver.fFreq))
                return;

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c         = &vChannels[i];

                switch (mode)
                {
                    case XOVER_CLASSIC:
                    {
                        dspu::Crossover * const xc     = &c->sXOver;

                        // Upate crossover parameters
                        xc->set_frequency(0, freq);
                        xc->set_slope(0, dspu::CROSS_SLOPE_LR2 + slope);

                        // Reconfigure the crossover if needed
                        if (xc->needs_reconfiguration())
                            xc->reconfigure();

                        // Update curve graphs
                        xc->freq_chart(0, vBuffer, sAnalysis.vFreqs, meta::deesser::FFT_MESH_POINTS);
                        dsp::pcomplex_mod(sXOver.vLoBand, vBuffer, meta::deesser::FFT_MESH_POINTS);
                        xc->freq_chart(1, vBuffer, sAnalysis.vFreqs, meta::deesser::FFT_MESH_POINTS);
                        dsp::pcomplex_mod(sXOver.vHiBand, vBuffer, meta::deesser::FFT_MESH_POINTS);
                    }
                    break;

                    case XOVER_MODERN:
                    {
                        const uint32_t f_base           = i * 2;
                        dspu::filter_params_t fp;

                        // Update filter parameters
                        fp.nType                        = dspu::FLT_BT_BWC_LOSHELF;
                        fp.nSlope                       = (slope == 0) ? 1 : (slope == 1) ? 2 : 4;
                        fp.fFreq                        = freq;
                        fp.fFreq2                       = freq;
                        fp.fGain                        = GAIN_AMP_0_DB;
                        fp.fQuality                     = 0.0f;
                        sFilters.set_params(f_base + 0, &fp);

                        fp.nType                        = dspu::FLT_BT_BWC_HISHELF;
                        sFilters.set_params(f_base + 1, &fp);
                    }
                    break;

                    case XOVER_LINEAR_PHASE:
                    {
                        dspu::FFTCrossover * const xf   = &c->sFFTXOver;

                        // Upate crossover parameters
                        xf->set_lpf_frequency(0, freq);
                        xf->set_hpf_frequency(1, freq);

                        const float slope_db    = (slope == 0) ? -12.0f : -24.0f * slope;
                        xf->set_lpf_slope(0, slope_db);
                        xf->set_hpf_slope(1, slope_db);

                        // Reconfigure the crossover if needed
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
                        (ratio != comp->ratio())))
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

            size_t active_channels = 0;
            for (size_t i=0; i<CH_TOTAL * nChannels; ++i)
            {
                plug::IPort * const sw  = sAnalysis.pOn[i];
                const bool on = (sw != NULL) ? sw->value() >= 0.5f : false;
                sAnalyzer.enable_channel(i, on);
                if (on)
                    ++active_channels;
            }
            sAnalyzer.set_activity(active_channels > 0);

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

        void deesser::update_settings()
        {
//            const float out_gain    = pGainOut->value();
            const bool bypass       = pBypass->value() >= 0.5f;
            bStereoSplit            = (pStereoSplit != NULL) ? pStereoSplit->value() >= 0.5f : false;
            fStereoLink             = (pStereoLink != NULL) ? pStereoLink->value() * 0.01f : 0.0f;

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];

                c->sBypass.set_bypass(bypass);
            }

            update_premix();
            update_sidechain();
            update_analyzer();
            update_preeq();
            update_xover();
            update_reduction();
        }

        void deesser::premix_channel(uint32_t channel, size_t count)
        {
            // Get pointers to buffers and advance position
            channel_t * const c     = &vChannels[channel];
            float * const in_buf    = sPremix.vIn[channel];
            float * const out_buf   = sPremix.vOut[channel];
            float * const sc_buf    = sPremix.vSc[channel];
            float * const link_buf  = sPremix.vLink[channel];

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
                    case XOVER_LINEAR_PHASE:
                        dsp::mul_k3(&p[2], sXOver.vLoBand, c->fLoGain, meta::deesser::FFT_MESH_POINTS);
                        dsp::fmadd_k3(&p[2], sXOver.vHiBand, c->fHiGain, meta::deesser::FFT_MESH_POINTS);
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

                sReduction.pEnv[i]->set_value(env);
                sReduction.pRed[i]->set_value(red);
                sReduction.pCurve[i]->set_value(curve);
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
                sReduction.fEnv[i]      = GAIN_AMP_M_INF_DB;
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

                // Pre-mix and measure input signal level
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t * const c     = &vChannels[i];

                    premix_channel(i, to_process);

                    float * const src       = select_buffer(vChannels[i]);
                    c->sSCEq.process(c->vScBuffer, src, to_process);
                    sc_in[i]                = c->vScBuffer;
                }

                // Apply sidechain
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t * const c     = &vChannels[i];
                    c->sSC.process(c->vBuffer, const_cast<const float **>(sc_in), to_process);
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


                    dsp::copy(c->vBuffer, c->vIn, to_process);
                    sAnalysis.vIn[a_base + CH_OUTPUT]   = c->vBuffer;

                    c->sBypass.process(c->vOut, c->vIn, c->vBuffer, to_process);
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
        }

        void deesser::ui_activated()
        {
            sPreEq.nSyncMesh   |= SCM_OUT_MESH;
            sReduction.bSync    = true;
        }

        void deesser::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            // TODO: fill parameters

            v->write("nChannels", nChannels);
            v->begin_array("vChannels", vChannels, nChannels);
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c         = &vChannels[i];

                v->begin_object(c, sizeof(channel_t));
                {
                    v->write_object("sBypass", &c->sBypass);

                    v->write("pIn", c->pIn);
                    v->write("pOut", c->pOut);
                    v->write("pScIn", c->pScIn);
                }
                v->end_object();
            }
            v->end_array();

            v->write("vBuffer", vBuffer);

            v->write("pBypass", pBypass);
            v->write("pGainOut", pGainOut);

            v->write("pData", pData);
        }

    } /* namespace plugins */
} /* namespace lsp */


