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
        static const port_item_t de_pf_filter_slope[] =
        {
            { "off",                "eq.slope.off"      },
            { "12 dB/oct",          "eq.slope.12dbo"    },
            { "24 dB/oct",          "eq.slope.24dbo"    },
            { "36 dB/oct",          "eq.slope.36dbo"    },
            { "48 dB/oct",          "eq.slope.48dbo"    },
            { NULL, NULL }
        };

        static const port_item_t de_split_modes[] =
        {
            { "Classic",            "deesser.modes.off"           },
            { "Classic",            "deesser.modes.classic"       },
            { "Modern",             "deesser.modes.modern"        },
            { "Linear Phase",       "deesser.modes.linear_phase"  },
            { NULL, NULL }
        };

        static const port_item_t de_slopes[] =
        {
            { "LR2 (12 dB/oct)",    "deesser.slope.12dbo"         },
            { "LR4 (24 dB/oct)",    "deesser.slope.24dbo"         },
            { "LR8 (48 dB/oct)",    "deesser.slope.48dbo"         },
            { NULL, NULL }
        };


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

        #define DE_FILTERS \
            COMBO("hpf_s", "High-pass filter slope", "HPF slope", 1, de_pf_filter_slope), \
            LOG_CONTROL("hpf_f", "High-pass filter frequency", "HPF freq", U_HZ, deesser::HPF_FREQ), \
            LOG_CONTROL("hpf_q", "High-pass filter qualifty factor", "HPF Q", U_NONE, deesser::PF_Q), \
            COMBO("lpf_s", "Low-pass filter slope", "LPF slope", 0, de_pf_filter_slope), \
            LOG_CONTROL("lpf_f", "Low-pass filter frequency", "LPF freq", U_HZ, deesser::LPF_FREQ), \
            LOG_CONTROL("lpf_q", "Low-pass filter qualifty factor", "LPF Q", U_NONE, deesser::PF_Q), \
            SWITCH("pk1_on", "Peak filter 1 on", "Peak 1 on", 1.0f), \
            LOG_CONTROL("pk1_f", "Peak filter 1 frequency", "Peak 1 freq", U_HZ, deesser::PEAK1_FREQ), \
            LOG_CONTROL("pk1_g", "Peak filter 1 gain", "Peak 1 gain", U_GAIN_AMP, deesser::PEAK_GAIN), \
            LOG_CONTROL("pk1_q", "Peak filter 1 qualifty factor", "Peak 1 Q", U_NONE, deesser::PEAK_Q), \
            SWITCH("pk2_on", "Peak filter 2 on", "Peak 2 on", 0.0f), \
            LOG_CONTROL("pk2_f", "Peak filter 2 frequency", "Peak 2 freq", U_HZ, deesser::PEAK2_FREQ), \
            LOG_CONTROL("pk2_g", "Peak filter 2 gain", "Peak 2 gain", U_GAIN_AMP, deesser::PEAK_GAIN), \
            LOG_CONTROL("pk2_q", "Peak filter 2 qualifty factor", "Peak 2 Q", U_NONE, deesser::PEAK_Q), \
            MESH("sceq", "Side-chain equalization chart", 6, deesser::FFT_MESH_POINTS + 4)

        #define DE_ANALYSIS(channels) \
            LOG_CONTROL("react", "FFT reactivity", "Reactivity", U_MSEC, deesser::REACT_TIME), \
            AMP_GAIN("shift", "Shift gain", "Shift", 1.0f, 100.0f), \
            MESH("fftg", "FFT analysis graph", 1 + channels*2, deesser::FFT_MESH_POINTS + 2)

        #define DE_CROSSOVER(channels) \
            COMBO("split", "Enable frequency split", "Split", 2, de_split_modes), \
            COMBO("slope", "Frequency split slope", "Slope", 2, de_slopes), \
            LOG_CONTROL("split_f", "Split frequency", "Split freq", U_HZ, deesser::SPLIT_FREQ), \
            PERCENTS("xlink", "Crossover linkage", "Split link", 0.0f, 0.001f), \
            MESH("rgain", "Reduction gain chart", 1 + channels, deesser::FFT_MESH_POINTS + 4)

        #define DE_COMMON \
            BYPASS, \
            IN_GAIN, \
            OUT_GAIN, \
            SWITCH("showsc", "Show sidechain overlay", "Show SC bar", 0.0f), \
            LOG_CONTROL("zoom", "Graph zoom", "Zoom", U_GAIN_AMP, deesser::ZOOM), \
            LOG_CONTROL("slink", "Stereo linking", "Stereo link", U_PERCENT, deesser::LINKING)

        #define DE_COMMON_MONO \
            DE_COMMON

        #define DE_COMMON_STEREO \
            DE_COMMON, \
            SWITCH("ssplit", "Stereo split", "Stereo split", 0.0f)

        static const port_t deesser_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            DE_SHM_LINK_MONO,
            DE_COMMON_MONO,
            DE_PREMIX,
            DE_ANALYSIS(1),
            DE_CROSSOVER(1),
            DE_FILTERS,

            PORTS_END
        };

        static const port_t deesser_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            DE_SHM_LINK_STEREO,
            DE_COMMON_STEREO,
            DE_PREMIX,
            DE_ANALYSIS(2),
            DE_CROSSOVER(2),
            DE_FILTERS,

            PORTS_END
        };

        static const port_t sc_deesser_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            PORTS_MONO_SIDECHAIN,
            DE_SHM_LINK_MONO,
            DE_COMMON_MONO,
            DE_SC_PREMIX,
            DE_ANALYSIS(1),
            DE_CROSSOVER(1),
            DE_FILTERS,

            PORTS_END
        };

        static const port_t sc_deesser_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            PORTS_STEREO_SIDECHAIN,
            DE_SHM_LINK_STEREO,
            DE_COMMON_STEREO,
            DE_SC_PREMIX,
            DE_ANALYSIS(2),
            DE_CROSSOVER(2),
            DE_FILTERS,

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



