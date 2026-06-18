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
            nChannels               = 0;
            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_in_port(p))
                    ++nChannels;

            // Initialize other parameters
            vChannels               = NULL;
            vBuffer                 = NULL;
            bSidechain              =
                (strcmp(meta->uid, meta::sc_deesser_mono.uid) == 0) ||
                (strcmp(meta->uid, meta::sc_deesser_stereo.uid) == 0);

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

            // Init analysis settings
            sAnalysis.vFreqs        = NULL;
            sAnalysis.vIndexes      = NULL;

            sAnalysis.pReactivity   = NULL;
            sAnalysis.pShiftGain    = NULL;
            sAnalysis.pMesh         = NULL;

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

            // Init common settings
            pBypass                 = NULL;
            pGainIn                 = NULL;
            pGainOut                = NULL;

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
            const size_t buf_sz             = lsp_max(BUFFER_SIZE, meta::deesser::FFT_MESH_POINTS * 2) * sizeof(float);
            const size_t freqs_sz           = align_size((meta::deesser::FFT_MESH_POINTS) * sizeof(float), OPTIMAL_ALIGN);
            const size_t idx_sz             = align_size((meta::deesser::FFT_MESH_POINTS) * sizeof(uint32_t), OPTIMAL_ALIGN);
            const size_t mesh_sz            = align_size((meta::deesser::FFT_MESH_POINTS + 4) * sizeof(float), OPTIMAL_ALIGN);
            const size_t alloc              =
                szof_channels +
                buf_sz +        // vBuffer
                idx_sz +        // vIndexes
                mesh_sz +       // vFreqs
                mesh_sz * (SCF_TOTAL + 1);  // sPreEq.vMeshData

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Initialize pointers to channels and temporary buffer
            vChannels               = advance_ptr_bytes<channel_t>(ptr, szof_channels);
            vBuffer                 = advance_ptr_bytes<float>(ptr, buf_sz);
            sAnalysis.vFreqs        = advance_ptr_bytes<float>(ptr, freqs_sz);
            sAnalysis.vIndexes      = advance_ptr_bytes<uint32_t>(ptr, idx_sz);
            for (size_t i=0; i <= SCF_TOTAL; ++i)
                sPreEq.vMeshData[i]     = advance_ptr_bytes<float>(ptr, mesh_sz);

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];

                // Construct in-place DSP processors
                c->sBypass.construct();
                c->sSCEq.construct();
                if (!c->sSCEq.init(SCF_TOTAL, 0))
                    return;
                c->sSCEq.set_mode(dspu::EQM_IIR);

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
                    BIND_PORT(vChannels[i].pOut);
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
            SKIP_PORT("Show sidechain overlay");
            SKIP_PORT("Zoom");

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

            // FFT analysis ports
            lsp_trace("Binding FFT analysis ports");
            BIND_PORT(sAnalysis.pReactivity);
            BIND_PORT(sAnalysis.pShiftGain);
            BIND_PORT(sAnalysis.pMesh);

            // Pre-equalization ports
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
                    c->sSCEq.destroy();
                }
                vChannels   = NULL;
            }

            // Destroy analyzer
            sAnalyzer.destroy();

            // Free previously allocated data chunk
            if (pData != NULL)
            {
                free_aligned(pData);
                pData       = NULL;
            }
        }

        void deesser::update_sample_rate(long sr)
        {
            const size_t max_latency        = 0; // TODO

            // Update analyzer's sample rate
            sAnalyzer.init(
                2*nChannels,
                meta::deesser::FFT_RANK,
                MAX_SAMPLE_RATE,
                meta::deesser::REFRESH_RATE,
                max_latency);
            sAnalyzer.set_sample_rate(sr);
            sAnalyzer.set_rank(meta::deesser::FFT_RANK);
            sAnalyzer.set_activity(false);
            sAnalyzer.set_envelope(dspu::envelope::PINK_NOISE);
            sAnalyzer.set_window(dspu::windows::HANN);
            sAnalyzer.set_rate(meta::deesser::REFRESH_RATE);

            // Update sample rate for the bypass processors
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];
                c->sBypass.init(sr);
                c->sSCEq.set_sample_rate(sr);
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

        void deesser::update_analyzer()
        {
            // Update analyzer parameters
            sAnalyzer.set_reactivity(sAnalysis.pReactivity->value());
            if (sAnalysis.pShiftGain != NULL)
                sAnalyzer.set_shift(sAnalysis.pShiftGain->value() * 100.0f);
//            sAnalyzer.set_activity(active_channels > 0);

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
            update_premix();
            update_analyzer();
            update_preeq();

//            const float out_gain    = pGainOut->value();
            const bool bypass       = pBypass->value() >= 0.5f;

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];

                c->sBypass.set_bypass(bypass);
            }
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
                float *p        = mesh->pvData[idx++];
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

        void deesser::process(size_t samples)
        {
            bind_input_channels();

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
//                    const float level   = dsp::abs_max(c->vIn, to_process) * fInGain;
//                    c->pInLvl->set_value(level);

                    dsp::copy(vBuffer, c->vIn, to_process);
                    c->sBypass.process(c->vOut, c->vIn, vBuffer, to_process);
                }

                offset     += to_process;
            }

            output_preeq_meshes();
        }

        void deesser::ui_activated()
        {
            sPreEq.nSyncMesh    |= SCM_OUT_MESH;
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


