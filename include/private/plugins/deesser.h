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

#include <lsp-plug.in/dsp-units/util/Delay.h>
#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
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

                typedef struct channel_t
                {
                    // DSP processing modules
                    dspu::Bypass        sBypass;            // Bypass

                    float              *vIn;                // Input signal
                    float              *vOut;               // Output signal
                    float              *vScIn;              // Sidechain signal
                    float              *vShmIn;             // Shared memory link signal

                    // Input ports
                    plug::IPort        *pIn;                // Input port
                    plug::IPort        *pOut;               // Output port
                    plug::IPort        *pScIn;              // Sidechain port
                    plug::IPort        *pShmIn;             // Shared memory link input
                } channel_t;

            protected:
                size_t              nChannels;          // Number of channels
                channel_t          *vChannels;          // Delay channels
                float              *vBuffer;            // Temporary buffer for audio processing
                bool                bSidechain;         // Sidechain version

                premix_t            sPremix;            // Premix settings

                plug::IPort        *pBypass;            // Bypass
                plug::IPort        *pGainIn;            // Input gain
                plug::IPort        *pGainOut;           // Output gain

                uint8_t            *pData;              // Allocated data

            protected:
                void                do_destroy();
                void                update_premix();
                void                premix_channel(uint32_t channel, size_t count);

            public:
                explicit deesser(const meta::plugin_t *meta);
                deesser (const deesser &) = delete;
                deesser (deesser &&) = delete;
                virtual ~deesser() override;

                deesser & operator = (const deesser &) = delete;
                deesser & operator = (deesser &&) = delete;

                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports) override;
                virtual void        destroy() override;

            public:
                virtual void        update_sample_rate(long sr) override;
                virtual void        update_settings() override;
                virtual void        process(size_t samples) override;
                virtual void        dump(dspu::IStateDumper *v) const override;
        };

    } /* namespace plugins */
} /* namespace lsp */


#endif /* PRIVATE_PLUGINS_DEESSER_H_ */

