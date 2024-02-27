/** @file pdlua_gfx.h
 *  @brief pdlua_gfx -- an extension to pdlua that allows GUI rendering and interaction in pure-data and plugdata
 *  @author Timothy Schoen <timschoen123@gmail.com>
 *  @date 2023
 *
 * Copyright (C) 2023 Timothy Schoen <timschoen123@gmail.com>
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

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define PDLUA_OBJECT_REGISTRTY_ID 512

// Functions that need to be implemented separately for each Pd flavour
static int gfx_initialize(t_pdlua *obj);

static int set_size(lua_State *L);
static int get_size(lua_State *L);
static int start_paint(lua_State *L);
static int end_paint(lua_State* L);

static int set_color(lua_State* L);

static int fill_ellipse(lua_State* L);
static int stroke_ellipse(lua_State* L);
static int fill_all(lua_State* L);
static int fill_rect(lua_State* L);
static int stroke_rect(lua_State* L);
static int fill_rounded_rect(lua_State* L);
static int stroke_rounded_rect(lua_State* L);

static int draw_line(lua_State* L);
static int draw_text(lua_State* L);

static int start_path(lua_State* L);
static int line_to(lua_State* L);
static int quad_to(lua_State* L);
static int cubic_to(lua_State* L);
static int close_path(lua_State* L);
static int stroke_path(lua_State* L);
static int fill_path(lua_State* L);

static int translate(lua_State* L);
static int scale(lua_State* L);
static int reset_transform(lua_State* L);

static int free_path(lua_State* L);


// pdlua_gfx_clear, pdlua_gfx_repaint and pdlua_gfx_mouse_* correspond to the various callbacks the user can assign

void pdlua_gfx_clear(t_pdlua *obj, int removed); // only for pd-vanilla, to delete all tcl/tk items

// Trigger repaint callback in lua script
void pdlua_gfx_repaint(t_pdlua *o, int firsttime) {
#if !PLUGDATA
    o->gfx.first_draw = firsttime;
#endif
    lua_getglobal(__L(), "pd");
    lua_getfield (__L(), -1, "_repaint");
    lua_pushlightuserdata(__L(), o);
    
    // Write object ptr to registry to make it reliably accessible
    lua_pushvalue(__L(), LUA_REGISTRYINDEX);
    lua_pushlightuserdata(__L(), o);
    lua_seti(__L(), -2, PDLUA_OBJECT_REGISTRTY_ID);
    lua_pop(__L(), 1);
    
    if (lua_pcall(__L(), 1, 0, 0))
    {
        pd_error(o, "lua: error in repaint:\n%s", lua_tostring(__L(), -1));
        lua_pop(__L(), 1); /* pop the error string */
    }
    
    lua_pop(__L(), 1); /* pop the global "pd" */
#if !PLUGDATA
    o->gfx.first_draw = 0;
#endif
}

// Pass mouse events to lua script
void pdlua_gfx_mouse_event(t_pdlua *o, int x, int y, int type) {
        
    lua_getglobal(__L(), "pd");
    lua_getfield (__L(), -1, "_mouseevent");
    lua_pushlightuserdata(__L(), o);
    lua_pushinteger(__L(), x);
    lua_pushinteger(__L(), y);
    lua_pushinteger(__L(), type);
    
    // Write object ptr to registry to make it reliably accessible
    lua_pushvalue(__L(), LUA_REGISTRYINDEX);
    lua_pushlightuserdata(__L(), o);
    lua_seti(__L(), -2, PDLUA_OBJECT_REGISTRTY_ID);
    lua_pop(__L(), 1);
    
    if (lua_pcall(__L(), 4, 0, 0))
    {
        pd_error(o, "lua: error in mouseevent:\n%s", lua_tostring(__L(), -1));
        lua_pop(__L(), 1); /* pop the error string */
    }
    
    lua_pop(__L(), 1); /* pop the global "pd" */
}

// Pass mouse events to lua script (but easier to understand)
void pdlua_gfx_mouse_down(t_pdlua *o, int x, int y) {
    pdlua_gfx_mouse_event(o, x, y, 0);
}
void pdlua_gfx_mouse_up(t_pdlua *o, int x, int y) {
    pdlua_gfx_mouse_event(o, x, y, 1);
}

void pdlua_gfx_mouse_move(t_pdlua *o, int x, int y) {
    pdlua_gfx_mouse_event(o, x, y, 2);
}

void pdlua_gfx_mouse_drag(t_pdlua *o, int x, int y) {
    pdlua_gfx_mouse_event(o, x, y, 3);
}

typedef struct _path_state
{
#if PLUGDATA
    t_symbol* path_id;
#else
    // Variables for managing vector paths
    int* path_segments;
    int num_path_segments;
    int num_path_segments_allocated;
    int path_start_x, path_start_y;
#endif
} path_state;

// We need to have access to the current object always, even in the constructor
// This function can guarantee that
static t_pdlua* get_current_object(lua_State* L)
{
    lua_pushvalue(L, LUA_REGISTRYINDEX);
    lua_geti(L, -1, PDLUA_OBJECT_REGISTRTY_ID);

    if (lua_islightuserdata(L, -1)) {
        return lua_touserdata(L, -1);
    }
    
    return NULL;
}


static int gfx_new(lua_State *L) {
    luaL_setmetatable(L, "graphics_context");
    return 1;
}

// Register functions with Lua
static const luaL_Reg gfx_lib[] = {
    {"set_size", set_size},
    {"get_size", get_size},
    {"start_paint", start_paint},
    {"end_paint", end_paint},
    {"gfx_new", gfx_new},
    {NULL, NULL} // Sentinel to end the list
};

static const luaL_Reg path_methods[] = {
    {"line_to", line_to},
    {"quad_to", quad_to},
    {"cubic_to", cubic_to},
    {"close", close_path},
    {"__gc", free_path},
    {NULL, NULL} // Sentinel to end the list
};

static const luaL_Reg path_constructor[] = {
    {"start", start_path},
    {NULL, NULL} // Sentinel to end the list
};

// Register functions with Lua
static const luaL_Reg gfx_methods[] = {
    {"set_color", set_color},
    {"fill_ellipse", fill_ellipse},
    {"stroke_ellipse", stroke_ellipse},
    {"fill_rect", fill_rect},
    {"stroke_rect", stroke_rect},
    {"fill_rounded_rect", fill_rounded_rect},
    {"stroke_rounded_rect", stroke_rounded_rect},
    {"draw_line", draw_line},
    {"draw_text", draw_text},
    {"stroke_path", stroke_path},
    {"fill_path", fill_path},
    {"fill_all", fill_all},
    {"translate", translate},
    {"scale", scale},
    {"reset_transform", reset_transform},
    {NULL, NULL} // Sentinel to end the list
};

int pdlua_gfx_setup(lua_State* L) {
    // Register functions with Lua
    luaL_newlib(L, gfx_lib);
    lua_setglobal(L, "_gfx_internal");
    
    luaL_newlib(L, path_constructor);
    lua_setglobal(L, "path");
    
    luaL_newmetatable(L, "path_state");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, path_methods, 0);
    
    luaL_newmetatable(L, "graphics_context");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, gfx_methods, 0);
    
    return 1; // Number of values pushed onto the stack
}


static int get_size(lua_State* L)
{
    t_pdlua* obj = get_current_object(L);
    lua_pushnumber(L, (lua_Number)obj->gfx.width);
    lua_pushnumber(L, (lua_Number)obj->gfx.height);
    return 2;
}

#if PLUGDATA

// Wrapper around draw callback to plugdata
static inline void plugdata_draw(t_pdlua* obj, t_symbol* sym, int argc, t_atom* argv)
{
    if(obj->gfx.plugdata_callback_target) {
        obj->gfx.plugdata_draw_callback(obj->gfx.plugdata_callback_target, sym, argc, argv);
    }
}

void pdlua_gfx_clear(t_pdlua* obj, int removed) {
}

static int gfx_initialize(t_pdlua* obj)
{
    pdlua_gfx_repaint(obj, 0); // Initial repaint
    return 0;
}

static int set_size(lua_State* L)
{
    t_pdlua* obj = get_current_object(L);
    obj->gfx.width = luaL_checknumber(L, 1);
    obj->gfx.height = luaL_checknumber(L, 2);
    t_atom args[2];
    SETFLOAT(args, luaL_checknumber(L, 1)); // w
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // h
    plugdata_draw(obj, gensym("lua_resized"), 2, args);
    return 0;
}

static int start_paint(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    plugdata_draw(obj, gensym("lua_start_paint"), 0, NULL);
    lua_pushboolean(L, 1); // Return a value, which decides whether we're allowed to paint or not
    return 1;
}

static int end_paint(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    plugdata_draw(obj, gensym("lua_end_paint"), 0, NULL);
    return 0;
}

static int set_color(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    if (lua_gettop(L) == 3) { // Single argument: parse as color ID instead of RGB
        t_atom arg;
        SETFLOAT(&arg, luaL_checknumber(L, 1)); // color ID
        plugdata_draw(obj, gensym("lua_set_color"), 1, &arg);
        return 0;
    }
    
    t_atom args[4];
    SETFLOAT(args, luaL_checknumber(L, 1)); // r
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // g
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // b
    
    if (lua_gettop(L) > 5) { // object and table are already on stack, hence 5
        // alpha (optional, default to 1.0)
        SETFLOAT(args + 3, luaL_checknumber(L, 4));
    }
    else {
        SETFLOAT(args + 3, 1.0f);
    }
    plugdata_draw(obj, gensym("lua_set_color"), 4, args);
    return 0;
}

static int fill_ellipse(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    t_atom args[4];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    plugdata_draw(obj, gensym("lua_fill_ellipse"), 4, args);
    return 0;
}

static int stroke_ellipse(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    t_atom args[5];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // width
    plugdata_draw(obj, gensym("lua_stroke_ellipse"), 5, args);
    return 0;
}

static int fill_all(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    plugdata_draw(obj, gensym("lua_fill_all"), 0, NULL);
    return 0;
}

static int fill_rect(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    t_atom args[4];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    plugdata_draw(obj, gensym("lua_fill_rect"), 4, args);
    return 0;
}

static int stroke_rect(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    t_atom args[5];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // corner_radius
    plugdata_draw(obj, gensym("lua_stroke_rect"), 5, args);
    return 0;
}

static int fill_rounded_rect(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    t_atom args[5];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // corner radius
    plugdata_draw(obj, gensym("lua_fill_rounded_rect"), 5, args);
    return 0;
}

static int stroke_rounded_rect(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    t_atom args[6];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // corner_radius
    SETFLOAT(args + 5, luaL_checknumber(L, 6)); // width
    plugdata_draw(obj, gensym("lua_stroke_rounded_rect"), 6, args);
    return 0;
}

static int draw_line(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    t_atom args[5];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // line width
    plugdata_draw(obj, gensym("lua_draw_line"), 5, args);
    
    return 0;
}

static int draw_text(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    const char* text = luaL_checkstring(L, 1);
    t_atom args[5];
    SETSYMBOL(args, gensym(text));
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // x
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // y
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // w
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // h
    plugdata_draw(obj, gensym("lua_draw_text"), 5, args);
    return 0;
}

t_symbol* generate_path_id() {
    int length = 32;
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char *random_string = malloc((length + 1) * sizeof(char));
    
    for (int i = 0; i < length; i++) {
        int index = rand() % (sizeof(charset) - 1);
        random_string[i] = charset[index];
    }

    random_string[length] = '\0';
    t_symbol* sym = gensym(random_string);
    free(random_string);
    return sym;
}

static int start_path(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    path_state *path = (path_state *)lua_newuserdata(L, sizeof(path_state));
    luaL_setmetatable(L, "path_state");
    path->path_id = generate_path_id();
    
    t_atom args[3];
    SETSYMBOL(args, path->path_id); // path id
    SETFLOAT(args + 1, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 2, luaL_checknumber(L, 2)); // y
    plugdata_draw(obj, gensym("lua_start_path"), 3, args);
    return 1;
}

static int line_to(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    
    t_atom args[3];
    SETSYMBOL(args, path->path_id); // path id
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // x
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // y
    plugdata_draw(obj, gensym("lua_line_to"), 3, args);
    return 0;
}

static int quad_to(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    
    t_atom args[5]; // Assuming quad_to takes 3 arguments
    SETSYMBOL(args, path->path_id); // path id
    SETFLOAT(args + 1, luaL_checknumber(L, 1)); // x1
    SETFLOAT(args + 2, luaL_checknumber(L, 2)); // y1
    SETFLOAT(args + 3, luaL_checknumber(L, 3)); // x2
    SETFLOAT(args + 4, luaL_checknumber(L, 4)); // y2
    
    // Forward the message to the appropriate function
    plugdata_draw(obj, gensym("lua_quad_to"), 5, args);
    return 0;
}

static int cubic_to(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    
    t_atom args[7]; // Assuming cubic_to takes 4 arguments
    
    SETSYMBOL(args, path->path_id); // path id
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // x1
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // y1
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // x2
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // y2
    SETFLOAT(args + 5, luaL_checknumber(L, 6)); // x3
    SETFLOAT(args + 6, luaL_checknumber(L, 7)); // y3
    
    // Forward the message to the appropriate function
    plugdata_draw(obj, gensym("lua_cubic_to"), 7, args);
    return 0;
}

static int close_path(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    
    t_atom args;
    SETSYMBOL(&args, path->path_id); // path id
    
    plugdata_draw(obj, gensym("lua_close_path"), 1, &args);
    return 0;
}

static int free_path(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    
    t_atom args;
    SETSYMBOL(&args, path->path_id); // path id
    plugdata_draw(obj, gensym("lua_free_path"), 1, &args);
    return 0;
}

static int stroke_path(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    
    t_atom args[2];
    SETSYMBOL(args, path->path_id); //  path id
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // line thickness
    plugdata_draw(obj, gensym("lua_stroke_path"), 2, args);
    return 0;
}

static int fill_path(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    
    t_atom args;
    SETSYMBOL(&args, path->path_id); // path id
    
    plugdata_draw(obj, gensym("lua_fill_path"), 1, &args);
    return 0;
}

static int translate(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    t_atom args[2];
    SETFLOAT(args, luaL_checknumber(L, 1)); // tx
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // ty
    plugdata_draw(obj, gensym("lua_translate"), 2, args);
    return 0;
}

static int scale(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    t_atom args[2];
    SETFLOAT(args, luaL_checknumber(L, 1)); // sx
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // sy
    plugdata_draw(obj, gensym("lua_scale"), 2, args);
    return 0;
}

static int reset_transform(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    plugdata_draw(obj, gensym("lua_reset_transform"), 0, NULL);
    return 0;
}
#else

static int can_draw(t_pdlua* obj)
{
    return (glist_isvisible(obj->canvas) && gobj_shouldvis(obj, obj->canvas)) || obj->gfx.first_draw;
}

static int free_path(lua_State* L)
{
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    freebytes(path->path_segments, path->num_path_segments_allocated * sizeof(int));
    return 0;
}

static void transform_size(t_pdlua_gfx* gfx, int* w, int* h) {
    for(int i = gfx->num_transforms - 1; i >= 0; i--)
    {
        if(gfx->transforms[i].type == SCALE)
        {
            *w *= gfx->transforms[i].x;
            *h *= gfx->transforms[i].y;
        }
    }
}

static void transform_point(t_pdlua_gfx* gfx, int* x, int* y) {
    for(int i = gfx->num_transforms - 1; i >= 0; i--)
    {
        if(gfx->transforms[i].type == SCALE)
        {
            *x *= gfx->transforms[i].x;
            *y *= gfx->transforms[i].y;
        }
        else // translate
        {
            *x += gfx->transforms[i].x;
            *y += gfx->transforms[i].y;
        }
    }
}

void pdlua_gfx_clear(t_pdlua *obj, int removed) {
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    pdgui_vmess(0, "crs", cnv, "delete", gfx->object_tag);
    gfx->current_paint_tag[0] = '\0';
    
    if(removed && gfx->order_tag[0] != '\0')
    {
        pdgui_vmess(0, "crs", cnv, "delete", gfx->order_tag);
        gfx->order_tag[0] = '\0';
    }

    glist_eraseiofor(glist_getcanvas(cnv), &obj->pd, gfx->object_tag);
}

static void get_bounds_args(lua_State* L, t_pdlua* obj, t_pdlua_gfx *gfx, int* x1, int* y1, int* x2, int* y2) {
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    
    int x = luaL_checknumber(L, 1);
    int y = luaL_checknumber(L, 2);
    int w = luaL_checknumber(L, 3);
    int h = luaL_checknumber(L, 4);
        
    transform_point(gfx, &x, &y);
    transform_size(gfx, &w, &h);
    
    x += text_xpix((t_object*)obj, obj->canvas) / glist_getzoom(cnv);
    y += text_ypix((t_object*)obj, obj->canvas) / glist_getzoom(cnv);
    
    *x1 = x * glist_getzoom(cnv);
    *y1 = y * glist_getzoom(cnv);
    *x2 = (x + w) * glist_getzoom(cnv);
    *y2 = (y + h) * glist_getzoom(cnv);
}

static void gfx_displace(t_pdlua *x, t_glist *glist, int dx, int dy)
{
    sys_vgui(".x%lx.c move .x%lx %d %d\n", glist_getcanvas(x->canvas), (long)x, dx, dy);
    canvas_fixlinesfor(glist, (t_text*)x);
    
    int xpos = text_xpix((t_object*)x, x->canvas);
    int ypos = text_ypix((t_object*)x, x->canvas);
    glist_drawiofor(glist_getcanvas(x->canvas), (t_object*)x, 0, x->gfx.object_tag, xpos, ypos, xpos + x->gfx.width, ypos + x->gfx.height);
}

static const char* register_drawing(t_pdlua *object)
{
    t_pdlua_gfx *gfx = &object->gfx;
    snprintf(gfx->current_paint_tag, 128, ".x%d", rand());
    gfx->current_paint_tag[127] = '\0';
    return gfx->current_paint_tag;
}

static int gfx_initialize(t_pdlua *obj)
{
    t_pdlua_gfx *gfx = &obj->gfx;
    
    snprintf(gfx->object_tag, 128, ".x%lx", (long)obj);
    gfx->object_tag[127] = '\0';
    gfx->current_paint_tag[0] = '\0';
    
    pdlua_gfx_repaint(obj, 0);
    return 0;
}

static int set_size(lua_State* L)
{
    t_pdlua* obj = get_current_object(L);
    obj->gfx.width = luaL_checknumber(L, 1);
    obj->gfx.height = luaL_checknumber(L, 2);
    pdlua_gfx_repaint(obj, 0);
    return 0;
}

static int start_paint(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    t_pdlua_gfx *gfx = &obj->gfx;
    
    if(gfx->object_tag[0] == '\0')
    {
        return 0;
    }

    int draw = can_draw(obj);
    lua_pushboolean(L, draw); // Return a value, which decides whether we're allowed to paint or not

    if(gfx->first_draw)
    {
        // Whenever the objects gets painted for the first time with a "vis" message,
        // we add a small invisible line that won't get touched or repainted later.
        // We can then use this line to set the correct z-index for the drawings, using the tcl/tk "lower" command
        t_canvas *cnv = glist_getcanvas(obj->canvas);
        snprintf(gfx->order_tag, 128, ".x%d", rand());
        gfx->order_tag[127] = '\0';
        
        const char* tags[] = { gfx->order_tag };
        pdgui_vmess(0, "crr iiii ri rS", cnv, "create", "line", 0, 0, 1, 1,
                    "-width", 1, "-tags", 1, tags);
    }
    
    // check if anything was painted before
    if(draw && strlen(gfx->current_paint_tag))
        pdlua_gfx_clear(obj, 0);
            
    return 1;
}

static int end_paint(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    t_pdlua_gfx *gfx = &obj->gfx;
    
    // Draw iolets on top
    int xpos = text_xpix((t_object*)obj, obj->canvas);
    int ypos = text_ypix((t_object*)obj, obj->canvas);
    glist_drawiofor(glist_getcanvas(obj->canvas), (t_object*)obj, 1, gfx->object_tag, xpos, ypos, xpos + gfx->width, ypos + gfx->height);
    
    if(!gfx->first_draw && gfx->order_tag[0] != '\0') {
        // Move everything to below the order marker, to make sure redrawn stuff isn't always on top
        pdgui_vmess(0, "crss", cnv, "lower", gfx->object_tag, gfx->order_tag);
    }
    
    return 0;
}

static int set_color(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    
    int r, g, b;
    if (lua_gettop(L) == 3) { // Single argument: parse as color ID instead of RGB
        int color_id = luaL_checknumber(L, 1);
        if(color_id != 1)
        {
            r = 255;
            g = 255;
            b = 255;
        }
        else {
            r = 0;
            g = 0;
            b = 0;
        }
    }
    else {
        r = luaL_checknumber(L, 1);
        g = luaL_checknumber(L, 2);
        b = luaL_checknumber(L, 3);
    }

    // AFAIK, alpha is not supported in tcl/tk

    snprintf(gfx->current_color, 8, "#%02X%02X%02X", r, g, b);
    gfx->current_color[7] = '\0';
    
    return 0;
}

static int fill_ellipse(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    
    int x1, y1, x2, y2;
    get_bounds_args(L, obj, gfx, &x1, &y1, &x2, &y2);
    
    const char* tags[] = { gfx->object_tag, register_drawing(obj) };
    
    pdgui_vmess(0, "crr iiii rs ri rS", cnv, "create", "oval", x1, y1, x2, y2, "-fill", gfx->current_color, "-width", 0, "-tags", 2, tags);
        
    return 0;
}

static int stroke_ellipse(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);

    int x1, y1, x2, y2;
    get_bounds_args(L, obj, gfx, &x1, &y1, &x2, &y2);
    
    int line_width = luaL_checknumber(L, 5) * glist_getzoom(cnv);
    
    const char* tags[] = { gfx->object_tag, register_drawing(obj) };
    
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "oval", x1, y1, x2, y2, "-width", line_width, "-outline", gfx->current_color, "-tags", 2, tags);
        
    return 0;
}

static int fill_all(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    
    int x1 = text_xpix((t_object*)obj, obj->canvas);
    int y1 = text_ypix((t_object*)obj, obj->canvas);
    int x2 = x1 + gfx->width * glist_getzoom(cnv);
    int y2 = y1 + gfx->height * glist_getzoom(cnv);
        
    const char* tags[] =  { gfx->object_tag, register_drawing(obj) };
    
    pdgui_vmess(0, "crr iiii rs rS", cnv, "create", "rectangle", x1, y1, x2, y2, "-fill", gfx->current_color, "-tags", 2, tags);
    
    return 0;
}

static int fill_rect(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    
    int x1, y1, x2, y2;
    get_bounds_args(L, obj, gfx, &x1, &y1, &x2, &y2);
    
    const char* tags[] = { gfx->object_tag, register_drawing(obj) };
    
    pdgui_vmess(0, "crr iiii rs ri rS", cnv, "create", "rectangle", x1, y1, x2, y2, "-fill", gfx->current_color, "-width", 0, "-tags", 2, tags);
    
    return 0;
}

static int stroke_rect(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    
    int x1, y1, x2, y2;
    get_bounds_args(L, obj, gfx, &x1, &y1, &x2, &y2);

    int line_width = luaL_checknumber(L, 5) * glist_getzoom(cnv);
    
    const char* tags[] = { gfx->object_tag, register_drawing(obj) };

    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "rectangle", x1, y1, x2, y2, "-width", line_width, "-outline", gfx->current_color, "-tags", 2, tags);
    
    return 0;
}

static int fill_rounded_rect(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    
    int x1, y1, x2, y2;
    get_bounds_args(L, obj, gfx, &x1, &y1, &x2, &y2);

    int radius = luaL_checknumber(L, 5);  // Radius for rounded corners
    int radius_x = radius * glist_getzoom(cnv);
    int radius_y = radius * glist_getzoom(cnv);

    transform_size(gfx, &radius_x, &radius_y);
        
    const char* tags[] = { gfx->object_tag, register_drawing(obj) };

    // Tcl/tk can't fill rounded rectangles, so we draw 2 smaller rectangles with 4 ovals over the corners
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "oval", x1, y1, x1 + radius_x * 2, y1 + radius_y * 2, "-width", 0, "-fill", gfx->current_color, "-tags", 2, tags);
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "oval", x2 - radius_x * 2 , y1, x2, y1 + radius_y * 2, "-width", 0, "-fill", gfx->current_color, "-tags", 2, tags);
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "oval", x1, y2 - radius_y * 2, x1 + radius_x * 2, y2, "-width", 0, "-fill", gfx->current_color, "-tags", 2, tags);
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "oval", x2 - radius_x * 2, y2 - radius_y * 2, x2, y2, "-width", 0, "-fill", gfx->current_color, "-tags", 2, tags);
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "rectangle", x1 + radius_x, y1, x2 - radius_x, y2, "-width", 0, "-fill", gfx->current_color, "-tag", 2, tags);
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "rectangle", x1, y1 + radius_y, x2, y2 - radius_y, "-width", 0, "-fill", gfx->current_color, "-tags", 2, tags);

    return 0;
}

static int stroke_rounded_rect(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    
    int x1, y1, x2, y2;
    get_bounds_args(L, obj, gfx, &x1, &y1, &x2, &y2);
    
    int radius = luaL_checknumber(L, 5);       // Radius for rounded corners
    int radius_x = radius * glist_getzoom(cnv);
    int radius_y = radius * glist_getzoom(cnv);
    transform_size(gfx, &radius_x, &radius_y);
    int line_width = luaL_checknumber(L, 6) * glist_getzoom(cnv);
    
    const char* tags[] = { gfx->object_tag, register_drawing(obj) };
    
    // Tcl/tk can't stroke rounded rectangles either, so we draw 2 lines connecting with 4 arcs at the corners
    pdgui_vmess(0, "crr iiii ri ri ri ri rs rs rS", cnv, "create", "arc", x1, y1 + radius_y*2, x1 + radius_x*2, y1,
                "-start", 0, "-extent", 90, "-width", line_width, "-start", 90, "-outline", gfx->current_color, "-style", "arc", "-tags", 2, tags);
    pdgui_vmess(0, "crr iiii ri ri ri ri rs rs rS", cnv, "create", "arc", x2 - radius_x*2, y1, x2, y1 + radius_y*2,
                "-start", 270, "-extent", 90, "-width", line_width, "-start", 0, "-outline", gfx->current_color, "-style", "arc", "-tags", 2, tags);
    pdgui_vmess(0, "crr iiii ri ri ri ri rs rs rS", cnv, "create", "arc", x1, y2 - radius_y*2, x1 + radius_x*2, y2,
                "-start", 180, "-extent", 90, "-width", line_width, "-start", 180, "-outline", gfx->current_color, "-style", "arc", "-tags", 2, tags);
    pdgui_vmess(0, "crr iiii ri ri ri ri rs rs rS", cnv, "create", "arc", x2 - radius_x*2, y2, x2, y2 - radius_y*2,
                "-start", 90, "-extent", 90, "-width", line_width, "-start", 270, "-outline", gfx->current_color, "-style", "arc", "-tags", 2, tags);
    
    // Connect with lines
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "line", x1 + radius_x, y1, x2 - radius_x, y1,
                "-width", line_width, "-fill", gfx->current_color, "-tags", 2, tags);
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "line", x1 + radius_y, y2, x2 - radius_y, y2,
                "-width", line_width,  "-fill", gfx->current_color, "-tags", 2, tags);
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "line", x1 , y1 + radius_y, x1, y2 - radius_y,
                "-width", line_width, "-fill", gfx->current_color, "-tags", 2, tags);
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "line", x2 , y1 + radius_y, x2, y2 - radius_y,
                "-width", line_width,  "-fill", gfx->current_color, "-tags", 2, tags);
    
    return 0;
}

static int draw_line(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    
    int x1 = luaL_checknumber(L, 1);
    int y1 = luaL_checknumber(L, 2);
    int x2 = luaL_checknumber(L, 3);
    int y2 = luaL_checknumber(L, 4);
    int lineWidth = luaL_checknumber(L, 5);
    
    transform_point(gfx, &x1, &y1);
    transform_point(gfx, &x2, &y2);
    
    int canvas_zoom = glist_getzoom(cnv);

    x1 += text_xpix((t_object*)obj, obj->canvas) / canvas_zoom;
    y1 += text_ypix((t_object*)obj, obj->canvas) / canvas_zoom;
    x2 += text_xpix((t_object*)obj, obj->canvas) / canvas_zoom;
    y2 += text_ypix((t_object*)obj, obj->canvas) / canvas_zoom;

    x1 *= canvas_zoom;
    y1 *= canvas_zoom;
    x2 *= canvas_zoom;
    y2 *= canvas_zoom;
    lineWidth *= canvas_zoom;
    
    const char* tags[] = { gfx->object_tag, register_drawing(obj) };
    
    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "line", x1, y1, x2, y2,
                "-width", lineWidth, "-fill", gfx->current_color, "-tags", 2, tags);

    return 0;
}

static int draw_text(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    
    const char* text = luaL_checkstring(L, 1); // Assuming text is a string
    int x = luaL_checknumber(L, 2);
    int y = luaL_checknumber(L, 3);
    int w = luaL_checknumber(L, 4);
    int fontHeight = luaL_checknumber(L, 5);
    
    transform_point(gfx, &x, &y);
    transform_size(gfx, &w, &fontHeight);
    
    int canvas_zoom = glist_getzoom(cnv);
    x += text_xpix((t_object*)obj, obj->canvas) / canvas_zoom;
    y += text_ypix((t_object*)obj, obj->canvas) / canvas_zoom;
    
    x *= canvas_zoom;
    y *= canvas_zoom;
    w *= canvas_zoom;
    
    // Font size is offset to make sure it matches the size in plugdata
    fontHeight *= 0.8f;
    
    const char* tags[] = { gfx->object_tag, register_drawing(obj) };
    
    pdgui_vmess(0, "crr ii rs ri rs rS", cnv, "create", "text",
                0, 0, "-anchor", "nw", "-width", w, "-text", text, "-tags", 2, tags);

    t_atom fontatoms[3];
    SETSYMBOL(fontatoms+0, gensym(sys_font));
    SETFLOAT (fontatoms+1, fontHeight * canvas_zoom);
    SETSYMBOL(fontatoms+2, gensym(sys_fontweight));

    pdgui_vmess(0, "crs rA rs rs", cnv, "itemconfigure", tags[1],
            "-font", 3, fontatoms,
            "-fill", gfx->current_color,
            "-justify", "left");
    
    pdgui_vmess(0, "crs ii", cnv, "coords", tags[1], x, y);
    
    return 0;
}

static void add_path_segment(path_state* path, int x, int y)
{
    int path_segment_space =  (path->num_path_segments + 1) * 2;
    if(!path->num_path_segments_allocated) {
        path->path_segments = (int*)getbytes((path_segment_space + 1) * sizeof(int));
    }
    else {
        path->path_segments = (int*)resizebytes(path->path_segments, path->num_path_segments_allocated * sizeof(int), MAX((path_segment_space + 1), path->num_path_segments_allocated) * sizeof(int));
    }

    path->num_path_segments_allocated = path_segment_space;
    
    path->path_segments[path->num_path_segments * 2] = x;
    path->path_segments[path->num_path_segments * 2 + 1] = y;
    path->num_path_segments++;
}

static int start_path(lua_State* L) {
    path_state *path = (path_state *)lua_newuserdata(L, sizeof(path_state));
    luaL_setmetatable(L, "path_state");

    path->num_path_segments = 0;
    path->num_path_segments_allocated = 0;
    path->path_start_x = luaL_checknumber(L, 1);
    path->path_start_y = luaL_checknumber(L, 2);
    
    add_path_segment(path, path->path_start_x, path->path_start_y);
    return 1;
}

// Function to add a line to the current path
static int line_to(lua_State* L) {
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    int x = luaL_checknumber(L, 2);
    int y = luaL_checknumber(L, 3);
    add_path_segment(path, x, y);
    return 0;
}

static int quad_to(lua_State* L) {
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    int x2 = luaL_checknumber(L, 2);
    int y2 = luaL_checknumber(L, 3);
    int x3 = luaL_checknumber(L, 4);
    int y3 = luaL_checknumber(L, 5);
    
    int x1 = path->num_path_segments > 0 ? path->path_segments[(path->num_path_segments - 1) * 2] : x2;
    int y1 = path->num_path_segments > 0 ? path->path_segments[(path->num_path_segments - 1) * 2 + 1] : y2;
    
    // Get the last point
    float t = 0.0;
    const float resolution = 100;
    while (t <= 1.0) {
        t += 1.0 / resolution;
        
        // Calculate quadratic bezier curve as points (source: https://en.wikipedia.org/wiki/B%C3%A9zier_curve)
        int x = (1.0f - t) * (1.0f - t) * x1 + 2.0f * (1.0f - t) * t * x2 + t * t * x3;
        int y = (1.0f - t) * (1.0f - t) * y1 + 2.0f * (1.0f - t) * t * y2 + t * t * y3;
        add_path_segment(path, x, y);
    }
    
    return 0;
}
static int cubic_to(lua_State* L) {
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    int x2 = luaL_checknumber(L, 2);
    int y2 = luaL_checknumber(L, 3);
    int x3 = luaL_checknumber(L, 4);
    int y3 = luaL_checknumber(L, 5);
    int x4 = luaL_checknumber(L, 6);
    int y4 = luaL_checknumber(L, 7);
    
    int x1 = path->num_path_segments > 0 ? path->path_segments[(path->num_path_segments - 1) * 2] : x2;
    int y1 = path->num_path_segments > 0 ? path->path_segments[(path->num_path_segments - 1) * 2 + 1] : y2;
   
    // Get the last point
    float t = 0.0;
    const float resolution = 100;
    while (t <= 1.0) {
        t += 1.0 / resolution;

        // Calculate cubic bezier curve as points (source: https://en.wikipedia.org/wiki/B%C3%A9zier_curve)
        int x = (1 - t)*(1 - t)*(1 - t) * x1 + 3 * (1 - t)*(1 - t) * t * x2 + 3 * (1 - t) * t*t * x3 + t*t*t * x4;
        int y = (1 - t)*(1 - t)*(1 - t) * y1 + 3 * (1 - t)*(1 - t) * t * y2 + 3 * (1 - t) * t*t * y3 + t*t*t * y4;
        
        add_path_segment(path, x, y);
    }
    
    return 0;
}

// Function to close the current path
static int close_path(lua_State* L) {
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    add_path_segment(path, path->path_start_x, path->path_start_y);
    return 0;
}

static int stroke_path(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");
    int stroke_width = luaL_checknumber(L, 2) * glist_getzoom(cnv);
    
    // Apply transformations to all coordinates
    // Apply transformations to all coordinates
    int obj_x = text_xpix((t_object*)obj, obj->canvas);
    int obj_y = text_ypix((t_object*)obj, obj->canvas);
    for (int i = 0; i < path->num_path_segments; i++) {
        int x = path->path_segments[i * 2], y = path->path_segments[i * 2 + 1];
                
        transform_point(gfx, &x, &y);
        
        int canvas_zoom = glist_getzoom(cnv);
        path->path_segments[i * 2] = (x * canvas_zoom) + obj_x;
        path->path_segments[i * 2 + 1] = (y * canvas_zoom) + obj_y;
    }
    
    int totalSize = 0;
    // Determine the total size needed
    for (int i = 0; i < path->num_path_segments; i++) {
        int x = path->path_segments[i * 2], y = path->path_segments[i * 2 + 1];
        // Calculate size for x and y
        totalSize += snprintf(NULL, 0, "%i %i ", x, y);
    }
    char *coordinates = (char*)getbytes(totalSize + 1); // +1 for null terminator

    int offset = 0;
    for (int i = 0; i < path->num_path_segments; i++) {
        int x = path->path_segments[i * 2], y = path->path_segments[i * 2 + 1];
        int charsWritten = snprintf(coordinates + offset, totalSize - offset, "%i %i ",  x, y);
        if (charsWritten >= 0) {
            offset += charsWritten;
        } else {
            break;
        }
    }
    // Replace the trailing space with string terminator
    if (offset > 0) {
        coordinates[offset - 1] = '\0';
    }
    
    const char* tags[] = { gfx->object_tag, register_drawing(obj) };

    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "line", 0, 0, 0, 0, "-width", stroke_width, "-fill", gfx->current_color, "-tags", 2, tags);
    
    pdgui_vmess(0, "crs r", cnv, "coords", tags[1], coordinates);

    freebytes(coordinates, totalSize+1);
    
    return 0;
}

static int fill_path(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    
    path_state* path = (path_state*)luaL_checkudata(L, 1, "path_state");

    // Apply transformations to all coordinates
    int obj_x = text_xpix((t_object*)obj, obj->canvas);
    int obj_y = text_ypix((t_object*)obj, obj->canvas);
    for (int i = 0; i < path->num_path_segments; i++) {
        int x = path->path_segments[i * 2], y = path->path_segments[i * 2 + 1];
        
        transform_point(gfx, &x, &y);
        
        path->path_segments[i * 2] = x * glist_getzoom(cnv) + obj_x;
        path->path_segments[i * 2 + 1] = y * glist_getzoom(cnv) + obj_y;
    }
    
    int totalSize = 0;
    // Determine the total size needed
    for (int i = 0; i < path->num_path_segments; i++) {
        int x = path->path_segments[i * 2], y = path->path_segments[i * 2 + 1];
        // Calculate size for x and y
        totalSize += snprintf(NULL, 0, "%i %i ", x, y);
    }
    char *coordinates = (char*)getbytes(totalSize + 1); // +1 for null terminator
    
    int offset = 0;
    for (int i = 0; i < path->num_path_segments; i++) {
        int x = path->path_segments[i * 2], y = path->path_segments[i * 2 + 1];
        int charsWritten = snprintf(coordinates + offset, totalSize - offset, "%i %i ",  x, y);
        if (charsWritten >= 0) {
            offset += charsWritten;
        } else {
            break;
        }
    }
    
    // Remove the trailing space
    if (offset > 0) {
        coordinates[offset - 1] = '\0';
    }
    
    const char* tags[] = { gfx->object_tag, register_drawing(obj) };

    pdgui_vmess(0, "crr r ri rs rS", cnv, "create", "polygon", coordinates, "-width", 0, "-fill", gfx->current_color, "-tags", 2, tags);
    
    freebytes(coordinates, totalSize+1);
    
    return 0;
}


static int translate(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    
    if(gfx->num_transforms == 0)
    {
        gfx->transforms = getbytes(sizeof(gfx_transform));
        
    }
    else
    {
        gfx->transforms = resizebytes(gfx->transforms, gfx->num_transforms * sizeof(gfx_transform), (gfx->num_transforms + 1) * sizeof(gfx_transform));
        
    }
    
    gfx->transforms[gfx->num_transforms].type = TRANSLATE;
    gfx->transforms[gfx->num_transforms].x = luaL_checknumber(L, 1);
    gfx->transforms[gfx->num_transforms].y = luaL_checknumber(L, 2);
    
    gfx->num_transforms++;
    return 0;
}

static int scale(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    
    gfx->transforms = resizebytes(gfx->transforms, gfx->num_transforms * sizeof(gfx_transform), (gfx->num_transforms + 1) * sizeof(gfx_transform));
    
    gfx->transforms[gfx->num_transforms].type = SCALE;
    gfx->transforms[gfx->num_transforms].x = luaL_checknumber(L, 1);
    gfx->transforms[gfx->num_transforms].y = luaL_checknumber(L, 2);
    
    gfx->num_transforms++;
    return 0;
}

static int reset_transform(lua_State* L) {
    t_pdlua* obj = get_current_object(L);
    
    t_pdlua_gfx *gfx = &obj->gfx;
    
    gfx->transforms = resizebytes(gfx->transforms, gfx->num_transforms * sizeof(gfx_transform), 0);
    gfx->num_transforms = 0;
    return 0;
}
#endif
