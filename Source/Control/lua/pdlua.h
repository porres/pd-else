 /**
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
 **/

#include "m_pd.h"

typedef enum {SCALE, TRANSLATE} transform_type;

typedef struct _gfx_transform
{
    transform_type type;
    float x, y;
} gfx_transform;

typedef struct _pdlua_gfx
{
    // Size variables
    int width, height;
    void *object;
    
#if !PLUGDATA
    char object_tag[128]; // Tcl/tk tag that is attached to all drawings
    char order_tag[64]; // Tag for invisible line, used to preserve correct object ordering
    char current_item_tag[64]; // Tcl/tk tag that is only attached to the current drawing in progress
    char** layer_tags;
    int num_layers;
    char* current_layer_tag;
    gfx_transform* transforms;
    int num_transforms;
    char current_color[10]; // Keep track of current color
    
    // Variables to keep track of mouse button state and drag position
    int mouse_drag_x, mouse_drag_y, mouse_down;
    int first_draw;
    
#else
    int current_layer;
    void(*plugdata_draw_callback)(void*, int, t_symbol*, int, t_atom*); // Callback to perform drawing in plugdata
#endif
} t_pdlua_gfx;

/** Pd object data. */
typedef struct pdlua 
{
    t_object                pd;               // We are a Pd object.
    int                     inlets;           // Number of inlets.
    struct pdlua_proxyinlet *proxy_in;        // The inlets themselves.
    t_inlet                 **in;
    int                     outlets;          // Number of outlets.
    t_outlet                **out;            // The outlets themselves.
    int                     siginlets;        // Number of signal inlets.
    int                     sigoutlets;       // Number of signal outlets.
    int                     sig_warned;       // Flag for perform signal errors.
    int                     blocksize;        // Blocksize set in dsp method.
    t_canvas                *canvas;          // The canvas that the object was created on.
    int                     has_gui;          // True if graphics are enabled.
    t_pdlua_gfx             gfx;              // Holds state for graphics.
    t_class                 *pdlua_class;     // Holds our class pointer.
    t_class                 *pdlua_class_gfx; // Holds our gfx class pointer.
    t_signal                **sp;             // Array of signal pointers for multichannel audio.
} t_pdlua;

lua_State* __L();
