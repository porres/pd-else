// porres 2017

#include <dirent.h>

#include "m_pd.h"
#include "g_canvas.h"
#include <string.h>
#include <math.h>

static t_class *dir_class;

typedef struct dir{
    t_object  x_obj;
    DIR      *x_dir;
    char      x_directory[MAXPDSTRING];
    t_symbol *x_getdir;
    t_int     x_nfiles;
    t_int     x_ignored;
    t_int     x_seek;
    t_outlet *x_out1;
    t_outlet *x_out2;
}t_dir;

static void dir_seek(t_dir *x, t_float f){
    t_int seek = (int)f;
    if(seek <= 0){
        pd_error(x, "dir: seek value cannot be <= 0");
        return;
    }
    seek = ((seek - 1) % x->x_nfiles) + 1;
    x->x_seek = seek;
    seek += x->x_ignored;
    rewinddir(x->x_dir);
    struct dirent *result = NULL;
    t_int i;
    for(i = 0; i < seek; i++)
            result = readdir(x->x_dir);
    outlet_symbol(x->x_out1, gensym(result->d_name));
    rewinddir(x->x_dir);
}

static void dir_next(t_dir *x){
    dir_seek(x, x->x_seek + 1);
}

static void dir_open(t_dir *x, t_symbol *dirname){
    if(!strcmp(dirname->s_name, "")){
        pd_error(x, "dir: no symbol given to 'open'");
        return;
    }
    char tempdir[MAXPDSTRING];
    strcpy(tempdir, x->x_directory);
    if(!strcmp(dirname->s_name, "..")){ // parent dir
        char *last_slash;
        last_slash = strrchr(x->x_directory, '/');
        *last_slash = '\0';
        if(!strcmp(x->x_directory, ""))
            strcpy(x->x_directory, "/");
    }
    else if(!strcmp(dirname->s_name, ".")){
        // do nothing / reopen same dir
    }
    else if(!strncmp(dirname->s_name, "/", 1)) // absolute path
        strncpy(x->x_directory, dirname->s_name, MAXPDSTRING);
    else // relative to current dir
        sprintf(x->x_directory, "%s/%s", x->x_directory, dirname->s_name );
    if(!opendir(x->x_directory)){
        pd_error(x, "dir: cannot open '%s'", dirname->s_name);
        strcpy(x->x_directory, tempdir);
        return;
    }
    x->x_dir = opendir(x->x_directory);
    rewinddir(x->x_dir);
    x->x_nfiles = x->x_ignored = 0;
    struct dirent *result = NULL;
    while((result = readdir(x->x_dir))){
        if(strncmp(result->d_name, ".", 1 ))
            x->x_nfiles++;
        else
            x->x_ignored++;
    }
    rewinddir(x->x_dir);
}

static void dir_n(t_dir *x){
    outlet_float(x->x_out2, x->x_nfiles);
}

static void dir_reset(t_dir *x){
    strncpy(x->x_directory, x->x_getdir->s_name, MAXPDSTRING);
    x->x_dir = opendir(x->x_getdir->s_name);
    rewinddir(x->x_dir);
    x->x_nfiles = x->x_ignored = x->x_seek = 0;
    struct dirent *result = NULL;
    while((result = readdir(x->x_dir))){
        if(strncmp(result->d_name, ".", 1 ))
            x->x_nfiles++;
        else
            x->x_ignored++;
        }
    rewinddir(x->x_dir);
}

static void dir_dump(t_dir *x){
    rewinddir(x->x_dir);
    struct dirent *result = NULL;
    while((result = readdir(x->x_dir)))
        if(strncmp(result->d_name, ".", 1 ))
            outlet_symbol(x->x_out1, gensym(result->d_name));
    rewinddir(x->x_dir);
}

static void dir_dir(t_dir *x){
    outlet_symbol(x->x_out2, gensym(x->x_directory));
}

static void dir_bang(t_dir *x){
    dir_dir(x);
    dir_dump(x);
}

static void *dir_new(t_floatarg f){
    t_dir *x = (t_dir *)pd_new(dir_class);
    t_canvas *canvas = canvas_getcurrent();
    int depth = (int)f;
    if(depth < 0)
        depth = 0;
    while(!canvas->gl_env)
        canvas = canvas->gl_owner;
    while(depth--){
        if(canvas->gl_owner){
            while(!canvas->gl_env)
                canvas = canvas->gl_owner;
        }
    }
    x->x_getdir = canvas_getdir(canvas);
    x->x_nfiles = x->x_ignored = x->x_seek = 0;
    strncpy(x->x_directory, x->x_getdir->s_name, MAXPDSTRING);
    x->x_dir = opendir(x->x_getdir->s_name);
/*   struct dirent *result = NULL;
    while((result = readdir(x->x_dir))){
        if(strncmp(result->d_name, ".", 1))
            x->x_nfiles++;
        else
            x->x_ignored++;
    } */
    x->x_out1 = outlet_new(&x->x_obj, &s_anything);
    x->x_out2 = outlet_new(&x->x_obj, &s_symbol);
    return(x);
}

void dir_setup(void){
    dir_class = class_new(gensym("dir"), (t_newmethod)dir_new, 0, sizeof(t_dir), 0, A_DEFFLOAT, 0);
    class_addbang(dir_class, dir_bang);
    class_addmethod(dir_class, (t_method)dir_n, gensym("n"), 0);
    class_addmethod(dir_class, (t_method)dir_dir, gensym("dir"), 0);
    class_addmethod(dir_class, (t_method)dir_dump, gensym("dump"), 0);
    class_addmethod(dir_class, (t_method)dir_reset, gensym("reset"), 0);
    class_addmethod(dir_class, (t_method)dir_next, gensym("next"), 0);
    class_addmethod(dir_class, (t_method)dir_open, gensym("open"), A_DEFSYMBOL, 0);
    class_addmethod(dir_class, (t_method)dir_seek, gensym("seek"), A_DEFFLOAT, 0);
}
