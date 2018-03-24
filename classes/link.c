
#include <stdio.h>
#include <string.h>
#include "m_pd.h"
#include "m_imp.h"  /* FIXME need access to c_externdir... */
#include "g_canvas.h"

typedef struct _link
{
    t_object   x_ob;
    t_glist   *x_glist;
    int        x_isboxed;
    char      *x_vistext;
    int        x_vissize;
    int        x_vislength;
    int        x_rtextactive;
    t_symbol  *x_dirsym;
    t_symbol  *x_ulink;
    t_atom     x_openargs[2];
    int        x_linktype;
    int        x_ishit;
} t_link;

static t_class *link_class;
static t_class *linkbox_class;

/* Code that might be merged back to g_text.c starts here: */

static void link_getrect(t_gobj *z, t_glist *glist,
			     int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_link *x = (t_link *)z;
    int width, height;
    float x1, y1, x2, y2;
    if (glist->gl_editor && glist->gl_editor->e_rtext)
    {
	if (x->x_rtextactive)
	{
	    t_rtext *y = glist_findrtext(glist, (t_text *)x);
	    width = rtext_width(y);
	    height = rtext_height(y) - 2;
	}
	else
	{
	    int font = glist_getfont(glist);
	    width = x->x_vislength * sys_fontwidth(font) + 2;
	    height = sys_fontheight(font) + 2;
	}
    }
    else width = height = 10;
    x1 = text_xpix((t_text *)x, glist);
    y1 = text_ypix((t_text *)x, glist);
    x2 = x1 + width;
    y2 = y1 + height;
    y1 += 1;
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}

static void link_displace(t_gobj *z, t_glist *glist, int dx, int dy)
{
    t_text *t = (t_text *)z;
    t->te_xpix += dx;
    t->te_ypix += dy;
    if (glist_isvisible(glist))
    {
        t_rtext *y = glist_findrtext(glist, t);
        rtext_displace(y, dx, dy);
    }
}

static void link_select(t_gobj *z, t_glist *glist, int state)
{
    t_link *x = (t_link *)z;
    t_rtext *y = glist_findrtext(glist, (t_text *)x);
    rtext_select(y, state);
    if (state)
        sys_vgui(".x%lx.c itemconfigure %s -fill blue\n",
                 glist, rtext_gettag(y));
    else
        sys_vgui(".x%lx.c itemconfigure %s -text {%s} -fill #0000dd -activefill #e70000\n",
                 glist, rtext_gettag(y), x->x_vistext);
}

static void link_activate(t_gobj *z, t_glist *glist, int state)
{
    t_link *x = (t_link *)z;
    t_rtext *y = glist_findrtext(glist, (t_text *)x);
    rtext_activate(y, state);
    x->x_rtextactive = state;
}

static void link_vis(t_gobj *z, t_glist *glist, int vis)
{
    t_link *x = (t_link *)z;
    t_rtext *y = glist_findrtext(glist, (t_text *)x);
    if (vis)
    {
        rtext_draw(y);
        sys_vgui(".x%lx.c itemconfigure %s -text {%s} -fill #0000dd -activefill #e70000\n",
                 glist_getcanvas(glist), rtext_gettag(y), x->x_vistext);
    }
    else
        rtext_erase(y);
}

static int link_wbclick(t_gobj *z, t_glist *glist, int xpix, int ypix,
			    int shift, int alt, int dbl, int doit);

static t_widgetbehavior link_widgetbehavior =
{
    link_getrect,
    link_displace,
    link_select,
    link_activate,
    0,
    link_vis,
    link_wbclick,
};

/* Code that might be merged back to g_text.c ends here. */

/* FIXME need access to glob_pdobject... */
static t_pd *link_pdtarget(t_link *x)
{
    t_pd *pdtarget = gensym("pd")->s_thing;
    if (pdtarget && !strcmp(class_getname(*pdtarget), "pd"))
	return (pdtarget);
    else
	return ((t_pd *)x);  /* internal error */
}

static void link_anything(t_link *x, t_symbol *s, int ac, t_atom *av)
{
    if (x->x_ishit)
    {
	startpost("link: internal error (%s", (s ? s->s_name : ""));
	postatom(ac, av);
	post(")");
    }
}

static void link_click(t_link *x, t_floatarg xpos, t_floatarg ypos,
			   t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    x->x_ishit = 1;
    sys_vgui("link_open {%s} {%s}\n",               \
             x->x_ulink->s_name, x->x_dirsym->s_name);
    x->x_ishit = 0;
}

static void link_bang(t_link *x)
{
  link_click(x, 0, 0, 0, 0, 0);
}

static int link_wbclick(t_gobj *z, t_glist *glist, int xpix, int ypix,
			    int shift, int alt, int dbl, int doit)
{
    t_link *x = (t_link *)z;
    if (doit)
    {
        link_click(x, (t_floatarg)xpix, (t_floatarg)ypix,
                       (t_floatarg)shift, 0, (t_floatarg)alt);
	return (1);
    }
    else
        return (0);
}

static int link_isoption(char *name)
{
    if (*name == '-')
    {
	char c = name[1];
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
    }
    else return (0);
}

static t_symbol *link_nextsymbol(int ac, t_atom *av, int opt, int *skipp)
{
    int ndx;
    for (ndx = 0; ndx < ac; ndx++, av++)
    {
	if (av->a_type == A_SYMBOL &&
	    (!opt || link_isoption(av->a_w.w_symbol->s_name)))
	{
	    *skipp = ++ndx;
	    return (av->a_w.w_symbol);
	}
    }
    return (0);
}

static int link_dooptext(char *dst, int maxsize, int ac, t_atom *av)
{
    int i, sz, sep, len;
    char buf[32], *src;
    for (i = 0, sz = 0, sep = 0; i < ac; i++, av++)
    {
	if (sep)
	{
	    sz++;
	    if (sz >= maxsize)
		break;
	    else if (dst)
	    {
		*dst++ = ' ';
		*dst = 0;
	    }
	}
	else sep = 1;
	if (av->a_type == A_SYMBOL)
	    src = av->a_w.w_symbol->s_name;
	else if (av->a_type == A_FLOAT)
	{
	    src = buf;
	    sprintf(src, "%g", av->a_w.w_float);
	}
	else
	{
	    sep = 0;
	    continue;
	}
	len = strlen(src);
	sz += len;
	if (sz >= maxsize)
	    break;
	else if (dst)
	{
	    strcpy(dst, src);
	    dst += len;
	}
    }
    return (sz);
}

static char *link_optext(int *sizep, int ac, t_atom *av)
{
    char *result;
    int sz = link_dooptext(0, MAXPDSTRING, ac, av);
    *sizep = sz + (sz >= MAXPDSTRING ? 4 : 1);
    result = getbytes(*sizep);
    link_dooptext(result, sz + 1, ac, av);
    if (sz >= MAXPDSTRING)
    {
	sz = strlen(result);
	strcpy(result + sz, "...");
    }
    return (result);
}

static void link_free(t_link *x)
{
    if (x->x_vistext)
	freebytes(x->x_vistext, x->x_vissize);
}

static void *link_new(t_symbol *s, int ac, t_atom *av)
{
    t_link xgen, *x;
    int skip;
    xgen.x_isboxed = 0;
    xgen.x_vistext = 0;
    xgen.x_vissize = 0;
    if ((xgen.x_ulink = link_nextsymbol(ac, av, 0, &skip)))
    {
	t_symbol *opt;
	ac -= skip;
	av += skip;
	while ((opt = link_nextsymbol(ac, av, 1, &skip)))
	{
	    ac -= skip;
	    av += skip;
	    if (opt == gensym("-box"))
		xgen.x_isboxed = 1;
	    else if (opt == gensym("-text"))
	    {
		t_symbol *nextsym = link_nextsymbol(ac, av, 1, &skip);
		int natoms = (nextsym ? skip - 1 : ac);
		if (natoms)
		    xgen.x_vistext =
			link_optext(&xgen.x_vissize, natoms, av);
	    }
	}
    }
    x = (t_link *)
	pd_new(xgen.x_isboxed ? linkbox_class : link_class);
    x->x_glist = canvas_getcurrent();
    x->x_dirsym = canvas_getdir(x->x_glist);  /* FIXME */

    x->x_isboxed = xgen.x_isboxed;
    x->x_vistext = xgen.x_vistext;
    x->x_vissize = xgen.x_vissize;
    x->x_vislength = (x->x_vistext ? strlen(x->x_vistext) : 0);
    x->x_rtextactive = 0;
    if (xgen.x_ulink)
        x->x_ulink = xgen.x_ulink;
    else
        x->x_ulink = gensym("Untitled");
    SETSYMBOL(&x->x_openargs[0], x->x_ulink);
    SETSYMBOL(&x->x_openargs[1], x->x_dirsym);
    x->x_ishit = 0;
    if (x->x_isboxed)
	outlet_new((t_object *)x, &s_anything);
    else
    {
	/* do we need to set ((t_text *)x)->te_type = T_TEXT; ? */
	if (!x->x_vistext)
	{
	    x->x_vislength = strlen(x->x_ulink->s_name);
	    x->x_vissize = x->x_vislength + 1;
	    x->x_vistext = getbytes(x->x_vissize);
	    strcpy(x->x_vistext, x->x_ulink->s_name);
	}
    }
    return (x);
}

void link_setup(void)
{
    t_symbol *dirsym;

    link_class = class_new(gensym("link"),
			       (t_newmethod)link_new,
			       (t_method)link_free,
			       sizeof(t_link),
			       CLASS_NOINLET | CLASS_PATCHABLE,
			       A_GIMME, 0);
    class_addanything(link_class, link_anything);
    class_setwidget(link_class, &link_widgetbehavior);

    linkbox_class = class_new(gensym("link"), 0,
				  (t_method)link_free,
				  sizeof(t_link), 0, A_GIMME, 0);
    class_addbang(linkbox_class, link_bang);
    class_addanything(linkbox_class, link_anything);
    class_addmethod(linkbox_class, (t_method)link_click,
		    gensym("click"),
		    A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);

    dirsym = link_class->c_externdir;  /* FIXME */
    
    sys_vgui("proc link_open {filename dir} {\n");
    sys_vgui("    if {[string first \"://\" $filename] > -1} {\n");
    sys_vgui("        menu_openfile $filename\n");
    sys_vgui("    } elseif {[file pathtype $filename] eq \"absolute\"} {\n");
    sys_vgui("        menu_openfile $filename\n");
    sys_vgui("    } elseif {[file exists [file join $dir $filename]]} {\n");
    sys_vgui("        set fullpath [file normalize [file join $dir $filename]]\n");
    sys_vgui("        set dir [file dirname $fullpath]\n");
    sys_vgui("        set filename [file tail $fullpath]\n");
    sys_vgui("        menu_doc_open $dir $filename\n");
    sys_vgui("    } else {\n");
    sys_vgui("        pdtk_post \"link: $filename can't be opened\n\"\n");
    sys_vgui("    }\n");
    sys_vgui("}\n");
    
}
