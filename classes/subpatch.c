#include "m_pd.h"
#include "g_canvas.h"

typedef struct _cnv_objlist{
    const t_pd *obj;
    struct _cnv_objlist *next;
}t_cnv_objlist;

typedef struct _cnv_canvaslist{
    const t_pd *parent;
    t_cnv_objlist*obj;
    struct _cnv_canvaslist*next;
}t_cnv_canvaslist;

static t_cnv_canvaslist*s_canvaslist = 0;

static t_cnv_canvaslist*findCanvas(const t_pd*parent){
    t_cnv_canvaslist*list = s_canvaslist;
    if(!parent  || !list)
        return 0;
    for(list = s_canvaslist; list; list=list->next){
        if(parent == list->parent)
            return list;
    }
    return 0;
}

static t_cnv_objlist*objectsInCanvas(const t_pd*parent){
    t_cnv_canvaslist*list = findCanvas(parent);
    if(list)
        return list->obj;
    return 0;
}

static t_cnv_canvaslist *addCanvas(const t_pd*parent){
    t_cnv_canvaslist *list = findCanvas(parent);
    if(!list){
        list=getbytes(sizeof(*list));
        list->parent=parent;
        list->obj = 0;
        list->next = 0;
        if(s_canvaslist == 0) // new list
            s_canvaslist=list;
        else{ // add to the end of existing list
            t_cnv_canvaslist*dummy=s_canvaslist;
            while(dummy->next)
                dummy = dummy->next;
            dummy->next = list;
        }
    }
    return list;
}

static void addObjectToCanvas(const t_pd*parent, const t_pd*obj){
    t_cnv_canvaslist*p=addCanvas(parent);
    t_cnv_objlist*list = 0;
    t_cnv_objlist*entry = 0;
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

static t_class *subpatch_class; // start of subpatch_class

typedef struct _subpatch{
  t_object  x_obj;
  t_outlet   *x_properties_bangout;
  t_outlet   *x_click_bangout;
}t_subpatch;

t_propertiesfn s_orgfun = NULL;

static void properties_bang(t_subpatch *x){
    outlet_bang(x->x_properties_bangout);
}

static void click_bang(t_subpatch *x){
    outlet_bang(x->x_click_bangout);
}

static void subpatch_click(t_gobj*z, t_glist*owner, t_canvas *x, t_floatarg xpos, t_floatarg ypos, t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
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

static void subpatch_properties(t_gobj*z, t_glist*owner){
  t_cnv_objlist *objs = objectsInCanvas((t_pd*)z);
  if(objs == NULL)
    s_orgfun(z, owner);
  while(objs){
      t_subpatch* x = (t_subpatch*)objs->obj;
      properties_bang(x);
      objs = objs->next;
  }
}

static void *subpatch_new(void){
  t_subpatch *x = (t_subpatch *)pd_new(subpatch_class);
  t_glist *glist = (t_glist *)canvas_getcurrent();
  t_canvas *canvas = (t_canvas*)glist_getcanvas(glist);
  t_class *class = ((t_gobj*)canvas)->g_pd;
  t_propertiesfn orgfun = NULL;
  x->x_properties_bangout = outlet_new((t_object *)x, &s_bang);
  x->x_click_bangout = outlet_new((t_object *)x, &s_bang);
  orgfun = class_getpropertiesfn(class);
  if(orgfun != subpatch_properties)
    s_orgfun = orgfun;
  class_setpropertiesfn(class, subpatch_properties);
    
  class_addmethod(class, (t_method)subpatch_click,
        gensym("click"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);

  addObjectToCanvas((t_pd*)canvas, (t_pd*)x);
  return(x);
}

void subpatch_setup(void){
  subpatch_class = class_new(gensym("subpatch"), (t_newmethod)subpatch_new,
            0, sizeof(t_subpatch), CLASS_NOINLET, 0);
    s_orgfun = NULL;
}
