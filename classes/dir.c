// porres 2017

#include <dirent.h>

#include "m_pd.h"
#include "g_canvas.h"
#include <string.h>

static t_class *dir_class;

typedef struct dir{
    t_object x_obj;
    DIR      *x_dir;
    char      x_directory[MAXPDSTRING];
    t_symbol *x_getdir;
}t_dir;

static void dir_open(t_dir *x, t_symbol *dirname){
    if(!opendir(dirname->s_name))
        pd_error(x, "filedir: cannot open directory: %s", dirname->s_name);
    else{
        strncpy(x->x_directory, dirname->s_name, MAXPDSTRING);
        x->x_dir = opendir(dirname->s_name);
    }
}

static void dir_dump(t_dir *x){
    rewinddir(x->x_dir);
    struct dirent *result = NULL;
    while(result = readdir(x->x_dir))
        outlet_symbol(x->x_obj.ob_outlet, gensym(result->d_name));
    rewinddir(x->x_dir);
}

static void dir_reset(t_dir *x){
    x->x_dir = opendir(x->x_getdir->s_name);
    strncpy(x->x_directory, x->x_getdir->s_name, MAXPDSTRING);
}

static void dir_bang(t_dir *x){
    outlet_symbol(x->x_obj.ob_outlet, gensym(x->x_directory));
}

static void *dir_new(void){
    t_dir *x = (t_dir *)pd_new(dir_class);
    t_canvas *canvas = canvas_getcurrent();
    x->x_getdir = canvas_getdir(canvas);
    dir_reset(x);
    outlet_new(&x->x_obj, &s_symbol);
    return (x);
}

void dir_setup(void){
    dir_class = class_new(gensym("dir"), (t_newmethod)dir_new, 0, sizeof(t_dir), 0, 0);
    class_addbang(dir_class, dir_bang);
    class_addmethod(dir_class, (t_method)dir_dump, gensym("dump"), 0);
    class_addmethod(dir_class, (t_method)dir_reset, gensym("reset"), 0);
    class_addmethod(dir_class, (t_method)dir_open, gensym("open"), A_DEFSYMBOL, 0);
}
