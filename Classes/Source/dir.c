// porres 2019 - sort code from 'sort' class (based M. Barber's code)

#include "m_pd.h"
#include "g_canvas.h"
#include <string.h>
#include <math.h>
#include <dirent.h>

#define MAXN 32768   // max n of files

static t_class *dir_class;

typedef struct _sortdata{
    int      d_maxn;    // as allocated
    int      d_natoms;  // as used
    t_atom  *d_buf;
    t_atom   d_bufini[MAXN];
}t_sortdata;

typedef struct dir{
    t_object    x_obj;
    DIR        *x_dir;
    char        x_directory[MAXPDSTRING];
    t_symbol   *x_getdir; // default directory
    t_symbol   *x_ext;
    t_atom      x_filelist[MAXN];
    t_int       x_nfiles;
    t_int       x_seek;
    t_sortdata  x_inbuf;
    t_outlet   *x_out1;
    t_outlet   *x_out2;
    t_outlet   *x_out3;
}t_dir;

static void sort_swap(t_atom *av, int i, int j){
    t_atom temp = av[j];
    av[j] = av[i];
    av[i] = temp;
}

static int sort_compare(t_atom *a1, t_atom *a2){
    if(a1->a_type == A_FLOAT && a2->a_type == A_SYMBOL)
        return(-1);
    else if(a1->a_type == A_SYMBOL && a2->a_type == A_FLOAT)
        return(1);
    else if(a1->a_type == A_FLOAT && a2->a_type == A_FLOAT){
        if(a1->a_w.w_float < a2->a_w.w_float)
            return(-1);
        else if(a1->a_w.w_float > a2->a_w.w_float)
            return(1);
        else
            return(0);
    }
    else
        return(strcmp(a1->a_w.w_symbol->s_name, a2->a_w.w_symbol->s_name));
}

static void sort_qsort(t_dir *x, t_atom *av1, int left, int right){
    if(left >= right)
        return;
    sort_swap(av1, left, (left + right)/2);
    int last = left;
    for(int i = left+1; i <= right; i++){
        if((sort_compare(av1 + i, av1 + left)) < 0)
            sort_swap(av1, ++last, i);
    }
    sort_swap(av1, left, last);
    sort_qsort(x, av1, left, last-1);
    sort_qsort(x, av1, last+1, right);
}

static void dir_sort(t_dir *x, t_sortdata *d, int ac, t_atom *av){
    if(ac){
        if(ac > d->d_maxn)
            ac = d->d_maxn;
        d->d_natoms = ac;
        memcpy(d->d_buf, av, ac*sizeof(*d->d_buf));
        sort_qsort(x, d->d_buf, 0, ac-1);
    }
}

static void dir_load(t_dir *x){
    x->x_dir = opendir(x->x_directory);
    x->x_nfiles = 0;
    struct dirent *result = NULL;
    while((result = readdir(x->x_dir))){
        if(strncmp(result->d_name, ".", 1)){
            if(x->x_ext == &s_){
                SETSYMBOL(x->x_filelist + x->x_nfiles, gensym(result->d_name));
                x->x_nfiles++;
            }
            else{
                int extlen = strlen(x->x_ext->s_name);
                int len = strlen(result->d_name);
                const char *extension = &result->d_name[len-extlen];
                if(!strcmp(extension, x->x_ext->s_name)){
                    SETSYMBOL(x->x_filelist+x->x_nfiles, gensym(result->d_name));
                    x->x_nfiles++;
                }
            }
        }
    }
    closedir(x->x_dir);
    dir_sort(x, &x->x_inbuf, x->x_inbuf.d_natoms = x->x_nfiles, x->x_filelist);
}

static void dir_load_dir(t_dir *x, t_symbol *dirname, int init){
    if(!strcmp(dirname->s_name, "")){
        pd_error(x, "[dir]: no symbol given to 'open'");
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
    else if(!strcmp(dirname->s_name, ".")){ // do nothing / reopen same dir
    }
    else if(!strncmp(dirname->s_name, "/", 1)) // absolute path
        strncpy(x->x_directory, dirname->s_name, MAXPDSTRING);
    else // relative to current dir
        sprintf(x->x_directory, "%s/%s", x->x_directory, dirname->s_name );
// search
    DIR *temp = opendir(x->x_directory);
    if(!temp){ // didn't find
        temp = NULL; // ???
        strcpy(x->x_directory, tempdir); // restore original directory
        if(init)
            pd_error(x, "[dir]: cannot open '%s'", dirname->s_name);
        else
            outlet_float(x->x_out3, 0);
        return;
    }
    else{ // found it
        closedir(temp);
        if(!init)
            outlet_float(x->x_out3, 1);
        dir_load(x);
    }
}

static void dir_open(t_dir *x, t_symbol *dirname){
    dir_load_dir(x, dirname, 0);
}

static void dir_ext(t_dir *x, t_symbol *ext){ // IMPROVE THIS!!!!!!
    if(!strcmp(ext->s_name, ""))
        x->x_ext = &s_;
    else
        x->x_ext = ext;
    dir_load(x); // reload
}

static void dir_seek(t_dir *x, t_float f){
    int i = (int)f;
    if(i < 1)
        i = 1;
    if(x->x_nfiles){
        x->x_seek = i = ((i - 1) % x->x_nfiles) + 1;
        outlet_list(((t_object *)x)->ob_outlet, &s_list, 1, x->x_inbuf.d_buf+i-1);
    }
    else
        post("[dir]: no files found to seek for");
}

static void dir_next(t_dir *x){
    dir_seek(x, x->x_seek + 1);
}

static void dir_n(t_dir *x){
    outlet_float(x->x_out2, x->x_nfiles);
}

static void dir_reset(t_dir *x){ // reset to default
    strncpy(x->x_directory, x->x_getdir->s_name, MAXPDSTRING);
}

static void dir_dump(t_dir *x){
    if(x->x_inbuf.d_natoms){
        for(int i = 0; i < x->x_inbuf.d_natoms; i++)
            outlet_list(((t_object *)x)->ob_outlet, &s_list, 1, x->x_inbuf.d_buf+i);
    }
    else
        post("[dir]: no files found");
}

static void dir_dir(t_dir *x){
    outlet_symbol(x->x_out2, gensym(x->x_directory));
}

static void dir_bang(t_dir *x){
    dir_dir(x);
    dir_dump(x);
}

static void sortdata_free(t_sortdata *d){
    if(d->d_buf != d->d_bufini)
        freebytes(d->d_buf, d->d_maxn * sizeof(*d->d_buf));
}

static void dir_free(t_dir *x){
    sortdata_free(&x->x_inbuf);
    outlet_free(x->x_out1);
    outlet_free(x->x_out2);
    outlet_free(x->x_out3);
}

static void sortdata_init(t_sortdata *d){
    d->d_natoms = 0;
    d->d_maxn = MAXN;
    d->d_buf = d->d_bufini;
}

static void *dir_new(t_symbol *s, int ac, t_atom* av){
    t_dir *x = (t_dir *)pd_new(dir_class);
    s = NULL; // get rid of warning
    sortdata_init(&x->x_inbuf);
    x->x_seek = 0;
    t_symbol *dirname = x->x_ext = &s_;
    int symarg = 0, flag = 0, depth = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT && !symarg && !flag){
            depth = (int)atom_getfloatarg(0, ac, av);
            ac--;
            av++;
        }
        else if(av->a_type == A_SYMBOL){
            if(!symarg)
                symarg = 1;
            t_symbol *cursym = atom_getsymbolarg(0, ac, av);
            if(!strcmp(cursym->s_name, "-ext") && !flag){
                flag = 1;
                if(ac == 2 && (av+1)->a_type == A_SYMBOL){
                    x->x_ext = atom_getsymbolarg(1, ac, av);
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!flag){
                if(!symarg)
                    symarg = 1;
                dirname = cursym;
                ac--;
                av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    t_canvas *canvas = canvas_getcurrent();
    while(!canvas->gl_env)
        canvas = canvas->gl_owner;
    if(depth < 0)
        depth = 0;
    while(depth--){
        if(canvas->gl_owner){
            canvas = canvas->gl_owner;
            while(!canvas->gl_env)
                canvas = canvas->gl_owner;
        }
    }
    x->x_getdir = canvas_getdir(canvas); // default
    strncpy(x->x_directory, x->x_getdir->s_name, MAXPDSTRING); // default
    dirname == &s_ ? dir_load_dir(x, x->x_getdir, 1) : dir_load_dir(x, dirname, 1);
    x->x_out1 = outlet_new(&x->x_obj, &s_anything);
    x->x_out2 = outlet_new(&x->x_obj, &s_symbol);
    x->x_out3 = outlet_new(&x->x_obj, &s_float);
    return(x);
errstate:
    pd_error(x, "[dir]: improper args");
    return NULL;
}

void dir_setup(void){
    dir_class = class_new(gensym("dir"), (t_newmethod)dir_new, (t_method)dir_free,
            sizeof(t_dir), 0, A_GIMME, 0);
    class_addbang(dir_class, dir_bang);
    class_addmethod(dir_class, (t_method)dir_n, gensym("n"), 0);
    class_addmethod(dir_class, (t_method)dir_dir, gensym("dir"), 0);
    class_addmethod(dir_class, (t_method)dir_dump, gensym("dump"), 0);
    class_addmethod(dir_class, (t_method)dir_reset, gensym("reset"), 0);
    class_addmethod(dir_class, (t_method)dir_next, gensym("next"), 0);;
    class_addmethod(dir_class, (t_method)dir_load, gensym("reopen"), 0);
    class_addmethod(dir_class, (t_method)dir_open, gensym("open"), A_DEFSYMBOL, 0);
    class_addmethod(dir_class, (t_method)dir_ext, gensym("ext"), A_DEFSYMBOL, 0);
    class_addmethod(dir_class, (t_method)dir_seek, gensym("seek"), A_DEFFLOAT, 0);
}
