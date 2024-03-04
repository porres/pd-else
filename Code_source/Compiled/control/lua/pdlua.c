/* This is a version hacked by Martin Peach 20110120 martin.peach@sympatico.ca */
/* Reformmatted the code and added some debug messages. Changed the name of the class to pdlua */
/** @file lua.c 
 *  @brief pdlua -- a Lua embedding for Pd.
 *  @author Claude Heiland-Allen <claude@mathr.co.uk>
 *  @date 2008
 *  @version 0.6~svn
 *
 * Copyright (C) 2007,2008 Claude Heiland-Allen <claude@mathr.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */ 

/* various C stuff, mainly for reading files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h> // for open
#include <sys/stat.h> // for open
#ifdef _MSC_VER
#include <io.h>
#include <fcntl.h> // for open
#define read _read
#define close _close
#define ssize_t int
#define snprintf _snprintf
#else
#include <sys/fcntl.h> // for open
#include <unistd.h>
#endif
/* we use Lua */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "pdlua.h"

#include "s_stuff.h" // for sys_register_loader()
#include "m_imp.h" // for struct _class
#include "g_canvas.h"
/* BAD: support for Pd < 0.41 */

#include "pdlua_gfx.h"

#ifdef PDINSTANCE

typedef struct _lua_Instance {
    void* pd_instance;
    lua_State* state;
    struct _lua_Instance* next;
} lua_Instance;

lua_Instance* lua_threads = NULL;

lua_State* __L()
{
    lua_Instance* iter = lua_threads;
    while(iter)
    {
        if(iter->pd_instance == pd_this)
        {
            return iter->state;
        }
        iter = iter->next;
    }

    return NULL; // should never happen
}

void initialise_lua_state()
{
    if(!lua_threads)
    {
        lua_threads = t_getbytes(sizeof(lua_Instance));
        lua_threads->pd_instance = pd_this;
        lua_threads->state = luaL_newstate();
        lua_threads->next = NULL;
        return;
    }
    
    lua_Instance* iter = lua_threads;
    while(iter->next)
    {
        iter = iter->next;
    }
    
    iter->next = t_getbytes(sizeof(lua_Instance));
    iter->next->pd_instance = pd_this;
    iter->next->state = lua_newthread(lua_threads->state);
    iter->next->next = NULL;
}

#else

static lua_State* __lua_state = NULL;

lua_State* __L()
{
    return __lua_state;
}

void initialise_lua_state()
{
    if (!__lua_state) {
        __lua_state = luaL_newstate();
    }
}

#endif


#if PD_MAJOR_VERSION == 0
# if PD_MINOR_VERSION >= 41
#  define PDLUA_PD41
/* use new garray support that is 64-bit safe */
#  define PDLUA_ARRAYGRAB garray_getfloatwords
#  define PDLUA_ARRAYTYPE t_word
#  define PDLUA_ARRAYELEM(arr,idx) ((arr)[(idx)].w_float)
# elif PD_MINOR_VERSION >= 40
#  define PDLUA_PD40
/* use old garray support, not 64-bit safe */
#  define PDLUA_ARRAYGRAB garray_getfloatarray
#  define PDLUA_ARRAYTYPE t_float
#  define PDLUA_ARRAYELEM(arr,idx) ((arr)[(idx)])
# elif PD_MINOR_VERSION >= 39
#  define PDLUA_PD39
/* use old garray support, not 64-bit safe */
#  define PDLUA_ARRAYGRAB garray_getfloatarray
#  define PDLUA_ARRAYTYPE t_float
#  define PDLUA_ARRAYELEM(arr,idx) ((arr)[(idx)])
# else
#  error "Pd version is too old, please upgrade"
# endif
#else
# error "Pd version is too new, please file a bug report"
#endif

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

/* BAD: end of bad section */

/* If defined, PDLUA_DEBUG lets pdlua post a lot of text */
//#define PDLUA_DEBUG post
#ifndef PDLUA_DEBUG
//static void PDLUA_DEBUG(const char *fmt, ...) {;}
# define PDLUA_DEBUG(x,y)
# define PDLUA_DEBUG2(x,y0,y1)
# define PDLUA_DEBUG3(x,y0,y1,y2)
#else
# define PDLUA_DEBUG2 PDLUA_DEBUG
# define PDLUA_DEBUG3 PDLUA_DEBUG
#endif

// In plugdata we're linked statically and thus c_externdir is empty.
// So we pass a data directory to the setup function instead and store it here
#if PLUGDATA
    char plugdata_datadir[MAXPDSTRING];

    // Hook to inform plugdata which class names are lua objects
    void(*plugdata_register_class)(const char*);
#endif

/** State for the Lua file reader. */
typedef struct pdlua_readerdata
{
    int         fd; /**< File descriptor to read from. */
    char        buffer[MAXPDSTRING]; /**< Buffer to read into. */
} t_pdlua_readerdata;

/** Proxy inlet object data. */
typedef struct pdlua_proxyinlet
{
    t_pd            pd; /**< Minimal Pd object. */
    struct pdlua    *owner; /**< The owning object to forward inlet messages to. */
    unsigned int    id; /**< The number of this inlet. */
} t_pdlua_proxyinlet;

/** Proxy receive object data. */
typedef struct pdlua_proxyreceive
{
    t_pd            pd; /**< Minimal Pd object. */
    struct pdlua    *owner; /**< The owning object to forward received messages to. */
    t_symbol        *name; /**< The receive-symbol to bind to. */
} t_pdlua_proxyreceive;

/** Proxy clock object data. */
typedef struct pdlua_proxyclock
{
    t_pd            pd; /**< Minimal Pd object. */
    struct pdlua    *owner; /**< Object to forward messages to. */
    t_clock         *clock; /** Pd clock to use. */
} t_pdlua_proxyclock;
/* prototypes*/

static const char *pdlua_reader (lua_State *L, void *rr, size_t *size);
/** Proxy inlet 'anything' method. */
static void pdlua_proxyinlet_anything (t_pdlua_proxyinlet *p, t_symbol *s, int argc, t_atom *argv);
/** Proxy inlet initialization. */
static void pdlua_proxyinlet_init (t_pdlua_proxyinlet *p, struct pdlua *owner, unsigned int id);
/** Register the proxy inlet class with Pd. */
static void pdlua_proxyinlet_setup (void);
/** Proxy receive 'anything' method. */
static void pdlua_proxyreceive_anything (t_pdlua_proxyreceive *r, t_symbol *s, int argc, t_atom *argv);
/** Proxy receive allocation and initialization. */
static t_pdlua_proxyreceive *pdlua_proxyreceive_new (struct pdlua *owner, t_symbol *name);
/** Proxy receive cleanup and deallocation. */
static void pdlua_proxyreceive_free (t_pdlua_proxyreceive *r /**< The proxy receive to free. */);
/** Register the proxy receive class with Pd. */
static void pdlua_proxyreceive_setup (void);
/** Proxy clock 'bang' method. */
static void pdlua_proxyclock_bang (t_pdlua_proxyclock *c);
/** Proxy clock allocation and initialization. */
static t_pdlua_proxyclock *pdlua_proxyclock_new (struct pdlua *owner);
/** Register the proxy clock class with Pd. */
static void pdlua_proxyclock_setup (void);
/** Dump an array of atoms into a Lua table. */
static void pdlua_pushatomtable (int argc, t_atom *argv);
/** Pd object constructor. */
static t_pdlua *pdlua_new (t_symbol *s, int argc, t_atom *argv);
/** Pd object destructor. */
static void pdlua_free (t_pdlua *o );
//static void pdlua_stack_dump (lua_State *L);
/** a handler for the open item in the right-click menu (mrpeach 20111025) */
/** Here we find the lua code for the object and open it in an editor */
static void pdlua_menu_open (t_pdlua *o);
/** Lua class registration. This is equivalent to the "setup" method for an ordinary Pd class */
static int pdlua_class_new (lua_State *L);
/** Lua object creation. */
static int pdlua_object_new (lua_State *L);
/** Lua object inlet creation. */
static int pdlua_object_createinlets (lua_State *L);
/** Lua object outlet creation. */
static int pdlua_object_createoutlets (lua_State *L);
/** Lua object receive creation. */
static int pdlua_receive_new (lua_State *L);
/** Lua object receive destruction. */
static int pdlua_receive_free (lua_State *L);
/** Lua object clock creation. */
static int pdlua_clock_new (lua_State *L);
/** Lua proxy clock delay. */
static int pdlua_clock_delay (lua_State *L);
/** Lua proxy clock set. */
static int pdlua_clock_set (lua_State *L);
/** Lua proxy clock unset. */
static int pdlua_clock_unset (lua_State *L);
/** Lua proxy clock destruction. */
static int pdlua_clock_free (lua_State *L);
/** Lua object destruction. */
static int pdlua_object_free (lua_State *L);
/** Dispatch Pd inlet messages to Lua objects. */
static void pdlua_dispatch (t_pdlua *o, unsigned int inlet, t_symbol *s, int argc, t_atom *argv);
/** Dispatch Pd receive messages to Lua objects. */
static void pdlua_receivedispatch (t_pdlua_proxyreceive *r, t_symbol *s, int argc, t_atom *argv);
/** Dispatch Pd clock messages to Lua objects. */
static void pdlua_clockdispatch(t_pdlua_proxyclock *clock);
/** Convert a Lua table into a Pd atom array. */
static t_atom *pdlua_popatomtable (lua_State *L, int *count, t_pdlua *o);
/** Send a message from a Lua object outlet. */
static int pdlua_outlet (lua_State *L);
/** Send a message from a Lua object to a Pd receiver. */
static int pdlua_send (lua_State *L);
/** Set a [value] object's value. */
static int pdlua_setvalue (lua_State *L);
/** Get a [value] object's value. */
static int pdlua_getvalue (lua_State *L);
/** Get a [table] object's array. */
static int pdlua_getarray (lua_State *L);
/** Read from a [table] object's array. */
static int pdlua_readarray (lua_State *L);
/** Write to a [table] object's array. */
static int pdlua_writearray (lua_State *L);
/** Redraw a [table] object's graph. */
static int pdlua_redrawarray (lua_State *L);
/** Post to Pd's console. */
static int pdlua_post (lua_State *L);
/** Report an error from a Lua object to Pd's console. */
static int pdlua_error (lua_State *L);
static void pdlua_setrequirepath (lua_State *L, const char *path);
static void pdlua_clearrequirepath (lua_State *L);
/** Run a Lua script using Pd's path. */
static int pdlua_dofile (lua_State *L);
/** Initialize the pd API for Lua. */
static void pdlua_init (lua_State *L);
/** Pd loader hook for loading and executing Lua scripts. */
static int pdlua_loader_legacy (t_canvas *canvas, char *name);
/** Start the Lua runtime and register our loader hook. */
#ifdef _WIN32
__declspec(dllexport)
#endif 
#ifdef PLUGDATA
void pdlua_setup(const char *datadir, char *versbuf, int versbuf_length, void(*register_class_callback)(const char*));
#else
void pdlua_setup (void);
#endif
/* end prototypes*/

/* globals */
struct pdlua_proxyinlet;
struct pdlua_proxyreceive;
struct pdlua_proxyclock;
/** Proxy inlet class pointer. */
static t_class *pdlua_proxyinlet_class;
/** Proxy receive class pointer. */
static t_class *pdlua_proxyreceive_class;
/** Proxy clock class pointer. */
static t_class *pdlua_proxyclock_class;

/** Lua file reader callback. */
static const char *pdlua_reader
(
    lua_State *UNUSED(L), /**< Lua interpreter state. */
    void *rr, /**< Lua file reader state. */
    size_t *size /**< How much data we have read. */
)
{
    t_pdlua_readerdata  *r = rr;
    ssize_t             s;
    PDLUA_DEBUG("pdlua_reader: fd is %d", r->fd);
    s = read(r->fd, r->buffer, MAXPDSTRING-2);
    PDLUA_DEBUG("pdlua_reader: s is %ld", s);////////
    if (s <= 0)
    {
        *size = 0;
        return NULL;
    }
    else
    {
        *size = s;
        return r->buffer;
    }
}

/** Proxy inlet 'anything' method. */
static void pdlua_proxyinlet_anything
(
    t_pdlua_proxyinlet  *p, /**< The proxy inlet that received the message. */
    t_symbol            *s, /**< The message selector. */
    int                 argc, /**< The message length. */
    t_atom              *argv /**< The atoms in the message. */
)
{
    pdlua_dispatch(p->owner, p->id, s, argc, argv);
}

static void pdlua_proxyinlet_fwd
(
    t_pdlua_proxyinlet  *p, /**< The proxy inlet that received the message. */
    t_symbol            *UNUSED(s), /**< The message selector, which is always "fwd" */
    int                 argc, /**< The message length. */
    t_atom              *argv /**< The atoms in the message. The first atom is the actual selector */
)
{
    if(!argc) return;
    pdlua_dispatch(p->owner, p->id, atom_getsymbol(argv), argc-1, argv+1);
}

/** Proxy inlet initialization. */
static void pdlua_proxyinlet_init
(
    t_pdlua_proxyinlet  *p, /**< The proxy inlet to initialize. */
    struct pdlua        *owner, /**< The owning object. */
    unsigned int        id /**< The inlet number. */
)
{
    p->pd = pdlua_proxyinlet_class;
    p->owner = owner;
    p->id = id;
}

/** Register the proxy inlet class with Pd. */
static void pdlua_proxyinlet_setup(void)
{
    pdlua_proxyinlet_class = class_new(gensym("pdlua proxy inlet"), 0, 0, sizeof(t_pdlua_proxyinlet), 0, 0);
    if (pdlua_proxyinlet_class) {
        class_addanything(pdlua_proxyinlet_class, pdlua_proxyinlet_anything);
        class_addmethod(pdlua_proxyinlet_class, (t_method)pdlua_proxyinlet_fwd, gensym("fwd"), A_GIMME, 0);
    }
}

/** Proxy receive 'anything' method. */
static void pdlua_proxyreceive_anything(
    t_pdlua_proxyreceive    *r, /**< The proxy receive that received the message. */
    t_symbol                *s, /**< The message selector. */
    int                     argc, /**< The message length. */
    t_atom                  *argv /**< The atoms in the message. */
)
{
    pdlua_receivedispatch(r, s, argc, argv);
}

/** Proxy receive allocation and initialization. */
static t_pdlua_proxyreceive *pdlua_proxyreceive_new
(
    struct pdlua    *owner, /**< The owning object. */
    t_symbol        *name /**< The symbol to bind to. */
)
{
    t_pdlua_proxyreceive *r = malloc(sizeof(t_pdlua_proxyreceive));
    r->pd = pdlua_proxyreceive_class;
    r->owner = owner;
    r->name = name;
    pd_bind(&r->pd, r->name);
    return r;
}

/** Proxy receive cleanup and deallocation. */
static void pdlua_proxyreceive_free(t_pdlua_proxyreceive *r /**< The proxy receive to free. */)
{
    pd_unbind(&r->pd, r->name);
    r->pd = NULL;
    r->owner = NULL;
    r->name = NULL;
    free(r);
}

/** Register the proxy receive class with Pd. */
static void pdlua_proxyreceive_setup()
{
    pdlua_proxyreceive_class = class_new(gensym("pdlua proxy receive"), 0, 0, sizeof(t_pdlua_proxyreceive), 0, 0);
    if (pdlua_proxyreceive_class)
        class_addanything(pdlua_proxyreceive_class, pdlua_proxyreceive_anything);
}

/** Proxy clock 'bang' method. */
static void pdlua_proxyclock_bang(t_pdlua_proxyclock *c /**< The proxy clock that received the message. */)
{
    pdlua_clockdispatch(c);
}

/** Proxy clock allocation and initialization. */
static t_pdlua_proxyclock *pdlua_proxyclock_new
(
    struct pdlua *owner /**< The object to forward messages to. */
)
{
    t_pdlua_proxyclock *c = malloc(sizeof(t_pdlua_proxyclock));
    c->pd = pdlua_proxyclock_class;
    c->owner = owner;
    c->clock = clock_new(c, (t_method) pdlua_proxyclock_bang);
    return c;
}

/** Register the proxy clock class with Pd. */
static void pdlua_proxyclock_setup(void)
{
    pdlua_proxyclock_class = class_new(gensym("pdlua proxy clock"), 0, 0, sizeof(t_pdlua_proxyclock), 0, 0);
}

/** Dump an array of atoms into a Lua table. */
static void pdlua_pushatomtable
(
    int     argc, /**< The number of atoms in the array. */
    t_atom  *argv /**< The array of atoms. */
)
{
    int i;

    PDLUA_DEBUG("pdlua_pushatomtable: stack top %d", lua_gettop(__L()));
    lua_newtable(__L());
    for (i = 0; i < argc; ++i)
    {
        lua_pushnumber(__L(), i+1);
        switch (argv[i].a_type)
        {
            case A_FLOAT:
                lua_pushnumber(__L(), argv[i].a_w.w_float);
                break;
            case A_SYMBOL:
                lua_pushstring(__L(), argv[i].a_w.w_symbol->s_name);
                break;
            case A_POINTER: /* FIXME: check experimentality */
                lua_pushlightuserdata(__L(), argv[i].a_w.w_gpointer);
            break;
            default:
                pd_error(NULL, "lua: zomg weasels!");
                lua_pushnil(__L());
            break;
        }
        lua_settable(__L(), -3);
    }
    PDLUA_DEBUG("pdlua_pushatomtable: end. stack top %d", lua_gettop(__L()));
}

static const char *basename(const char *name)
{
  /* strip dir from name : */
  const char *basenamep = strrchr(name, '/');
#ifdef _WIN32
  if (!basenamep)
    basenamep = strrchr(name, '\\');
#endif
  if (!basenamep)
    basenamep = name;
  else basenamep++;   /* strip last '/' */
  return basenamep;
}

/** Pd object constructor. */
static t_pdlua *pdlua_new
(
    t_symbol    *s, /**< The construction message selector. */
    int         argc, /**< The construction message atom count. */
    t_atom      *argv /**< The construction message atoms. */
)
{
    int i;
    PDLUA_DEBUG("pdlua_new: s->s_name is %s", s->s_name);
    for (i = 0; i < argc; ++i)
    {
        switch (argv[i].a_type)
        {
        case A_FLOAT:
            PDLUA_DEBUG2("argv[%d]: %f", i, argv[i].a_w.w_float);
            break;
        case A_SYMBOL:
            PDLUA_DEBUG2("argv[%d]: %s", i, argv[i].a_w.w_symbol->s_name);
            break;
        default:
            pd_error(NULL, "pdlua_new: bad argument type"); // should never happen
            return NULL;
        }
    }
        
    PDLUA_DEBUG("pdlua_new: start with stack top %d", lua_gettop(__L()));
    lua_getglobal(__L(), "pd");
    lua_getfield(__L(), -1, "_checkbase");
    lua_pushstring(__L(), s->s_name);
    lua_pcall(__L(), 1, 1, 0);
    int needs_base = lua_toboolean(__L(), -1);
    lua_pop(__L(), 1); /* pop boolean */
    /* have to load the .pd_lua file for basename if another class is owning it */
    if(needs_base) {
        char                buf[MAXPDSTRING];
        char                *ptr;
        t_pdlua_readerdata  reader;
        t_canvas* current = canvas_getcurrent();
        int fd = canvas_open(current, s->s_name, ".pd_lua", buf, &ptr, MAXPDSTRING, 1);
        if (fd >= 0)
        {
            PDLUA_DEBUG("basename open: stack top %d", lua_gettop(__L()));
            /* save old loadname, restore later in case of
             * nested loading */ 
            int n, load_name_save, load_path_save;
            lua_getfield(__L(), -1, "_loadname");
            load_name_save = luaL_ref(__L(), LUA_REGISTRYINDEX);
            lua_pushnil(__L());
            lua_setfield(__L(), -2, "_loadname");
            lua_getfield(__L(), -1, "_loadpath");
            load_path_save = luaL_ref(__L(), LUA_REGISTRYINDEX);
            lua_pushstring(__L(), buf);
            lua_setfield(__L(), -2, "_loadpath");

            PDLUA_DEBUG("pdlua_new (basename load) path is %s", buf);
            //pdlua_setpathname(o, buf);/* change the scriptname to include its path 
            pdlua_setrequirepath(__L(), buf);
            class_set_extern_dir(gensym(buf));
            strncpy(buf, s->s_name, MAXPDSTRING - 8);
            strcat(buf, ".pd_lua");
            reader.fd = fd;
            n = lua_gettop(__L());
#if LUA_VERSION_NUM	< 502
            if (lua_load(__L(), pdlua_reader, &reader, buf))
#else // 5.2 style
            if (lua_load(__L(), pdlua_reader, &reader, buf, NULL))
#endif // LUA_VERSION_NUM	< 502
            {
                close(fd);
                pdlua_clearrequirepath(__L());
                lua_error(__L());
            }
            else
            {
                if (lua_pcall(__L(), 0, LUA_MULTRET, 0))
                {
                    pd_error(NULL, "lua: error running `%s':\n%s", buf, lua_tostring(__L(), -1));
                    lua_pop(__L(), 1);
                    close(fd);
                    pdlua_clearrequirepath(__L());
                }
                else
                {
                    /* succeeded */
                    close(fd);
                    pdlua_clearrequirepath(__L());
                }
            }
            class_set_extern_dir(&s_);
            lua_settop(__L(), n); /* discard results of load */
            lua_rawgeti(__L(), LUA_REGISTRYINDEX, load_path_save);
            lua_setfield(__L(), -2, "_loadpath");
            luaL_unref(__L(), LUA_REGISTRYINDEX, load_path_save);
            lua_rawgeti(__L(), LUA_REGISTRYINDEX, load_name_save);
            lua_setfield(__L(), -2, "_loadname");
            luaL_unref(__L(), LUA_REGISTRYINDEX, load_name_save);
        }
        else pd_error(NULL, "lua: error loading `%s': canvas_open() failed", buf);
    }
    
    PDLUA_DEBUG("pdlua_new: after load script. stack top %d", lua_gettop(__L()));
    lua_getfield(__L(), -1, "_constructor");
    lua_pushstring(__L(), s->s_name);
    pdlua_pushatomtable(argc, argv);
    PDLUA_DEBUG("pdlua_new: before lua_pcall(L, 2, 1, 0) stack top %d", lua_gettop(__L()));
    if (lua_pcall(__L(), 2, 1, 0))
    {
        pd_error(NULL, "pdlua_new: error in constructor for `%s':\n%s", s->s_name, lua_tostring(__L(), -1));
        lua_pop(__L(), 2); /* pop the error string and the global "pd" */
        return NULL;
    }
    else
    {
        t_pdlua *object = NULL;
        PDLUA_DEBUG("pdlua_new: done lua_pcall(L, 2, 1, 0) stack top %d", lua_gettop(__L()));
        if (lua_islightuserdata(__L(), -1))
        {
            object = lua_touserdata(__L(), -1);
            lua_pop(__L(), 2);/* pop the userdata and the global "pd" */
            PDLUA_DEBUG2("pdlua_new: before returning object %p stack top %d", object, lua_gettop(__L()));
             return object;
        }
        else
        {
            lua_pop(__L(), 2);/* pop the userdata and the global "pd" */
            PDLUA_DEBUG("pdlua_new: done FALSE lua_islightuserdata(L, -1)", 0);
            return NULL;
        }
    }
}

/** Pd object destructor. */
static void pdlua_free( t_pdlua *o /**< The object to destruct. */)
{
    PDLUA_DEBUG("pdlua_free: stack top %d", lua_gettop(__L()));
    lua_getglobal(__L(), "pd");
    lua_getfield (__L(), -1, "_destructor");
    lua_pushlightuserdata(__L(), o);
    if (lua_pcall(__L(), 1, 0, 0))
    {
        pd_error(NULL, "lua: error in destructor:\n%s", lua_tostring(__L(), -1));
        lua_pop(__L(), 1); /* pop the error string */
    }
    lua_pop(__L(), 1); /* pop the global "pd" */
    PDLUA_DEBUG("pdlua_free: end. stack top %d", lua_gettop(__L()));
    
    // Collect garbage
    // If we don't do this here, it could potentially leak if no other pdlua objects are used afterwards
    lua_gc(__L(), LUA_GCCOLLECT);
    
    return;
}

void pdlua_vis(t_gobj *z, t_glist *glist, int vis){
    
    t_pdlua* x = (t_pdlua *)z;
    // If there's no gui, use default text vis behavior
    if(!x->has_gui)
    {
        text_widgetbehavior.w_visfn(z, glist, vis);
        return;
    }

    // Otherwise, repaint or clear the custom graphics
    if(vis)
    {
        pdlua_gfx_repaint(x, 1);
    }
    else {
        pdlua_gfx_clear(x, 1);
    }
}

static void pdlua_delete(t_gobj *z, t_glist *glist){
    if(!((t_pdlua *)z)->has_gui)
    {
        text_widgetbehavior.w_deletefn(z, glist);
        return;
    }
    if(glist_isvisible(glist) && gobj_shouldvis(z, glist)) {
        pdlua_vis(z, glist, 0);
    }
    
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void pdlua_motion(t_gobj *z, t_floatarg dx, t_floatarg dy,
    t_floatarg up)
{
#if !PLUGDATA
    if (!up)
    {
        t_pdlua *x = (t_pdlua *)z;
        x->gfx.mouse_drag_x = x->gfx.mouse_drag_x + dx;
        x->gfx.mouse_drag_y = x->gfx.mouse_drag_y + dy;
        int zoom = glist_getzoom(glist_getcanvas(x->canvas));
        int xpos = (x->gfx.mouse_drag_x - text_xpix(&x->pd, x->canvas)) / zoom;
        int ypos = (x->gfx.mouse_drag_y - text_ypix(&x->pd, x->canvas)) / zoom;
        pdlua_gfx_mouse_drag(x, xpos, ypos);
    }
#endif
}

static int pdlua_click(t_gobj *z, t_glist *gl, int xpos, int ypos, int shift, int alt, int dbl, int doit){
    t_pdlua *x = (t_pdlua *)z;
#if !PLUGDATA
    if(x->has_gui)
    {
        int zoom = glist_getzoom(gl);
        int xpix = (xpos - text_xpix(&x->pd, gl)) / zoom; 
        int ypix = (ypos - text_ypix(&x->pd, gl)) / zoom;
                
        if(doit){
            if(!x->gfx.mouse_down)
            {
                pdlua_gfx_mouse_down(x, xpix, ypix);
                x->gfx.mouse_drag_x = xpos;
                x->gfx.mouse_drag_y = ypos;
            }
            
            glist_grab(x->canvas, &x->pd.te_g, (t_glistmotionfn)pdlua_motion, NULL, xpos, ypos);
        }
        else {
            pdlua_gfx_mouse_move(x, xpix, ypix);
            
            if(x->gfx.mouse_down)
            {
                pdlua_gfx_mouse_up(x, xpix, ypix);
            }
        }
        
        x->gfx.mouse_down = doit;
        return 1;
    }
#endif
    return text_widgetbehavior.w_clickfn(z, gl, xpos, ypos, shift, alt, dbl, doit);
}

// The _reload method will tell the pdlua object to reload the original script that spawned it
// This is used in plugdata for dynamic reloading, but can be useful in other environments too
// Prefixed with _ to prevent namespace pollution
static void pdlua_reload(t_gobj* z)
{
    pdlua_dispatch((t_pdlua *)z, 0, gensym("_reload"), 0, NULL);
}

static void pdlua_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_pdlua *x = (t_pdlua *)z;
    

    if(x->has_gui)
    {
       x->pd.te_xpix += dx, x->pd.te_ypix += dy;
       dx *= glist_getzoom(glist), dy *= glist_getzoom(glist);
#if !PLUGDATA
        gfx_displace((t_pdlua*)z, glist, dx, dy);
#endif
    }
    else {
        text_widgetbehavior.w_displacefn(z, glist, dx, dy);
    }

    
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void pdlua_activate(t_gobj *z, t_glist *glist, int state)
{
    if(!((t_pdlua *)z)->has_gui)
    {
        // Bypass to text widgetbehaviour if we're not a GUI
        text_widgetbehavior.w_activatefn(z, glist, state);
    }
}

static void pdlua_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_pdlua *x = (t_pdlua *)z;
    if(x->has_gui) {
        float x1 = text_xpix((t_text *)x, glist), y1 = text_ypix((t_text *)x, glist);
        *xp1 = x1;
        *yp1 = y1;
        *xp2 = x1 + x->gfx.width * glist->gl_zoom;
        *yp2 = y1 + x->gfx.height * glist->gl_zoom;
    }
    else {
        // Bypass to text widgetbehaviour if we're not a GUI
        text_widgetbehavior.w_getrectfn(z, glist, xp1, yp1, xp2, yp2);
    }
}

#if 0
static void pdlua_stack_dump (lua_State *L)
{
    int i;
    int top = lua_gettop(L);

    for (i = 1; i <= top; i++)
    {  /* repeat for each level */
        int t = lua_type(L, i);
        switch (t)
        {
            case LUA_TSTRING:  /* strings */
                printf("`%s'", lua_tostring(L, i));
                break;

            case LUA_TBOOLEAN:  /* booleans */
                printf(lua_toboolean(L, i) ? "true" : "false");
                break;

            case LUA_TNUMBER:  /* numbers */
                printf("%g", lua_tonumber(L, i));
                break;

            default:  /* other values */
                printf("%s", lua_typename(L, t));
                break;
        }
        printf("  ");  /* put a separator */
    }
    printf("\n");  /* end the listing */
}
#endif

/* nw.js support. If this is non-NULL then we're running inside Jonathan
   Wilkes' Pd-L2Ork variant and access to the GUI uses JavaScript. */
static void (*nw_gui_vmess)(const char *sel, char *fmt, ...) = NULL;

/* plugdata support. Similarly, if we're running inside plugdata, we can send GUI messages with plugdata_forward_message
 This allows opening an in-gui text editor instead of opening another app
 */
#if PLUGDATA
void plugdata_forward_message(void* x, t_symbol *s, int argc, t_atom *argv);
#endif
/** a handler for the open item in the right-click menu (mrpeach 20111025) */
/** Here we find the lua code for the object and open it in an editor */
static void pdlua_menu_open(t_pdlua *o)
{
    #if PLUGDATA
        // This is a more reliable method of finding out what file an object came from
        // TODO: we might also want to use something like this for pd-vanilla?
        lua_getglobal(__L(), "pd");
        lua_getfield(__L(), -1, "_whereami");
        lua_pushstring(__L(),  o->pd.te_pd->c_name->s_name);

        if (lua_pcall(__L(), 1, 1, 0))
        {
            pd_error(NULL, "lua: error in whereami:\n%s", lua_tostring(__L(), -1));
            lua_pop(__L(), 2); /* pop the error string and the global "pd" */
            return;
        }
        if(lua_isstring(__L(), -1)) {
            const char* fullpath = luaL_checkstring(__L(), -1);
            if(fullpath) {
                t_atom arg;
                SETSYMBOL(&arg, gensym(fullpath));
                plugdata_forward_message(o, gensym("open_textfile"), 1, &arg);
            }
            return;
        }
#endif

    const char  *name;
    const char  *path;
    char        pathname[FILENAME_MAX];
    t_class     *class;

    PDLUA_DEBUG("pdlua_menu_open stack top is %d", lua_gettop(__L()));
    /** Get the scriptname of the object */
    lua_getglobal(__L(), "pd");
    lua_getfield(__L(), -1, "_whoami");
    lua_pushlightuserdata(__L(), o);
    if (lua_pcall(__L(), 1, 1, 0))
    {
        pd_error(NULL, "lua: error in whoami:\n%s", lua_tostring(__L(), -1));
        lua_pop(__L(), 2); /* pop the error string and the global "pd" */
        return;
    }
    name = luaL_checkstring(__L(), -1);
    PDLUA_DEBUG3("pdlua_menu_open: L is %p, name is %s stack top is %d", __L(), name, lua_gettop(__L()));
    if (name)
    {
        lua_getglobal(__L(), "pd");
        lua_getfield(__L(), -1, "_get_class");
        lua_pushlightuserdata(__L(), o);
        if (lua_pcall(__L(), 1, 1, 0))
        {
            pd_error(NULL, "lua: error in get_class:\n%s", lua_tostring(__L(), -1));
            lua_pop(__L(), 4); /* pop the error string, global "pd", name, global "pd"*/
            return;
        }
        class = (t_class *)lua_touserdata(__L(), -1);
#if PLUGDATA
        if (!*class->c_externdir->s_name)
            path = plugdata_datadir;
        else
#endif
        path = class->c_externdir->s_name;
        snprintf(pathname, FILENAME_MAX-1, "%s/%s", path, name);
        lua_pop(__L(), 4); /* pop class, global "pd", name, global "pd"*/
        
#if PD_MAJOR_VERSION==0 && PD_MINOR_VERSION<43
        post("Opening %s for editing", pathname);
#else
        logpost(NULL, 3, "Opening %s for editing", pathname);
#endif
#if PLUGDATA
        t_atom arg;
        SETSYMBOL(&arg, gensym(pathname));
        plugdata_forward_message(o, gensym("open_textfile"), 1, &arg);
#else
        if (nw_gui_vmess)
          nw_gui_vmess("open_textfile", "s", pathname);
        else
          sys_vgui("::pd_menucommands::menu_openfile {%s}\n", pathname);
#endif
    }
    PDLUA_DEBUG("pdlua_menu_open end. stack top is %d", lua_gettop(__L()));
}

static t_int *pdlua_perform(t_int *w){
    t_pdlua *o = (t_pdlua *)(w[1]);
    int nblock = (int)(w[2]);
    
    PDLUA_DEBUG("pdlua_perform: stack top %d", lua_gettop(__L()));
    lua_getglobal(__L(), "pd");
    lua_getfield (__L(), -1, "_perform_dsp");
    lua_pushlightuserdata(__L(), o);
    
    for (int i = 0; i < o->siginlets; i++)
    {
        lua_newtable(__L());
        t_float *in = (t_float*)w[i + 3];
        for (int j = 0; j < nblock; j++)
        {
            lua_pushinteger(__L(), j + 1);
            lua_pushnumber(__L(), in[j]);
            lua_settable(__L(), -3);
        }
    }
    
    if (lua_pcall(__L(), 1 + o->siginlets, o->sigoutlets, 0))
    {
        pd_error(o, "pdlua: error in perform:\n%s", lua_tostring(__L(), -1));
        lua_pop(__L(), 2); /* pop the error string and global pd */
        return w + o->siginlets + o->sigoutlets + 3;
    }
    
    if (!lua_istable(__L(), -1))
    {
        const char *s = "pdlua: 'perform' function should return";
        if (o->sigoutlets == 1)
            pd_error(o, "%s %s", s, "a table");
        else if (o->sigoutlets > 1)
            pd_error(o, "%s %d %s", s, o->sigoutlets, "tables");
        lua_pop(__L(), 1 + o->sigoutlets);
        return w + o->siginlets + o->sigoutlets + 3;
    }
    
    for (int i = o->sigoutlets - 1; i >= 0; i--)
    {
        t_float *out = (t_float*)w[i + 3 + o->siginlets];
        for (int j = 0; j < nblock; j++)
        {
            lua_pushinteger(__L(), (lua_Integer)(j + 1));
            lua_gettable(__L(), -2);
            if (lua_isnumber(__L(), -1))
                out[j] = (t_float)lua_tonumber(__L(), -1);
            else if (lua_isboolean(__L(), -1))
                out[j] = (t_float)lua_toboolean(__L(), -1);
            else
                out[j] = 0.0f;
            lua_pop(__L(), 1);
        }
        lua_pop(__L(), 1);
    }

    lua_pop(__L(), 1); /* pop the global "pd" */
    
    PDLUA_DEBUG("pdlua_perform: end. stack top %d", lua_gettop(__L()));
    
    return w + o->siginlets + o->sigoutlets + 3;
}

static void pdlua_dsp(t_pdlua *x, t_signal **sp){
    int sum = x->siginlets + x->sigoutlets;
    if(sum == 0) return;
    
    PDLUA_DEBUG("pdlua_dsp: stack top %d", lua_gettop(__L()));
    lua_getglobal(__L(), "pd");
    lua_getfield (__L(), -1, "_dsp");
    lua_pushlightuserdata(__L(), x);
    lua_pushnumber(__L(), sys_getsr());
    lua_pushnumber(__L(), sys_getblksize());
    
    if (lua_pcall(__L(), 3, 0, 0))
    {
        pd_error(x, "pdlua: error in dsp:\n%s", lua_tostring(__L(), -1));
        lua_pop(__L(), 1); /* pop the error string */
    }
    lua_pop(__L(), 1); /* pop the global "pd" */
    
    PDLUA_DEBUG("pdlua_dsp: end. stack top %d", lua_gettop(__L()));
    
    int sigvecsize = sum + 2;
    t_int* sigvec = getbytes(sigvecsize * sizeof(t_int));
    
    sigvec[0] = (t_int)x;
    sigvec[1] = (t_int)sp[0]->s_n;
    
    for (int i = 0; i < sum; i++)
        sigvec[i + 2] = (t_int)sp[i]->s_vec;
    
    dsp_addv(pdlua_perform, sigvecsize, sigvec);
    freebytes(sigvec, sigvecsize * sizeof(t_int));
}

static int pdlua_set_arguments(lua_State *L)
{
    // Check if the first argument is a valid user data pointer
    if (lua_islightuserdata(L, 1))
    {
        // Retrieve the userdata pointer
        t_pdlua *o = lua_touserdata(L, 1);

        // Retrieve the binbuf
        t_binbuf* b = o->pd.te_binbuf;

        if (!b) return 0;

        t_atom name;
        SETSYMBOL(&name, atom_getsymbol(binbuf_getvec(b)));
        binbuf_clear(b);
        binbuf_add(b, 1, &name);

        // Check if the second argument is a table
        if (lua_istable(L, 2)) {

            // Get the number of elements in the table
            int argc = lua_rawlen(L, 2);

            // Iterate through the table elements
            for (int i = 1; i <= argc; i++) {
                // Push the i-th element of the table onto the stack
                lua_rawgeti(L, 2, i);

                // Check the type of the element
                if (lua_isnumber(L, -1)) {
                    // If it's a number, add it to binbuf as a float
                    double num = lua_tonumber(L, -1);
                    t_atom atom;
                    SETFLOAT(&atom, num);
                    binbuf_add(b, 1, &atom);
                }
                else if (lua_isstring(L, -1)) {
                    // If it's a string, convert it to a symbol and add to binbuf
                    const char* str = lua_tostring(L, -1);
                    t_atom atom;
                    SETSYMBOL(&atom, gensym(str));
                    binbuf_add(b, 1, &atom);
                }

                // Pop the value from the stack
                lua_pop(L, 1);
            }
        }
    }

    return 0;
}


t_widgetbehavior pdlua_widgetbehavior;

/** Lua class registration. This is equivalent to the "setup" method for an ordinary Pd class */
static int pdlua_class_new(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Class name string.
  * \par Outputs:
  * \li \c 1 Pd class pointer.
  * */
{
    const char  *name;
    t_class     *c;

    name = luaL_checkstring(L, 1);
    PDLUA_DEBUG3("pdlua_class_new: L is %p, name is %s stack top is %d", L, name, lua_gettop(L));
    c = class_new(gensym((char *) name), (t_newmethod) pdlua_new,
        (t_method) pdlua_free, sizeof(t_pdlua), CLASS_NOINLET, A_GIMME, 0);
    
    // Let plugdata know this class is a lua object
#if PLUGDATA
    plugdata_register_class(name);
#endif

#ifndef PURR_DATA
    /* Vanilla Pd and plugdata require this for the gfx routines, but this
       interferes with Purr Data's handling of canvas events and is thus
       disabled there. XXXTODO: When we add the gfx API in Purr Data, we'll
       have to figure out how to tie into Purr Data's JavaScript GUI in order
       to implement these callbacks. -ag */
    // Set custom widgetbehaviour for GUIs
    pdlua_widgetbehavior.w_getrectfn  = pdlua_getrect;
    pdlua_widgetbehavior.w_displacefn = pdlua_displace;
    pdlua_widgetbehavior.w_selectfn   = text_widgetbehavior.w_selectfn;
    pdlua_widgetbehavior.w_deletefn   = pdlua_delete;
    pdlua_widgetbehavior.w_clickfn    = pdlua_click;
    pdlua_widgetbehavior.w_visfn      = pdlua_vis;
    pdlua_widgetbehavior.w_activatefn = pdlua_activate;
    class_setwidget(c, &pdlua_widgetbehavior);
#endif

    if (c) {
        /* a class with a "menu-open" method will have the "Open" item highlighted in the right-click menu */
        class_addmethod(c, (t_method)pdlua_menu_open, gensym("menu-open"), A_NULL);/* (mrpeach 20111025) */
        class_addmethod(c, (t_method)pdlua_reload, gensym("_reload"), A_NULL);/* (mrpeach 20111025) */
        class_addmethod(c, (t_method)pdlua_dsp, gensym("dsp"), A_CANT, 0); /* timschoen 20240226 */
    }
/**/

    lua_pushlightuserdata(L, c);
    PDLUA_DEBUG("pdlua_class_new: end stack top is %d", lua_gettop(L));
    return 1;
}

/** Lua object creation. */
static int pdlua_object_new(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd class pointer.
  * \par Outputs:
  * \li \c 2 Pd object pointer.
  * */
{
    PDLUA_DEBUG("pdlua_object_new: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_class *c = lua_touserdata(L, 1);
        if(c)
        {
            PDLUA_DEBUG("pdlua_object_new: path is %s", c->c_externdir->s_name);
            t_pdlua *o = (t_pdlua *) pd_new(c);
            if (o)
            {
                o->inlets = 0;
                o->in = NULL;
                o->proxy_in = NULL;
                o->outlets = 0;
                o->out = NULL;
                o->siginlets = 0;
                o->sigoutlets = 0;
                o->canvas = canvas_getcurrent();
                
                o->gfx.width = 80;
                o->gfx.height = 80;
               
#if !PLUGDATA
                // Init graphics state for pd
                o->gfx.mouse_drag_x = 0;
                o->gfx.mouse_drag_y = 0;
                o->gfx.mouse_down = 0;
#else
                // NULL until plugdata overrides them with something useful
                o->gfx.plugdata_draw_callback = NULL;
#endif
                
                lua_pushlightuserdata(L, o);
                PDLUA_DEBUG("pdlua_object_new: success end. stack top is %d", lua_gettop(L));
                return 1;
            }
        }
    }
    PDLUA_DEBUG("pdlua_object_new: fail end. stack top is %d", lua_gettop(L));
    return 0;
}

static int pdlua_object_creategui(lua_State *L)
{
    t_pdlua *o = lua_touserdata(L, 1);
#ifndef PURR_DATA
    o->has_gui = 1;
    gfx_initialize(o);
#else
    // We avoid the gfx initalization here, since it produces unwanted
    // artifacts on the canvas and at present none of the graphics routines
    // will work in Purr Data anyway. -ag
    o->has_gui = 0;
    gfx_not_implemented();
#endif
    return 0;
}

/** Lua object inlet creation. */
static int pdlua_object_createinlets(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd object pointer.
  * \li \c 2 Number of inlets.
  * */
{

    PDLUA_DEBUG("pdlua_object_createinlets: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_pdlua *o = lua_touserdata(L, 1);
        if(o) {
            if (lua_isnumber(L, 2)) {
                // If it's a number, it means the number of data inlets
                o->inlets = luaL_checknumber(L, 2);
                o->proxy_in = malloc(o->inlets * sizeof(t_pdlua_proxyinlet));
                o->in = malloc(o->inlets * sizeof(t_inlet*));
                for (int i = 0; i < o->inlets; ++i)
                {
                    pdlua_proxyinlet_init(&o->proxy_in[i], o, i);
                    o->in[i] = inlet_new(&o->pd, &o->proxy_in[i].pd, 0, 0);
                }
            } else if (lua_istable(L, 2)) {
                // If it's a table, it means a list of inlet types (data or signal)
                o->inlets = lua_rawlen(L, 2);
                o->proxy_in = malloc(o->inlets * sizeof(t_pdlua_proxyinlet));
                o->in = malloc(o->inlets * sizeof(t_inlet*));
                for (int i = 0; i < o->inlets; ++i)
                {
                    lua_rawgeti(L, 2, i + 1); // Get element at index i+1
                    if (lua_isnumber(L, -1)) {
                        int is_signal = lua_tonumber(L, -1);
                        o->siginlets += is_signal;
                        
                        pdlua_proxyinlet_init(&o->proxy_in[i], o, i);
                        o->in[i] = inlet_new(&o->pd, &o->proxy_in[i].pd, is_signal ? &s_signal : 0, is_signal ? &s_signal : 0);
                    }
                    lua_pop(L, 1); // Pop the value from the stack
                }
            } else {
                // Invalid argument type
                return luaL_error(L, "inlets must be a number or a table");
            }
        }
        
    }
    PDLUA_DEBUG("pdlua_object_createinlets: end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Lua object outlet creation. */
static int pdlua_object_createoutlets(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd object pointer.
  * \li \c 2 Number of outlets.
  * */
{
    PDLUA_DEBUG("pdlua_object_createoutlets: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_pdlua *o = lua_touserdata(L, 1);
        if (o)
        {
            if (lua_isnumber(L, 2)) {
                // If it's a number, it means the number of data outlets
                o->outlets = luaL_checknumber(L, 2);
                if (o->outlets > 0)
                {
                    o->out = malloc(o->outlets * sizeof(t_outlet *));
                    for (int i = 0; i < o->outlets; ++i) o->out[i] = outlet_new(&o->pd, 0);
                }
                else o->out = NULL;
            } else if (lua_istable(L, 2)) {
                // If it's a table, it means a list of outlet types (data or signal)
                o->outlets = lua_rawlen(L, 2);
                if (o->outlets > 0)
                {
                    o->out = malloc(o->outlets * sizeof(t_outlet *));
                    for (int i = 0; i < o->outlets; ++i)
                    {
                        lua_rawgeti(L, 2, i + 1); // Get element at index i+1
                        if (lua_isnumber(L, -1)) {
                            int is_signal = lua_tonumber(L, -1);
                            o->sigoutlets += is_signal;
                            o->out[i] = outlet_new(&o->pd, is_signal ? &s_signal : 0);
                        }
                        lua_pop(L, 1); // Pop the value from the stack
                    }
                }
                else o->out = NULL;
            } else {
                // Invalid argument type
                return luaL_error(L, "outlets must be a number or a table");
            }
            
        }
    }
    PDLUA_DEBUG("pdlua_object_createoutlets: end stack top is %d", lua_gettop(L));
    return 0;
}

/* get canvas path of an object */
static int pdlua_object_canvaspath(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd object pointer.
  * \par Outputs:
  * \li \c 1 Canvas path string.
  * */
{
    PDLUA_DEBUG("pdlua_object_canvaspath: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_pdlua *o = lua_touserdata(L, 1);
        if (o)
        {
           lua_pushstring(L, canvas_getdir(o->canvas)->s_name);
        }
    }
    PDLUA_DEBUG("pdlua_object_canvaspath: end stack top is %d", lua_gettop(L));
    return 1;
}

/** Lua object receive creation. */
static int pdlua_receive_new(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd object pointer.
  * \li \c 2 Receive name string.
  * \par Outputs:
  * \li \c 1 Pd receive pointer.
  * */
{
    PDLUA_DEBUG("pdlua_receive_new: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_pdlua *o = lua_touserdata(L, 1);
        if (o)
        {
            const char *name = luaL_checkstring(L, 2);
            if (name)
            {
                t_pdlua_proxyreceive *r =  pdlua_proxyreceive_new(o, gensym((char *) name)); /* const cast */
                lua_pushlightuserdata(L, r);
                PDLUA_DEBUG("pdlua_receive_new: success end. stack top is %d", lua_gettop(L));
                return 1;
            }
        }
    }
    PDLUA_DEBUG("pdlua_receive_new: fail end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Lua object receive destruction. */
static int pdlua_receive_free(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd recieve pointer.
  * */
{
    PDLUA_DEBUG("pdlua_receive_free: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_pdlua_proxyreceive *r = lua_touserdata(L, 1);
        if (r) pdlua_proxyreceive_free(r);
    }
    PDLUA_DEBUG("pdlua_receive_free: end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Lua object clock creation. */
static int pdlua_clock_new(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd object pointer.
  * \par Outputs:
  * \li \c 1 Pd clock pointer.
  * */
{
    PDLUA_DEBUG("pdlua_clock_new: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_pdlua *o = lua_touserdata(L, 1);
        if (o)
        {
            t_pdlua_proxyclock *c =  pdlua_proxyclock_new(o);
            lua_pushlightuserdata(L, c);
            PDLUA_DEBUG("pdlua_clock_new: success end. stack top is %d", lua_gettop(L));
            return 1;
        }
    }
    PDLUA_DEBUG("pdlua_clock_new: fail end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Lua proxy clock delay. */
static int pdlua_clock_delay(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd clock pointer.
  * \li \c 2 Number of milliseconds to delay.
  * */
{
    PDLUA_DEBUG("pdlua_clock_delay: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_pdlua_proxyclock *c = lua_touserdata(L, 1);
        if (c)
        {
            double delaytime = luaL_checknumber(L, 2);
            clock_delay(c->clock, delaytime);
        }
    }
    PDLUA_DEBUG("pdlua_clock_delay: end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Lua proxy clock set. */
static int pdlua_clock_set(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd clock pointer.
  * \li \c 2 Number to set the clock.
  * */
{
    PDLUA_DEBUG("pdlua_clock_set: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_pdlua_proxyclock *c = lua_touserdata(L, 1);
        if (c)
        {
            double systime = luaL_checknumber(L, 2);
            clock_set(c->clock, systime);
        }
    }
    PDLUA_DEBUG("pdlua_clock_set: end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Lua proxy clock unset. */
static int pdlua_clock_unset(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd clock pointer.
  * */
{
    PDLUA_DEBUG("pdlua_clock_unset: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_pdlua_proxyclock *c = lua_touserdata(L, 1);
        if (c) clock_unset(c->clock);
    }
    PDLUA_DEBUG("pdlua_clock_unset: end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Lua proxy clock destruction. */
static int pdlua_clock_free(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd clock pointer.
  * */
{
    PDLUA_DEBUG("pdlua_clock_free: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_pdlua_proxyclock *c = lua_touserdata(L, 1);
        if (c)
        {
            clock_free(c->clock);
            free(c);
        }
    }
    PDLUA_DEBUG("pdlua_clock_free: end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Lua object destruction. */
static int pdlua_object_free(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd object pointer.
  * */
{
    int i;

    PDLUA_DEBUG("pdlua_object_free: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        t_pdlua *o = lua_touserdata(L, 1);
 
        if (o)
        {
            if(o->in)
            {
                for (i = 0; i < o->inlets; ++i) inlet_free(o->in[i]);
                free(o->in);
                o->in = NULL;
            }
            
            if (o->proxy_in) freebytes(o->proxy_in, sizeof(struct pdlua_proxyinlet) * o->inlets);
            
            if(o->out)
            {
                for (i = 0; i < o->outlets; ++i) outlet_free(o->out[i]);
                free(o->out);
                o->out = NULL;
            }
        }
    }
    PDLUA_DEBUG("pdlua_object_free: end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Dispatch Pd inlet messages to Lua objects. */
static void pdlua_dispatch
(
    t_pdlua         *o, /**< The object that received the message. */
    unsigned int    inlet, /**< The inlet that the message arrived at. */
    t_symbol        *s, /**< The message selector. */
    int             argc, /**< The message length. */
    t_atom          *argv /**< The atoms in the message. */
)
{
    PDLUA_DEBUG("pdlua_dispatch: stack top %d", lua_gettop(__L()));
    lua_getglobal(__L(), "pd");
    lua_getfield (__L(), -1, "_dispatcher");
    lua_pushlightuserdata(__L(), o);
    lua_pushnumber(__L(), inlet + 1); /* C has 0.., Lua has 1.. */
    lua_pushstring(__L(), s->s_name);
    pdlua_pushatomtable(argc, argv);
    
    if (lua_pcall(__L(), 4, 0, 0))
    {
        pd_error(o, "lua: error in dispatcher:\n%s", lua_tostring(__L(), -1));
        lua_pop(__L(), 1); /* pop the error string */
    }
    lua_pop(__L(), 1); /* pop the global "pd" */
    
    
    
    PDLUA_DEBUG("pdlua_dispatch: end. stack top %d", lua_gettop(__L()));
    return;  
}

/** Dispatch Pd receive messages to Lua objects. */
static void pdlua_receivedispatch
(
    t_pdlua_proxyreceive    *r, /**< The proxy receive that received the message. */
    t_symbol                *s, /**< The message selector. */
    int                     argc, /**< The message length. */
    t_atom                  *argv /**< The atoms in the message. */
)
{
    PDLUA_DEBUG("pdlua_receivedispatch: stack top %d", lua_gettop(__L()));
    lua_getglobal(__L(), "pd");
    lua_getfield (__L(), -1, "_receivedispatch");
    lua_pushlightuserdata(__L(), r);
    lua_pushstring(__L(), s->s_name);
    pdlua_pushatomtable(argc, argv);
        
    if (lua_pcall(__L(), 3, 0, 0))
    {
        pd_error(r->owner, "lua: error in receive dispatcher:\n%s", lua_tostring(__L(), -1));
        lua_pop(__L(), 1); /* pop the error string */
    }
    lua_pop(__L(), 1); /* pop the global "pd" */
    PDLUA_DEBUG("pdlua_receivedispatch: end. stack top %d", lua_gettop(__L()));
    return;  
}

/** Dispatch Pd clock messages to Lua objects. */
static void pdlua_clockdispatch( t_pdlua_proxyclock *clock)
/**< The proxy clock that received the message. */
{
    PDLUA_DEBUG("pdlua_clockdispatch: stack top %d", lua_gettop(__L()));
    lua_getglobal(__L(), "pd");
    lua_getfield (__L(), -1, "_clockdispatch");
    lua_pushlightuserdata(__L(), clock);

    if (lua_pcall(__L(), 1, 0, 0))
    {
        pd_error(clock->owner, "lua: error in clock dispatcher:\n%s", lua_tostring(__L(), -1));
        lua_pop(__L(), 1); /* pop the error string */
    }
    lua_pop(__L(), 1); /* pop the global "pd" */
    PDLUA_DEBUG("pdlua_clockdispatch: end. stack top %d", lua_gettop(__L()));
    return;  
}

/** Convert a Lua table into a Pd atom array. */
static t_atom *pdlua_popatomtable
(
    lua_State   *L, /**< Lua interpreter state.
  * \par Inputs:
  * \li \c -1 Table to convert.
  * */
    int         *count, /**< Where to store the array length. */
    t_pdlua     *o /**< Object reference for error messages. */
)
{
    int         i;
    int         ok = 1;
    t_float     f;
    const char  *s;
    void        *p;
    size_t      sl;
    t_atom      *atoms = NULL;

    PDLUA_DEBUG("pdlua_popatomtable: stack top %d", lua_gettop(L));
    if (lua_istable(L, -1))
    {
#if LUA_VERSION_NUM	< 502
        *count = lua_objlen(L, -1);
#else // 5.2 style
        *count = lua_rawlen(L, -1);
#endif // LUA_VERSION_NUM	< 502
        if (*count > 0) atoms = malloc(*count * sizeof(t_atom));
        i = 0;
        lua_pushnil(L);
        while (lua_next(L, -2) != 0)
        {
            if (i == *count)
            {
                pd_error(o, "lua: error: too many table elements");
                ok = 0;
                break;
            }
            switch (lua_type(L, -1))
            {
                case (LUA_TNUMBER):
                    f = lua_tonumber(L, -1);
                    SETFLOAT(&atoms[i], f);
                    break;
                case (LUA_TSTRING):
                    s = lua_tolstring(L, -1, &sl);
                    if (s)
                    {
                        if (strlen(s) != sl) pd_error(o, "lua: warning: symbol munged (contains \\0 in body)");
                        SETSYMBOL(&atoms[i], gensym((char *) s));
                    }
                    else
                    {
                        pd_error(o, "lua: error: null string in table");
                        ok = 0;
                    }
                    break;
                case (LUA_TLIGHTUSERDATA): /* FIXME: check experimentality */
                    p = lua_touserdata(L, -1);
                    SETPOINTER(&atoms[i], p);
                    break;
                default:
                    pd_error(o, "lua: error: table element must be number or string or pointer");
                    ok = 0;
                    break;
            }
            lua_pop(L, 1);
            ++i;
        }
        if (i != *count)
        {
            pd_error(o, "lua: error: too few table elements");
            ok = 0;
        }
    }
    else 
    {
        pd_error(o, "lua: error: not a table");
        ok = 0;
    }
    lua_pop(L, 1);
    PDLUA_DEBUG("pdlua_popatomtable: end. stack top %d", lua_gettop(L));
    if (ok) return atoms;
    if (atoms) free(atoms);
    return NULL;
}

/** Send a message from a Lua object outlet. */
static int pdlua_outlet(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd object pointer.
  * \li \c 2 Outlet number.
  * \li \c 3 Message selector string.
  * \li \c 4 Message atom table.
  * */
{
    t_pdlua         *o;
    int             out;
    size_t          sl;
    const char      *s;
    t_symbol        *sym;
    int             count;
    t_atom          *atoms;

    PDLUA_DEBUG("pdlua_outlet: stack top %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        o = lua_touserdata(L, 1);
        if (o) 
        {
            if (lua_isnumber(L, 2)) out = lua_tonumber(L, 2) - 1; /* C has 0.., Lua has 1.. */
            else
            {
                pd_error(o, "lua: error: outlet must be a number");
                lua_pop(L, 4); /* pop all the arguments */
                return 0;
            }
            if (0 <= out && out < o->outlets) 
            {
                if (lua_isstring(L, 3)) 
                {
                    s = lua_tolstring(L, 3, &sl);
                    sym = gensym((char *) s); /* const cast */
                    if (s)
                    {
                        if (strlen(s) != sl) pd_error(o, "lua: warning: symbol munged (contains \\0 in body)");
                        lua_pushvalue(L, 4);
                        atoms = pdlua_popatomtable(L, &count, o);
                        if (count == 0 || atoms) outlet_anything(o->out[out], sym, count, atoms);
                        else pd_error(o, "lua: error: no atoms??");
                        if (atoms) 
                        {
                            free(atoms);
                            lua_pop(L, 4); /* pop all the arguments */
                            return 0;
                        }
                    }
                    else pd_error(o, "lua: error: null selector");
                }
                else pd_error(o, "lua: error: selector must be a string");
            }
            else pd_error(o, "lua: error: outlet out of range");
        }
        else pd_error(NULL, "lua: error: no object to outlet from");
    }
    else pd_error(NULL, "lua: error: bad arguments to outlet");
    lua_pop(L, 4); /* pop all the arguments */
    PDLUA_DEBUG("pdlua_outlet: end. stack top %d", lua_gettop(L));
    return 0;
}

/** Send a message from a Lua object to a Pd receiver. */
static int pdlua_send(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Receiver string.
  * \li \c 2 Message selector string.
  * \li \c 3 Message atom table.
  * */

{
    size_t          receivenamel;
    const char      *receivename;
    t_symbol        *receivesym;
    size_t          selnamel;
    const char      *selname;
    t_symbol        *selsym;
    int             count;
    t_atom          *atoms;

    PDLUA_DEBUG("pdlua_send: stack top is %d", lua_gettop(L));
    if (lua_isstring(L, 1)) 
    {
        receivename = lua_tolstring(L, 1, &receivenamel);
        receivesym = gensym((char *) receivename); /* const cast */
        if (receivesym) 
        {
            if (strlen(receivename) != receivenamel) pd_error(NULL, "lua: warning: symbol munged (contains \\0 in body)");
            if (lua_isstring(L, 2)) 
            {
                selname = lua_tolstring(L, 2, &selnamel);
                selsym = gensym((char *) selname); /* const cast */
                if (selsym)
                {
                    if (strlen(selname) != selnamel) pd_error(NULL, "lua: warning: symbol munged (contains \\0 in body)");
                    lua_pushvalue(L, 3);
                    atoms = pdlua_popatomtable(L, &count, NULL);
                    if ((count == 0 || atoms) && (receivesym->s_thing)) typedmess(receivesym->s_thing, selsym, count, atoms);
                    else pd_error(NULL, "lua: error: no atoms??");
                    if (atoms) 
                    {
                        free(atoms);
                        PDLUA_DEBUG("pdlua_send: success end. stack top is %d", lua_gettop(L));
                        return 0;
                    }
                }
                else pd_error(NULL, "lua: error: null selector");
            }
            else pd_error(NULL, "lua: error: selector must be a string");
        }
        else pd_error(NULL, "lua: error: null receive name");
    }
    else pd_error(NULL, "lua: error: receive name must be string");
    PDLUA_DEBUG("pdlua_send: fail end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Set a [value] object's value. */
static int pdlua_setvalue(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Value name string.
  * \li \c 2 Value number.
  * \par Outputs:
  * \li \c 1 success (usually depends on a [value] existing or not).
  */
{
    const char  *str = luaL_checkstring(L, 1);
    t_float     val = luaL_checknumber(L, 2);
    int         err = value_setfloat(gensym(str), val);

    PDLUA_DEBUG("pdlua_setvalue: stack top is %d", lua_gettop(L));
    lua_pushboolean(L, !err);
    PDLUA_DEBUG("pdlua_setvalue: end. stack top is %d", lua_gettop(L));
    return 1;
}

/** Get a [value] object's value. */
static int pdlua_getvalue(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Value name string.
  * \par Outputs:
  * \li \c 1 Value number, or nil for failure.
  * */
{
    const char  *str = luaL_checkstring(L, 1);
    t_float     val;
    int         err = value_getfloat(gensym(str), &val);

    PDLUA_DEBUG("pdlua_getvalue: stack top is %d", lua_gettop(L));
    if (!err) lua_pushnumber(L, val);
    else lua_pushnil(L);
    PDLUA_DEBUG("pdlua_getvalue: end. stack top is %d", lua_gettop(L));
    return 1;
}

/** Get a [table] object's array. */
static int pdlua_getarray(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Table name string.
  * \par Outputs:
  * \li \c 1 Table length, or < 0 for failure.
  * \li \c 2 Table pointer, or nil for failure.
  * */
{
    t_garray        *a;
    int             n;
    PDLUA_ARRAYTYPE *v;
    const char      *str = luaL_checkstring(L, 1);

    PDLUA_DEBUG("pdlua_getarray: stack top is %d", lua_gettop(L));
    if (!(a = (t_garray *) pd_findbyclass(gensym(str), garray_class))) 
    {
        lua_pushnumber(L, -1);
        PDLUA_DEBUG("pdlua_getarray: end 1. stack top is %d", lua_gettop(L));
        return 1;
    }
    else if (!PDLUA_ARRAYGRAB(a, &n, &v)) 
    {
        lua_pushnumber(L, -2);
        PDLUA_DEBUG("pdlua_getarray: end 2. stack top is %d", lua_gettop(L));
        return 1;
    }
    else 
    {
        lua_pushnumber(L, n);
        lua_pushlightuserdata(L, v);
        PDLUA_DEBUG("pdlua_getarray: end 3. stack top is %d", lua_gettop(L));
        return 2;
    }
}

/** Read from a [table] object's array. */
static int pdlua_readarray(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Table length number.
  * \li \c 2 Table array pointer.
  * \li \c 3 Table index number.
  * \par Outputs:
  * \li \c 1 Table element value, or nil for index out of range.
  * */
{
    int             n = luaL_checknumber(L, 1);
    PDLUA_ARRAYTYPE *v = lua_islightuserdata(L, 2) ? lua_touserdata(L, 2) : NULL;
    int             i = luaL_checknumber(L, 3);

    PDLUA_DEBUG("pdlua_readarray: stack top is %d", lua_gettop(L));
    if (0 <= i && i < n && v) 
    {
        lua_pushnumber(L, PDLUA_ARRAYELEM(v, i));
        PDLUA_DEBUG("pdlua_readarray: end 1. stack top is %d", lua_gettop(L));
        return 1;
    }
    PDLUA_DEBUG("pdlua_readarray: end 2. stack top is %d", lua_gettop(L));
    return 0;
}

/** Write to a [table] object's array. */
static int pdlua_writearray(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Table length number.
  * \li \c 2 Table array pointer.
  * \li \c 3 Table index number.
  * \li \c 4 Table element value number.
  * */
{
    int                 n = luaL_checknumber(L, 1);
    PDLUA_ARRAYTYPE     *v = lua_islightuserdata(L, 2) ? lua_touserdata(L, 2) : NULL;
    int                 i = luaL_checknumber(L, 3);
    t_float             x = luaL_checknumber(L, 4);

    PDLUA_DEBUG("pdlua_writearray: stack top is %d", lua_gettop(L));
    if (0 <= i && i < n && v) PDLUA_ARRAYELEM(v, i) = x;
    PDLUA_DEBUG("pdlua_writearray: end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Redraw a [table] object's graph. */
static int pdlua_redrawarray(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Table name string.
  * */
{
    t_garray    *a;
    const char  *str = luaL_checkstring(L, 1);

    PDLUA_DEBUG("pdlua_redrawarray: stack top is %d", lua_gettop(L));
    if ((a = (t_garray *) pd_findbyclass(gensym(str), garray_class))) garray_redraw(a);
    PDLUA_DEBUG("pdlua_redrawarray: end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Post to Pd's console. */
static int pdlua_post(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Message string.
  * */
{
    const char *str = luaL_checkstring(L, 1);
    PDLUA_DEBUG("pdlua_post: stack top is %d", lua_gettop(L));
    post("%s", str);
    PDLUA_DEBUG("pdlua_post: end. stack top is %d", lua_gettop(L));
    return 0;
}

/** Report an error from a Lua object to Pd's console. */
static int pdlua_error(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd object pointer.
  * \li \c 2 Message string.
  * */
{
    t_pdlua     *o;
    const char  *s;

    PDLUA_DEBUG("pdlua_error: stack top is %d", lua_gettop(L));
    if (lua_islightuserdata(L, 1))
    {
        o = lua_touserdata(L, 1);
        if (o)
        {
            s = luaL_checkstring(L, 2);
            if (s) pd_error(o, "%s", s);
            else pd_error(o, "lua: error: null string in error function");
        }
        else pd_error(NULL, "lua: error: null object in error function");
    }
    else pd_error(NULL, "lua: error: bad arguments to error function");
    PDLUA_DEBUG("pdlua_error: end. stack top is %d", lua_gettop(L));
    return 0;
}

static void pdlua_setrequirepath
( /* FIXME: documentation (is this of any use at all?) */
    lua_State   *L,
    const char  *path
)
{
    PDLUA_DEBUG("pdlua_setrequirepath: stack top %d", lua_gettop(L));
    lua_getglobal(L, "pd");
    lua_pushstring(L, "_setrequirepath");
    lua_gettable(L, -2);
    lua_pushstring(L, path);
    if (lua_pcall(L, 1, 0, 0) != 0)
    {
        pd_error(NULL, "lua: internal error in `pd._setrequirepath': %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    PDLUA_DEBUG("pdlua_setrequirepath: end. stack top %d", lua_gettop(L));
}

static void pdlua_clearrequirepath
( /* FIXME: documentation (is this of any use at all?) */
    lua_State *L
)
{
    PDLUA_DEBUG("pdlua_clearrequirepath: stack top %d", lua_gettop(L));
    lua_getglobal(L, "pd");
    lua_pushstring(L, "_clearrequirepath");
    lua_gettable(L, -2);
    if (lua_pcall(L, 0, 0, 0) != 0)
    {
        pd_error(NULL, "lua: internal error in `pd._clearrequirepath': %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    PDLUA_DEBUG("pdlua_clearrequirepath: end. stack top %d", lua_gettop(L));
}

/** Run a Lua script using class path */
static int pdlua_dofilex(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd class pointer.
  * \li \c 2 Filename string.
  * \par Outputs:
  * \li \c * Determined by the script.
  * */
{
    char                buf[MAXPDSTRING];
    char                *ptr;
    t_pdlua_readerdata  reader;
    int                 fd;
    int                 n;
    const char          *filename;
    t_class             *c;

    PDLUA_DEBUG("pdlua_dofilex: stack top %d", lua_gettop(L));
    n = lua_gettop(L);
    if (lua_islightuserdata(L, 1))
    {
        c = lua_touserdata(L, 1);
        if (c)
        {
            filename = luaL_optstring(L, 2, NULL);
            fd = sys_trytoopenone(c->c_externdir->s_name, filename, "",
              buf, &ptr, MAXPDSTRING, 1);
            if (fd >= 0)
            {
                PDLUA_DEBUG("pdlua_dofilex path is %s", buf);
                pdlua_setrequirepath(L, buf);
                reader.fd = fd;
#if LUA_VERSION_NUM	< 502
                if (lua_load(L, pdlua_reader, &reader, filename))
#else // 5.2 style
                if (lua_load(L, pdlua_reader, &reader, filename, NULL))
#endif // LUA_VERSION_NUM	< 502
                {
                    close(fd);
                    pdlua_clearrequirepath(L);
                    lua_error(L);
                }
                else
                {
                    if (lua_pcall(L, 0, LUA_MULTRET, 0))
                    {
                        pd_error(NULL, "lua: error running `%s':\n%s", filename, lua_tostring(L, -1));
                        lua_pop(L, 1);
                        close(fd);
                        pdlua_clearrequirepath(L);
                    }
                    else
                    {
                        /* succeeded */
                        close(fd);
                        pdlua_clearrequirepath(L);
                    }
                }
            }
            else pd_error(NULL, "lua: error loading `%s': sys_trytoopenone() failed", filename);
        }
        else pd_error(NULL, "lua: error in class:dofilex() - class is null");
    }
    else pd_error(NULL, "lua: error in class:dofilex() - object is wrong type");
    lua_pushstring(L, buf); /* return the path as well so we can open it later with pdlua_menu_open() */
    PDLUA_DEBUG("pdlua_dofilex end. stack top is %d", lua_gettop(L));
 
    return lua_gettop(L) - n;
}

/** Run a Lua script using Pd's path. */
static int pdlua_dofile(lua_State *L)
/**< Lua interpreter state.
  * \par Inputs:
  * \li \c 1 Pd object pointer.
  * \li \c 2 Filename string.
  * \par Outputs:
  * \li \c * Determined by the script.
  * */
{
    char                buf[MAXPDSTRING];
    char                *ptr;
    t_pdlua_readerdata  reader;
    int                 fd;
    int                 n;
    const char          *filename;
    t_pdlua             *o;

    PDLUA_DEBUG("pdlua_dofile: stack top %d", lua_gettop(L));
    n = lua_gettop(L);
    if (lua_islightuserdata(L, 1))
    {
        o = lua_touserdata(L, 1);
        if (o)
        {
            filename = luaL_optstring(L, 2, NULL);
            fd = canvas_open(o->canvas, filename, "", buf, &ptr, MAXPDSTRING, 1);
            if (fd >= 0)
            {
                PDLUA_DEBUG("pdlua_dofile path is %s", buf);
                //pdlua_setpathname(o, buf);/* change the scriptname to include its path */
                pdlua_setrequirepath(L, buf);
                reader.fd = fd;
#if LUA_VERSION_NUM	< 502
                if (lua_load(L, pdlua_reader, &reader, filename))
#else // 5.2 style
                if (lua_load(L, pdlua_reader, &reader, filename, NULL))
#endif // LUA_VERSION_NUM	< 502
                {
                    close(fd);
                    pdlua_clearrequirepath(L);
                    lua_error(L);
                }
                else
                {
                    if (lua_pcall(L, 0, LUA_MULTRET, 0))
                    {
                        pd_error(o, "lua: error running `%s':\n%s", filename, lua_tostring(L, -1));
                        lua_pop(L, 1);
                        close(fd);
                        pdlua_clearrequirepath(L);
                    }
                    else
                    {
                        /* succeeded */
                        close(fd);
                        pdlua_clearrequirepath(L);
                    }
                }
            }
            else pd_error(o, "lua: error loading `%s': canvas_open() failed", filename);
        }
        else pd_error(NULL, "lua: error in object:dofile() - object is null");
    }
    else pd_error(NULL, "lua: error in object:dofile() - object is wrong type");
    lua_pushstring(L, buf); /* return the path as well so we can open it later with pdlua_menu_open() */
    PDLUA_DEBUG("pdlua_dofile end. stack top is %d", lua_gettop(L));
    
    return lua_gettop(L) - n;
}

/** Initialize the pd API for Lua. */
static void pdlua_init(lua_State *L)
/**< Lua interpreter state. */
{
    lua_newtable(L);
    lua_setglobal(L, "pd");
    lua_getglobal(L, "pd");
    lua_pushstring(L, "_iswindows");
#ifdef _WIN32
    lua_pushboolean(L, 1);
#else
    lua_pushboolean(L, 0);
#endif // _WIN32
    lua_settable(L, -3);
    lua_pushstring(L, "_register");
    lua_pushcfunction(L, pdlua_class_new);
    lua_settable(L, -3);
    lua_pushstring(L, "_create");
    lua_pushcfunction(L, pdlua_object_new);
    lua_settable(L, -3);
    lua_pushstring(L, "_createinlets");
    lua_pushcfunction(L, pdlua_object_createinlets);
    lua_settable(L, -3);
    lua_pushstring(L, "_createoutlets");
    lua_pushcfunction(L, pdlua_object_createoutlets);
    lua_settable(L, -3);
    lua_pushstring(L, "_creategui");
    lua_pushcfunction(L, pdlua_object_creategui);
    lua_settable(L, -3);
    lua_pushstring(L, "_canvaspath");
    lua_pushcfunction(L, pdlua_object_canvaspath);
    lua_settable(L, -3);
    lua_pushstring(L, "_destroy");
    lua_pushcfunction(L, pdlua_object_free);
    lua_settable(L, -3);
    lua_pushstring(L, "_outlet");
    lua_pushcfunction(L, pdlua_outlet);
    lua_settable(L, -3);
    lua_pushstring(L, "_createreceive");
    lua_pushcfunction(L, pdlua_receive_new);
    lua_settable(L, -3);
    lua_pushstring(L, "_receivefree");
    lua_pushcfunction(L, pdlua_receive_free);
    lua_settable(L, -3);
    lua_pushstring(L, "_createclock");
    lua_pushcfunction(L, pdlua_clock_new);
    lua_settable(L, -3);
    lua_pushstring(L, "_clockfree");
    lua_pushcfunction(L, pdlua_clock_free);
    lua_settable(L, -3);
    lua_pushstring(L, "_clockset");
    lua_pushcfunction(L, pdlua_clock_set);
    lua_settable(L, -3);
    lua_pushstring(L, "_clockunset");
    lua_pushcfunction(L, pdlua_clock_unset);
    lua_settable(L, -3);
    lua_pushstring(L, "_clockdelay");
    lua_pushcfunction(L, pdlua_clock_delay);
    lua_settable(L, -3);
    lua_pushstring(L, "_dofile");
    lua_pushcfunction(L, pdlua_dofile);
    lua_settable(L, -3);
    lua_pushstring(L, "_dofilex");
    lua_pushcfunction(L, pdlua_dofilex);
    lua_settable(L, -3);
    lua_pushstring(L, "send");
    lua_pushcfunction(L, pdlua_send);
    lua_settable(L, -3);
    lua_pushstring(L, "getvalue");
    lua_pushcfunction(L, pdlua_getvalue);
    lua_settable(L, -3);
    lua_pushstring(L, "setvalue");
    lua_pushcfunction(L, pdlua_setvalue);
    lua_settable(L, -3);
    lua_pushstring(L, "_getarray");
    lua_pushcfunction(L, pdlua_getarray);
    lua_settable(L, -3);
    lua_pushstring(L, "_readarray");
    lua_pushcfunction(L, pdlua_readarray);
    lua_settable(L, -3);
    lua_pushstring(L, "_writearray");
    lua_pushcfunction(L, pdlua_writearray);
    lua_settable(L, -3);
    lua_pushstring(L, "_redrawarray");
    lua_pushcfunction(L, pdlua_redrawarray);
    lua_settable(L, -3);
    lua_pushstring(L, "post");
    lua_pushcfunction(L, pdlua_post);
    lua_settable(L, -3);
    lua_pushstring(L, "_set_args");
    lua_pushcfunction(L, pdlua_set_arguments);
    lua_settable(L, -3);
    lua_pushstring(L, "_error");
    lua_pushcfunction(L, pdlua_error);
    lua_settable(L, -3);
    lua_pop(L, 1);
    PDLUA_DEBUG("pdlua_init: end. stack top is %d", lua_gettop(L));
}

/** Pd loader hook for loading and executing Lua scripts. */
static int pdlua_loader_fromfd
(
    int         fd, /**< file-descriptor of .pd_lua file */
    const char *name, /**< The name of the script (without .pd_lua extension). */
    const char *dirbuf /**< The name of the directory the .pd_lua files lives in */
)
{
    t_pdlua_readerdata  reader;

    PDLUA_DEBUG("pdlua_loader: stack top %d", lua_gettop(__L()));
    class_set_extern_dir(gensym(dirbuf));
    pdlua_setrequirepath(__L(), dirbuf);
    reader.fd = fd;
#if LUA_VERSION_NUM	< 502
    if (lua_load(__L(), pdlua_reader, &reader, name) || lua_pcall(__L(), 0, 0, 0))
#else // 5.2 style
    if (lua_load(__L(), pdlua_reader, &reader, name, NULL) || lua_pcall(__L(), 0, 0, 0))
#endif // LUA_VERSION_NUM	< 502
    {
      pd_error(NULL, "lua: error loading `%s':\n%s", name, lua_tostring(__L(), -1));
      lua_pop(__L(), 1);
      pdlua_clearrequirepath(__L());
      class_set_extern_dir(&s_);
      PDLUA_DEBUG("pdlua_loader: script error end. stack top %d", lua_gettop(__L()));
      return 0;
    }
    pdlua_clearrequirepath(__L());
    class_set_extern_dir(&s_);
    PDLUA_DEBUG("pdlua_loader: end. stack top %d", lua_gettop(__L()));
    return 1;
}

static int pdlua_loader_wrappath
(
    int         fd, /**< file-descriptor of .pd_lua file */
    const char *name, /**< The name of the script (without .pd_lua extension). */
    const char *dirbuf /**< The name of the directory the .pd_lua files lives in */
)
{
  int result = 0;
  if (fd>=0)
  {
    const char* basenamep = basename(name);
    int load_name_save = 0, load_path_save;
    const int is_loadname = basenamep > name;
    lua_getglobal(__L(), "pd");
    if (is_loadname)
    {
      /* save old loadname, restore later in case of
       * nested loading */
      lua_getfield(__L(), -1, "_loadname");
      load_name_save = luaL_ref(__L(), LUA_REGISTRYINDEX);
      lua_pushstring(__L(), name);
      lua_setfield(__L(), -2, "_loadname");
    }
    lua_getfield(__L(), -1, "_loadpath");
    load_path_save = luaL_ref(__L(), LUA_REGISTRYINDEX);
    lua_pushstring(__L(), dirbuf);
    lua_setfield(__L(), -2, "_loadpath");
    result=pdlua_loader_fromfd(fd, basenamep, dirbuf);
    lua_rawgeti(__L(), LUA_REGISTRYINDEX, load_path_save);
    lua_setfield(__L(), -2, "_loadpath");
    luaL_unref(__L(), LUA_REGISTRYINDEX, load_path_save);
    if (is_loadname) {
      lua_rawgeti(__L(), LUA_REGISTRYINDEX, load_name_save);
      lua_setfield(__L(), -2, "_loadname");
      luaL_unref(__L(), LUA_REGISTRYINDEX, load_name_save);
    }
    lua_pop(__L(), 1);
    sys_close(fd);
  }
  return result;
}

static int pdlua_loader_legacy
(
    t_canvas    *canvas, /**< Pd canvas to use to find the script. */
    char        *name /**< The name of the script (without .pd_lua extension). */
)
{
    char                dirbuf[MAXPDSTRING];
    char                *ptr;
    int                 fd;

    fd = canvas_open(canvas, name, ".pd_lua", dirbuf, &ptr, MAXPDSTRING, 1);
    return pdlua_loader_wrappath(fd, name, dirbuf);
}

static int pdlua_loader_pathwise
(
    t_canvas    *UNUSED(canvas), /**< Pd canvas to use to find the script. */
    const char  *objectname, /**< The name of the script (without .pd_lua extension). */
    const char  *path /**< The directory to search for the script */
)
{
    char                dirbuf[MAXPDSTRING], filename[MAXPDSTRING];
    char                *ptr;
    const char          *classname;
    int                 fd;

    if(!path)
    {
      /* we already tried all paths, so skip this */
      return 0;
    }
    if ((classname = strrchr(objectname, '/')))
        classname++;
    else classname = objectname;
    /* ag: Try loading <path>/<classname>.pd_lua (experimental).
       sys_trytoopenone will correctly find the file in a subdirectory if a
       path is given, and it will then return that subdir in dirbuf. */
    if ((fd = sys_trytoopenone(path, objectname, ".pd_lua",
        dirbuf, &ptr, MAXPDSTRING, 1)) >= 0)
        if(pdlua_loader_wrappath(fd, objectname, dirbuf))
            return 1;

    /* next try (objectname)/(classname).(sys_dllextent) ... */
    strncpy(filename, objectname, MAXPDSTRING);
    filename[MAXPDSTRING-2] = 0;
    strcat(filename, "/");
    strncat(filename, classname, MAXPDSTRING-strlen(filename));
    filename[MAXPDSTRING-1] = 0;
    if ((fd = sys_trytoopenone(path, filename, ".pd_lua",
        dirbuf, &ptr, MAXPDSTRING, 1)) >= 0)
        if(pdlua_loader_wrappath(fd, objectname, dirbuf))
            return 1;
    return 0;
}

#ifdef WIN32
#include <windows.h>
#else
#define __USE_GNU // to get RTLD_DEFAULT
#include <dlfcn.h> // for dlsym
#ifndef RTLD_DEFAULT
/* If RTLD_DEFAULT still isn't defined then just passing NULL will hopefully
   do the trick. */
#define RTLD_DEFAULT NULL
#endif
#endif

#define xstr(s) str(s)
#define str(s) #s

/** Start the Lua runtime and register our loader hook. */
#ifdef _WIN32
__declspec(dllexport)
#endif


void pdlua_instance_setup()
{
#if PDINSTANCE
    initialise_lua_state();
#endif
}

#ifdef PLUGDATA
void pdlua_setup(const char *datadir, char *versbuf, int versbuf_length, void(*register_class_callback)(const char*))
#else
void pdlua_setup(void)
#endif
{
    char                pd_lua_path[MAXPDSTRING];
    t_pdlua_readerdata  reader;
    int                 fd;
    int                 result;
    char                pdluaver[MAXPDSTRING];
    char                compiled[MAXPDSTRING];
    char                luaversionStr[MAXPDSTRING];
#if LUA_VERSION_NUM	< 504
    const lua_Number    *luaversion = lua_version (NULL);
#else
    const lua_Number    luavers = lua_version (NULL), *luaversion = &luavers;
#endif
    int                 lvm, lvl;

#ifndef BUILD_DATE
# define BUILD_DATE __DATE__" "__TIME__
#endif

#ifdef PDLUA_VERSION
    char *pdlua_version = xstr(PDLUA_VERSION);
#else
    char *pdlua_version = "";
#endif
    if (strlen(pdlua_version) == 0) {
      // NOTE: This should be set from the Makefile, otherwise we fall back to:
      pdlua_version = "0.11.6";
    }
    snprintf(pdluaver, MAXPDSTRING-1, "pdlua %s (GPL) 2008 Claude Heiland-Allen, 2014 Martin Peach et al.", pdlua_version);
    snprintf(compiled, MAXPDSTRING-1, "pdlua: compiled for pd-%d.%d on %s",
             PD_MAJOR_VERSION, PD_MINOR_VERSION, BUILD_DATE);

    lvm = (*luaversion)/100;
    lvl = (*luaversion) - (100*lvm);
    snprintf(luaversionStr, MAXPDSTRING-1, "Using lua version %d.%d", lvm, lvl);

#if PLUGDATA
    plugdata_register_class = register_class_callback;
    
    snprintf(versbuf, versbuf_length-1,
#ifdef ELSE
             "pdlua %s ELSE (lua %d.%d)",
#else
             "pdlua %s (lua %d.%d)",
#endif
             pdlua_version, lvm, lvl);
#endif
// post version and other information
    post(pdluaver);
#ifdef ELSE
    post("Distributed as part of ELSE");
#else
    post(compiled);
#endif
    post(luaversionStr);

    pdlua_proxyinlet_setup();
    PDLUA_DEBUG("pdlua pdlua_proxyinlet_setup done", 0);
    pdlua_proxyreceive_setup();
    PDLUA_DEBUG("pdlua pdlua_proxyreceive_setup done", 0);
    pdlua_proxyclock_setup();
    PDLUA_DEBUG("pdlua pdlua_proxyclock_setup done", 0);
    if (! pdlua_proxyinlet_class || ! pdlua_proxyreceive_class || ! pdlua_proxyclock_class)
    {
        pd_error(NULL, "lua: error creating proxy classes");
        pd_error(NULL, "lua: loader will not be registered!");
        pd_error(NULL, "lua: (is Pd using a different float size?)");
        return;
    }
    
    initialise_lua_state();
    
    PDLUA_DEBUG("pdlua lua_open done L = %p", __L());
    luaL_openlibs(__L());
    PDLUA_DEBUG("pdlua luaL_openlibs done", 0);
    pdlua_init(__L());
    PDLUA_DEBUG("pdlua pdlua_init done", 0);
    /* "pd.lua" is the Lua part of pdlua, want to keep the C part minimal */
    /* canvas_open can't find pd.lua unless we give the path to pd beforehand like pd -path /usr/lib/extra/pdlua */
    /* To avoid this we can use c_externdir from m_imp.h, struct _class: t_symbol *c_externdir; */
    /* c_externdir is the directory the extern was loaded from and is also the directory contining pd.lua */
#ifdef PLUGDATA
    // In plugdata we're linked statically and thus c_externdir is empty.
    // Instead, we get our data directory from plugdata and expect to find the
    // external dir in <datadir>/pdlua.
    snprintf(plugdata_datadir, MAXPDSTRING-1, "%s/pdlua", datadir);
    snprintf(pd_lua_path, MAXPDSTRING-1, "%s/pdlua/pd.lua", datadir);
#else
    snprintf(pd_lua_path, MAXPDSTRING-1, "%s/pd.lua", pdlua_proxyinlet_class->c_externdir->s_name); /* the full path to pd.lua */
#endif
    PDLUA_DEBUG("pd_lua_path %s", pd_lua_path);
    fd = open(pd_lua_path, O_RDONLY);
/*    fd = canvas_open(canvas_getcurrent(), "pd", ".lua", buf, &ptr, MAXPDSTRING, 1);  looks all over and rarely succeeds */
    PDLUA_DEBUG ("pd.lua loaded from %s", pd_lua_path);
    PDLUA_DEBUG("pdlua canvas_open done fd = %d", fd);
    PDLUA_DEBUG("pdlua_setup: stack top %d", lua_gettop(__L()));
    if (fd >= 0)
    { /* pd.lua was opened */
        reader.fd = fd;
#if LUA_VERSION_NUM	< 502
        result = lua_load(__L(), pdlua_reader, &reader, "pd.lua");
#else // 5.2 style
        result = lua_load(__L(), pdlua_reader, &reader, "pd.lua", NULL); // mode bt for binary or text
#endif // LUA_VERSION_NUM	< 502
        PDLUA_DEBUG ("pdlua lua_load returned %d", result);
        if (0 == result)
        {
            result = lua_pcall(__L(), 0, 0, 0);
            PDLUA_DEBUG ("pdlua lua_pcall returned %d", result);
        }
        if (0 != result)
        //if (lua_load(__L(), pdlua_reader, &reader, "pd.lua") || lua_pcall(__L(), 0, 0, 0))
        {
            pd_error(NULL, "lua: error loading `pd.lua':\n%s", lua_tostring(__L(), -1));
            pd_error(NULL, "lua: loader will not be registered!");
            pd_error(NULL, "lua: (is `pd.lua' in Pd's path list?)");
            lua_pop(__L(), 1);
        }
        else
        {
            int maj=0,min=0,bug=0;
            sys_getversion(&maj,&min,&bug);
            if((maj==0) && (min<47))
                /* before Pd<0.47, the loaders had to iterate over each path themselves */
                sys_register_loader((loader_t)pdlua_loader_legacy);
            else
                /* since Pd>=0.47, Pd tries the loaders for each path */
                sys_register_loader((loader_t)pdlua_loader_pathwise);
        }
        close(fd);
    }
    else
    {
        pd_error(NULL, "lua: error loading `pd.lua': canvas_open() failed");
        pd_error(NULL, "lua: loader will not be registered!");
    }

    pdlua_gfx_setup(__L());
    
    PDLUA_DEBUG("pdlua_setup: end. stack top %d", lua_gettop(__L()));
#ifndef PLUGDATA
    /* nw.js support. */
#ifdef WIN32
    nw_gui_vmess = (void*)GetProcAddress(GetModuleHandle("pd.dll"), "gui_vmess");
#else
    nw_gui_vmess = dlsym(RTLD_DEFAULT, "gui_vmess");
#endif
    if (nw_gui_vmess)
      post("pdlua: using JavaScript interface (nw.js)");
#endif

}

/* EOF */
