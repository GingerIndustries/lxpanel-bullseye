/*
 * Copyright (C) 2014-2016 Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This file is a part of LXPanel project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "plugin.h"
#include "gtk-compat.h"

#include <keybinder.h>

static GHashTable *all_bindings = NULL;

/* generated by glib-genmarshal for BOOL:STRING */
static
void _marshal_BOOLEAN__STRING (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
    typedef gboolean (*GMarshalFunc_BOOLEAN__STRING) (gpointer     data1,
                                                      gpointer     arg_1,
                                                      gpointer     data2);
    register GMarshalFunc_BOOLEAN__STRING callback;
    register GCClosure *cc = (GCClosure*) closure;
    register gpointer data1, data2;
    gboolean v_return;

    g_return_if_fail (return_value != NULL);
    g_return_if_fail (n_param_values == 2);

    if (G_CCLOSURE_SWAP_DATA (closure))
    {
        data1 = closure->data;
        data2 = g_value_peek_pointer (param_values + 0);
    }
    else
    {
        data1 = g_value_peek_pointer (param_values + 0);
        data2 = closure->data;
    }
    callback = (GMarshalFunc_BOOLEAN__STRING) (marshal_data ? marshal_data : cc->callback);

    v_return = callback (data1, (char*) g_value_get_string(param_values + 1), data2);

    g_value_set_boolean (return_value, v_return);
}

#define PANEL_TYPE_CFG_INPUT_BUTTON     (config_input_button_get_type())

extern GType config_input_button_get_type (void) G_GNUC_CONST;

typedef struct _PanelCfgInputButton      PanelCfgInputButton;
typedef struct _PanelCfgInputButtonClass PanelCfgInputButtonClass;

struct _PanelCfgInputButton
{
    GtkFrame parent;
    GtkToggleButton *none;
    GtkToggleButton *custom;
    GtkButton *btn;
    gboolean do_key;
    gboolean do_click;
    guint key;
    GdkModifierType mods;
    gboolean has_focus;
};

struct _PanelCfgInputButtonClass
{
    GtkFrameClass parent_class;
    gboolean (*changed)(PanelCfgInputButton *btn, char *accel);
};

enum
{
    CHANGED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];


/* ---- Events on test button ---- */

static void on_focus_in_event(GtkButton *test, GdkEvent *event,
                              PanelCfgInputButton *btn)
{
    /* toggle radiobuttons */
    gtk_toggle_button_set_active(btn->custom, TRUE);
    btn->has_focus = TRUE;
    if (btn->do_key)
#if GTK_CHECK_VERSION(3, 0, 0)
        gdk_seat_grab (gdk_display_get_default_seat (gdk_display_get_default ()),
            gtk_widget_get_window(GTK_WIDGET(test)), GDK_SEAT_CAPABILITY_KEYBOARD,
            TRUE, NULL, event, NULL, NULL);
#else
        gdk_keyboard_grab(gtk_widget_get_window(GTK_WIDGET(test)),
                          TRUE, GDK_CURRENT_TIME);
#endif
}

static void on_focus_out_event(GtkButton *test, GdkEvent *event,
                               PanelCfgInputButton *btn)
{
    /* stop accepting mouse clicks */
    btn->has_focus = FALSE;
    if (btn->do_key)
#if GTK_CHECK_VERSION(3, 0, 0)
        gdk_seat_ungrab (gdk_display_get_default_seat (gdk_display_get_default ()));
#else
        gdk_keyboard_ungrab(GDK_CURRENT_TIME);
#endif
}

static void _button_set_click_label(GtkButton *btn, guint keyval, GdkModifierType state)
{
    char *mod_text, *text;
    const char *btn_text;
    char buff[64];

    mod_text = gtk_accelerator_get_label(0, state);
    btn_text = gdk_keyval_name(keyval);
    if (btn_text == NULL)
    {
        gtk_button_set_label(btn, "");
        g_free(mod_text);
        return;
    }
    switch (btn_text[0])
    {
    case '1':
        btn_text = _("LeftBtn");
        break;
    case '2':
        btn_text = _("MiddleBtn");
        break;
    case '3':
        btn_text = _("RightBtn");
        break;
    default:
        snprintf(buff, sizeof(buff), _("Btn%s"), btn_text);
        btn_text = buff;
    }
    text = g_strdup_printf("%s%s", mod_text, btn_text);
    gtk_button_set_label(btn, text);
    g_free(text);
    g_free(mod_text);
}

static gboolean on_key_event(GtkButton *test, GdkEventKey *event,
                             PanelCfgInputButton *btn)
{
    GdkModifierType state;
    char *text;
    gboolean ret = FALSE;

    /* ignore Tab completely so user can leave focus */
    if (event->keyval == GDK_KEY_Tab)
        return FALSE;
    /* request mods directly, event->state isn't updated yet */
#if GTK_CHECK_VERSION(3, 0, 0)
    gdk_window_get_device_position (gtk_widget_get_window (GTK_WIDGET(test)),
        gdk_seat_get_pointer (gdk_display_get_default_seat (gdk_display_get_default ())),
        NULL, NULL, &state);
#else
    gdk_window_get_pointer(gtk_widget_get_window(GTK_WIDGET(test)),
                           NULL, NULL, &state);
#endif
    /* special support for Win key, it doesn't work sometimes */
    if ((state & GDK_SUPER_MASK) == 0 && (state & GDK_MOD4_MASK) != 0)
        state |= GDK_SUPER_MASK;
    state &= gtk_accelerator_get_default_mod_mask();
    /* if mod key event then update test label and go */
    if (event->is_modifier)
    {
        if (state != 0)
            text = gtk_accelerator_get_label(0, state);
        /* if no modifiers currently then show original state */
        else if (btn->do_key)
            text = gtk_accelerator_get_label(btn->key, btn->mods);
        else
        {
            _button_set_click_label(test, btn->key, btn->mods);
            return FALSE;
        }
        gtk_button_set_label(test, text);
        g_free(text);
        return FALSE;
    }
    /* if not keypress query then ignore key press */
    if (event->type != GDK_KEY_PRESS || !btn->do_key)
        return FALSE;
    /* if keypress is equal to previous then nothing to do */
    if (state == btn->mods && event->keyval == btn->key)
    {
        text = gtk_accelerator_get_label(event->keyval, state);
        gtk_button_set_label(test, text);
        g_free(text);
        return FALSE;
    }
    /* if BackSpace pressed then just clear the button */
    if (state == 0 && event->keyval == GDK_KEY_BackSpace)
    {
        g_signal_emit(btn, signals[CHANGED], 0, NULL, &ret);
        if (ret)
        {
            btn->mods = 0;
            btn->key = 0;
        }
        goto _done;
    }
    /* drop single printable and printable with single Shift, Ctrl, Alt */
    if (event->length != 0 && (state == 0 || state == GDK_SHIFT_MASK ||
                               state == GDK_CONTROL_MASK || state == GDK_MOD1_MASK))
    {
        GtkWidget* dlg;
        text = gtk_accelerator_get_label(event->keyval, state);
        dlg = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                     _("Key combination '%s' cannot be used as"
                                       " a global hotkey, sorry."), text);
        g_free(text);
        gtk_window_set_title(GTK_WINDOW(dlg), _("Error"));
        gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        return FALSE;
    }
    /* send a signal that it's changed */
    text = gtk_accelerator_name(event->keyval, state);
    g_signal_emit(btn, signals[CHANGED], 0, text, &ret);
    g_free(text);
    if (ret)
    {
        btn->mods = state;
        btn->key = event->keyval;
    }
_done:
    text = gtk_accelerator_get_label(btn->key, btn->mods);
    gtk_button_set_label(test, text);
    g_free(text);
    return FALSE;
}

static gboolean on_button_press_event(GtkButton *test, GdkEventButton *event,
                                      PanelCfgInputButton *btn)
{
    GdkModifierType state;
    char *text;
    char digit[4];
    guint keyval;
    gboolean ret = FALSE;

    if (!btn->do_click)
        return FALSE;
    /* if not focused yet then take facus and skip event */
    if (!btn->has_focus)
    {
        btn->has_focus = TRUE;
        return FALSE;
    }
    /* if simple right-click then just ignore it */
    state = event->state & gtk_accelerator_get_default_mod_mask();
    if (event->button == 3 && state == 0)
        return FALSE;
    /* FIXME: how else to represent buttons? */
    snprintf(digit, sizeof(digit), "%u", event->button);
    keyval = gdk_keyval_from_name(digit);
    /* if click is equal to previous then nothing to do */
    if (state == btn->mods && keyval == btn->key)
    {
        _button_set_click_label(test, keyval, state);
        return FALSE;
    }
    /* send a signal that it's changed */
    text = gtk_accelerator_name(keyval, state);
    g_signal_emit(btn, signals[CHANGED], 0, text, &ret);
    g_free(text);
    if (ret)
    {
        btn->mods = state;
        btn->key = keyval;
    }
    _button_set_click_label(test, btn->key, btn->mods);
    return FALSE;
}

static void on_reset(GtkRadioButton *rb, PanelCfgInputButton *btn)
{
    gboolean ret = FALSE;
    if (!gtk_toggle_button_get_active(btn->none))
        return;
    btn->mods = 0;
    btn->key = 0;
    gtk_button_set_label(btn->btn, "");
    g_signal_emit(btn, signals[CHANGED], 0, NULL, &ret);
}

/* ---- Class implementation ---- */

G_DEFINE_TYPE(PanelCfgInputButton, config_input_button, GTK_TYPE_FRAME)

static void config_input_button_class_init(PanelCfgInputButtonClass *klass)
{
    signals[CHANGED] =
        g_signal_new("changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PanelCfgInputButtonClass, changed),
                     g_signal_accumulator_true_handled, NULL,
                     _marshal_BOOLEAN__STRING,
                     G_TYPE_BOOLEAN, 1, G_TYPE_STRING);
}

static void config_input_button_init(PanelCfgInputButton *self)
{
#if GTK_CHECK_VERSION(3, 0, 0)
    GtkWidget *w = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
#else
    GtkWidget *w = gtk_hbox_new(FALSE, 6);
#endif
    GtkBox *box = GTK_BOX(w);

    /* GtkRadioButton "None" */
    w = gtk_radio_button_new_with_label(NULL, _("None"));
    gtk_box_pack_start(box, w, FALSE, FALSE, 6);
    self->none = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(self->none, TRUE);
    g_signal_connect(w, "toggled", G_CALLBACK(on_reset), self);
    /* GtkRadioButton "Custom:" */
    w = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(w),
                                                    _("Custom:"));
    gtk_box_pack_start(box, w, FALSE, FALSE, 0);
    gtk_widget_set_can_focus(w, FALSE);
    self->custom = GTK_TOGGLE_BUTTON(w);
    /* test GtkButton */
    w = gtk_button_new_with_label(NULL);
    gtk_box_pack_start(box, w, TRUE, TRUE, 0);
    self->btn = GTK_BUTTON(w);
    gtk_button_set_label(self->btn, "        "); /* set some minimum size */
    g_signal_connect(w, "focus-in-event", G_CALLBACK(on_focus_in_event), self);
    g_signal_connect(w, "focus-out-event", G_CALLBACK(on_focus_out_event), self);
    g_signal_connect(w, "key-press-event", G_CALLBACK(on_key_event), self);
    g_signal_connect(w, "key-release-event", G_CALLBACK(on_key_event), self);
    g_signal_connect(w, "button-press-event", G_CALLBACK(on_button_press_event), self);
    /* HBox */
    w = (GtkWidget *)box;
    gtk_widget_show_all(w);
    gtk_container_add(GTK_CONTAINER(self), w);
}

static PanelCfgInputButton *_config_input_button_new(const char *label)
{
    return g_object_new(PANEL_TYPE_CFG_INPUT_BUTTON,
                        "label", label, NULL);
}

GtkWidget *panel_config_hotkey_button_new(const char *label, const char *hotkey)
{
    PanelCfgInputButton *btn = _config_input_button_new(label);
    char *text;

    btn->do_key = TRUE;
    if (hotkey && *hotkey)
    {
        gtk_accelerator_parse(hotkey, &btn->key, &btn->mods);
        text = gtk_accelerator_get_label(btn->key, btn->mods);
        gtk_button_set_label(btn->btn, text);
        g_free(text);
        gtk_toggle_button_set_active(btn->custom, TRUE);
    }
    return GTK_WIDGET(btn);
}

GtkWidget *panel_config_click_button_new(const char *label, const char *click)
{
    PanelCfgInputButton *btn = _config_input_button_new(label);

    btn->do_click = TRUE;
    if (click && *click)
    {
        gtk_accelerator_parse(click, &btn->key, &btn->mods);
        _button_set_click_label(btn->btn, btn->key, btn->mods);
        gtk_toggle_button_set_active(btn->custom, TRUE);
    }
    return GTK_WIDGET(btn);
}

gboolean lxpanel_apply_hotkey(char **hkptr, const char *keystring,
                              void (*handler)(const char *keystring, gpointer user_data),
                              gpointer user_data, gboolean show_error)
{
    g_return_val_if_fail(hkptr != NULL, FALSE);
    g_return_val_if_fail(handler != NULL, FALSE);

    if (G_UNLIKELY(all_bindings == NULL))
        all_bindings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    if (keystring != NULL &&
        /* check if it was already grabbed in some other place */
        (g_hash_table_lookup(all_bindings, keystring) != NULL ||
         !keybinder_bind(keystring, handler, user_data)))
    {
        if (show_error)
        {
            GtkWidget* dlg;

            dlg = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                         _("Cannot assign '%s' as a global hotkey:"
                                           " it is already bound."), keystring);
            gtk_window_set_title(GTK_WINDOW(dlg), _("Error"));
            gtk_window_set_keep_above(GTK_WINDOW(dlg), TRUE);
            gtk_dialog_run(GTK_DIALOG(dlg));
            gtk_widget_destroy(dlg);
        }
        return FALSE;
    }
    if (*hkptr != NULL)
    {
        keybinder_unbind(*hkptr, handler);
        if (!g_hash_table_remove(all_bindings, *hkptr))
            g_warning("%s: hotkey %s not found in hast table", __FUNCTION__, *hkptr);
    }
    *hkptr = g_strdup(keystring);
    if (*hkptr)
        g_hash_table_insert(all_bindings, *hkptr, *hkptr);
    return TRUE;
}

guint panel_config_click_parse(const char *keystring, GdkModifierType *mods)
{
    guint key;
    const char *name;

    if (keystring == NULL)
        return 0;
    gtk_accelerator_parse(keystring, &key, mods);
    name = gdk_keyval_name(key);
    if (name && name[0] >= '1' && name[0] <= '9')
        return (name[0] - '0');
    return 0;
}
#if 0
// test code, can be used as an example until erased. :)
static void handler(const char *keystring, void *user_data)
{
}

static char *hotkey = NULL;

static gboolean cb(PanelCfgInputButton *btn, char *text, gpointer unused)
{
    g_print("got keystring \"%s\"\n", text);

    if (!btn->do_key)
        return TRUE;
    return lxpanel_apply_hotkey(&hotkey, text, &handler, NULL, TRUE);
}

int main(int argc, char **argv)
{
    GtkWidget *dialog;
    GtkWidget *btn;

    gtk_init(&argc, &argv);
    dialog = gtk_dialog_new();
    lxpanel_apply_hotkey(&hotkey, "<Super>z", &handler, NULL, FALSE);
    btn = panel_config_hotkey_button_new("test", hotkey);
//    btn = panel_config_click_button_new("test", NULL);
    gtk_widget_show(btn);
    g_signal_connect(btn, "changed", G_CALLBACK(cb), NULL);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), btn);
    gtk_dialog_run(GTK_DIALOG(dialog));
    return 0;
}
#endif
