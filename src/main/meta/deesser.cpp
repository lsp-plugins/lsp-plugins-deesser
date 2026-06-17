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

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/plug-fw/meta/registry.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/deesser.h>

#define LSP_PLUGINS_DEESSER_VERSION_MAJOR       1
#define LSP_PLUGINS_DEESSER_VERSION_MINOR       0
#define LSP_PLUGINS_DEESSER_VERSION_MICRO       0

#define LSP_PLUGINS_DEESSER_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_DEESSER_VERSION_MAJOR, \
        LSP_PLUGINS_DEESSER_VERSION_MINOR, \
        LSP_PLUGINS_DEESSER_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        //-------------------------------------------------------------------------
        // Plugin metadata

        #define DE_PREMIX \
            SWITCH("showpmx", "Show pre-mix overlay", "Show premix bar", 0.0f), \
            AMP_GAIN10("in2lk", "Input to Link mix", "In to Link mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("lk2in", "Link to Input mix", "Link to In mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("lk2sc", "Link to Sidechain mix", "Link to SC mix", GAIN_AMP_M_INF_DB)

        #define DE_SC_PREMIX \
            DE_PREMIX, \
            AMP_GAIN10("in2sc", "Input to Sidechain mix", "In to SC mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("sc2in", "Sidechain to Input mix", "SC to In mix", GAIN_AMP_M_INF_DB), \
            AMP_GAIN10("sc2lk", "Sidechain to Link mix", "SC to Link mix", GAIN_AMP_M_INF_DB)

        #define DE_SHM_LINK_MONO \
            OPT_RETURN_MONO("link", "shml", "Side-chain shared memory link")

        #define DE_SHM_LINK_STEREO \
            OPT_RETURN_STEREO("link", "shml_", "Side-chain shared memory link")

        #define DE_COMMON \
            BYPASS, \
            IN_GAIN, \
            OUT_GAIN, \
            SWITCH("showmx", "Show mix overlay", "Show mix bar", 0.0f), \
            SWITCH("showsc", "Show sidechain overlay", "Show SC bar", 0.0f), \
            LOG_CONTROL("zoom", "Graph zoom", "Zoom", U_GAIN_AMP, deesser::ZOOM)

        static const port_t deesser_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            DE_SHM_LINK_MONO,
            DE_PREMIX,
            DE_COMMON,

            PORTS_END
        };

        static const port_t deesser_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            DE_SHM_LINK_STEREO,
            DE_PREMIX,
            DE_COMMON,

            PORTS_END
        };

        static const port_t sc_deesser_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            PORTS_MONO_SIDECHAIN,
            DE_SHM_LINK_MONO,
            DE_SC_PREMIX,
            DE_COMMON,

            PORTS_END
        };

        static const port_t sc_deesser_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            DE_SHM_LINK_STEREO,
            DE_SC_PREMIX,
            DE_COMMON,

            PORTS_END
        };

        static const int plugin_classes[]       = { C_DYNAMICS, -1 };
        static const int clap_features_mono[]   = { CF_AUDIO_EFFECT, CF_DEESSER, CF_MONO, -1 };
        static const int clap_features_stereo[] = { CF_AUDIO_EFFECT, CF_DEESSER, CF_STEREO, -1 };

        const meta::bundle_t deesser_bundle =
        {
            "deesser",
            "Deesser",
            B_DYNAMICS,
            "", // TODO: provide ID of the video on YouTube
            "" // TODO: write plugin description, should be the same to the english version in 'bundles.json'
        };

        const plugin_t deesser_mono =
        {
            "Deesser Mono",
            "Deesser Mono",
            "Desser Mono",
            "DS1M",
            &developers::v_sadovnikov,
            "deesser_mono",
            {
                LSP_LV2_URI("deesser_mono"),
                LSP_LV2UI_URI("deesser_mono"),
                "ds1m",
                LSP_VST3_UID("ds1m    ds1m"),
                LSP_VST3UI_UID("ds1m    ds1m"),
                LSP_LADSPA_DEESSER_BASE + 0,
                LSP_LADSPA_URI("deesser_mono"),
                LSP_CLAP_URI("deesser_mono"),
                LSP_GST_UID("deesser_mono"),
            },
            LSP_PLUGINS_DEESSER_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE,
            deesser_mono_ports,
            "plugins/dynamics/deesser.xml",
            NULL,
            mono_plugin_port_groups,
            &deesser_bundle,
            2
        };
        LSP_REGISTER_METADATA(deesser_mono);

        const plugin_t deesser_stereo =
        {
            "Deesser Stereo",
            "Deesser Stereo",
            "Deesser Stereo",
            "DS1S",
            &developers::v_sadovnikov,
            "deesser_stereo",
            {
                LSP_LV2_URI("deesser_stereo"),
                LSP_LV2UI_URI("deesser_stereo"),
                "ds1s",
                LSP_VST3_UID("ds1s    ds1s"),
                LSP_VST3UI_UID("ds1s    ds1s"),
                LSP_LADSPA_DEESSER_BASE + 1,
                LSP_LADSPA_URI("deesser_stereo"),
                LSP_CLAP_URI("deesser_stereo"),
                LSP_GST_UID("deesser_stereo"),
            },
            LSP_PLUGINS_DEESSER_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE,
            deesser_stereo_ports,
            "plugins/dynamics/deesser.xml",
            NULL,
            stereo_plugin_port_groups,
            &deesser_bundle,
            1
        };
        LSP_REGISTER_METADATA(deesser_stereo);

        const plugin_t sc_deesser_mono =
        {
            "Sidechain Deesser Mono",
            "Sidechain Deesser Mono",
            "SC Desser Mono",
            "SCDS1M",
            &developers::v_sadovnikov,
            "sc_deesser_mono",
            {
                LSP_LV2_URI("sc_deesser_mono"),
                LSP_LV2UI_URI("sc_deesser_mono"),
                "DS1m",
                LSP_VST3_UID("scds1m  DS1m"),
                LSP_VST3UI_UID("scds1m  DS1m"),
                LSP_LADSPA_DEESSER_BASE + 2,
                LSP_LADSPA_URI("sc_deesser_mono"),
                LSP_CLAP_URI("sc_deesser_mono"),
                LSP_GST_UID("sc_deesser_mono"),
            },
            LSP_PLUGINS_DEESSER_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE,
            sc_deesser_mono_ports,
            "plugins/dynamics/deesser.xml",
            NULL,
            mono_plugin_sidechain_port_groups,
            &deesser_bundle,
            4
        };
        LSP_REGISTER_METADATA(sc_deesser_mono);

        const plugin_t sc_deesser_stereo =
        {
            "Sidechain Deesser Stereo",
            "Sidechain Deesser Stereo",
            "SC Deesser Stereo",
            "SCDS1S",
            &developers::v_sadovnikov,
            "sc_deesser_stereo",
            {
                LSP_LV2_URI("sc_deesser_stereo"),
                LSP_LV2UI_URI("sc_deesser_stereo"),
                "DS1s",
                LSP_VST3_UID("scds1s  DS1s"),
                LSP_VST3UI_UID("scds1s  DS1s"),
                LSP_LADSPA_DEESSER_BASE + 3,
                LSP_LADSPA_URI("sc_deesser_stereo"),
                LSP_CLAP_URI("sc_deesser_stereo"),
                LSP_GST_UID("sc_deesser_stereo"),
            },
            LSP_PLUGINS_DEESSER_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE,
            sc_deesser_stereo_ports,
            "plugins/dynamics/deesser.xml",
            NULL,
            stereo_plugin_sidechain_port_groups,
            &deesser_bundle,
            3
        };
        LSP_REGISTER_METADATA(sc_deesser_stereo);
    } /* namespace meta */
} /* namespace lsp */



