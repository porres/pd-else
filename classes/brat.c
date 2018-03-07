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
    if(!parent  || !list)
        return 0;
    for(list = s_canvaslist; list; list = list->next){
        if(parent == list->parent)
            return list;
    }
    return 0;
}

static t_cnv_objlist *objectsInCanvas(const t_pd *parent){
    t_cnv_canvaslist *list = findCanvas(parent);
    if(list)
        return list->obj;
    return 0;
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
    return list;
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
// start of brat_class
///////////////////////

static t_class *brat_class;

typedef struct _brat{
  t_object  x_obj;
  t_outlet *x_properties_bangout;
  t_outlet *x_click_bangout;
}t_brat;

t_propertiesfn s_orgfun = NULL;

static void properties_bang(t_brat *x){
    outlet_bang(x->x_properties_bangout);
}

static void click_bang(t_brat *x){
    outlet_bang(x->x_click_bangout);
}

static void brat_click(t_gobj *z, t_canvas *x){
    t_cnv_objlist *objs = objectsInCanvas((t_pd*)z);
    if(objs == NULL){
      canvas_vis(z, 1);
    }
    while(objs){
        t_canvas* x = (t_canvas*)objs->obj;
        click_bang(x);
        objs = objs->next;
    }
}

static void brat_properties(t_gobj *z, t_glist *owner){
  t_cnv_objlist *objs = objectsInCanvas((t_pd*)z);
  if(objs == NULL)
    s_orgfun(z, owner);
  while(objs){
      t_brat* x = (t_brat*)objs->obj;
      properties_bang(x);
      objs = objs->next;
  }
}

static void *brat_new(void){
  t_brat *x = (t_brat *)pd_new(brat_class);
  t_glist *glist = (t_glist *)canvas_getcurrent();
  t_canvas *canvas = (t_canvas*)glist_getcanvas(glist);
  t_class *class = ((t_gobj*)canvas)->g_pd;
  t_propertiesfn orgfun = NULL;
  x->x_properties_bangout = outlet_new((t_object *)x, &s_bang);
  x->x_click_bangout = outlet_new((t_object *)x, &s_bang);
  orgfun = class_getpropertiesfn(class);
  if(orgfun != brat_properties)
    s_orgfun = orgfun;
  class_setpropertiesfn(class, brat_properties);
  class_addmethod(class, (t_method)brat_click, gensym("click"), 0);
  addObjectToCanvas((t_pd*)canvas, (t_pd*)x);
  return(x);
}

void brat_setup(void){
  brat_class = class_new(gensym("brat"), (t_newmethod)brat_new,
            0, sizeof(t_brat), CLASS_NOINLET, 0);
    s_orgfun = NULL;
}
