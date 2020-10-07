#include "m_pd.h"
#include "g_canvas.h"

typedef struct _cnv_objlist{
    const t_pd *obj;
    struct _cnv_objlist *next;
}t_cnv_objlist;

typedef struct _cnv_canvaslist{
    const t_pd              *parent;
    t_cnv_objlist           *obj;
    struct _cnv_canvaslist  *next;
}t_cnv_canvaslist;

static t_cnv_canvaslist *s_canvaslist = 0;

static t_cnv_canvaslist *findCanvas(const t_pd *parent){
    t_cnv_canvaslist*list = s_canvaslist;
    if(!parent || !list)
        return(0);
    for(list = s_canvaslist; list; list = list->next){
        if(parent == list->parent)
            return(list);
    }
    return(0);
}

static t_cnv_objlist *objectsInCanvas(const t_pd *parent){
    t_cnv_canvaslist *list = findCanvas(parent);
    if(list)
        return(list->obj);
    return(0);
}

static t_cnv_canvaslist *addCanvas(const t_pd *parent){
    t_cnv_canvaslist *list = findCanvas(parent);
    if(!list){
        list = getbytes(sizeof(*list));
        list->parent = parent;
        list->obj = 0;
        list->next = 0;
        if(s_canvaslist == 0) // new list
            s_canvaslist = list;
        else{ // add to the end of existing list
            t_cnv_canvaslist *dummy = s_canvaslist;
            while(dummy->next)
                dummy = dummy->next;
            dummy->next = list;
        }
    }
    return(list);
}

static void removeObjectFromCanvas(const t_pd*parent, const t_pd*obj) {
    t_cnv_canvaslist *p = findCanvas(parent);
    t_cnv_objlist *list = 0, *last = 0, *next = 0;
    if(!p || !obj)return;
    list = p->obj;
    if(!list)
        return;
    while(list && obj!=list->obj) {
        last = list;
        list = list->next;
    }
    if(!list) /* couldn't find this object */
        return;
    next = list->next;
    if(last)
        last->next = next;
    else
        p->obj = next;
    freebytes(list, sizeof(*list));
    list = 0;
}

static void removeObjectFromCanvases(const t_pd*obj) {
    t_cnv_canvaslist*parents=s_canvaslist;
    while(parents){
        removeObjectFromCanvas(parents->parent, obj);
        parents = parents->next;
    }
}

static void addObjectToCanvas(const t_pd *parent, const t_pd *obj){
    t_cnv_canvaslist *p = addCanvas(parent);
    t_cnv_objlist *list = 0;
    t_cnv_objlist *entry = 0;
    if(!p || !obj)
        return;
    list = p->obj;
    if(list && obj == list->obj)
        return;
    while(list && list->next){
        if(obj == list->obj) // obj already in list
            return;
        list = list->next;
    }
    entry = getbytes(sizeof(*entry)); // at end of list not containing obj yet, so add it
    entry->obj = obj;
    entry->next = 0;
    if(list)
        list->next = entry;
    else
        p->obj = entry;
}

///////////////////////
// start of else_click_class
///////////////////////

static t_class *else_click_class;

typedef struct _else_click{
  t_object  x_obj;
  t_outlet *x_else_click_bangout;
}t_else_click;

static void else_click_free(t_else_click *x){
  removeObjectFromCanvases((t_pd*)x);
}

static void else_click_bang(t_else_click *x){
    outlet_bang(x->x_else_click_bangout);
}

static void else_click_else_click(t_gobj *z, t_canvas *x){
    x = NULL;
    t_cnv_objlist *objs = objectsInCanvas((t_pd*)z);
    if(objs == NULL)
      canvas_vis((t_glist*)z, 1);
    while(objs){
        t_canvas* cv = (t_canvas*)objs->obj;
        else_click_bang((t_else_click*)cv);
        objs = objs->next;
    }
}

static void *else_click_new(t_floatarg f){
    t_else_click *x = (t_else_click *)pd_new(else_click_class);
    t_int subpatch_mode = f != 0;;
    t_glist *glist = (t_glist *)canvas_getcurrent();
    t_canvas *canvas = (t_canvas*)glist_getcanvas(glist);
    t_class *class = ((t_gobj*)canvas)->g_pd;
    if(!subpatch_mode){
        while(!canvas->gl_env)
            canvas = canvas->gl_owner;
    }
    class_addmethod(class, (t_method)else_click_else_click, gensym("click"), 0);
    x->x_else_click_bangout = outlet_new((t_object *)x, &s_bang);
    addObjectToCanvas((t_pd*)canvas, (t_pd*)x);
    return(x);
}

void click_setup(void){
    else_click_class = class_new(gensym("click"), (t_newmethod)else_click_new,
        (t_method)else_click_free, sizeof(t_else_click), CLASS_NOINLET, A_DEFFLOAT, 0);
}
