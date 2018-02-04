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
#define NP     KING

static GtkDrawingArea *board;

static int click_x, click_y, hover_x, hover_y;
static float offset_x, offset_y;

static int pieces[8][8];
static int clicked;

static cairo_surface_t *img_piece[NP*2+1];
static cairo_surface_t *img_dark;
static cairo_surface_t *img_light;

static void apply_css(GtkWidget *widget, GtkStyleProvider *provider) {
    gtk_style_context_add_provider(gtk_widget_get_style_context(widget), provider, G_MAXUINT);
    if (GTK_IS_CONTAINER(widget)) {
        gtk_container_forall(GTK_CONTAINER(widget), (GtkCallback)apply_css, provider);
    }
}

static void add_piece(int type, int x, int y) {
    pieces[x][y] = type;
}

static void initialize_images() {
    img_piece[NP-PAWN]   = cairo_image_surface_create_from_png("img/bp.png");
    img_piece[NP-KNIGHT] = cairo_image_surface_create_from_png("img/bn.png");
    img_piece[NP-BISHOP] = cairo_image_surface_create_from_png("img/bb.png");
    img_piece[NP-ROOK]   = cairo_image_surface_create_from_png("img/br.png");
    img_piece[NP-QUEEN]  = cairo_image_surface_create_from_png("img/bq.png");
    img_piece[NP-KING]   = cairo_image_surface_create_from_png("img/bk.png");
    img_piece[NP+PAWN]   = cairo_image_surface_create_from_png("img/wp.png");
    img_piece[NP+KNIGHT] = cairo_image_surface_create_from_png("img/wn.png");
    img_piece[NP+BISHOP] = cairo_image_surface_create_from_png("img/wb.png");
    img_piece[NP+ROOK]   = cairo_image_surface_create_from_png("img/wr.png");
    img_piece[NP+QUEEN]  = cairo_image_surface_create_from_png("img/wq.png");
    img_piece[NP+KING]   = cairo_image_surface_create_from_png("img/wk.png");
    img_dark  = cairo_image_surface_create_from_png("img/black.png");
    img_light = cairo_image_surface_create_from_png("img/white.png");
}

static gboolean mouse_pressed(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    click_x = event->x / 64;
    click_y = event->y / 64;
    if (click_x < 8 || click_y < 8) clicked = pieces[click_x][click_y];

    gtk_widget_queue_draw_area(GTK_WIDGET(board), 0, 0, 512, 512);

    return TRUE;
}

static gboolean mouse_moved(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    hover_x = event->x / 64;
    hover_y = event->y / 64;
    if (hover_x >= 8 || hover_y >= 8) {
        hover_x = -1;
    }

    offset_x = event->x;
    offset_y = event->y;

    gtk_widget_queue_draw_area(GTK_WIDGET(board), 0, 0, 512, 512);

    return TRUE;
}

static gboolean mouse_released(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (clicked) {
        pieces[hover_x][hover_y] = clicked;
        pieces[click_x][click_y] = 0;

        clicked = 0;
    }

    gtk_widget_queue_draw_area(GTK_WIDGET(board), 0, 0, 512, 512);

    return TRUE;
}

static gboolean draw_board(GtkWidget *widget, cairo_t *cr, gpointer data) {
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            // draw square
            cairo_set_source_surface(cr, (i + j) % 2 ? img_dark : img_light, i*64, j*64);
            cairo_paint(cr);

            // shade square if hovering
            if (hover_x == i && hover_y == j) {
                cairo_set_source_rgba(cr, 1, 1, 1, 0.2);
                cairo_rectangle(cr, i*64, j*64, 64, 64);
                cairo_fill(cr);
            }

            // draw piece, if any
            if (pieces[i][j] && !(clicked && click_x == i && click_y == j)) {
                cairo_set_source_surface(cr, img_piece[NP+pieces[i][j]], i*64, j*64);
                cairo_paint(cr);
            }
        }
    }

    // draw piece being held, if any
    if (clicked) {
        cairo_set_source_surface(cr, img_piece[NP+clicked], offset_x - 32, offset_y - 32);
        cairo_paint(cr);
    }

    return FALSE;
}

static void generate_pieces() {
    gtk_widget_set_size_request(GTK_WIDGET(board), 512, 512);
    g_signal_connect(board, "draw", G_CALLBACK(draw_board), NULL);

    for (int i = 0; i < 8; ++i) {
        add_piece(-PAWN, i, 1);
        add_piece(+PAWN, i, 6);
    }

    add_piece(-ROOK, 0, 0);
    add_piece(-ROOK, 7, 0);
    add_piece(+ROOK, 0, 7);
    add_piece(+ROOK, 7, 7);

    add_piece(-KNIGHT, 1, 0);
    add_piece(-KNIGHT, 6, 0);
    add_piece(+KNIGHT, 1, 7);
    add_piece(+KNIGHT, 6, 7);

    add_piece(-BISHOP, 2, 0);
    add_piece(-BISHOP, 5, 0);
    add_piece(+BISHOP, 2, 7);
    add_piece(+BISHOP, 5, 7);

    add_piece(-QUEEN, 3, 0);
    add_piece(+QUEEN, 3, 7);

    add_piece(-KING, 4, 0);
    add_piece(+KING, 4, 7);
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

    initialize_images();
    board = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "board"));
    generate_pieces();

    gtk_widget_show_all(GTK_WIDGET(win));

    gtk_main();
}
