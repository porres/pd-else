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

#ifdef PURR_DATA

// Purr Data doesn't currently define these vanilla API routines, and the
// graphics interface needs them, so for now we're providing dummy versions
// here. XXXFIXME: This needs to go once the graphics interface has been
// implemented in Purr Data. At present you'll just get a warning when the
// graphics interface is utilized. -ag

static void gfx_not_implemented(void)
{
  static int init = 0;
  if (!init) {
    post("pd-lua[gfx]: WARNING: graphics interface not yet implemented!");
    init = 1;
  }
}

int glist_getzoom(t_glist *x)
{
  gfx_not_implemented();
  return 1;
}

void pdgui_vmess(const char* message, const char* format, ...)
{
  gfx_not_implemented();
}

// this has an extra argument in vanilla

int wrap_hostfontsize(int fontsize, int zoom)
{
  return sys_hostfontsize(fontsize);
}

#define sys_hostfontsize wrap_hostfontsize

#endif

#define MAX(a, b) (((a) > (b)) ? (a) : (b))


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

static void pdlua_gfx_clear(t_pdlua *obj, int removed); // only for pd-vanilla, to delete all tcl/tk items

// Trigger repaint callback in lua script
void pdlua_gfx_repaint(t_pdlua *o, int firsttime) {
#if !PLUGDATA
    o->gfx.first_draw = firsttime;
#endif
    lua_getglobal(__L(), "pd");
    lua_getfield (__L(), -1, "_repaint");
    lua_pushlightuserdata(__L(), o);


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

// Represents a path object, created with path.new(x, y)
// for pd-vanilla, this contains all the points that the path contains. bezier curves are flattened out to points before being added
// for plugdata, it only contains a unique ID to the juce::Path that this is mapped to
typedef struct _path_state
{
    // Variables for managing vector paths
    float* path_segments;
    int num_path_segments;
    int num_path_segments_allocated;
    float path_start_x, path_start_y;
} t_path_state;


// Pops the graphics context off the argument list and returns it
static t_pdlua_gfx *pop_graphics_context(lua_State* L)
{
    t_pdlua_gfx* ctx = (t_pdlua_gfx*)luaL_checkudata(L, 1, "GraphicsContext");
    lua_remove(L, 1);
    return ctx;
}

// Register functions with Lua
static const luaL_Reg gfx_lib[] = {
    {"set_size", set_size},
    {"get_size", get_size},
    {"start_paint", start_paint},
    {"end_paint", end_paint},
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
    // for Path(x, y) constructor
    lua_pushcfunction(L, start_path);
    lua_setglobal(L, "Path");

    luaL_newmetatable(L, "Path");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, path_methods, 0);

    luaL_newmetatable(L, "GraphicsContext");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, gfx_methods, 0);

    // Register functions with Lua
    luaL_newlib(L, gfx_lib);
    lua_setglobal(L, "_gfx_internal");

    return 1; // Number of values pushed onto the stack
}

static int get_size(lua_State* L)
{
    if (!lua_islightuserdata(L, 1)) {
        return 0;
    }

    t_pdlua *obj = (t_pdlua*)lua_touserdata(L, 1);
    lua_pushnumber(L, (lua_Number)obj->gfx.width);
    lua_pushnumber(L, (lua_Number)obj->gfx.height);
    return 2;
}

#if PLUGDATA

// we make this global because paths are disconnected from object, but still need to send messages to plugdata
// it really doesn't matter since all these function callbacks point to the same function anyway
static void(*plugdata_draw_callback)(void*, t_symbol*, int, t_atom*) = NULL;

// Wrapper around draw callback to plugdata
static inline void plugdata_draw(t_pdlua *obj, t_symbol* sym, int argc, t_atom* argv)
{
    if(plugdata_draw_callback) {
        plugdata_draw_callback(obj, sym, argc, argv);
    }
}

static inline void plugdata_draw_path(t_symbol* sym, int argc, t_atom* argv)
{
    if(plugdata_draw_callback) {
        plugdata_draw_callback(NULL, sym, argc, argv);
    }
}

static void pdlua_gfx_clear(t_pdlua *obj, int removed) {
}

static int gfx_initialize(t_pdlua *obj)
{
    obj->gfx.object = obj;
    pdlua_gfx_repaint(obj, 0); // Initial repaint
    return 0;
}

static int set_size(lua_State* L)
{
    if (!lua_islightuserdata(L, 1)) {
        return 0;
    }

    t_pdlua *obj = (t_pdlua*)lua_touserdata(L, 1);
    obj->gfx.width = luaL_checknumber(L, 2);
    obj->gfx.height = luaL_checknumber(L, 3);
    t_atom args[2];
    SETFLOAT(args, obj->gfx.width); // w
    SETFLOAT(args + 1, obj->gfx.height); // h
    plugdata_draw(obj, gensym("lua_resized"), 2, args);
    return 0;
}

static int start_paint(lua_State* L) {
    if (!lua_islightuserdata(L, 1)) {
        lua_pushboolean(L, 0); // Return false if the argument is not a pointer
        return 1;
    }
    t_pdlua *obj = (t_pdlua*)lua_touserdata(L, 1);

    lua_pushlightuserdata(L, &obj->gfx);
    luaL_setmetatable(L, "GraphicsContext");

    plugdata_draw_callback = obj->gfx.plugdata_draw_callback;
    plugdata_draw(obj, gensym("lua_start_paint"), 0, NULL);
    return 1;
}

static int end_paint(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;
    plugdata_draw(obj, gensym("lua_end_paint"), 0, NULL);
    return 0;
}

static int set_color(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;
    if (lua_gettop(L) == 1) { // Single argument: parse as color ID instead of RGB
        t_atom arg;
        SETFLOAT(&arg, luaL_checknumber(L, 1)); // color ID
        plugdata_draw(obj, gensym("lua_set_color"), 1, &arg);
        return 0;
    }

    t_atom args[4];
    SETFLOAT(args, luaL_checknumber(L, 1)); // r
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // g
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // b

    if (lua_gettop(L) > 4) { // object and table are already on stack, hence 5
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
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_atom args[4];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    plugdata_draw(gfx->object, gensym("lua_fill_ellipse"), 4, args);
    return 0;
}

static int stroke_ellipse(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_atom args[5];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // width
    plugdata_draw(gfx->object, gensym("lua_stroke_ellipse"), 5, args);
    return 0;
}

static int fill_all(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;
    plugdata_draw(obj, gensym("lua_fill_all"), 0, NULL);
    return 0;
}

static int fill_rect(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_atom args[4];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    plugdata_draw(gfx->object, gensym("lua_fill_rect"), 4, args);
    return 0;
}

static int stroke_rect(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_atom args[5];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // corner_radius
    plugdata_draw(gfx->object, gensym("lua_stroke_rect"), 5, args);
    return 0;
}

static int fill_rounded_rect(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_atom args[5];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // corner radius
    plugdata_draw(gfx->object, gensym("lua_fill_rounded_rect"), 5, args);
    return 0;
}

static int stroke_rounded_rect(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_atom args[6];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // corner_radius
    SETFLOAT(args + 5, luaL_checknumber(L, 6)); // width
    plugdata_draw(gfx->object, gensym("lua_stroke_rounded_rect"), 6, args);
    return 0;
}

static int draw_line(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_atom args[5];
    SETFLOAT(args, luaL_checknumber(L, 1)); // x
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // y
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // w
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // h
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // line width
    plugdata_draw(gfx->object, gensym("lua_draw_line"), 5, args);

    return 0;
}

static int draw_text(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    const char* text = luaL_checkstring(L, 1);
    t_atom args[5];
    SETSYMBOL(args, gensym(text));
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // x
    SETFLOAT(args + 2, luaL_checknumber(L, 3)); // y
    SETFLOAT(args + 3, luaL_checknumber(L, 4)); // w
    SETFLOAT(args + 4, luaL_checknumber(L, 5)); // h
    plugdata_draw(gfx->object, gensym("lua_draw_text"), 5, args);
    return 0;
}

static int stroke_path(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    t_path_state* path = (t_path_state*)luaL_checkudata(L, 1, "Path");
    int stroke_width = luaL_checknumber(L, 2) * glist_getzoom(cnv);

    float last_x = 0;
    float last_y = 0;

    t_atom* coordinates = malloc(2 * path->num_path_segments * sizeof(t_atom) + 1);
    SETFLOAT(coordinates, stroke_width);

    int num_real_segments = 0;

    for (int i = 0; i < path->num_path_segments; i++) {
        float x = path->path_segments[i * 2], y = path->path_segments[i * 2 + 1];
        if(i != 0 && x == last_x && y == last_y) continue; // In case integer rounding causes the same point twice

        SETFLOAT(coordinates + (num_real_segments * 2) + 1, x);
        SETFLOAT(coordinates + (num_real_segments * 2) + 2, y);
        num_real_segments++;

        last_x = x;
        last_y = y;
    }

    plugdata_draw(gfx->object, gensym("lua_stroke_path"), num_real_segments * 2 + 1, coordinates);
    free(coordinates);

    return 0;
}

static int fill_path(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    t_path_state* path = (t_path_state*)luaL_checkudata(L, 1, "Path");

    float last_x = 0;
    float last_y = 0;

    t_atom* coordinates = malloc(2 * path->num_path_segments * sizeof(t_atom));
    int num_real_segments = 0;

    for (int i = 0; i < path->num_path_segments; i++) {
        float x = path->path_segments[i * 2], y = path->path_segments[i * 2 + 1];
        if(i != 0 && x == last_x && y == last_y) continue; // In case integer rounding causes the same point twice

        SETFLOAT(coordinates + (num_real_segments * 2), x);
        SETFLOAT(coordinates + (num_real_segments * 2) + 1, y);
        num_real_segments++;

        last_x = x;
        last_y = y;
    }

    plugdata_draw(gfx->object, gensym("lua_fill_path"), num_real_segments * 2, coordinates);
    free(coordinates);

    return 0;
}


static int translate(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;
    t_atom args[2];
    SETFLOAT(args, luaL_checknumber(L, 1)); // tx
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // ty
    plugdata_draw(obj, gensym("lua_translate"), 2, args);
    return 0;
}

static int scale(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;
    t_atom args[2];
    SETFLOAT(args, luaL_checknumber(L, 1)); // sx
    SETFLOAT(args + 1, luaL_checknumber(L, 2)); // sy
    plugdata_draw(obj, gensym("lua_scale"), 2, args);
    return 0;
}

static int reset_transform(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;
    plugdata_draw(obj, gensym("lua_reset_transform"), 0, NULL);
    return 0;
}
#else

static unsigned long long custom_rand() {
    // We use a custom random function to ensure proper randomness across all OS
    static unsigned long long seed = 0;
    const unsigned long long a = 1664525;
    const unsigned long long c = 1013904223;
    const unsigned long long m = 4294967296;  // 2^32
    seed = (a * seed + c) % m;
    if(seed == 0) seed = 1; // We cannot return 0 since we use modulo on this. Having the rhs operator of % be zero leads to div-by-zero error on Windows

    return seed;
}

// Generate a new random alphanumeric string to be used as a ID for a tcl/tk drawing
static void generate_random_id(char *str, size_t len) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t charset_len = strlen(charset);

    str[0] = '.';
    str[1] = 'x';

    for (size_t i = 2; i < len - 1; ++i) {
        int key = custom_rand() % charset_len;
        str[i] = charset[key];
    }

    str[len - 1] = '\0';
}

static void transform_size(t_pdlua_gfx *gfx, int* w, int* h) {
    for(int i = gfx->num_transforms - 1; i >= 0; i--)
    {
        if(gfx->transforms[i].type == SCALE)
        {
            *w *= gfx->transforms[i].x;
            *h *= gfx->transforms[i].y;
        }
    }
}

static void transform_point(t_pdlua_gfx *gfx, int* x, int* y) {
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

static void transform_point_float(t_pdlua_gfx *gfx, float* x, float* y) {
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

static void pdlua_gfx_clear(t_pdlua *obj, int removed) {
    t_pdlua_gfx *gfx = &obj->gfx;
    t_canvas *cnv = glist_getcanvas(obj->canvas);
    pdgui_vmess(0, "crs", cnv, "delete", gfx->object_tag);

    if(removed && gfx->order_tag[0] != '\0')
    {
        pdgui_vmess(0, "crs", cnv, "delete", gfx->order_tag);
        gfx->order_tag[0] = '\0';
    }

    glist_eraseiofor(glist_getcanvas(cnv), &obj->pd, gfx->object_tag);
}

static void get_bounds_args(lua_State* L, t_pdlua *obj, int* x1, int* y1, int* x2, int* y2) {
    t_canvas *cnv = glist_getcanvas(obj->canvas);

    int x = luaL_checknumber(L, 1);
    int y = luaL_checknumber(L, 2);
    int w = luaL_checknumber(L, 3);
    int h = luaL_checknumber(L, 4);

    transform_point(&obj->gfx, &x, &y);
    transform_size(&obj->gfx, &w, &h);

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

    int scale = glist_getzoom(glist_getcanvas(x->canvas));

    int xpos = text_xpix((t_object*)x, x->canvas);
    int ypos = text_ypix((t_object*)x, x->canvas);
    glist_drawiofor(x->canvas, (t_object*)x, 0, x->gfx.object_tag, xpos, ypos, xpos + (x->gfx.width * scale), ypos + (x->gfx.height * scale));
}

static const char* register_drawing(t_pdlua_gfx *gfx)
{
    generate_random_id(gfx->current_item_tag, 64);
    return gfx->current_item_tag;
}

static int gfx_initialize(t_pdlua *obj)
{
    t_pdlua_gfx *gfx = &obj->gfx;

    snprintf(gfx->object_tag, 128, ".x%lx", (long)obj);
    gfx->object_tag[127] = '\0';
    gfx->order_tag[0] = '\0';
    gfx->object = obj;
    gfx->transforms = NULL;
    gfx->num_transforms = 0;

    pdlua_gfx_repaint(obj, 0);
    return 0;
}

static int set_size(lua_State* L)
{
    if (!lua_islightuserdata(L, 1)) {
        return 0;
    }

    t_pdlua *obj = (t_pdlua*)lua_touserdata(L, 1);
    obj->gfx.width = luaL_checknumber(L, 2);
    obj->gfx.height = luaL_checknumber(L, 3);
    pdlua_gfx_repaint(obj, 0);
    if(glist_isvisible(obj->canvas) && gobj_shouldvis(&obj->pd.te_g, obj->canvas)) {
        canvas_fixlinesfor(obj->canvas, (t_text*)obj);
    }
    return 0;
}

static int start_paint(lua_State* L) {
    if (!lua_islightuserdata(L, 1)) {
        lua_pushnil(L);
        return 1;
    }

    t_pdlua* obj = (t_pdlua*)lua_touserdata(L, 1);
    t_pdlua_gfx *gfx = &obj->gfx;
    if(gfx->object_tag[0] == '\0')
    {
        lua_pushnil(L);
        return 1;
    }

    // Check if:
    // 1. The canvas and object are visible
    // 2. This is the first repaint since "vis" was called
    // If neither are true, we are not allowed to draw because the targeted tcl/tk canvas is not visible
    int can_draw = (glist_isvisible(obj->canvas) && gobj_shouldvis(&obj->pd.te_g, obj->canvas)) || obj->gfx.first_draw;
    if(can_draw)
    {
        if(gfx->transforms) freebytes(gfx->transforms, gfx->num_transforms * sizeof(gfx_transform));
        gfx->num_transforms = 0;
        gfx->transforms = NULL;

        lua_pushlightuserdata(L, gfx);
        luaL_setmetatable(L, "GraphicsContext");

        // clear anything that was painted before
        if(strlen(gfx->object_tag)) pdlua_gfx_clear(obj, 0);

        if(gfx->first_draw)
        {
            // Whenever the objects gets painted for the first time with a "vis" message,
            // we add a small invisible line that won't get touched or repainted later.
            // We can then use this line to set the correct z-index for the drawings, using the tcl/tk "lower" command
            t_canvas *cnv = glist_getcanvas(obj->canvas);
            generate_random_id(gfx->order_tag, 64);

            const char* tags[] = { gfx->order_tag };
            pdgui_vmess(0, "crr iiii ri rS", cnv, "create", "line", 0, 0, 0, 0,
                        "-width", 1, "-tags", 1, tags);
        }

        return 1;
    }

    lua_pushnil(L);
    return 1;
}

static int end_paint(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = (t_pdlua*)gfx->object;
    t_canvas *cnv = glist_getcanvas(obj->canvas);

    int scale = glist_getzoom(glist_getcanvas(obj->canvas));

    // Draw iolets on top
    int xpos = text_xpix((t_object*)obj, obj->canvas);
    int ypos = text_ypix((t_object*)obj, obj->canvas);

    glist_drawiofor(glist_getcanvas(obj->canvas), (t_object*)obj, 1, gfx->object_tag, xpos, ypos, xpos + (gfx->width * scale), ypos + (gfx->height * scale));

    if(!gfx->first_draw && gfx->order_tag[0] != '\0') {
        // Move everything to below the order marker, to make sure redrawn stuff isn't always on top
        pdgui_vmess(0, "crss", cnv, "lower", gfx->object_tag, gfx->order_tag);
    }

    return 0;
}

static int set_color(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);

    int r, g, b;
    if (lua_gettop(L) == 1) { // Single argument: parse as color ID instead of RGB
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
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    int x1, y1, x2, y2;
    get_bounds_args(L, obj, &x1, &y1, &x2, &y2);

    const char* tags[] = { gfx->object_tag, register_drawing(gfx) };

    pdgui_vmess(0, "crr iiii rs ri rS", cnv, "create", "oval", x1, y1, x2, y2, "-fill", gfx->current_color, "-width", 0, "-tags", 2, tags);

    return 0;
}

static int stroke_ellipse(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    int x1, y1, x2, y2;
    get_bounds_args(L, obj, &x1, &y1, &x2, &y2);

    int line_width = luaL_checknumber(L, 5) * glist_getzoom(cnv);

    const char* tags[] = { gfx->object_tag, register_drawing(gfx) };

    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "oval", x1, y1, x2, y2, "-width", line_width, "-outline", gfx->current_color, "-tags", 2, tags);

    return 0;
}

static int fill_all(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    int x1 = text_xpix((t_object*)obj, obj->canvas);
    int y1 = text_ypix((t_object*)obj, obj->canvas);
    int x2 = x1 + gfx->width * glist_getzoom(cnv);
    int y2 = y1 + gfx->height * glist_getzoom(cnv);

    const char* tags[] =  { gfx->object_tag, register_drawing(gfx) };

    pdgui_vmess(0, "crr iiii rs rS", cnv, "create", "rectangle", x1, y1, x2, y2, "-fill", gfx->current_color, "-tags", 2, tags);

    return 0;
}

static int fill_rect(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    int x1, y1, x2, y2;
    get_bounds_args(L, obj, &x1, &y1, &x2, &y2);

    const char* tags[] = { gfx->object_tag, register_drawing(gfx) };

    pdgui_vmess(0, "crr iiii rs ri rS", cnv, "create", "rectangle", x1, y1, x2, y2, "-fill", gfx->current_color, "-width", 0, "-tags", 2, tags);

    return 0;
}

static int stroke_rect(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    int x1, y1, x2, y2;
    get_bounds_args(L, obj, &x1, &y1, &x2, &y2);

    int line_width = luaL_checknumber(L, 5) * glist_getzoom(cnv);

    const char* tags[] = { gfx->object_tag, register_drawing(gfx) };

    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "rectangle", x1, y1, x2, y2, "-width", line_width, "-outline", gfx->current_color, "-tags", 2, tags);

    return 0;
}

static int fill_rounded_rect(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    int x1, y1, x2, y2;
    get_bounds_args(L, obj, &x1, &y1, &x2, &y2);

    int radius = luaL_checknumber(L, 5);  // Radius for rounded corners
    int radius_x = radius * glist_getzoom(cnv);
    int radius_y = radius * glist_getzoom(cnv);

    transform_size(gfx, &radius_x, &radius_y);

    const char* tags[] = { gfx->object_tag, register_drawing(gfx) };

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
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    int x1, y1, x2, y2;
    get_bounds_args(L, obj, &x1, &y1, &x2, &y2);

    int radius = luaL_checknumber(L, 5);       // Radius for rounded corners
    int radius_x = radius * glist_getzoom(cnv);
    int radius_y = radius * glist_getzoom(cnv);
    transform_size(gfx, &radius_x, &radius_y);
    int line_width = luaL_checknumber(L, 6) * glist_getzoom(cnv);

    const char* tags[] = { gfx->object_tag, register_drawing(gfx) };

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
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    int x1 = luaL_checknumber(L, 1);
    int y1 = luaL_checknumber(L, 2);
    int x2 = luaL_checknumber(L, 3);
    int y2 = luaL_checknumber(L, 4);
    int line_width = luaL_checknumber(L, 5);

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
    line_width *= canvas_zoom;

    const char* tags[] = { gfx->object_tag, register_drawing(gfx) };

    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "line", x1, y1, x2, y2,
                "-width", line_width, "-fill", gfx->current_color, "-tags", 2, tags);

    return 0;
}

static int draw_text(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    const char* text = luaL_checkstring(L, 1); // Assuming text is a string
    int x = luaL_checknumber(L, 2);
    int y = luaL_checknumber(L, 3);
    int w = luaL_checknumber(L, 4);
    int font_height = luaL_checknumber(L, 5);
    font_height = sys_hostfontsize(font_height, glist_getzoom(cnv));

    transform_point(gfx, &x, &y);
    transform_size(gfx, &w, &font_height);

    int canvas_zoom = glist_getzoom(cnv);
    x += text_xpix((t_object*)obj, obj->canvas) / canvas_zoom;
    y += text_ypix((t_object*)obj, obj->canvas) / canvas_zoom;

    x *= canvas_zoom;
    y *= canvas_zoom;
    w *= canvas_zoom;

    const char* tags[] = { gfx->object_tag, register_drawing(gfx) };

    pdgui_vmess(0, "crr ii rs ri rs rS", cnv, "create", "text",
                0, 0, "-anchor", "nw", "-width", w, "-text", text, "-tags", 2, tags);

    t_atom fontatoms[3];
    SETSYMBOL(fontatoms+0, gensym(sys_font));
    SETFLOAT (fontatoms+1, -font_height); // Size is wrong on hi-dpi Windows is this is not negative
    SETSYMBOL(fontatoms+2, gensym(sys_fontweight));

    pdgui_vmess(0, "crs rA rs rs", cnv, "itemconfigure", tags[1],
            "-font", 3, fontatoms,
            "-fill", gfx->current_color,
            "-justify", "left");

    pdgui_vmess(0, "crs ii", cnv, "coords", tags[1], x, y);

    return 0;
}

static int stroke_path(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    t_path_state* path = (t_path_state*)luaL_checkudata(L, 1, "Path");
    if(path->num_path_segments < 3)
    {
        return 0;
    }

    int stroke_width = luaL_checknumber(L, 2) * glist_getzoom(cnv);
    int obj_x = text_xpix((t_object*)obj, obj->canvas);
    int obj_y = text_ypix((t_object*)obj, obj->canvas);
    int canvas_zoom = glist_getzoom(cnv);

    const char* tags[] = { gfx->object_tag, register_drawing(gfx) };

    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "line", 0, 0, 0, 0, "-width", stroke_width, "-fill", gfx->current_color, "-tags", 2, tags);

    float last_x, last_y;

    sys_vgui(".x%lx.c coords %s", cnv, tags[1]);
    for (int i = 0; i < path->num_path_segments; i++) {
        float x = path->path_segments[i * 2], y = path->path_segments[i * 2 + 1];
        last_x = x;
        last_y = y;

        transform_point_float(gfx, &x, &y);
        sys_vgui(" %f %f", (x * canvas_zoom) + obj_x, (y * canvas_zoom) + obj_y);
    }
    sys_vgui("\n");

    return 0;
}

static int fill_path(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    t_pdlua *obj = gfx->object;

    t_canvas *cnv = glist_getcanvas(obj->canvas);

    t_path_state* path = (t_path_state*)luaL_checkudata(L, 1, "Path");
    if(path->num_path_segments < 3)
    {
        return 0;
    }

    // Apply transformations to all coordinates
    int obj_x = text_xpix((t_object*)obj, obj->canvas);
    int obj_y = text_ypix((t_object*)obj, obj->canvas);
    int canvas_zoom = glist_getzoom(cnv);

    const char* tags[] = { gfx->object_tag, register_drawing(gfx) };

    pdgui_vmess(0, "crr iiii ri rs rS", cnv, "create", "polygon", 0, 0, 0, 0, "-width", 0, "-fill", gfx->current_color, "-tags", 2, tags);

    float last_x, last_y;

    sys_vgui(".x%lx.c coords %s", cnv, tags[1]);
    for (int i = 0; i < path->num_path_segments; i++) {
        float x = path->path_segments[i * 2], y = path->path_segments[i * 2 + 1];
        last_x = x;
        last_y = y;

        transform_point_float(gfx, &x, &y);
        sys_vgui(" %f %f", (x * canvas_zoom) + obj_x, (y * canvas_zoom) + obj_y);
    }
    sys_vgui("\n");

    return 0;
}


static int translate(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);

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
    t_pdlua_gfx *gfx = pop_graphics_context(L);

    gfx->transforms = resizebytes(gfx->transforms, gfx->num_transforms * sizeof(gfx_transform), (gfx->num_transforms + 1) * sizeof(gfx_transform));

    gfx->transforms[gfx->num_transforms].type = SCALE;
    gfx->transforms[gfx->num_transforms].x = luaL_checknumber(L, 1);
    gfx->transforms[gfx->num_transforms].y = luaL_checknumber(L, 2);

    gfx->num_transforms++;
    return 0;
}

static int reset_transform(lua_State* L) {
    t_pdlua_gfx *gfx = pop_graphics_context(L);
    freebytes(gfx->transforms, gfx->num_transforms * sizeof(gfx_transform));
    gfx->transforms = NULL;
    gfx->num_transforms = 0;
    return 0;
}
#endif
static void add_path_segment(t_path_state* path, float x, float y)
{
    int path_segment_space = (path->num_path_segments + 1) * 2;
    int old_size = path->num_path_segments_allocated;
    int new_size = MAX(path_segment_space, path->num_path_segments_allocated);
    if(!path->num_path_segments_allocated) {
        path->path_segments = (float*)getbytes(new_size * sizeof(float));
    }
    else {
        path->path_segments = (float*)resizebytes(path->path_segments, old_size * sizeof(float), new_size * sizeof(float));
    }

    path->num_path_segments_allocated = new_size;

    path->path_segments[path->num_path_segments * 2] = x;
    path->path_segments[path->num_path_segments * 2 + 1] = y;
    path->num_path_segments++;
}

static int start_path(lua_State* L) {
    t_path_state *path = (t_path_state *)lua_newuserdata(L, sizeof(t_path_state));
    luaL_setmetatable(L, "Path");

    path->num_path_segments = 0;
    path->num_path_segments_allocated = 0;
    path->path_start_x = luaL_checknumber(L, 1);
    path->path_start_y = luaL_checknumber(L, 2);

    add_path_segment(path, path->path_start_x, path->path_start_y);
    return 1;
}

// Function to add a line to the current path
static int line_to(lua_State* L) {
    t_path_state* path = (t_path_state*)luaL_checkudata(L, 1, "Path");
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);
    add_path_segment(path, x, y);
    return 0;
}

static int quad_to(lua_State* L) {
    t_path_state* path = (t_path_state*)luaL_checkudata(L, 1, "Path");
    float x2 = luaL_checknumber(L, 2);
    float y2 = luaL_checknumber(L, 3);
    float x3 = luaL_checknumber(L, 4);
    float y3 = luaL_checknumber(L, 5);

    float x1 = path->num_path_segments > 0 ? path->path_segments[(path->num_path_segments - 1) * 2] : x2;
    float y1 = path->num_path_segments > 0 ? path->path_segments[(path->num_path_segments - 1) * 2 + 1] : y2;

    // heuristic for deciding the number of lines in our bezier curve
    float dx = x3 - x1;
    float dy = y3 - y1;
    float distance = sqrtf(dx * dx + dy * dy);
    float resolution = MAX(10.0f, distance);

    // Get the last point
    float t = 0.0;
    while (t <= 1.0) {
        t += 1.0 / resolution;

        // Calculate quadratic bezier curve as points (source: https://en.wikipedia.org/wiki/B%C3%A9zier_curve)
        float x = (1.0f - t) * (1.0f - t) * x1 + 2.0f * (1.0f - t) * t * x2 + t * t * x3;
        float y = (1.0f - t) * (1.0f - t) * y1 + 2.0f * (1.0f - t) * t * y2 + t * t * y3;
        add_path_segment(path, x, y);
    }

    return 0;
}

static int cubic_to(lua_State* L) {
    t_path_state* path = (t_path_state*)luaL_checkudata(L, 1, "Path");
    float x2 = luaL_checknumber(L, 2);
    float y2 = luaL_checknumber(L, 3);
    float x3 = luaL_checknumber(L, 4);
    float y3 = luaL_checknumber(L, 5);
    float x4 = luaL_checknumber(L, 6);
    float y4 = luaL_checknumber(L, 7);

    float x1 = path->num_path_segments > 0 ? path->path_segments[(path->num_path_segments - 1) * 2] : x2;
    float y1 = path->num_path_segments > 0 ? path->path_segments[(path->num_path_segments - 1) * 2 + 1] : y2;

    // heuristic for deciding the number of lines in our bezier curve
    float dx = x3 - x1;
    float dy = y3 - y1;
    float distance = sqrtf(dx * dx + dy * dy);
    float resolution = MAX(10.0f, distance);

    // Get the last point
    float t = 0.0;
    while (t <= 1.0) {
        t += 1.0 / resolution;

        // Calculate cubic bezier curve as points (source: https://en.wikipedia.org/wiki/B%C3%A9zier_curve)
        float x = (1 - t)*(1 - t)*(1 - t) * x1 + 3 * (1 - t)*(1 - t) * t * x2 + 3 * (1 - t) * t*t * x3 + t*t*t * x4;
        float y = (1 - t)*(1 - t)*(1 - t) * y1 + 3 * (1 - t)*(1 - t) * t * y2 + 3 * (1 - t) * t*t * y3 + t*t*t * y4;

        add_path_segment(path, x, y);
    }

    return 0;
}

// Function to close the current path
static int close_path(lua_State* L) {
    t_path_state* path = (t_path_state*)luaL_checkudata(L, 1, "Path");
    add_path_segment(path, path->path_start_x, path->path_start_y);
    return 0;
}

static int free_path(lua_State* L)
{
    t_path_state* path = (t_path_state*)luaL_checkudata(L, 1, "Path");
    freebytes(path->path_segments, path->num_path_segments_allocated * sizeof(int));
    return 0;
}
