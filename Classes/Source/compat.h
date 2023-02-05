
#ifndef COMPAT_H
#define COMPAT_H

// Definitions for Purr Data compatibility which still has an older API than
// current vanilla.

#ifndef IHEIGHT
// Purr Data doesn't have these, hopefully the vanilla values will work
#define IHEIGHT 3       /* height of an inlet in pixels */
#define OHEIGHT 3       /* height of an outlet in pixels */
#endif

#endif
