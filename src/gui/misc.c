
#include <string.h>
#include "gui/misc.h"

GtkWidget *
gui_misc_add_menu_item (GtkWidget *menu, char *type, char *label, GCallback callback, gpointer data,
                        GClosureNotify destroy_data)
{
    GtkWidget *item ;
    if (!strcmp (type, "#separator")) item = gtk_separator_menu_item_new ();
    else if (!strcmp (type, "#toggle"))
    {
        item = gtk_check_menu_item_new_with_label (label);
        if (callback) g_signal_connect_data (G_OBJECT (item), "toggled", callback, data,
                                             destroy_data, 0);
    }
    else if (!strcmp (type, "#stock"))
    {
        item = gtk_image_menu_item_new_from_stock (label, NULL);
        if (callback) g_signal_connect_data (G_OBJECT (item), "activate", callback, data,
                                             destroy_data, 0);
    }
    else
    {
        item = gtk_menu_item_new_with_label (label);
        if (callback) g_signal_connect_data (G_OBJECT (item), "activate", callback, data,
                                             destroy_data, 0);
        if (!strcmp (type, "#ghost"))
            gtk_widget_set_sensitive (item, FALSE);
    }
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show (item);
    return item;
}

static GtkListStore *
gui_misc_dropdown_init (GtkWidget *combo_box)
{
    GtkListStore *store = NULL;
    GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
    if (model)
    {
        store = GTK_LIST_STORE (model);
    }
    else
    {
        store = gtk_list_store_new (1, G_TYPE_STRING);
        gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
                                        "text", 0, NULL);
    }
    return store;
}

void
gui_misc_dropdown_append (GtkWidget *combo_box, char *text)
{
    GtkTreeIter item;
    GtkListStore *store = gui_misc_dropdown_init (combo_box);
    gtk_list_store_append (store, &item);
    gtk_list_store_set (store, &item, 0, text, -1);
}

void
gui_misc_dropdown_prepend (GtkWidget *combo_box, char *text)
{
    GtkTreeIter item;
    GtkListStore *store = gui_misc_dropdown_init (combo_box);
    gtk_list_store_prepend (store, &item);
    gtk_list_store_set (store, &item, 0, text, -1);
}

char *
gui_misc_dropdown_get_text (GtkWidget *combo_box, int index)
{
    char *text = NULL;
    GtkTreeIter item;
    GtkListStore *store = gui_misc_dropdown_init (combo_box);
    if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &item, NULL, index))
        gtk_tree_model_get (GTK_TREE_MODEL (store), &item, 0, &text, -1);

    return text;
}

char *
gui_misc_dropdown_get_active_text (GtkWidget *combo_box)
{
    char *text = NULL;
    //GtkTreeIter item;
    //GtkListStore *store = gui_misc_dropdown_init (combo_box);
    int active_item = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));
    if (active_item >= 0)
        text = gui_misc_dropdown_get_text (combo_box, active_item);

    return text;
}

void
gui_misc_dropdown_set_active_text (GtkWidget *combo_box, char *text)
{
    GtkTreeModel *model = GTK_TREE_MODEL (gui_misc_dropdown_init (combo_box));
    int i, ii = gtk_tree_model_iter_n_children (model, NULL);
    for (i = 0; i < ii; i++)
    {
        if (!strcmp (text, gui_misc_dropdown_get_text (combo_box, i)))
        {
            gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), i);
            break;
        }
    }
}

void
gui_misc_dropdown_clear (GtkWidget *combo_box)
{
    GtkListStore *store = gui_misc_dropdown_init (combo_box);
    gtk_list_store_clear (store);
}

void
gui_misc_normalize_dialog_spacing (GtkWidget *widget)
{
    if (GTK_IS_TABLE (widget))
    {
        gtk_table_set_col_spacings (GTK_TABLE (widget), 8);
        gtk_table_set_row_spacings (GTK_TABLE (widget), 4);
    }
    else if (GTK_IS_ALIGNMENT (widget) && GTK_IS_FRAME (gtk_widget_get_parent(widget)))
    {
        gtk_alignment_set_padding (GTK_ALIGNMENT (widget), 4, 0, 16, 0);
    }
    else if (GTK_IS_FRAME (widget) && GTK_IS_BOX (gtk_widget_get_parent(widget)))
    {
        gboolean expand, fill;
        guint padding;
        GtkPackType pack_type;
        gtk_box_query_child_packing (GTK_BOX (gtk_widget_get_parent(widget)), widget, &expand,
                                     &fill, &padding, &pack_type);
        gtk_box_set_child_packing (GTK_BOX (gtk_widget_get_parent(widget)), widget, expand,
                                   fill, 4, pack_type);
    }

    if (GTK_IS_CONTAINER (widget))
    {
        GList *item, *list = gtk_container_get_children (GTK_CONTAINER (widget));
        int i;
        for (i = 0; (item = g_list_nth (list, i)); i++)
            gui_misc_normalize_dialog_spacing (GTK_WIDGET (item->data));

        g_list_free (list);
    }

}

