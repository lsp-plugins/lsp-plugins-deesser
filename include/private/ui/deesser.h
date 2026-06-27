/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-deesser
 * Created on: 27 июн. 2026 г.
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

#ifndef PRIVATE_UI_DEESSER_H_
#define PRIVATE_UI_DEESSER_H_

#include <lsp-plug.in/plug-fw/ui.h>
#include <lsp-plug.in/tk/tk.h>
#include <lsp-plug.in/lltl/darray.h>

namespace lsp
{
    namespace plugui
    {
        /**
         * UI for Deesser plugin series
         */
        class deesser_ui: public ui::Module, public ui::IPortListener
        {
            protected:
                typedef struct filter_label_t
                {
                    ui::IPort          *pFreq;          // Port that contains frequency value
                    ui::IPort          *pLevel;         // Port that contains amplification value
                    tk::Widget         *wWidget;        // Associated widget
                    tk::GraphText      *wText;          // Associated text
                    const char         *sLcKey;         // Localization key
                    uint32_t            nID;            // ID
                } filter_label_t;

                typedef struct filter_label_meta_t
                {
                    const uint32_t      id;
                    const char         *freq_id;
                    const char         *lvl_id;
                    const char         *widget_id;
                    const char         *text_id;
                    const char         *lc_key;
                } filter_label_meta_t;

            protected:
                static const filter_label_meta_t filter_label_meta[];

            protected:
                lltl::darray<filter_label_t>    vFilterLabels;

            protected:
                static status_t     slot_filter_label_mouse_in(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_filter_label_mouse_out(tk::Widget *sender, void *ptr, void *data);

            protected:
                void                add_filter_label(const filter_label_meta_t * meta);
                void                update_filter_label_text(filter_label_t *label);

            public:
                explicit deesser_ui(const meta::plugin_t *meta);
                deesser_ui(const deesser_ui &) = delete;
                deesser_ui(deesser_ui &&) = delete;
                virtual ~deesser_ui() override;

                deesser_ui & operator = (const deesser_ui &) = delete;
                deesser_ui & operator = (deesser_ui &&) = delete;

            public:
                virtual status_t    post_init() override;

                virtual void        notify(ui::IPort *port, size_t flags) override;
        };
    } /* namespace plugui */
} /* namespace lsp */


#endif /* PRIVATE_UI_DEESSER_H_ */
