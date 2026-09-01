// porres 2026

#ifndef DICT_H
#define DICT_H

#include <m_pd.h>
#include <g_canvas.h>
#include "elsefile.h"
#include "cJSON.h"

typedef struct _dict t_dict;

struct _dict{
    t_object x_obj;
    t_canvas *x_canvas;
    t_symbol *x_name;
    cJSON *x_root;
    t_elsefile *x_filehandle;
    t_symbol *x_bindsym;
    t_dict *x_next;
    int x_filearg;
    int x_is_opened;
    int x_keep;
    t_symbol *x_readfile;
};

t_dict *dict_get(t_symbol *name, t_symbol *obj);
void dict_do_update(t_dict *x);
void dict_open_window(unsigned long handle);
void dict_do_open(t_dict *x);
void dict_open(t_dict *x);
void dict_dirty(t_dict *x);

#endif // DICT_H
