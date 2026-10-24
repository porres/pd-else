
/* Copyright (c) 2004-2005 krzYszcz and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#ifndef __OS_H__
#define __OS_H__

EXTERN_STRUCT _osdir;
#define t_osdir  struct _osdir

#define OSDIR_FILEMODE  1
#define OSDIR_DIRMODE   2

int else_ospath_length(char *path, char *cwd);
char *else_ospath_absolute(char *path, char *cwd, char *result);

FILE *else_fileread_open(char *filename, t_canvas *cv, int textmode);
FILE *else_filewrite_open(char *filename, t_canvas *cv, int textmode);

t_osdir *else_osdir_open(char *dirname);
void else_osdir_setmode(t_osdir *dp, int flags);
void else_osdir_close(t_osdir *dp);
void else_osdir_rewind(t_osdir *dp);
char *else_osdir_next(t_osdir *dp);
int else_osdir_isfile(t_osdir *dp);
int else_osdir_isdir(t_osdir *dp);

#endif

#ifndef __FILE_H__
#define __FILE_H__

EXTERN_STRUCT _elsefile;
#define t_elsefile  struct _elsefile

typedef void (*t_elsefilefn)(t_pd *, t_symbol *, int, t_atom *);
typedef void (*t_embedfn)(t_pd *, t_binbuf *, t_symbol *);

void elsefile_editor_open(t_elsefile *f, char *title, char *owner);
void else_editor_close(t_elsefile *f, int ask);
void else_editor_append(t_elsefile *f, char *contents);
void else_editor_setdirty(t_elsefile *f, int flag);
void elsefile_panel_click_open(t_elsefile *f);
void else_panel_setopendir(t_elsefile *f, t_symbol *dir);
t_symbol *else_panel_getopendir(t_elsefile *f);
void elsefile_panel_save(t_elsefile *f, t_symbol *inidir, t_symbol *inifile);
void else_panel_setsavedir(t_elsefile *f, t_symbol *dir);
t_symbol *else_panel_getsavedir(t_elsefile *f);
int elsefile_ismapped(t_elsefile *f);
int elsefile_isloading(t_elsefile *f);
int elsefile_ispasting(t_elsefile *f);
void elsefile_free(t_elsefile *f);
t_elsefile *elsefile_new(t_pd *master, t_embedfn embedfn, t_elsefilefn readfn, t_elsefilefn writefn, t_elsefilefn updatefn);
void elsefile_setup(t_class *c, int embeddable);

#endif
