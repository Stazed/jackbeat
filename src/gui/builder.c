/*
 *   Jackbeat - JACK sequencer
 *    
 *   Copyright (c) 2004-2008 Olivier Guilyardi <olivier {at} samalyse {dot} com>
 *    
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   SVN:$Id: $
 */

#include <string.h>
#include <stdlib.h>
#include <config.h>
#include <gui/builder.h>
#ifdef USE_PHAT
#include <gui/phat/phat.h>
#endif

#define DEBUG(M, ...) ({ printf("GBU  %s(): ", __func__); printf(M, ## __VA_ARGS__); printf("\n"); })

struct gui_builder_t
{
    GtkWidget **roots;
    int         nroots;
} ;

static void
gui_builder_connect_func (GtkBuilder *builder,
                          GObject *object,
                          const gchar *signal_name,
                          const gchar *handler_name,
                          GObject *connect_object,
                          GConnectFlags flags,
                          gpointer userdata)
{
    static GModule *symbols = NULL;
    GCallback func;

    if (!symbols)
    {
        if (!g_module_supported ())
            DEBUG ("Error: gmodule isn't available");

        symbols = g_module_open (NULL, 0);
    }

    if (!g_module_symbol (symbols, handler_name, (gpointer *) & func))
        g_warning ("could not find signal handler '%s'.", handler_name);
    else
    {
        if (connect_object)
        {
            DEBUG ("Warning: connect object called, can't pass user data");
            g_signal_connect_object (object, signal_name,
                                     func, connect_object,
                                     flags);

        }
        else
        {
            if (flags & G_CONNECT_AFTER)
                g_signal_connect_after (object, signal_name, func, userdata);
            else
                g_signal_connect (object, signal_name, func, userdata);
        }
    }
}

gui_builder_t *
gui_builder_new (char *filename, void *userdata, ...)
{
    gui_builder_t *self = malloc (sizeof (gui_builder_t));
    self->roots = NULL;
    self->nroots = 0;

    GtkBuilder *builder = gtk_builder_new ();
    GError *err = NULL;
    gtk_builder_add_from_file (builder, filename, &err);
    if (err == NULL)
    {
        DEBUG ("Read UI definition from %s", filename);
    }
    else
    {
        DEBUG ("Unable to read UI definition file %s: %s\n", filename, err->message);
        g_error_free (err);
    }
    va_list ap;
    va_start (ap, userdata);
    char *root;
    while ((root = va_arg (ap, char *)))
    {
        self->roots = realloc (self->roots, (self->nroots + 1) * sizeof (GtkWidget *));
        self->roots[self->nroots++] = (GtkWidget *) gtk_builder_get_object (builder, root);

    }
    va_end (ap);
    gtk_builder_connect_signals_full (builder, gui_builder_connect_func, userdata);
    g_object_unref (G_OBJECT (builder));
    return self;
}

void
gui_builder_destroy (gui_builder_t *self)
{
    int i;
    for (i = 0; i < self->nroots; i++)
        gtk_widget_destroy (self->roots[i]);

    free (self->roots);
    free (self);
}

GtkWidget *
find_descendant_by_name (GtkWidget *widget, char *name)
{
    GtkWidget *result = NULL;
    if (GTK_IS_WIDGET (widget) && GTK_IS_BUILDABLE (widget))
    {
        const char *buildable_name = gtk_buildable_get_name (GTK_BUILDABLE (widget));
        if (buildable_name)
        {
            if (!strcmp (buildable_name, name))
            {
                result = widget;
            }
            else if (GTK_IS_CONTAINER (widget))
            {
                GList *item, *list = gtk_container_get_children (GTK_CONTAINER (widget));
                int i;
                for (i = 0; (item = g_list_nth (list, i)); i++)
                    if ((result = find_descendant_by_name (GTK_WIDGET (item->data), name)))
                        break;
                g_list_free (list);
            }
        }
    }
    return result;
}

GtkWidget *
gui_builder_get_widget (gui_builder_t *self, char *name)
{
    int i;
    GtkWidget *result = NULL;
    for (i = 0; i < self->nroots; i++)
        if ((result = find_descendant_by_name (self->roots[i], name)))
            break;

    if (!result)
        DEBUG ("No such widget: %s", name);

    return result;
}

void
gui_builder_get_widgets (gui_builder_t *self, ...)
{
    va_list ap;
    va_start (ap, self);
    char *name;
    GtkWidget **widgetptr;
    while (1)
    {
        name = va_arg (ap, char *);
        if (!name)
            break;
        widgetptr = va_arg (ap, GtkWidget **);
        *widgetptr = gui_builder_get_widget (self, name);
    }
    va_end (ap);
}
