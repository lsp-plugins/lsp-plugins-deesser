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

#include <lsp-plug.in/stdlib/locale.h>
#include <lsp-plug.in/stdlib/stdio.h>
#include <lsp-plug.in/plug-fw/ui.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <private/plugins/deesser.h>
#include <private/ui/deesser.h>

namespace lsp
{
    namespace plugui
    {
        //---------------------------------------------------------------------
        // Plugin UI factory
        static const meta::plugin_t *plugin_uis[] =
        {
            &meta::deesser_mono,
            &meta::deesser_stereo,
            &meta::sc_deesser_mono,
            &meta::sc_deesser_stereo,
        };

        static ui::Module *ui_factory(const meta::plugin_t *meta)
        {
            return new deesser_ui(meta);
        }

        static ui::Factory factory(ui_factory, plugin_uis, 4);

        //---------------------------------------------------------------------
        static const char *note_names[] =
        {
            "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"
        };

        //---------------------------------------------------------------------
        const deesser_ui::filter_label_meta_t deesser_ui::filter_label_meta[] =
        {
            { 1, "hpf_f", NULL, "filter_dot_hpf", "hpf_note", "lists.deesser.notes.hpf" },
            { 1, "pk1_f", "pk1_g", "filter_dot_pk1", "pk1_note", "lists.deesser.notes.peak" },
            { 2, "pk2_f", "pk2_g", "filter_dot_pk2", "pk2_note", "lists.deesser.notes.peak" },
            { 1, "lpf_f", NULL, "filter_dot_lpf", "lpf_note", "lists.deesser.notes.lpf" },
            { 1, "split", NULL, "split_marker", "split_note", "lists.deesser.notes.split" }
        };

        deesser_ui::deesser_ui(const meta::plugin_t *meta): ui::Module(meta)
        {
        }

        deesser_ui::~deesser_ui()
        {
        }

        status_t deesser_ui::post_init()
        {
            status_t res = ui::Module::post_init();
            if (res != STATUS_OK)
                return res;

            // Add filter labels
            for (size_t i=0; i<sizeof(filter_label_meta)/sizeof(filter_label_meta[0]); ++i)
                add_filter_label(&filter_label_meta[i]);

            return STATUS_OK;
        }

        void deesser_ui::add_filter_label(const filter_label_meta_t * meta)
        {
            // Fetch related objects
            ui::IPort * const freq = pWrapper->port(meta->freq_id);
            if (freq == NULL)
                return;

            ui::IPort * const lvl  = (meta->lvl_id != NULL) ? pWrapper->port(meta->lvl_id) : NULL;
            if ((meta->lvl_id != NULL) && (lvl == NULL))
                return;

            tk::Widget * const widget = pWrapper->find_widget(meta->widget_id);
            if (widget == NULL)
                return;

            tk::GraphText * const text = pWrapper->get_widget<tk::GraphText>(meta->text_id);
            if (text == NULL)
                return;

            // Create event handler
            filter_label_t * const label = vFilterLabels.add();
            if (label == NULL)
                return;

            label->pFreq        = freq;
            label->pLevel       = lvl;
            label->wWidget      = widget;
            label->wText        = text;
            label->sLcKey       = meta->lc_key;
            label->nID          = meta->id;

            // Bind events
            widget->slots()->bind(tk::SLOT_MOUSE_IN, slot_filter_label_mouse_in, this);
            widget->slots()->bind(tk::SLOT_MOUSE_OUT, slot_filter_label_mouse_out, this);
            freq->bind(this);
            if (lvl != NULL)
                lvl->bind(this);
        }

        void deesser_ui::notify(ui::IPort *port, size_t flags)
        {
            // Apply frequency changes to labels
            for (lltl::iterator<filter_label_t> it=vFilterLabels.values(); it; ++it)
            {
                filter_label_t * const label = it.get();
                if (label == NULL)
                    continue;
                if ((label->pFreq == port) || (label->pLevel == port))
                    update_filter_label_text(label);
            }
        }

        void deesser_ui::update_filter_label_text(filter_label_t *label)
        {
            if ((label->pFreq == NULL) ||
                (label->wWidget == NULL) ||
                (label->wText == NULL))
            {
                if (label->wText != NULL)
                    label->wText->visibility()->set(false);
                return;
            }

            if (!label->wText->visibility()->get())
                return;

            // Get the frequency
            const float freq    = label->pFreq->value();
            const float level   = (label->pLevel != NULL) ? label->pLevel->value() : GAIN_AMP_0_DB;

            // Fill the parameters
            expr::Parameters params;
            tk::prop::String lc_string;
            LSPString text;
            lc_string.bind(label->wText->style(), display()->dictionary());
            SET_LOCALE_SCOPED(LC_NUMERIC, "C");

            // Frequency
            params.set_int("id", label->nID);
            params.set_float("frequency", freq);
            params.set_float("gain", dspu::gain_to_db(level));

            // Process split note
            char buf[64];
            float note_full             = dspu::frequency_to_note(freq);
            if (note_full != dspu::NOTE_OUT_OF_RANGE)
            {
                note_full                  += 0.5f;
                const ssize_t note_number   = ssize_t(note_full);

                // Note name
                const ssize_t note          = note_number % 12;
                text.fmt_ascii("lists.notes.names.%s", note_names[note]);
                lc_string.set(&text);
                lc_string.format(&text);
                params.set_string("note", &text);

                // Octave number
                const ssize_t octave        = (note_number / 12) - 1;
                params.set_int("octave", octave);

                // Cents
                const ssize_t note_cents    = ssize_t((note_full - float(note_number)) * 100.0f - 50.0f);
                if (note_cents < 0)
                    text.fmt_ascii(" - %02d", -note_cents);
                else
                    text.fmt_ascii(" + %02d", note_cents);
                params.set_string("cents", &text);

                snprintf(buf, sizeof(buf), "%s.full", label->sLcKey);
            }
            else
                snprintf(buf, sizeof(buf), "%s.unknown", label->sLcKey);

            // Set new text value
            label->wText->text()->set(buf, &params);
        }

        status_t deesser_ui::slot_filter_label_mouse_in(tk::Widget *sender, void *ptr, void *data)
        {
            deesser_ui * const self = static_cast<deesser_ui *>(ptr);
            if (self == NULL)
                return STATUS_OK;

            for (lltl::iterator<filter_label_t> it=self->vFilterLabels.values(); it; ++it)
            {
                filter_label_t * const label = it.get();
                if ((label == NULL) || (label->wText == NULL))
                    continue;

                label->wText->visibility()->set(
                    (label->wWidget->visibility()->get()) &&
                    (label->wWidget == sender));
                self->update_filter_label_text(label);
            }

            return STATUS_OK;
        }

        status_t deesser_ui::slot_filter_label_mouse_out(tk::Widget *sender, void *ptr, void *data)
        {
            deesser_ui * const self = static_cast<deesser_ui *>(ptr);
            if (self == NULL)
                return STATUS_OK;

            for (lltl::iterator<filter_label_t> it=self->vFilterLabels.values(); it; ++it)
            {
                filter_label_t * const label = it.get();
                if ((label != NULL) && (label->wText != NULL))
                    label->wText->visibility()->set(false);
            }

            return STATUS_OK;
        }

    } /* namespace plugui */
} /* namespace lsp */


