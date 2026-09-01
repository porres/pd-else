// porres 2026

#ifndef NLIST_H
#define NLIST_H

#include <m_pd.h>
#include "elsefile.h"

#define NLIST_STACK_INIT 16

typedef struct _nlist_node t_nlist_node;
typedef struct _nlist t_nlist;

struct _nlist_node{
    int type;
    t_atom atom;
    t_nlist_node *child;
    t_nlist_node *next;
};

struct _nlist{
    t_object x_obj;
    t_canvas *x_canvas;
    t_symbol *x_name;
    t_nlist_node *x_root;
    int x_len;
    int x_depth;
    t_elsefile *x_filehandle;
    t_symbol *x_bindsym;
    t_nlist *x_next;
    int x_is_opened;
    int x_keep;
};

typedef struct _nlist_frame{
    t_nlist_node *owner;
    t_nlist_node *first;
    t_nlist_node *last;
} t_nlist_frame;

t_nlist *nlist_get(t_symbol *name, t_symbol *obj);
t_nlist_node *nlist_parse_all(t_nlist *x, int ac, t_atom *av);
void nlist_clear_nodes(t_nlist_node *node);
void nlist_post_indented(int depth, const char *content);
void nlist_print_nodes(t_nlist_node *node, int depth);
void nlist_editor_show_nodes(t_elsefile *fh, t_nlist_node *node, int depth);
void nlist_do_update(t_nlist *x);
void nlist_open(t_nlist *x);

#endif // NLIST_H
