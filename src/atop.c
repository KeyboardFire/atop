/*
 * atop - opening database for atomic chess
 * Copyright (C) 2018  Keyboard Fire <andy@keyboardfire.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "atop.h"

#include <gtk/gtk.h>

static void apply_css(GtkWidget *widget, GtkStyleProvider *provider) {
    gtk_style_context_add_provider(gtk_widget_get_style_context(widget),
            provider, G_MAXUINT);
    if (GTK_IS_CONTAINER(widget)) {
        gtk_container_forall(GTK_CONTAINER(widget), (GtkCallback)apply_css,
                provider);
    }
}

static void generate_board(GtkGrid *grid) {
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            GtkWidget *img = gtk_image_new_from_file(
                    (i + j) % 2 ? "img/black.png" : "img/white.png");
            gtk_grid_attach(grid, img, i, j, 1, 1);
        }
    }
}

static void generate_pieces(GtkOverlay *overlay) {
    GtkFixed *fixed = GTK_FIXED(gtk_fixed_new());

    for (int i = 0; i < 8; ++i) {
        GtkWidget *bp = gtk_image_new_from_file("img/bp.png");
        GtkWidget *wp = gtk_image_new_from_file("img/wp.png");
        gtk_fixed_put(fixed, bp, i*64, 64);
        gtk_fixed_put(fixed, wp, i*64, 6*64);
    }

    GtkWidget *br1 = gtk_image_new_from_file("img/br.png");
    GtkWidget *br2 = gtk_image_new_from_file("img/br.png");
    GtkWidget *wr1 = gtk_image_new_from_file("img/wr.png");
    GtkWidget *wr2 = gtk_image_new_from_file("img/wr.png");
    gtk_fixed_put(fixed, br1, 0*64, 0);
    gtk_fixed_put(fixed, br2, 7*64, 0);
    gtk_fixed_put(fixed, wr1, 0*64, 7*64);
    gtk_fixed_put(fixed, wr2, 7*64, 7*64);

    GtkWidget *bn1 = gtk_image_new_from_file("img/bn.png");
    GtkWidget *bn2 = gtk_image_new_from_file("img/bn.png");
    GtkWidget *wn1 = gtk_image_new_from_file("img/wn.png");
    GtkWidget *wn2 = gtk_image_new_from_file("img/wn.png");
    gtk_fixed_put(fixed, bn1, 1*64, 0);
    gtk_fixed_put(fixed, bn2, 6*64, 0);
    gtk_fixed_put(fixed, wn1, 1*64, 7*64);
    gtk_fixed_put(fixed, wn2, 6*64, 7*64);

    GtkWidget *bb1 = gtk_image_new_from_file("img/bb.png");
    GtkWidget *bb2 = gtk_image_new_from_file("img/bb.png");
    GtkWidget *wb1 = gtk_image_new_from_file("img/wb.png");
    GtkWidget *wb2 = gtk_image_new_from_file("img/wb.png");
    gtk_fixed_put(fixed, bb1, 2*64, 0);
    gtk_fixed_put(fixed, bb2, 5*64, 0);
    gtk_fixed_put(fixed, wb1, 2*64, 7*64);
    gtk_fixed_put(fixed, wb2, 5*64, 7*64);

    GtkWidget *bq = gtk_image_new_from_file("img/bq.png");
    GtkWidget *wq = gtk_image_new_from_file("img/wq.png");
    gtk_fixed_put(fixed, bq, 3*64, 0);
    gtk_fixed_put(fixed, wq, 3*64, 7*64);

    GtkWidget *bk = gtk_image_new_from_file("img/bk.png");
    GtkWidget *wk = gtk_image_new_from_file("img/wk.png");
    gtk_fixed_put(fixed, bk, 4*64, 0);
    gtk_fixed_put(fixed, wk, 4*64, 7*64);

    gtk_overlay_add_overlay(overlay, GTK_WIDGET(fixed));
}

void atop_init(int *argc, char ***argv) {
    gtk_init(argc, argv);

    GtkBuilder *builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "src/builder.ui", NULL);
    GObject *win = gtk_builder_get_object(builder, "window");

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "src/builder.css", NULL);
    apply_css(GTK_WIDGET(win), GTK_STYLE_PROVIDER(provider));

    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    generate_board(GTK_GRID(gtk_builder_get_object(builder, "board")));
    generate_pieces(GTK_OVERLAY(gtk_builder_get_object(builder, "overlay")));

    gtk_widget_show_all(GTK_WIDGET(win));

    gtk_main();
}
