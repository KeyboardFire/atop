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
#include <math.h>
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

#define ADD_CLASS(x,k) gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(x)), (k))
#define DEL_CLASS(x,k) gtk_style_context_remove_class(gtk_widget_get_style_context(GTK_WIDGET(x)), (k))

static GtkDrawingArea *board;
static GtkGrid *moves;

static int click_x, click_y, hover_x, hover_y;
static float offset_x, offset_y;
static struct move *hover_move;

static int pieces[8][8];
static int legal[8][8];
static int clicked;

gulong focus_signal;

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
                    new->from = buf[idx];
                    if (child) new->parent = cur, cur->child = new, cur = new;
                    else new->parent = cur->parent, cur->next = new, cur = new;
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
                       len = strnlen((const char*)buf + idx, max),
                       prevlen = cur->desc ? strlen(cur->desc) : 0;

                cur->desc = realloc(cur->desc, prevlen + len + 1);
                strncpy(cur->desc + prevlen, (const char*)buf + idx, len);
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

static void save_edit(GtkTextView *text, struct move *move) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(text);
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buf, &start);
    gtk_text_buffer_get_end_iter(buf, &end);
    gchar *desc = gtk_text_buffer_get_text(buf, &start, &end, TRUE);

    g_signal_handler_disconnect(text, focus_signal);
    gtk_widget_destroy(GTK_WIDGET(text));

    if (move->parent == cur_node->parent) {
        GtkGrid *grid = GTK_GRID(gtk_widget_get_ancestor(GTK_WIDGET(text), GTK_TYPE_GRID));
        GtkLabel *lbl = GTK_LABEL(gtk_label_new(desc));
        ADD_CLASS(lbl, "desc");
        gtk_label_set_line_wrap(lbl, TRUE);
        gtk_label_set_xalign(lbl, 0);
        gtk_grid_attach(grid, GTK_WIDGET(lbl), 0, 1, 1, 1);
        gtk_widget_show(GTK_WIDGET(lbl));
    }

    free(move->desc);
    move->desc = desc;
    save_db();
}

static gboolean save_edit_focus(GtkWidget *text, GdkEventFocus *event, gpointer data) {
    (void)event;
    save_edit(GTK_TEXT_VIEW(text), data);
    return TRUE;
}

static gboolean save_edit_key(GtkWidget *text, GdkEventKey *event, gpointer data) {
    if (event->keyval == GDK_KEY_Return) {
        save_edit(GTK_TEXT_VIEW(text), data);
        return TRUE;
    }
    return FALSE;
}

static GtkTextView* request_edit(struct move *move, GtkGrid *parent, int y) {
    GtkTextView *text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(text), move->desc, -1);
    gtk_text_view_set_wrap_mode(text, GTK_WRAP_WORD_CHAR);
    gtk_grid_attach(parent, GTK_WIDGET(text), 0, y, 1, 1);
    gtk_widget_set_size_request(GTK_WIDGET(text), 256, 0);
    gtk_widget_show(GTK_WIDGET(text));
    gtk_widget_grab_focus(GTK_WIDGET(text));
    focus_signal = g_signal_connect(text, "focus_out_event", G_CALLBACK(save_edit_focus), move);
    g_signal_connect(text, "key_press_event", G_CALLBACK(save_edit_key), move);
    return text;
}

static gboolean edit_move(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)event;
    GtkGrid *grid = GTK_GRID(gtk_widget_get_ancestor(widget, GTK_TYPE_GRID));

    GList *children = gtk_container_get_children(GTK_CONTAINER(grid));
    gtk_widget_destroy(GTK_WIDGET(children->data));
    g_list_free(children);

    request_edit(data, grid, 1);

    return TRUE;
}

static void perform_move(int fx, int fy, int tx, int ty);
static gboolean move_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget; (void)event;
    if (event->button == 1) {
        struct move *move = (struct move*)data;
        perform_move(X(move->from), Y(move->from), X(move->to), Y(move->to));
        hover_move = NULL;
        gtk_widget_queue_draw_area(GTK_WIDGET(board), 0, 0, 512, 512);
        return TRUE;
    } else return FALSE;
}

static gboolean move_entered(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    (void)event;
    hover_move = data;
    ADD_CLASS(widget, "hover");
    gtk_widget_queue_draw_area(GTK_WIDGET(board), 0, 0, 512, 512);
    return TRUE;
}

static gboolean move_left(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    (void)event; (void)data;
    hover_move = NULL;
    DEL_CLASS(widget, "hover");
    gtk_widget_queue_draw_area(GTK_WIDGET(board), 0, 0, 512, 512);
    return TRUE;
}

static char* algebraic(int fx, int fy, int tx, int ty) {
    char *buf = malloc(10);
    int idx = 0;
    int type = abs(pieces[fx][fy]);
    if (type != 1) buf[idx++] = "  NBRQK"[type];
    if (pieces[tx][ty]) buf[idx++] = 'x';
    buf[idx++] = 'a' + tx;
    buf[idx++] = '8' - ty;
    buf[idx] = '\0';
    return buf;
}

static void update_moves() {
    gtk_container_foreach(GTK_CONTAINER(moves), (GtkCallback)gtk_widget_destroy, NULL);

    for (struct move *m = cur_node->child; m; m = m->next) {
        GtkGrid *container = GTK_GRID(gtk_grid_new());
        GtkOverlay *overlay = GTK_OVERLAY(gtk_overlay_new());

        char *header = algebraic(X(m->from), Y(m->from), X(m->to), Y(m->to));
        GtkLabel *head = GTK_LABEL(gtk_label_new(header));
        gtk_widget_set_size_request(GTK_WIDGET(head), 256, 0);
        ADD_CLASS(head, "head");
        free(header);

        GtkEventBox *btn = GTK_EVENT_BOX(gtk_event_box_new());
        ADD_CLASS(btn, "editbtn");
        gtk_container_add(GTK_CONTAINER(btn), gtk_image_new_from_file("img/edit.png"));
        gtk_widget_set_halign(GTK_WIDGET(btn), GTK_ALIGN_END);
        g_signal_connect(btn, "button_press_event", G_CALLBACK(edit_move), m);

        gtk_container_add(GTK_CONTAINER(overlay), GTK_WIDGET(head));
        gtk_overlay_add_overlay(overlay, GTK_WIDGET(btn));
        gtk_grid_attach(container, GTK_WIDGET(overlay), 0, 0, 1, 1);

        GtkLabel *txt = GTK_LABEL(gtk_label_new(m->desc));
        ADD_CLASS(txt, "desc");
        gtk_label_set_line_wrap(txt, TRUE);
        gtk_label_set_xalign(txt, 0);
        gtk_grid_attach(container, GTK_WIDGET(txt), 0, 1, 1, 1);

        GtkEventBox *box = GTK_EVENT_BOX(gtk_event_box_new());
        gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(container));
        gtk_grid_attach_next_to(moves, GTK_WIDGET(box), NULL, GTK_POS_BOTTOM, 1, 1);
        g_signal_connect(box, "button_press_event", G_CALLBACK(move_clicked), m);
        g_signal_connect(box, "enter_notify_event", G_CALLBACK(move_entered), m);
        g_signal_connect(box, "leave_notify_event", G_CALLBACK(move_left), NULL);
    }

    gtk_widget_show_all(GTK_WIDGET(moves));
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

static void perform_move(int fx, int fy, int tx, int ty) {
    hist = realloc(hist, ++nhist * sizeof *hist);
    hist[nhist-1] = malloc(sizeof pieces);
    memcpy(hist[nhist-1], pieces, sizeof pieces);

    if (pieces[tx][ty]) {
        pieces[tx][ty] = 0;
        if (tx-1 >= 0 && ty-1 >= 0 && abs(pieces[tx-1][ty-1]) != 1) pieces[tx-1][ty-1] = 0;
        if (tx-1 >= 0              && abs(pieces[tx-1][ty])   != 1) pieces[tx-1][ty]   = 0;
        if (tx-1 >= 0 && ty+1 <  8 && abs(pieces[tx-1][ty+1]) != 1) pieces[tx-1][ty+1] = 0;
        if (             ty-1 >= 0 && abs(pieces[tx][ty-1])   != 1) pieces[tx][ty-1]   = 0;
        if (             ty+1 <  8 && abs(pieces[tx][ty+1])   != 1) pieces[tx][ty+1]   = 0;
        if (tx+1 <  8 && ty-1 >= 0 && abs(pieces[tx+1][ty-1]) != 1) pieces[tx+1][ty-1] = 0;
        if (tx+1 <  8              && abs(pieces[tx+1][ty])   != 1) pieces[tx+1][ty]   = 0;
        if (tx+1 <  8 && ty+1 <  8 && abs(pieces[tx+1][ty+1]) != 1) pieces[tx+1][ty+1] = 0;
    } else pieces[tx][ty] = pieces[fx][fy];
    pieces[fx][fy] = 0;

    struct move *prev = NULL;
    for (struct move *m = cur_node->child; m; prev = m, m = m->next) {
        if (m->from == SQ(fx, fy) && m->to == SQ(tx, ty)) {
            cur_node = m;
            update_moves();
            return;
        }
    }

    struct move *new_move = new_node();
    new_move->parent = cur_node;
    new_move->from = SQ(fx, fy);
    new_move->to = SQ(tx, ty);
    new_move->desc = calloc(1, 1);
    if (prev) prev->next = new_move;
    else cur_node->child = new_move;

    cur_node = new_move;
    save_db();

    update_moves();
    request_edit(new_move, moves, 0);
}

static gboolean mouse_pressed(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget; (void)data;

    if (event->type == GDK_BUTTON_PRESS && event->button == 3 && nhist) {
        memcpy(pieces, hist[--nhist], sizeof pieces);
        free(hist[nhist]);
        cur_node = cur_node->parent;
        update_moves();
        gtk_widget_queue_draw_area(GTK_WIDGET(board), 0, 0, 512, 512);
        return TRUE;
    }

    return FALSE;
}

static gboolean board_pressed(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget; (void)data;

    if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
        click_x = event->x / 64;
        click_y = event->y / 64;
        if (click_x < 8 && click_y < 8 && pieces[click_x][click_y] * (nhist%2*2-1) < 0) {
            clicked = pieces[click_x][click_y];
            update_legal(abs(clicked), (clicked > 0) - (clicked < 0), click_x, click_y);
            gtk_widget_queue_draw_area(GTK_WIDGET(board), 0, 0, 512, 512);
        }
        return TRUE;
    }

    return FALSE;
}

static gboolean board_moved(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
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

static gboolean board_released(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget; (void)event; (void)data;

    if (clicked) {
        if (legal[hover_x][hover_y]) {
            perform_move(click_x, click_y, hover_x, hover_y);
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
                cairo_set_source_rgb(cr, 0.2, 0.2, 0.4);
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

    // draw arrow indicating prospective move, if any
    if (hover_move) {
        int fx = X(hover_move->from)*64+32, fy = Y(hover_move->from)*64+32,
            tx = X(hover_move->to)*64+32, ty = Y(hover_move->to)*64+32;
        double angle = atan2(ty-fy, tx-fx);

        // draw line
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.4);
        cairo_set_line_width(cr, 5);
        cairo_move_to(cr, fx, fy);
        cairo_line_to(cr, tx, ty);
        cairo_stroke(cr);

        // draw arrowhead
        double x = tx + 10*cos(angle), y = ty + 10*sin(angle);
        cairo_move_to(cr, x, y);
        cairo_line_to(cr, x - 30*cos(angle+0.3), y - 30*sin(angle+0.3));
        cairo_line_to(cr, x - 30*cos(angle-0.3), y - 30*sin(angle-0.3));
        cairo_fill(cr);
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
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_DIALOG);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "src/builder.css", NULL);
    gtk_style_context_add_provider_for_screen(
            gdk_display_get_default_screen(gdk_display_get_default()),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_widget_add_events(GTK_WIDGET(win), GDK_BUTTON_PRESS_MASK);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(win, "button_press_event", G_CALLBACK(mouse_pressed), NULL);

    board = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "board"));
    gtk_widget_set_size_request(GTK_WIDGET(board), 512, 512);
    gtk_widget_add_events(GTK_WIDGET(board),
            GDK_BUTTON_PRESS_MASK |
            GDK_POINTER_MOTION_MASK |
            GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(board, "draw", G_CALLBACK(draw_board), NULL);
    g_signal_connect(board, "button_press_event", G_CALLBACK(board_pressed), NULL);
    g_signal_connect(board, "motion_notify_event", G_CALLBACK(board_moved), NULL);
    g_signal_connect(board, "button_release_event", G_CALLBACK(board_released), NULL);

    initialize_db();
    initialize_images();
    initialize_pieces();

    moves = GTK_GRID(gtk_builder_get_object(builder, "moves"));
    gtk_grid_set_row_spacing(moves, 20);
    gtk_widget_set_size_request(GTK_WIDGET(gtk_builder_get_object(builder, "scroll")), 256, 512);
    update_moves();

    gtk_widget_show_all(GTK_WIDGET(win));

    gtk_main();
}
