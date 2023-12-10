// porres 2022

#include "m_pd.h"
#include "g_canvas.h"

static t_class *findfile_class;

typedef struct _findfile{
    t_object    x_obj;
    t_canvas   *x_cv;
    t_int       x_i;
    t_outlet   *x_fail;
}t_findfile;

static void findfile_symbol(t_findfile *x, t_symbol *file){
    char path[MAXPDSTRING], *fn;
    int fd = canvas_open(x->x_cv, file->s_name, "", path, &fn, MAXPDSTRING, 1);
    if(fd >= 0){
        sys_close(fd);
        if(fn > path)
            fn[-1] = '/';
        outlet_symbol(x->x_obj.ob_outlet, gensym(path));
    }
    else
        outlet_symbol(x->x_fail, file);
}

static void findfile_anything(t_findfile *x, t_symbol *s, int ac, t_atom *av){
    ac = 0;
    av = NULL;
    findfile_symbol(x, s);
}

static void *findfile_new(t_floatarg f){
    t_findfile *x = (t_findfile *)pd_new(findfile_class);
    x->x_cv = canvas_getrootfor(canvas_getcurrent());
    int i = f < 0 ? 0 : (int)f;
    while(i-- && x->x_cv->gl_owner)
        x->x_cv = canvas_getrootfor(x->x_cv->gl_owner);
    outlet_new(&x->x_obj, &s_);
    x->x_fail = outlet_new(&x->x_obj, &s_);
    return(x);
}

void findfile_setup(void){
    findfile_class = class_new(gensym("findfile"), (t_newmethod)findfile_new,
        0, sizeof(t_findfile), 0, A_DEFFLOAT, 0);
    class_addsymbol(findfile_class, findfile_symbol);
    class_addanything(findfile_class, findfile_anything);
}
