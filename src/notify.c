/*
Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "private.h"
#include "panel.h"
#include "plugin.h"

/*----------------------------------------------------------------------------*/
/* Macros and typedefs */
/*----------------------------------------------------------------------------*/

#define HIDE_TIME_MS 15000
#define SPACING 5

typedef struct {
    GtkWidget *popup;               /* Popup message window*/
    guint hide_timer;               /* Timer to hide message window */
    unsigned char seq;              /* Sequence number */
} NotifyWindow;


/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

static GList *nwins = NULL;         /* List of current notifications */
static unsigned char nseq = 0;      /* Sequence number for notifications */


/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static void show_message (LXPanel *panel, NotifyWindow *nw, char *str);
static gboolean hide_message (NotifyWindow *nw);
static void update_positions (GList *item, int offset);
static gboolean window_click (GtkWidget *widget, GdkEventButton *event, NotifyWindow *nw);

/*----------------------------------------------------------------------------*/
/* Private functions */
/*----------------------------------------------------------------------------*/

/* Create a notification window and position appropriately */

static void show_message (LXPanel *panel, NotifyWindow *nw, char *str)
{
    GtkWidget *box, *item;
    GList *plugins;
    gint x, y;
    char *fmt;

    /*
     * In order to get a window which looks exactly like a system tooltip, client-side decoration
     * must be requested for it. This cannot be done by any public API call in GTK+3.24, but there is an
     * internal call _gtk_window_request_csd which sets the csd_requested flag in the class' private data.
     * The code below is compatible with a hacked GTK+3 library which uses GTK_WINDOW_POPUP + 1 as the type
     * for a window with CSD requested. It should also not fall over with the standard library...
     */
    nw->popup = gtk_window_new (GTK_WINDOW_POPUP + 1);
    if (!nw->popup) nw->popup = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_type_hint (GTK_WINDOW (nw->popup), GDK_WINDOW_TYPE_HINT_TOOLTIP);
    gtk_window_set_resizable (GTK_WINDOW (nw->popup), FALSE);

    GtkStyleContext *context = gtk_widget_get_style_context (nw->popup);
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOOLTIP);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add (GTK_CONTAINER (nw->popup), box);

    fmt = g_strcompress (str);
    item = gtk_label_new (fmt);
    gtk_box_pack_start (GTK_BOX (box), item, FALSE, FALSE, 0);
    g_free (fmt);

    gtk_widget_show_all (nw->popup);
    gtk_widget_hide (nw->popup);

    plugins = gtk_container_get_children (GTK_CONTAINER (panel->priv->box));
    lxpanel_plugin_popup_set_position_helper (panel, (GtkWidget *) (g_list_last (plugins))->data, nw->popup, &x, &y);
    if (panel->priv->edge == EDGE_BOTTOM) gdk_window_move (gtk_widget_get_window (nw->popup), x, SPACING);
    else gdk_window_move (gtk_widget_get_window (nw->popup), x, y);
    g_list_free (plugins);

    gdk_window_set_events (gtk_widget_get_window (nw->popup), gdk_window_get_events (gtk_widget_get_window (nw->popup)) | GDK_BUTTON_PRESS_MASK);
    g_signal_connect (G_OBJECT (nw->popup), "button-press-event", G_CALLBACK (window_click), nw);
    gtk_window_present (GTK_WINDOW (nw->popup));
    nw->hide_timer = g_timeout_add (HIDE_TIME_MS, (GSourceFunc) hide_message, nw);
}

/* Destroy a notification window and remove from list */

static gboolean hide_message (NotifyWindow *nw)
{
    if (nw->hide_timer) g_source_remove (nw->hide_timer);
    if (nw->popup) gtk_widget_destroy (nw->popup);
    nwins = g_list_remove (nwins, nw);
    g_free (nw);
    return FALSE;
}

/* Relocate notifications below the supplied item by the supplied vertical offset */

static void update_positions (GList *item, int offset)
{
    NotifyWindow *nw;
    int x, y;

    while (item)
    {
        nw = (NotifyWindow *) item->data;
        gdk_window_get_position (gtk_widget_get_window (nw->popup), &x, &y);
        gdk_window_move (gtk_widget_get_window (nw->popup), x, y + offset);
        item = item->next;
    }
}

/* Handler for mouse click in notification window - closes window */

static gboolean window_click (GtkWidget *widget, GdkEventButton *event, NotifyWindow *nw)
{
    GList *item;
    int w, h;

    item = g_list_find (nwins, nw);
    gtk_window_get_size (GTK_WINDOW (nw->popup), &w, &h);
    update_positions (item->next, - (h + SPACING));

    hide_message (nw);

    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Public API */
/*----------------------------------------------------------------------------*/

unsigned char lxpanel_notify (LXPanel *panel, char *message)
{
    NotifyWindow *nw;
    int w, h;

    // create a new notification window and add it to the front of the list
    nw = g_new (NotifyWindow, 1);
    nwins = g_list_prepend (nwins, nw);

    // set the sequence number for this notification
    nseq++;
    nw->seq = nseq;

    // show the window
    show_message (panel, nw, message);

    // shuffle existing notifications down
    gtk_window_get_size (GTK_WINDOW (nw->popup), &w, &h);
    update_positions (nwins->next, h + SPACING);

    return nseq;
}

void lxpanel_notify_clear (unsigned char seq)
{
    NotifyWindow *nw;
    int w, h;
    GList *item = nwins;

    // loop through windows in the list, looking for the sequence number
    while (item)
    {
        nw = (NotifyWindow *) item->data;

        // sequence number matches
        if (nw->seq == seq)
        {
            // shuffle notifications below up
            gtk_window_get_size (GTK_WINDOW (nw->popup), &w, &h);
            update_positions (item->next, - (h + SPACING));

            // hide the window
            hide_message (nw);
            return;
        }
        item = item->next;
    }
}


/* End of file */
/*----------------------------------------------------------------------------*/
