#include "m_pd.h"
#include "m_imp.h"
#include <string.h>

typedef struct else_obj{
    t_object t_ob;
}t_else_obj;

t_class *else_obj_class;

static int printed;

static int min_major = 0;
static int min_minor = 50;
static int min_bugfix = 3;
static int else_beta_version = 28;

void print_else_obj(t_else_obj *x){
    char else_obj_dir[MAXPDSTRING];
    strcpy(else_obj_dir, else_obj_class->c_externdir->s_name);
    int major = 0, minor = 0, bugfix = 0;
    sys_getversion(&major, &minor, &bugfix);
    post("");
    post("----------------------------------------------------------------------------------");
    post("   ~~~~~~~~~~|| ELSE - EL Locus Solus' Externals for Pure Data ||~~~~~~~~~~");
    post("----------------------------------------------------------------------------------");
    post("Author: Alexandre Torres Porres");
    post("Repositoty: https://github.com/porres/pd-else");
    post("License: Do What The Fuck You Want To Public License, unless otherwise noted");
    post("Version: 1.0 beta %d; Unreleased", else_beta_version);
    if(min_major >= major && min_minor >= minor && min_bugfix >= bugfix){
        post("ELSE 1.0 beta %d needs at least Pd %d.%d-%d (you have %d.%d-%d, you're good!)",
             else_beta_version,
             min_major,
             min_minor,
             min_bugfix,
             major,
             minor,
             bugfix);
    }
    else{
        pd_error(x, "ELSE 1.0 beta %d needs at least Pd %d.%d-%d, you have %d.%d-%d, please upgrade",
            else_beta_version,
            min_major,
            min_minor,
            min_bugfix,
            major,
            minor,
            bugfix);
    }
    post("Loading the ELSE library added %s", else_obj_dir);
    post("to Pd's path so the its objects can be loaded");
    post("----------------------------------------------------------------------------------");
    post("");
}

static void else_obj_about(t_else_obj *x){
    print_else_obj(x);
}

static void *else_obj_new(void){
    t_else_obj *x = (t_else_obj *)pd_new(else_obj_class);
    if(!printed){
        else_obj_about(x);
        printed = 1;
    }
    return (x);
}

/* ----------------------------- SETUP ------------------------------ */

void else_setup(void){
    else_obj_class = class_new(gensym("else"), else_obj_new, 0, sizeof(t_else_obj), 0, 0);
    class_addmethod(else_obj_class, (t_method)else_obj_about, gensym("about"), 0);
    
    t_else_obj *x = (t_else_obj *)pd_new(else_obj_class);

    char else_obj_dir[MAXPDSTRING];
    strcpy(else_obj_dir, else_obj_class->c_externdir->s_name);
    char encoded[MAXPDSTRING+1];
    sprintf(encoded, "+%s", else_obj_dir);

    t_atom ap[2];
    SETSYMBOL(ap, gensym(encoded));
    SETFLOAT (ap+1, 0.f);
    pd_typedmess(gensym("pd")->s_thing, gensym("add-to-path"), 2, ap);

   if(!printed){
       print_else_obj(x);
       printed = 1;
    }
}
