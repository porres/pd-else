// Porres 2026

#include <m_pd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "nlist.h"

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

static t_nlist_node *nlist_new_list(void){
    t_nlist_node *node = (t_nlist_node *)getbytes(sizeof(*node));
    node->type = 1;
    node->child = NULL;
    node->next = NULL;
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
    return(node);
}

t_nlist_node *nlist_parse_all(t_nlist *x, int ac, t_atom *av){
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
                    pd_error(x, "[nlist]: unexpected ']'");
                    warned_extra = 1;
                }
                break;
            }
            stack[depth].owner->child = stack[depth].first;
            depth--;
        }
    }
    if(depth > 0){
        pd_error(x, "[nlist]: missing ']'");
        while(depth > 0){
            stack[depth].owner->child = stack[depth].first;
            depth--;
        }
    }
    t_nlist_node *root = stack[0].first;
    x->x_len = len;
    x->x_depth = maxdepth;
    freebytes(stack, sizeof(t_nlist_frame) * capacity);
    return(root);
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

static int nlist_has_list(t_nlist_node *node){
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
        else
            else_editor_setdirty(x->x_filehandle, 0);
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

void nlist_clear_nodes(t_nlist_node *node){
    while(node){
        t_nlist_node *next = node->next;
        if(node->type == 1)
            nlist_clear_nodes(node->child);
        freebytes(node, sizeof(*node));
        node = next;
    }
}
