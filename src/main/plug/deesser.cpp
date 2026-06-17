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
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/plug-fw/core/AudioBuffer.h>
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/shared/debug.h>

#include <private/plugins/deesser.h>

namespace lsp
{
    namespace plugins
    {
        /* The size of temporary buffer for audio processing */
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

        static plug::Factory factory(plugin_factory, plugins, 2);

        //---------------------------------------------------------------------
        // Implementation
        deesser::deesser(const meta::plugin_t *meta):
            Module(meta)
        {
            // Compute the number of audio channels by the number of inputs
            nChannels           = 0;
            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_in_port(p))
                    ++nChannels;

            // Initialize other parameters
            vChannels           = NULL;
            vBuffer             = NULL;
            bSidechain          =
                (strcmp(meta->uid, meta::sc_deesser_mono.uid) == 0) ||
                (strcmp(meta->uid, meta::sc_deesser_stereo.uid) == 0);

            // Init premix settings
            sPremix.fInToSc     = GAIN_AMP_M_INF_DB;
            sPremix.fInToLink   = GAIN_AMP_M_INF_DB;
            sPremix.fLinkToIn   = GAIN_AMP_M_INF_DB;
            sPremix.fLinkToSc   = GAIN_AMP_M_INF_DB;
            sPremix.fScToIn     = GAIN_AMP_M_INF_DB;
            sPremix.fScToLink   = GAIN_AMP_M_INF_DB;

            for (size_t i=0; i<2; ++i)
            {
                sPremix.vIn[i]      = NULL;
                sPremix.vOut[i]     = NULL;
                sPremix.vSc[i]      = NULL;
                sPremix.vLink[i]    = NULL;
                sPremix.vTmpIn[i]   = NULL;
                sPremix.vTmpSc[i]   = NULL;
                sPremix.vTmpLink[i] = NULL;
            }

            sPremix.pInToSc     = NULL;
            sPremix.pInToLink   = NULL;
            sPremix.pLinkToIn   = NULL;
            sPremix.pLinkToSc   = NULL;
            sPremix.pScToIn     = NULL;
            sPremix.pScToLink   = NULL;

            // Init common settings
            pBypass             = NULL;
            pGainIn             = NULL;
            pGainOut            = NULL;

            pData               = NULL;
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
            size_t szof_channels    = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);
            size_t buf_sz           = BUFFER_SIZE * sizeof(float);
            size_t alloc            = szof_channels + buf_sz;

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Initialize pointers to channels and temporary buffer
            vChannels               = advance_ptr_bytes<channel_t>(ptr, szof_channels);
            vBuffer                 = advance_ptr_bytes<float>(ptr, buf_sz);

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];

                // Construct in-place DSP processors
                c->sBypass.construct();

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

            // Pre-mixing ports
            lsp_trace("Binding pre-mix ports");
            SKIP_PORT("Show premix overlay");
            BIND_PORT(sPremix.pInToLink);
            BIND_PORT(sPremix.pLinkToIn);
            BIND_PORT(sPremix.pLinkToSc);
            if (bSidechain)
            {
                BIND_PORT(sPremix.pInToSc);
                BIND_PORT(sPremix.pScToIn);
                BIND_PORT(sPremix.pScToLink);
            }

            // Bind common parameters
            BIND_PORT(pBypass);
            BIND_PORT(pGainIn);
            BIND_PORT(pGainOut);
            SKIP_PORT("Show pre-mix overlay");
            SKIP_PORT("Show sidechain overlay");
            SKIP_PORT("Zoom");
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
                    channel_t *c    = &vChannels[i];
                    c->sBypass.destroy();
                }
                vChannels   = NULL;
            }

            vBuffer     = NULL;

            // Free previously allocated data chunk
            if (pData != NULL)
            {
                free_aligned(pData);
                pData       = NULL;
            }
        }

        void deesser::update_sample_rate(long sr)
        {
            // Update sample rate for the bypass processors
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c     = &vChannels[i];
                c->sBypass.init(sr);
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

        void deesser::update_settings()
        {
            update_premix();

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

        void deesser::process(size_t samples)
        {
            // Bind input signal
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t * const c = &vChannels[i];
                core::AudioBuffer * const shm_buf   = (c->pShmIn != NULL) ? c->pShmIn->buffer<core::AudioBuffer>() : NULL;

                sPremix.vIn[i]      = c->pIn->buffer<float>();
                sPremix.vOut[i]     = c->pOut->buffer<float>();
                sPremix.vSc[i]      = (c->pScIn != NULL) ? c->pScIn->buffer<float>() : sPremix.vIn[i];
                sPremix.vLink[i]    = ((shm_buf != NULL) && (shm_buf->active())) ? shm_buf->buffer() : NULL;
            }

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


