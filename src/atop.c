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
#include <string.h>

#define M_PI 3.14159265358979323846

#define PAWN   1
#define KNIGHT 2
#define BISHOP 3
#define ROOK   4
#define QUEEN  5
#define KING   6
#define NP     KING

#define SQ(x,y) ((x)*8+(y))
#define X(sq) ((sq)/8)
#define Y(sq) ((sq)%8)

static GtkDrawingArea *board;
static GtkGrid *moves;

static int click_x, click_y, hover_x, hover_y;
static float offset_x, offset_y;

static int pieces[8][8];
static int legal[8][8];
static int clicked;

static int **hist;
int nhist;

static cairo_surface_t *img_piece[NP*2+1];
static cairo_surface_t *img_dark;
static cairo_surface_t *img_light;

struct move {
    int from;
    int to;
    char *desc;
    struct move *next;
    struct move *child;
    struct move *parent;
};
struct move *db;
struct move *cur_node;

#define BUF_SIZE 1024
#define PENDING_FROM 0
#define PENDING_TO   1
#define READING_DESC 2
static struct move* new_node() {
    struct move *node = malloc(sizeof *node);
    node->desc = NULL;
    node->next = NULL;
    node->child = NULL;
    node->parent = NULL;
    return node;
}

static void initialize_db() {
    db = new_node();
    struct move *cur = db;
    cur_node = db;
    int child = 1;

    FILE *f = fopen("atop.db", "rb");
    if (!f) return;
    unsigned char buf[BUF_SIZE];
    size_t nread = 0;

    size_t idx = 0, state = PENDING_FROM;
    while (idx != nread || (idx = 0, (nread = fread(buf, 1, BUF_SIZE, f)) > 0)) {
        switch (state) {
            case PENDING_FROM:
                if (buf[idx] == 0xff) {
                    if (child) child = 0;
                    else if (cur->parent) cur = cur->parent;
                    else goto done;
                } else {
                    struct move *new = new_node();
                    new->parent = cur;
                    new->from = buf[idx];
                    if (child) cur->child = new, cur = new;
                    else cur->next = new, cur = new;
                    state = PENDING_TO;
                }
                ++idx;
                break;
            case PENDING_TO:
                cur->to = buf[idx++];
                state = READING_DESC;
                break;
            case READING_DESC: {
                size_t max = nread - idx,
                       len = strnlen(buf + idx, max),
                       prevlen = cur->desc ? strlen(cur->desc) : 0;

                cur->desc = realloc(cur->desc, prevlen + len + 1);
                strncpy(cur->desc + prevlen, buf + idx, len);
                cur->desc[prevlen + len] = '\0';

                idx += len;
                if (max != len) {
                    child = 1;
                    state = PENDING_FROM;
                    ++idx;
                }
                break;
            }
        }
    }

done:
    fclose(f);
}

static void write_node(FILE *f, struct move *node) {
    if (!node) return;

    fputc(node->from, f);
    fputc(node->to, f);
    fputs(node->desc, f);
    fputc(0, f);
    write_node(f, node->child);
    fputc(255, f);
    write_node(f, node->next);
}

static void save_db() {
    FILE *f = fopen("atop.db", "wb");
    write_node(f, db->child);
    fputc(255, f);
    fclose(f);
}

static void apply_css(GtkWidget *widget, GtkStyleProvider *provider) {
    gtk_style_context_add_provider(gtk_widget_get_style_context(widget), provider, G_MAXUINT);
    if (GTK_IS_CONTAINER(widget)) {
        gtk_container_forall(GTK_CONTAINER(widget), (GtkCallback)apply_css, provider);
    }
}

static void update_moves() {
    gtk_container_foreach(GTK_CONTAINER(moves), (GtkCallback)gtk_widget_destroy, NULL);

    for (struct move *m = cur_node->child; m; m = m->next) {
        char *header = malloc(20);
        sprintf(header, "%c%d to %c%d",
                'a'+X(m->from), 1+Y(m->from),
                'a'+X(m->to), 1+Y(m->to));
        GtkLabel *head = GTK_LABEL(gtk_label_new(header));
        free(header);

        GtkLabel *txt = GTK_LABEL(gtk_label_new(m->desc));

        gtk_grid_attach_next_to(moves, GTK_WIDGET(head), NULL, GTK_POS_BOTTOM, 1, 1);
        gtk_widget_show(GTK_WIDGET(head));
        gtk_grid_attach_next_to(moves, GTK_WIDGET(txt), NULL, GTK_POS_BOTTOM, 1, 1);
        gtk_widget_show(GTK_WIDGET(txt));
    }
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

static void update_legal(int type, int color, int fx, int fy) {
    if (type == PAWN) {
        if (!pieces[fx][fy-color]) {
            legal[fx][fy-color] = 1;
            if ((color == 1 ? fy == 6 : fy == 1) && !pieces[fx][fy-2*color]) {
                legal[fx][fy-2*color] = 1;
            }
        }
        if (fx > 0 && color*pieces[fx-1][fy-color] < 0) legal[fx-1][fy-color] = 1;
        if (fx < 7 && color*pieces[fx+1][fy-color] < 0) legal[fx+1][fy-color] = 1;
        return;
    }

    if (type == KNIGHT) {
        if (fx+1 <  8 && fy+2 <  8 && color*pieces[fx+1][fy+2] <= 0) legal[fx+1][fy+2] = 1;
        if (fx+1 <  8 && fy-2 >= 0 && color*pieces[fx+1][fy-2] <= 0) legal[fx+1][fy-2] = 1;
        if (fx+2 <  8 && fy+1 <  8 && color*pieces[fx+2][fy+1] <= 0) legal[fx+2][fy+1] = 1;
        if (fx+2 <  8 && fy-1 >= 0 && color*pieces[fx+2][fy-1] <= 0) legal[fx+2][fy-1] = 1;
        if (fx-1 >= 0 && fy+2 <  8 && color*pieces[fx-1][fy+2] <= 0) legal[fx-1][fy+2] = 1;
        if (fx-1 >= 0 && fy-2 >= 0 && color*pieces[fx-1][fy-2] <= 0) legal[fx-1][fy-2] = 1;
        if (fx-2 >= 0 && fy+1 <  8 && color*pieces[fx-2][fy+1] <= 0) legal[fx-2][fy+1] = 1;
        if (fx-2 >= 0 && fy-1 >= 0 && color*pieces[fx-2][fy-1] <= 0) legal[fx-2][fy-1] = 1;
        return;
    }

    if (type == KING) {
        if (fx-1 >= 0 && fy-1 >= 0 && !pieces[fx-1][fy-1]) legal[fx-1][fy-1] = 1;
        if (fx-1 >= 0              && !pieces[fx-1][fy])   legal[fx-1][fy]   = 1;
        if (fx-1 >= 0 && fy+1 <  8 && !pieces[fx-1][fy+1]) legal[fx-1][fy+1] = 1;
        if (             fy-1 >= 0 && !pieces[fx][fy-1])   legal[fx][fy-1]   = 1;
        if (             fy+1 <  8 && !pieces[fx][fy+1])   legal[fx][fy+1]   = 1;
        if (fx+1 <  8 && fy-1 >= 0 && !pieces[fx+1][fy-1]) legal[fx+1][fy-1] = 1;
        if (fx+1 <  8              && !pieces[fx+1][fy])   legal[fx+1][fy]   = 1;
        if (fx+1 <  8 && fy+1 <  8 && !pieces[fx+1][fy+1]) legal[fx+1][fy+1] = 1;
        return;
    }

    if (type == ROOK || type == QUEEN) {
        for (int i = 1; fx+i < 8; ++i) {
            legal[fx+i][fy] = color*pieces[fx+i][fy] <= 0;
            if (pieces[fx+i][fy]) break;
        }
        for (int i = 1; fx-i >= 0; ++i) {
            legal[fx-i][fy] = color*pieces[fx-i][fy] <= 0;
            if (pieces[fx-i][fy]) break;
        }
        for (int i = 1; fy+i < 8; ++i) {
            legal[fx][fy+i] = color*pieces[fx][fy+i] <= 0;
            if (pieces[fx][fy+i]) break;
        }
        for (int i = 1; fy-i >= 0; ++i) {
            legal[fx][fy-i] = color*pieces[fx][fy-i] <= 0;
            if (pieces[fx][fy-i]) break;
        }
    }

    if (type == BISHOP || type == QUEEN) {
        for (int i = 1; fx+i < 8 && fy+i < 8; ++i) {
            legal[fx+i][fy+i] = color*pieces[fx+i][fy+i] <= 0;
            if (pieces[fx+i][fy+i]) break;
        }
        for (int i = 1; fx+i < 8 && fy-i >= 0; ++i) {
            legal[fx+i][fy-i] = color*pieces[fx+i][fy-i] <= 0;
            if (pieces[fx+i][fy-i]) break;
        }
        for (int i = 1; fx-i >= 0 && fy+i < 8; ++i) {
            legal[fx-i][fy+i] = color*pieces[fx-i][fy+i] <= 0;
            if (pieces[fx-i][fy+i]) break;
        }
        for (int i = 1; fx-i >= 0 && fy-i >= 0; ++i) {
            legal[fx-i][fy-i] = color*pieces[fx-i][fy-i] <= 0;
            if (pieces[fx-i][fy-i]) break;
        }
    }
}

static gboolean mouse_pressed(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget; (void)data;

    if (event->type != GDK_BUTTON_PRESS) return TRUE;

    if (event->button == 1) {
        click_x = event->x / 64;
        click_y = event->y / 64;
        if (click_x < 8 && click_y < 8 && pieces[click_x][click_y] * (nhist%2*2-1) < 0) {
            clicked = pieces[click_x][click_y];
            update_legal(abs(clicked), (clicked > 0) - (clicked < 0), click_x, click_y);
            gtk_widget_queue_draw_area(GTK_WIDGET(board), 0, 0, 512, 512);
        }
    } else if (event->button == 3 && nhist) {
        memcpy(pieces, hist[--nhist], sizeof pieces);
        free(hist[nhist]);
        cur_node = cur_node->parent;
        update_moves();
        gtk_widget_queue_draw_area(GTK_WIDGET(board), 0, 0, 512, 512);
    }

    return TRUE;
}

static gboolean mouse_moved(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    (void)widget; (void)data;

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
    (void)widget; (void)event; (void)data;

    if (clicked) {
        if (legal[hover_x][hover_y]) {
            hist = realloc(hist, ++nhist * sizeof *hist);
            hist[nhist-1] = malloc(sizeof pieces);
            memcpy(hist[nhist-1], pieces, sizeof pieces);

            pieces[hover_x][hover_y] = clicked;
            pieces[click_x][click_y] = 0;

            struct move *prev = NULL;
            for (struct move *m = cur_node->child; m; prev = m, m = m->next) {
                if (m->from == SQ(click_x, click_y) && m->to == SQ(hover_x, hover_y)) {
                    cur_node = m;
                    goto done;
                }
            }

            struct move *new_move = new_node();
            new_move->parent = cur_node;
            new_move->from = SQ(click_x, click_y);
            new_move->to = SQ(hover_x, hover_y);
            new_move->desc = "this is a description";
            if (prev) prev->next = new_move;
            else cur_node->child = new_move;

            cur_node = new_move;
            done:

            save_db();
            update_moves();
        }

        clicked = 0;
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
                legal[i][j] = 0;
            }
        }
    }

    gtk_widget_queue_draw_area(GTK_WIDGET(board), 0, 0, 512, 512);

    return TRUE;
}

static gboolean draw_board(GtkWidget *widget, cairo_t *cr, gpointer data) {
    (void)widget; (void)data;

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

            // draw indicator if we can move here
            if (legal[i][j]) {
                cairo_set_source_rgb(cr, 0.2, 0.6, 0.1);
                cairo_arc(cr, i*64+32, j*64+32, 30, 0, 2*M_PI);
                cairo_stroke(cr);
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

static void initialize_pieces() {
    for (int i = 0; i < 8; ++i) {
        pieces[i][1] = -PAWN;
        pieces[i][6] = +PAWN;
    }

    pieces[0][0] = pieces[7][0] = -ROOK;
    pieces[0][7] = pieces[7][7] = +ROOK;

    pieces[1][0] = pieces[6][0] = -KNIGHT;
    pieces[1][7] = pieces[6][7] = +KNIGHT;

    pieces[2][0] = pieces[5][0] = -BISHOP;
    pieces[2][7] = pieces[5][7] = +BISHOP;

    pieces[3][0] = -QUEEN;
    pieces[3][7] = +QUEEN;

    pieces[4][0] = -KING;
    pieces[4][7] = +KING;
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

    board = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "board"));
    gtk_widget_set_size_request(GTK_WIDGET(board), 512, 512);
    g_signal_connect(board, "draw", G_CALLBACK(draw_board), NULL);

    moves = GTK_GRID(gtk_builder_get_object(builder, "moves"));
    gtk_widget_set_size_request(GTK_WIDGET(gtk_builder_get_object(builder, "scroll")), 256, 512);
    initialize_db();
    update_moves();

    initialize_images();
    initialize_pieces();

    gtk_widget_show_all(GTK_WIDGET(win));

    gtk_main();
}
