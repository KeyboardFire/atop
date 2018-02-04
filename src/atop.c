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
#include <stdlib.h>

#define PAWN   1
#define KNIGHT 2
#define BISHOP 3
#define ROOK   4
#define QUEEN  5
#define KING   6

static GtkFixed *board;
static int click_x = -1, click_y;
static float offset_x, offset_y;
static GtkWidget *images[8][8];
static int pieces[8][8];

static void apply_css(GtkWidget *widget, GtkStyleProvider *provider) {
    gtk_style_context_add_provider(gtk_widget_get_style_context(widget), provider, G_MAXUINT);
    if (GTK_IS_CONTAINER(widget)) {
        gtk_container_forall(GTK_CONTAINER(widget), (GtkCallback)apply_css, provider);
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

static void add_piece(char *path, int type, int x, int y) {
    images[x][y] = gtk_image_new_from_file(path);
    pieces[x][y] = type;
    gtk_fixed_put(board, images[x][y], x*64, y*64);
}

static gboolean mouse_pressed(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    click_x = event->x / 64;
    click_y = event->y / 64;

    if (click_x < 8 && click_y < 8 && images[click_x][click_y]) {
        offset_x = event->x - click_x * 64;
        offset_y = event->y - click_y * 64;
    } else click_x = -1;

    return TRUE;
}

static gboolean mouse_moved(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    if (click_x != -1) {
        gtk_fixed_move(board, images[click_x][click_y],
                event->x - offset_x, event->y - offset_y);
    }
    return TRUE;
}

static gboolean mouse_released(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    int target_x = event->x / 64,
        target_y = event->y / 64;
    gtk_fixed_move(board, images[click_x][click_y], target_x * 64, target_y * 64);

    images[target_x][target_y] = images[click_x][click_y];
    images[click_x][click_y] = NULL;
    pieces[target_x][target_y] = pieces[click_x][click_y];
    pieces[click_x][click_y] = 0;

    click_x = -1;

    return TRUE;
}

static void generate_pieces(GtkOverlay *overlay) {
    board = GTK_FIXED(gtk_fixed_new());

    for (int i = 0; i < 8; ++i) {
        add_piece("img/bp.png", -PAWN, i, 1);
        add_piece("img/wp.png", +PAWN, i, 6);
    }

    add_piece("img/br.png", -ROOK, 0, 0);
    add_piece("img/br.png", -ROOK, 7, 0);
    add_piece("img/wr.png", +ROOK, 0, 7);
    add_piece("img/wr.png", +ROOK, 7, 7);

    add_piece("img/bn.png", -KNIGHT, 1, 0);
    add_piece("img/bn.png", -KNIGHT, 6, 0);
    add_piece("img/wn.png", +KNIGHT, 1, 7);
    add_piece("img/wn.png", +KNIGHT, 6, 7);

    add_piece("img/bb.png", -BISHOP, 2, 0);
    add_piece("img/bb.png", -BISHOP, 5, 0);
    add_piece("img/wb.png", +BISHOP, 2, 7);
    add_piece("img/wb.png", +BISHOP, 5, 7);

    add_piece("img/bq.png", -QUEEN, 3, 0);
    add_piece("img/wq.png", +QUEEN, 3, 7);

    add_piece("img/bk.png", -KING, 4, 0);
    add_piece("img/wk.png", +KING, 4, 7);

    gtk_overlay_add_overlay(overlay, GTK_WIDGET(board));
}

void atop_init(int *argc, char ***argv) {
    gtk_init(argc, argv);

    GtkBuilder *builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "src/builder.ui", NULL);
    GObject *win = gtk_builder_get_object(builder, "window");

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "src/builder.css", NULL);
    apply_css(GTK_WIDGET(win), GTK_STYLE_PROVIDER(provider));

    gtk_widget_add_events(GTK_WIDGET(win),
            GDK_POINTER_MOTION_MASK |
            GDK_BUTTON_RELEASE_MASK);

    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(win, "button_press_event", G_CALLBACK(mouse_pressed), NULL);
    g_signal_connect(win, "motion_notify_event", G_CALLBACK(mouse_moved), NULL);
    g_signal_connect(win, "button_release_event", G_CALLBACK(mouse_released), NULL);

    generate_board(GTK_GRID(gtk_builder_get_object(builder, "board")));
    generate_pieces(GTK_OVERLAY(gtk_builder_get_object(builder, "overlay")));

    gtk_widget_show_all(GTK_WIDGET(win));

    gtk_main();
}
