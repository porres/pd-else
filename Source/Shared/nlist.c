// Porres 2026

#include "nlist.h"
#include <m_pd.h>
#include <g_canvas.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Every node gets a unique ID at creation. IDs are never reused (unlike
// addresses, which getbytes/freebytes happily recycle), so an ID is a safe
// way for an iterator to remember "where it was" across a mutation.
// 0 is reserved as "none".
static unsigned long nlist_next_id = 1;

t_nlist *nlist_get(t_symbol *name, t_symbol *obj){
    if(name == NULL){
        post("[nlist.%s] no name given", obj->s_name);
        return(NULL);
    }
    t_nlist *nlist = (t_nlist *)pd_findbyclassname(name, gensym("nlist"));
    t_nlist *nl2 = (t_nlist *)pd_findbyclassname(name, gensym("nlist.define"));
    if(nlist && nl2)
        post("warning %s multiply defined", name->s_name);
    if(!nlist && nl2)
        nlist = nl2;
    if(!nlist)
        post("[nlist.%s] \"%s\" name not found", obj->s_name, name->s_name);
    return(nlist);
}

// Silent lookup, used by iterators to re-resolve the list on every step
// (the [nlist.define] could have been deleted by whatever we output to).
t_nlist *nlist_find(t_symbol *name){
    if(name == NULL)
        return(NULL);
    t_nlist *nlist = (t_nlist *)pd_findbyclassname(name, gensym("nlist"));
    if(!nlist)
        nlist = (t_nlist *)pd_findbyclassname(name, gensym("nlist.define"));
    return(nlist);
}

t_nlist_node *nlist_new_list(void){
    t_nlist_node *node = (t_nlist_node *)getbytes(sizeof(*node));
    node->type = 1;
    node->child = NULL;
    node->next = NULL;
    node->id = nlist_next_id++;
    return(node);
}

static void nlist_frame_append(t_nlist_frame *frame, t_nlist_node *node){
    if(!frame->first)
        frame->first = node;
    else
        frame->last->next = node;
    frame->last = node;
}

static t_nlist_node *nlist_new_atom(t_atom *a){
    t_nlist_node *node = (t_nlist_node *)getbytes(sizeof(*node));
    node->type = 0;
    node->atom = *a;
    node->child = NULL;
    node->next = NULL;
    node->id = nlist_next_id++;
    return(node);
}

// Parses a bracket-tagged atom stream into a tree. `owner` is only used to
// report errors (pass the calling object). len/depth are optional outputs.
t_nlist_node *nlist_parse(void *owner, int ac, t_atom *av, int *len_out, int *depth_out){
    int capacity = NLIST_STACK_INIT;
    t_nlist_frame *stack = (t_nlist_frame *)getbytes(sizeof(t_nlist_frame) * capacity);
    int depth = 0;
    int len = 0;
    int maxdepth = 0;
    stack[0].owner = NULL;
    stack[0].first = stack[0].last = NULL;
    int warned_extra = 0;
    for(int i = 0; i < ac; i++){
        t_atom *a = av + i;
        int nopen = 0, nclose = 0;
        t_atom inner;
        int has_inner = 0;
        if(a->a_type == A_SYMBOL){
            const char *s = a->a_w.w_symbol->s_name;
            int slen = (int)strlen(s);
            while(nopen < slen && s[nopen] == '[')
                nopen++;
            while(nclose < (slen - nopen) && s[slen - 1 - nclose] == ']')
                nclose++;
            int size = slen - nopen - nclose;
            if(size > 0){
                char buf[MAXPDSTRING];
                int copy = size >= MAXPDSTRING ? MAXPDSTRING - 1 : size;
                memcpy(buf, s + nopen, copy);
                buf[copy] = 0;
                char *endptr;
                double f = strtod(buf, &endptr);
                if(*buf && *endptr == 0)
                    SETFLOAT(&inner, f);
                else
                    SETSYMBOL(&inner, gensym(buf));
                has_inner = 1;
            }
        }
        else{
            inner = *a;
            has_inner = 1;
        }
        for(int k = 0; k < nopen; k++){
            t_nlist_node *listnode = nlist_new_list();
            nlist_frame_append(&stack[depth], listnode);
            if(depth == 0)
                len++;
            depth++;
            if(depth > maxdepth)
                maxdepth = depth;
            if(depth >= capacity){
                int newcap = capacity * 2;
                t_nlist_frame *newstack =
                    (t_nlist_frame *)getbytes(sizeof(t_nlist_frame) * newcap);
                memcpy(newstack, stack, sizeof(t_nlist_frame) * capacity);
                freebytes(stack, sizeof(t_nlist_frame) * capacity);
                stack = newstack;
                capacity = newcap;
            }
            stack[depth].owner = listnode;
            stack[depth].first = stack[depth].last = NULL;
        }
        if(has_inner && !(inner.a_type == A_SYMBOL && inner.a_w.w_symbol->s_name[0] == 0)){
            nlist_frame_append(&stack[depth], nlist_new_atom(&inner));
            if(depth == 0)
                len++;
        }
        for(int k = 0; k < nclose; k++){
            if(depth == 0){
                if(!warned_extra){
                    pd_error(owner, "[nlist]: unexpected ']'");
                    warned_extra = 1;
                }
                break;
            }
            stack[depth].owner->child = stack[depth].first;
            depth--;
        }
    }
    if(depth > 0){
        pd_error(owner, "[nlist]: missing ']'");
        while(depth > 0){
            stack[depth].owner->child = stack[depth].first;
            depth--;
        }
    }
    t_nlist_node *root = stack[0].first;
    if(len_out)
        *len_out = len;
    if(depth_out)
        *depth_out = maxdepth;
    freebytes(stack, sizeof(t_nlist_frame) * capacity);
    return(root);
}

// Same as nlist_parse(), but stores len/depth into the given nlist.
t_nlist_node *nlist_parse_all(t_nlist *x, int ac, t_atom *av){
    return(nlist_parse(x, ac, av, &x->x_len, &x->x_depth));
}

// ------------ Tree / editor formatting ---------------------------------
static void nlist_buf_append(char *buf, size_t bufsize, size_t *pos, const char *str){
    if(*pos + 1 >= bufsize)
        return;
    size_t len = strlen(str);
    size_t avail = bufsize - 1 - *pos;
    size_t copy = len < avail ? len : avail;
    memcpy(buf + *pos, str, copy);
    *pos += copy;
    buf[*pos] = 0;
}

void nlist_post_indented(int depth, const char *content){
    char buf[MAXPDSTRING];
    size_t pos = 0;
    buf[0] = 0;
    for(int i = 0; i < depth; i++)
        nlist_buf_append(buf, sizeof(buf), &pos, "    ");
    nlist_buf_append(buf, sizeof(buf), &pos, content);
    post("%s", buf);
}

static void nlist_atom_to_buf(t_atom *a, char *buf, size_t bufsize){
    if(a->a_type == A_FLOAT)
        snprintf(buf, bufsize, "%g", atom_getfloat(a));
    else if(a->a_type == A_SYMBOL)
        snprintf(buf, bufsize, "%s", atom_getsymbol(a)->s_name);
    else
        buf[0] = 0;
}

static void nlist_inline_to_buf(t_nlist_node *node, char *buf, size_t bufsize, size_t *pos){
    nlist_buf_append(buf, bufsize, pos, "[");
    int first = 1;
    while(node){
        if(!first)
            nlist_buf_append(buf, bufsize, pos, " ");
        if(node->type == 1)
            nlist_inline_to_buf(node->child, buf, bufsize, pos);
        else{
            char tmp[64];
            nlist_atom_to_buf(&node->atom, tmp, sizeof(tmp));
            nlist_buf_append(buf, bufsize, pos, tmp);
        }
        first = 0;
        node = node->next;
    }
    nlist_buf_append(buf, bufsize, pos, "]");
}

static void nlist_editor_line(t_elsefile *fh, int depth, const char *content){
    char buf[MAXPDSTRING];
    size_t pos = 0;
    buf[0] = 0;
    for(int i = 0; i < depth; i++)
        nlist_buf_append(buf, sizeof(buf), &pos, "    ");
    nlist_buf_append(buf, sizeof(buf), &pos, content);
    nlist_buf_append(buf, sizeof(buf), &pos, "\n");
    else_editor_append(fh, buf);
}

int nlist_has_list(t_nlist_node *node){
    while(node){
        if(node->type == 1)
            return(1);
        node = node->next;
    }
    return(0);
}

void nlist_editor_show_nodes(t_elsefile *fh, t_nlist_node *node, int depth){
    while(node){
        if(node->type == 1 && nlist_has_list(node->child)){
            nlist_editor_line(fh, depth, "[");
            nlist_editor_show_nodes(fh, node->child, depth + 1);
            nlist_editor_line(fh, depth, "]");
        }
        else{
            char buf[MAXPDSTRING];
            size_t pos = 0;
            buf[0] = 0;
            if(node->type == 1)
                nlist_inline_to_buf(node->child, buf, sizeof(buf), &pos);
            else
                nlist_atom_to_buf(&node->atom, buf, sizeof(buf));
            nlist_editor_line(fh, depth, buf);
        }
        node = node->next;
    }
}

void nlist_dirty(t_nlist *x){
    if(x->x_keep && x->x_canvas)
        canvas_dirty(x->x_canvas, 1);
}

void nlist_do_update(t_nlist *x){
    if(x->x_is_opened && x->x_filehandle){
        t_nlist_node *root = x->x_root;
        char buf[256];
        snprintf(buf, sizeof(buf),
            "if {[winfo exists .%lx]} { .%lx.text delete 1.0 end }",
            (unsigned long)x->x_filehandle,
            (unsigned long)x->x_filehandle);
        pdgui_vmess(buf, NULL);
        if(root)
            nlist_editor_show_nodes(x->x_filehandle, root, 0);
        else_editor_setdirty(x->x_filehandle, 0);
    }
}

void nlist_open_window(unsigned long handle){
    char buf[256];
    snprintf(buf, sizeof(buf), "wm deiconify .%lx; raise .%lx; focus .%lx.text",
        handle, handle, handle);
    pdgui_vmess(buf, NULL);
}

void nlist_do_open(t_nlist *x){
    t_nlist_node *root = x->x_root;
    if(x->x_is_opened)
        nlist_open_window((unsigned long)x->x_filehandle);
    else{
        const char *title = (x->x_name ? x->x_name->s_name : "Untitled");
        elsefile_editor_open(x->x_filehandle, (char *)title, "nlist");
        if(root)
            nlist_editor_show_nodes(x->x_filehandle, root, 0);
        else_editor_setdirty(x->x_filehandle, 0);
        x->x_is_opened = 1;
    }
}

void nlist_open(t_nlist *x){
    char buf[512];
    snprintf(buf, sizeof(buf),
        "if {[winfo exists .%lx]} {"
        "pdsend \"%s _is_opened 1\""
        "} else {"
        "pdsend \"%s _is_opened 0\""
        "}",
        (unsigned long)x->x_filehandle,
        x->x_bindsym->s_name,
        x->x_bindsym->s_name);
    pdgui_vmess(buf, NULL);
}

// ------------ Serialization (inverse of nlist_parse_all) -----------------

// Reconstructs the flat, bracket-tagged atom stream that nlist_parse_all()
// would have consumed to build this same tree. Every sub-list is emitted as
// a standalone "[" atom, its children (recursively), then a standalone "]"
// atom. Since these bracket atoms carry no payload of their own (size == 0
// once stripped), nlist_parse_all() opens/closes a frame without pushing an
// extra element, so this round-trips through the "list" message exactly.
void nlist_tree_to_binbuf(t_nlist_node *node, t_binbuf *bb){
    t_atom a;
    while(node){
        if(node->type == 1){
            SETSYMBOL(&a, gensym("["));
            binbuf_add(bb, 1, &a);
            nlist_tree_to_binbuf(node->child, bb);
            SETSYMBOL(&a, gensym("]"));
            binbuf_add(bb, 1, &a);
        }
        else
            binbuf_add(bb, 1, &node->atom);
        node = node->next;
    }
}

void nlist_clear_nodes(t_nlist_node *node){
    while(node){
        t_nlist_node *next = node->next;
        if(node->type == 1)
            nlist_clear_nodes(node->child);
        freebytes(node, sizeof(*node));
        node = next;
    }
}

t_nlist_node **nlist_find_link(t_nlist_node **node, int index){
    while(*node && index > 0){
        node = &(*node)->next;
        index--;
    }
    return(node);
}

int nlist_count_leaves(t_nlist_node *node){
    int n = 0;
    while(node){
        if(node->type == 1){
            if(node->child)
                n += nlist_count_leaves(node->child);
            else
                n++;
        }
        else
            n++;
        node = node->next;
    }
    return(n);
}

int nlist_get_len(t_nlist_node *node){
    int n = 0;
    while(node){
        n++;
        node = node->next;
    }
    return(n);
}

int nlist_get_depth(t_nlist_node *node){
    int depth = 0;
    while(node){
        if(node->type == 1){
            int d = nlist_get_depth(node->child) + 1;
            if(d > depth)
                depth = d;
        }
        node = node->next;
    }
    return(depth);
}

// Inserts `contents` (a sibling chain) at the given path. On failure (return
// 0) nothing is linked into the tree and the caller still owns `contents`.
int nlist_insert_at(t_nlist *nlist, int path_ac, int *path, t_nlist_node *contents){
    int debug = 0;
    if(!nlist || !contents){
        if(debug) post("[nlist.insert_at] FAIL: missing nlist or contents");
        return(0);
    }
    if(debug){
        post("[nlist.insert_at] path:");
        for(int i = 0; i < path_ac; i++)
            post("  path[%d] = %d", i, path[i]);
    }
    t_nlist_node **link = &nlist->x_root;
    for(int i = 0; i < path_ac; i++){
        if(debug)
            post("[nlist.insert_at] level %d, index %d", i, path[i]);
        if(i == path_ac - 1)
            break;
        link = nlist_find_link(link, path[i]);
        if(!*link){
            if(i == path_ac - 2){
                if(debug)
                    post("[nlist.insert_at] creating missing child list at index 0");
                *link = nlist_new_list();
            }
            else{
                if(debug)
                    post("[nlist.insert_at] FAIL: path[%d] does not exist", i);
                return(0);
            }
        }
        if((*link)->type != 1){
            if(debug)
                post("[nlist.insert_at] FAIL: path[%d] is not a list", i);
            return(0);
        }
        link = &(*link)->child;
    }
    if(path_ac)
        link = nlist_find_link(link, path[path_ac - 1]);
    if(debug)
        post("[nlist.insert_at] target link = %p, target node = %p", (void *)link, (void *)*link);
    t_nlist_node *last = contents;
    while(last->next)
        last = last->next;
    last->next = *link;
    *link = contents;
    nlist->x_len = nlist_get_len(nlist->x_root);
    nlist->x_depth = nlist_get_depth(nlist->x_root);
    if(debug)
        post("[nlist.insert_at] SUCCESS");
    return(1);
}

int nlist_parse_path(int ac, t_atom *av, int **path_out, int *path_ac_out){
    if(!ac || !av)
        return(0);
    int *path = (int *)getbytes(sizeof(int) * ac);
    for(int i = 0; i < ac; i++){
        if(av[i].a_type != A_FLOAT){
            freebytes(path, sizeof(int) * ac);
            return(0);
        }
        int index = (int)atom_getfloat(av + i);
        if(index < 0){
            freebytes(path, sizeof(int) * ac);
            return(0);
        }
        path[i] = index;
    }
    *path_out = path;
    *path_ac_out = ac;
    return(1);
}

// ------------ Identity-based scanning / cursor ---------------------------
//
// nlist_scan() does one DFS over the live tree and finds the first node that
// matches the selector (see NLIST_SEL_*) at-or-after / strictly-after the node
// whose ID is `anchor` (anchor 0 = from the very beginning). It also reports
// the ID of the NEXT matching node (succ), so a caller can remember where it
// is going next by ID before it fires an outlet.
//
//   anchor != 0, inclusive = 1 : start AT the anchor node
//   anchor != 0, inclusive = 0 : start right AFTER the anchor node; the
//                                anchor's own subtree (if it grew children
//                                meanwhile) is skipped
//
// If the anchor ID no longer exists in the tree, nothing is found (returns 0).
// The path is written as floats into `path` (up to pathcap entries) and its
// length is depth + 1. Nothing is allocated for the caller and no outlet is
// ever fired in here, so the returned node pointer is valid until the caller
// next lets control leave the object.
typedef struct _nlist_scan{
    int mode;
    int target;
    unsigned long anchor;
    int inclusive;
    int passed;
    int found;
    int done;
    t_atom *path;
    int pathcap;
    int *stk;
    int cap;
    int depth;
    t_nlist_node *node;
    unsigned long id;
    unsigned long succ;
}t_nlist_scan;

static int nlist_scan_match(t_nlist_scan *s, t_nlist_node *n, int depth){
    switch(s->mode){
    case NLIST_SEL_LEAF:
        return(n->child == NULL);
    case NLIST_SEL_LEVEL:
        return(depth == s->target);
    case NLIST_SEL_PARENT:
        return(depth == s->target && n->child != NULL);
    }
    return(0);
}

static int nlist_scan_descend(t_nlist_scan *s, int depth){
    if(s->mode == NLIST_SEL_LEAF)
        return(1);
    return(depth < s->target);
}

static void nlist_scan_walk(t_nlist_scan *s, t_nlist_node *node, int depth){
    int index = 0;
    while(node && !s->done){
        if(depth < s->cap)
            s->stk[depth] = index;
        index++;
        int candidate = s->passed;
        int skip_children = 0;
        if(!s->passed && s->anchor && node->id == s->anchor){
            s->passed = 1;
            if(s->inclusive)
                candidate = 1;
            else
                skip_children = 1;
        }
        if(candidate && nlist_scan_match(s, node, depth)){
            if(!s->found){
                s->found = 1;
                s->node = node;
                s->id = node->id;
                s->depth = depth;
                if(s->path){
                    for(int i = 0; i <= depth && i < s->pathcap && i < s->cap; i++)
                        SETFLOAT(&s->path[i], (t_float)s->stk[i]);
                }
            }
            else{
                s->succ = node->id;
                s->done = 1;
            }
        }
        else if(!skip_children && node->child && nlist_scan_descend(s, depth))
            nlist_scan_walk(s, node->child, depth + 1);
        node = node->next;
    }
}

int nlist_scan(t_nlist_node *root, int mode, int target,
    unsigned long anchor, int inclusive,
    t_atom *path, int pathcap, int *depth_out, t_nlist_node **node_out,
    unsigned long *id_out, unsigned long *succ_out){
    t_nlist_scan s;
    memset(&s, 0, sizeof(s));
    s.mode = mode;
    s.target = target;
    s.anchor = anchor;
    s.inclusive = inclusive;
    s.passed = (anchor == 0);
    s.path = path;
    s.pathcap = pathcap;
    s.cap = nlist_get_depth(root) + 2;
    s.stk = (int *)getbytes(sizeof(int) * s.cap);
    nlist_scan_walk(&s, root, 0);
    freebytes(s.stk, sizeof(int) * s.cap);
    if(s.found){
        if(depth_out)
            *depth_out = s.depth;
        if(node_out)
            *node_out = s.node;
        if(id_out)
            *id_out = s.id;
        if(succ_out)
            *succ_out = s.succ;
    }
    return(s.found);
}

void nlist_cursor_init(t_nlist_cursor *c){
    c->started = 0;
    c->cur = 0;
    c->succ = 0;
}

// Advances the cursor and returns the next node to emit, or 0 when done.
// The list is re-resolved by name and re-scanned from the live root on every
// call, so mutations made by whatever the previous emission went through are
// always taken into account. Resume rules:
//   1. the node that was "next" when we last emitted still exists -> go there
//      (anything inserted/set between us and it is skipped: it is not part
//      of this pass)
//   2. otherwise, if the last emitted node still exists -> go to the node
//      after it
//   3. otherwise stop (we've lost our place; we never guess by position)
// If we were already on the last node when we last emitted, we stop.
//
// On success *path_out is allocated (cap_out entries, path length is
// depth_out + 1) and the caller must free it with
//   freebytes(path, cap * sizeof(t_atom))
// using the returned cap, not a re-read depth. node_out is optional and only
// valid until the caller fires an outlet.
int nlist_cursor_next(t_nlist_cursor *c, t_symbol *name, int mode, int target,
    t_atom **path_out, int *cap_out, int *depth_out, t_nlist_node **node_out){
    t_nlist *nlist = nlist_find(name);
    if(!nlist)
        return(0);
    if(c->started && !c->succ)
        return(0);
    t_nlist_node *root = nlist->x_root;
    int cap = nlist_get_depth(root) + 1;
    t_atom *path = (t_atom *)getbytes(sizeof(t_atom) * cap);
    int depth = 0;
    t_nlist_node *node = NULL;
    unsigned long id = 0, succ = 0;
    int found;
    if(!c->started)
        found = nlist_scan(root, mode, target, 0, 0, path, cap, &depth, &node, &id, &succ);
    else{
        found = nlist_scan(root, mode, target, c->succ, 1, path, cap, &depth, &node, &id, &succ);
        if(!found)
            found = nlist_scan(root, mode, target, c->cur, 0, path, cap, &depth, &node, &id, &succ);
    }
    if(!found){
        freebytes(path, sizeof(t_atom) * cap);
        return(0);
    }
    c->started = 1;
    c->cur = id;
    c->succ = succ;
    *path_out = path;
    *cap_out = cap;
    *depth_out = depth;
    if(node_out)
        *node_out = node;
    return(1);
}
