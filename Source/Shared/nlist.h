// porres 2026

#ifndef NLIST_H
#define NLIST_H

#include <m_pd.h>
#include <g_canvas.h>
#include "elsefile.h"

#define NLIST_STACK_INIT 16

// Selectors for nlist_scan() / nlist_cursor_next()
#define NLIST_SEL_LEAF   0  // every childless node (atoms and empty lists), any depth
#define NLIST_SEL_LEVEL  1  // every node at depth == target
#define NLIST_SEL_PARENT 2  // every node at depth == target that has children

typedef struct _nlist_node t_nlist_node;
typedef struct _nlist t_nlist;

struct _nlist_node{
    int type;
    t_atom atom;
    t_nlist_node *child;
    t_nlist_node *next;
    unsigned long id;   // unique, never reused, assigned at creation
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
    int x_filearg;
    int x_is_opened;
    int x_keep;
    t_symbol *x_readfile;      // Last read/written filename
};

typedef struct _nlist_frame{
    t_nlist_node *owner;
    t_nlist_node *first;
    t_nlist_node *last;
} t_nlist_frame;

// Identity-based iteration state. It holds node IDs only, never pointers,
// so it is safe to keep across outlet calls that mutate the list.
typedef struct _nlist_cursor{
    int started;
    unsigned long cur;   // ID of the node emitted last
    unsigned long succ;  // ID of the node that was next when it was emitted (0 = none)
} t_nlist_cursor;

t_nlist_node *nlist_new_list(void);
t_nlist_node **nlist_find_link(t_nlist_node **node, int index);
t_nlist *nlist_get(t_symbol *name, t_symbol *obj);
t_nlist *nlist_find(t_symbol *name);   // silent version of nlist_get
t_nlist_node *nlist_parse(void *owner, int ac, t_atom *av, int *len_out, int *depth_out);
t_nlist_node *nlist_parse_all(t_nlist *x, int ac, t_atom *av);
int nlist_get_len(t_nlist_node *node);
int nlist_get_depth(t_nlist_node *node);
int nlist_has_list(t_nlist_node *node);
int nlist_count_leaves(t_nlist_node *node);
int nlist_parse_path(int ac, t_atom *av, int **path_out, int *path_ac_out);
int nlist_insert_at(t_nlist *nlist, int path_ac, int *path, t_nlist_node *contents);
void nlist_tree_to_binbuf(t_nlist_node *node, t_binbuf *bb);
void nlist_clear_nodes(t_nlist_node *node);
void nlist_post_indented(int depth, const char *content);
void nlist_print_nodes(t_nlist_node *node, int depth);
void nlist_editor_show_nodes(t_elsefile *fh, t_nlist_node *node, int depth);
void nlist_do_update(t_nlist *x);
void nlist_open_window(unsigned long handle);
void nlist_do_open(t_nlist *x);
void nlist_open(t_nlist *x);
void nlist_dirty(t_nlist *x);

int nlist_scan(t_nlist_node *root, int mode, int target,
    unsigned long anchor, int inclusive,
    t_atom *path, int pathcap, int *depth_out, t_nlist_node **node_out,
    unsigned long *id_out, unsigned long *succ_out);
void nlist_cursor_init(t_nlist_cursor *c);
int nlist_cursor_next(t_nlist_cursor *c, t_symbol *name, int mode, int target,
    t_atom **path_out, int *cap_out, int *depth_out, t_nlist_node **node_out);

#endif // NLIST_H
