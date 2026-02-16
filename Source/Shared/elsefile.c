/* Copyright (c) 2002-2005 krzYszcz and others.

The 'file' proxy allows:
   - embeding data in a .pd file
   - openpanel/savepanel management
   - A text editor window
For embedding, a master class sends a flag to setup and a nonzero 'embedfn'
function pointer to the constructor. For panels, it passes a 'readfn' and/or
writefn functions to the constructor. For text editor, updatefn is passed to
the constructor. LATER extract the embedding stuff. */

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "m_pd.h"
#include "g_canvas.h"
#include "elsefile.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PATH FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int ospath_doabsolute(char *path, char *cwd, char *result){
    if(*path == 0){
        if(result)
            strcpy(result, cwd);
        else
            return
            (strlen(cwd));
    }
    else if(*path == '~'){
        path++;
        if(*path == '/' || *path == 0){
#ifndef _WIN32
            char *home = getenv("HOME");
            if(home){
                if(result){
                    strcpy(result, home);
                    if(*path)
                        strcat(result, path);
                }
                else return(strlen(home) + strlen(path));
            }
            else goto badpath;
#else
            goto badpath;
#endif
        }
        else
            goto badpath;
    }
    else if(*path == '/'){
#ifdef _WIN32
        if(*cwd && cwd[1] == ':'){  // absolute, drive is implicit, LATER UNC?
            if(result){
                *result = *cwd;
                result[1] = ':';
                strcpy(result + 2, path);
            }
            else
                return(2 + strlen(path));
        }
        else
            goto badpath;
#else
        if(result) // absolute
            strcpy(result, path);
        else
            return(strlen(path));
#endif
    }
    else{
#ifdef _WIN32
        if(path[1] == ':'){
            if(path[2] == '/'){
                /* path is absolute */
                if(result)
                    strcpy(result, path);
                else
                    return(strlen(path));
            }
            else if(*cwd == *path){
                if(result){ // relative, drive is explicitly current
                    int ndx = strlen(cwd);
                    strcpy(result, cwd);
                    result[ndx++] = '/';
                    strcpy(result + ndx, path + 2);
                }
                else
                    return(strlen(cwd) + strlen(path) - 1);
            }
            else // we do not maintain per-drive cwd, LATER rethink
                goto badpath;
        }
        else{  // LATER devices?
            if(result){ // path is relative
                int ndx = strlen(cwd);
                strcpy(result, cwd);
                result[ndx++] = '/';
                strcpy(result + ndx, path);
            }
            else
                return(strlen(cwd) + 1 + strlen(path));
        }
#else
        if(result){ // path is relative
            int ndx = strlen(cwd);
            strcpy(result, cwd);
            result[ndx++] = '/';
            strcpy(result + ndx, path);
        }
        else
            return(strlen(cwd) + 1 + strlen(path));
#endif
    }
    if(result && *result && *result != '.'){
        // clean-up
        char *inptr, *outptr = result;
        int ndx = strlen(result);
        if(result[ndx - 1] == '.'){
            result[ndx] = '/';  // guarding slash
            result[ndx + 1] = 0;
        }
        for(inptr = result + 1; *inptr; inptr++){
            if(*inptr == '/'){
                if(*outptr == '/')
                    continue;
                else if(*outptr == '.'){
                    if(outptr[-1] == '/'){
                        outptr--;
                        continue;
                    }
                    else if(outptr[-1] == '.' && outptr[-2] == '/'){
                        outptr -= 2;
                        if(outptr == result)
                            continue;
                        else for(outptr--; outptr != result; outptr--)
                            if(*outptr == '/')
                                break;
                        continue;
                    }
                }
            }
            *++outptr = *inptr;
        }
        if(*outptr == '/' && outptr != result)
            *outptr = 0;
        else
            outptr[1] = 0;
    }
    else
        bug("ospath_doabsolute 1");
    return(0);
badpath:
    if(result)
        bug("ospath_doabsolute 2");
    return(0);
}

// Return absolute length; ospath_absolute()'s length may be shorter (no
// superfluous slashes/dots). Both args must be unbashed (system-independent),
// cwd must be absolute. Returns 0 in case errors.
int ospath_length(char *path, char *cwd){
    return(ospath_doabsolute(path, cwd, 0) + 1); // + extra guarding slash
}

// Copy absolute path to result. Args (path and cwd) are the same as in
// ospath_length(), which must be first consulted and  allocate at least
// ospath_length() + 1 bytes to the result buffer. A failure is a bug.
char *ospath_absolute(char *path, char *cwd, char *result){
    ospath_doabsolute(path, cwd, result);
    return(result);
}

FILE *fileread_open(char *filename, t_canvas *cv, int textmode){
    char path[MAXPDSTRING+2], *nameptr;
    t_symbol *dirsym = (cv ? canvas_getdir(cv) : 0);
    // path arg is returned unbashed (system-independent)
    int fd = open_via_path((dirsym ? dirsym->s_name : ""), filename, "", path, &nameptr, MAXPDSTRING, 1);
    if(fd < 0)
        return(0);
// Unnecessary in linux, could've tried convert fd to fp, but in windows
// open_via_path() seems to return invalid fd. LATER try understand it.
    close(fd);
    if(path != nameptr){
        char *slashpos = path + strlen(path);
        *slashpos++ = '/';
        if(nameptr != slashpos) // try not dependent on current open_via_path()
            strcpy(slashpos, nameptr);
    }
    return(sys_fopen(path, (textmode ? "r" : "rb")));
}

FILE *filewrite_open(char *filename, t_canvas *cv, int textmode){
    char path[MAXPDSTRING+2];
    if(cv) // path arg is returned unbashed (system-independent)
        canvas_makefilename(cv, filename, path, MAXPDSTRING);
    else{
        strncpy(path, filename, MAXPDSTRING);
        path[MAXPDSTRING-1] = 0;
    }
    return(sys_fopen(path, (textmode ? "w" : "wb")));
}

// FIXME add MSW
struct _osdir{
#ifndef _WIN32
    DIR            *dir_handle;
    struct dirent  *dir_entry;
#endif
    int             dir_flags;
};

// returns 0 on error, a caller is then expected to call
// loud_syserror(owner, "cannot open \"%s\"", dirname)
t_osdir *osdir_open(char *dirname){
#ifndef _WIN32
    DIR *handle = opendir(dirname);
    if(handle){
#endif
        t_osdir *dp = getbytes(sizeof(*dp));
#ifndef _WIN32
        dp->dir_handle = handle;
        dp->dir_entry = 0;
#endif
        dp->dir_flags = 0;
        return(dp);
#ifndef _WIN32
    }
    else
        return(0);
#endif
}

void osdir_setmode(t_osdir *dp, int flags){
    if(dp)
        dp->dir_flags = flags;
}

void osdir_close(t_osdir *dp){
    if(dp){
#ifndef _WIN32
        closedir(dp->dir_handle);
#endif
        freebytes(dp, sizeof(*dp));
    }
}

void osdir_rewind(t_osdir *dp){
    if(dp){
#ifndef _WIN32
        rewinddir(dp->dir_handle);
        dp->dir_entry = 0;
#endif
    }
}

char *osdir_next(t_osdir *dp){
#ifndef _WIN32
    if(dp){
        while((dp->dir_entry = readdir(dp->dir_handle))){
            if(!dp->dir_flags || (dp->dir_entry->d_type == DT_REG
            && (dp->dir_flags & OSDIR_FILEMODE))
            || (dp->dir_entry->d_type == DT_DIR
            && (dp->dir_flags & OSDIR_DIRMODE)))
                return(dp->dir_entry->d_name);
        }
    }
#endif
    return(0);
}

int osdir_isfile(t_osdir *dp){
#ifndef _WIN32
    return(dp && dp->dir_entry && dp->dir_entry->d_type == DT_REG);
#else
    return(0);
#endif
}

int osdir_isdir(t_osdir *dp){
#ifndef _WIN32
    return(dp && dp->dir_entry && dp->dir_entry->d_type == DT_DIR);
#else
    return(0);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FILE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct _elsefile{
    t_pd                 f_pd;
    t_pd                *f_master;
    t_canvas            *f_canvas;
    t_symbol            *f_bindname;
    t_symbol            *f_currentdir;
    t_symbol            *f_inidir;
    t_symbol            *f_inifile;
    t_elsefilefn       f_panelfn;
    t_elsefilefn       f_editorfn;
    t_embedfn           f_embedfn;
    t_binbuf           *f_binbuf;
    t_clock            *f_panelclock;
    t_clock            *f_editorclock;
    struct _elsefile  *f_savepanel;
    struct _elsefile  *f_next;
};

static t_class *elsefile_class = 0;
static t_elsefile *elsefile_proxies;
static t_symbol *ps__C;

static t_elsefile *elsefile_getproxy(t_pd *master){
    t_elsefile *f;
    for(f = elsefile_proxies; f; f = f->f_next)
        if(f->f_master == master)
            return(f);
    return(0);
}

static void elsefile_text_window_editor_guidefs(void){
    static const char *script =
    "proc else_editor_open {name geometry title sendable} {\n"
    " if {[winfo exists $name]} {\n"
    "  $name.text delete 1.0 end\n"
    " } else {\n"
    "  toplevel $name\n"
    "  wm title $name $title\n"
    "  wm geometry $name $geometry\n"
    "  if {$sendable} {\n"
    "   wm protocol $name WM_DELETE_WINDOW [concat else_editor_close $name 1]\n"
    "   bind $name <<Modified>> \"else_editor_dodirty $name\"\n"
    "   if {[tk windowingsystem] eq \"aqua\"} {\n"
    "    bind $name <Command-w> \"else_editor_close $name 1\"\n"
    "    bind $name <Command-s> \"else_editor_send $name; else_editor_setdirty $name 0\"\n"
    "   } else {\n"
    "    bind $name <Control-w> \"else_editor_close $name 1\"\n"
    "    bind $name <Control-s> \"else_editor_send $name; else_editor_setdirty $name 0\"\n"
    "   }\n"
    "  }\n"
    "  text $name.text -relief raised -bd 2 \\\n"
    "   -font -*-courier-medium--normal--12-* \\\n"
    "   -yscrollcommand \"$name.scroll set\" -background white\n"
    "  scrollbar $name.scroll -command \"$name.text yview\"\n"
    "  pack $name.scroll -side right -fill y\n"
    "  pack $name.text -side left -fill both -expand 1\n"
    " }\n"
    "}\n"
    "proc else_editor_dodirty {name} {\n"
    " if {[catch {$name.text edit modified} dirty]} {set dirty 1}\n"
    " set title [wm title $name]\n"
    " set dt [string equal -length 1 $title \"*\"]\n"
    " if {$dirty} {\n"
    "  if {$dt == 0} {wm title $name *$title}\n"
    " } else {\n"
    "  if {$dt} {wm title $name [string range $title 1 end]}\n"
    " }\n"
    "}\n"
    "proc else_editor_setdirty {name flag} {\n"
    " if {[winfo exists $name]} {catch {$name.text edit modified $flag}}\n"
    "}\n"
    "proc else_editor_doclose {name} {destroy $name}\n"
    "proc else_editor_append {name contents} {\n"
    " if {[winfo exists $name]} {$name.text insert end $contents}\n"
    "}\n"
    "proc else_editor_send {name} {\n"
    " if {[winfo exists $name]} {\n"
    "  pdsend \"ELSEFILE$name clear\"\n"
    "  for {set i 1} {[$name.text compare $i.end < end]} {incr i 1} {\n"
    "   set lin [$name.text get $i.0 $i.end]\n"
    "   if {$lin != \"\"} {\n"
    "    regsub -all \\; $lin \"  _semi_ \" tmplin\n"
    "    regsub -all \\, $tmplin \"  _comma_ \" lin\n"
    "    pdsend \"ELSEFILE$name addline $lin\"\n"
    "   }\n"
    "  }\n"
    "  pdsend \"ELSEFILE$name end\"\n"
    " }\n"
    "}\n"
    "proc else_editor_close {name ask} {\n"
    " if {[winfo exists $name]} {\n"
    "  if {[catch {$name.text edit modified} dirty]} {set dirty 1}\n"
    "  if {$ask && $dirty} {\n"
    "   set title [wm title $name]\n"
    "   if {[string equal -length 1 $title \"*\"]} {set title [string range $title 1 end]}\n"
    "   set answer [tk_messageBox -type yesnocancel -icon question \\\n"
    "    -message [concat Save changes to \\\"$title\\\"?]]\n"
    "   if {$answer == \"yes\"} {else_editor_send $name}\n"
    "   if {$answer != \"cancel\"} {else_editor_doclose $name}\n"
    "  } else {else_editor_doclose $name}\n"
    " }\n"
    "}\n";
    pdgui_vmess(script, NULL);
}

// null owner defaults to class name, pass "" to suppress
void else_editor_open(t_elsefile *f, char *title, char *owner){
    if (!owner)
        owner = (char *)(class_getname(*f->f_master));
    if(!*owner)
        owner = 0;
    if(!title){
        title = owner;
        owner = 0;
    }
    char buf[256];
    if(owner){
        snprintf(buf, sizeof(buf),
            "else_editor_open .%lx %dx%d {%s: %s} %d",
            (unsigned long)f,
            600, 340,
            owner, title,
            (f->f_editorfn != 0));
    }
    else{
        snprintf(buf, sizeof(buf),
            "else_editor_open .%lx %dx%d {%s} %d",
            (unsigned long)f,
            600, 340,
            (title ? title : "Untitled"),
            (f->f_editorfn != 0));
    }
    pdgui_vmess(buf, NULL);
}


static void else_editor_tick(t_elsefile *f){
    char buf[64];
    snprintf(buf, sizeof(buf), "else_editor_close .%lx 1", (unsigned long)f);
    pdgui_vmess(buf, NULL);
}

void else_editor_close(t_elsefile *f, int ask){
    if(ask && f->f_editorfn)
	// hack: deferring modal dialog creation in order to allow for
	// a message box redraw to happen -- LATER investigate
        clock_delay(f->f_editorclock, 0);
    else{
        char buf[64];
        snprintf(buf, sizeof(buf), "else_editor_close .%lx 0", (unsigned long)f);
        pdgui_vmess(buf, NULL);
    }
}

void else_editor_append(t_elsefile *f, char *contents)
{
    if (contents) {
        char *ptr;
        char buf[512];

        for (ptr = contents; *ptr; ptr++) {
            if (*ptr == '{' || *ptr == '}') {
                char c = *ptr;
                *ptr = 0;

                snprintf(buf, sizeof(buf),
                    "else_editor_append .%lx {%s}",
                    (unsigned long)f, contents);
                pdgui_vmess(buf, NULL);

                snprintf(buf, sizeof(buf),
                    "else_editor_append .%lx \"%c\"",
                    (unsigned long)f, c);
                pdgui_vmess(buf, NULL);

                *ptr = c;
                contents = ptr + 1;
            }
        }

        if (*contents) {
            snprintf(buf, sizeof(buf),
                "else_editor_append .%lx {%s}",
                (unsigned long)f, contents);
            pdgui_vmess(buf, NULL);
        }
    }
}

void else_editor_setdirty(t_elsefile *f, int flag){
    if (f->f_editorfn){
        char buf[64];
        snprintf(buf, sizeof(buf),
            "else_editor_setdirty .%lx %d",
            (unsigned long)f, flag);
        pdgui_vmess(buf, NULL);
    }
}

static void else_editor_clear(t_elsefile *f){
    if(f->f_editorfn){
        if(f->f_binbuf)
            binbuf_clear(f->f_binbuf);
        else
            f->f_binbuf = binbuf_new();
    }
}

static void else_editor_addline(t_elsefile *f, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(f->f_editorfn){
        int i;
        t_atom *ap;
        for(i = 0, ap = av; i < ac; i++, ap++){
            if(ap->a_type == A_SYMBOL){
                /* LATER rethink semi/comma mapping */
                if(!strcmp(ap->a_w.w_symbol->s_name, "_semi_"))
                    SETSEMI(ap);
                else if(!strcmp(ap->a_w.w_symbol->s_name, "_comma_"))
                    SETCOMMA(ap);
            }
        }
        binbuf_add(f->f_binbuf, ac, av);
    }
}

static void else_editor_end(t_elsefile *f){
    if(f->f_editorfn){
        (*f->f_editorfn)(f->f_master, 0, binbuf_getnatom(f->f_binbuf), binbuf_getvec(f->f_binbuf));
	binbuf_clear(f->f_binbuf);
    }
}

static void elsefile_panel_guidefs(void){
    static const char script[] =
    "proc panel_open {target inidir} {\n"
    " global pd_opendir\n"
    " if {$inidir == \"\"} {\n"
    "  set inidir $pd_opendir\n"
    " }\n"
    " set filename [tk_getOpenFile \\\n"
    "  -initialdir $inidir]\n"
    " if {$filename != \"\"} {\n"
    "  set directory [string range $filename 0 \\\n"
    "   [expr [string last / $filename ] - 1]]\n"
    "  if {$directory == \"\"} {set directory \"/\"}\n"
    "  puts stderr [concat $directory]\n"
    "  pdsend \"$target path \\\n"
    "   [enquote_path $filename] [enquote_path $directory] \"\n"
    " }\n"
    "}\n"
    "proc panel_save {target inidir inifile} {\n"
    " if {$inifile != \"\"} {\n"
    "  set filename [tk_getSaveFile \\\n"
    "   -initialdir $inidir -initialfile $inifile]\n"
    " } else {\n"
    "  set filename [tk_getSaveFile]\n"
    " }\n"
    " if {$filename != \"\"} {\n"
    "  set directory [string range $filename 0 \\\n"
    "   [expr [string last / $filename ] - 1]]\n"
    "  if {$directory == \"\"} {set directory \"/\"}\n"
    "  pdsend \"$target path \\\n"
    "   [enquote_path $filename] [enquote_path $directory] \"\n"
    " }\n"
    "}\n";
    pdgui_vmess(script, NULL);
}

/* There are two modes of -initialdir persistence:
   1. Using last reply from gui (if any, default is canvas directory):
   pass null to panel_open/save() (for explicit cd, optionally call
   panel_setopen/savedir() first).
   2. Starting always in the same directory (eg. canvasdir):
   feed panel_open/save().
   Usually, first mode fits opening better, the second -- saving. */
static void panel_path(t_elsefile *f, t_symbol *s1, t_symbol *s2){
    if(s2 && s2 != &s_)
        f->f_currentdir = s2;
    if(s1 && s1 != &s_ && f->f_panelfn)
        (*f->f_panelfn)(f->f_master, s1, 0, 0);
}

static void panel_tick(t_elsefile *f){
    char buf[256];
    if(f->f_savepanel){
        snprintf(buf, sizeof(buf),
            "panel_open %s {%s}",
            f->f_bindname->s_name,
            f->f_inidir->s_name);
    }
    else{
        snprintf(buf, sizeof(buf),
            "panel_save %s {%s} {%s}",
            f->f_bindname->s_name,
            f->f_inidir->s_name,
            f->f_inifile->s_name);
    }
    pdgui_vmess(buf, NULL);
}

// these are hacks: deferring modal dialog creation in order to allow for
// a message box redraw to happen -- LATER investigate
void elsefile_panel_click_open(t_elsefile *f){
    // make it remember the last opened dir...
    f->f_inidir = (f->f_currentdir ? f->f_currentdir : &s_);
    clock_delay(f->f_panelclock, 0);
}

void panel_setopendir(t_elsefile *f, t_symbol *dir){
    if(f->f_currentdir && f->f_currentdir != &s_){
        if(dir && dir != &s_){
            int length = ospath_length((char *)(dir->s_name), (char *)(f->f_currentdir->s_name));
            if(length){
                char *path = getbytes(length + 1);
                if(ospath_absolute((char *)(dir->s_name), (char *)(f->f_currentdir->s_name), path))
                /* LATER stat (think how to report a failure) */
                    f->f_currentdir = gensym(path);
                freebytes(path, length + 1);
            }
        }
        else if(f->f_canvas)
            f->f_currentdir = canvas_getdir(f->f_canvas);
    }
    else
        bug("panel_setopendir");
}

t_symbol *panel_getopendir(t_elsefile *f){
    return(f->f_currentdir);
}

void elsefile_panel_save(t_elsefile *f, t_symbol *inidir, t_symbol *inifile){
    if((f = f->f_savepanel)){
        if(inidir)
            f->f_inidir = inidir;
        else // LATER ask if we can rely on s_ pointing to ""
            f->f_inidir = (f->f_currentdir ? f->f_currentdir : &s_);
        f->f_inifile = (inifile ? inifile : &s_);
        clock_delay(f->f_panelclock, 0);
    }
}

void panel_setsavedir(t_elsefile *f, t_symbol *dir){
    if((f = f->f_savepanel))
        panel_setopendir(f, dir);
}

t_symbol *panel_getsavedir(t_elsefile *f){
    return(f->f_savepanel ? f->f_savepanel->f_currentdir : 0);
}

// embeddable  classes don't use 'saveto' method, but add a creation method
// to pd_canvasmaker -- then saving could be done with a 'proper' sequence:
//   #N <master> <args>; #X <whatever>; ...; #X restore <x> <y>;
// However, this works only for -lib externals.  So, we choose a sequence:
//   #X obj <x> <y> <master> <args>; #C <whatever>; ...; #C restore;
// Since the 1st message in the sequence is a valid creation message,
// we distinguish loading from a .pd file, and other cases (editing).
static void embed_gc(t_pd *x, t_symbol *s, int expected){
    t_pd *garbage;
    int count = 0;
    while((garbage = pd_findbyclass(s, *x)))
        pd_unbind(garbage, s), count++;
    if(count != expected)
	bug("embed_gc (%d garbage bindings)", count);
}

static void embed_restore(t_pd *master){
    embed_gc(master, ps__C, 1);
}

void embed_save(t_gobj *master, t_binbuf *bb){
    t_elsefile *f = elsefile_getproxy((t_pd *)master);
    t_text *t = (t_text *)master;
    binbuf_addv(bb, "ssii", &s__X, gensym("obj"), (int)t->te_xpix, (int)t->te_ypix);
    binbuf_addbinbuf(bb, t->te_binbuf);
    binbuf_addsemi(bb);
    if(f && f->f_embedfn)
        (*f->f_embedfn)(f->f_master, bb, ps__C);
    binbuf_addv(bb, "ss;", ps__C, gensym("restore"));
}

int elsefile_ismapped(t_elsefile *f){
    return(f->f_canvas->gl_mapped);
}

int elsefile_isloading(t_elsefile *f){
    return(f->f_canvas->gl_loading);
}

int elsefile_ispasting(t_elsefile *f){ // LATER find a better way
    int result = 0;
    t_canvas *cv = f->f_canvas;
    if(!cv->gl_loading){
        t_pd *z = s__X.s_thing;
        if(z == (t_pd *)cv){
            pd_popsym(z);
            if(s__X.s_thing == (t_pd *)cv) result = 1;
            pd_pushsym(z);
        }
        else if(z)
            result = 1;
    }
    return(result);
}

void elsefile_free(t_elsefile *f){
    t_elsefile *prev, *next;
    else_editor_close(f, 0);
    if(f->f_embedfn) // in case of missing 'restore'
        embed_gc(f->f_master, ps__C, 0);
    if(f->f_savepanel){
        pd_unbind((t_pd *)f->f_savepanel, f->f_savepanel->f_bindname);
        pd_free((t_pd *)f->f_savepanel);
    }
    if(f->f_bindname)
        pd_unbind((t_pd *)f, f->f_bindname);
    if(f->f_panelclock)
        clock_free(f->f_panelclock);
    if(f->f_editorclock)
        clock_free(f->f_editorclock);
    for(prev = 0, next = elsefile_proxies; next; prev = next, next = next->f_next)
        if(next == f)
            break;
    if(prev)
        prev->f_next = f->f_next;
    else if(f == elsefile_proxies)
        elsefile_proxies = f->f_next;
    pd_free((t_pd *)f);
}

t_elsefile *elsefile_new(t_pd *master, t_embedfn embedfn, t_elsefilefn readfn,
t_elsefilefn writefn, t_elsefilefn updatefn){
    t_elsefile *result = (t_elsefile *)pd_new(elsefile_class);
    result->f_master = master;
    result->f_next = elsefile_proxies;
    elsefile_proxies = result;
    if(!(result->f_canvas = canvas_getcurrent())){
        bug("elsefile_new: out of context");
        return(result);
    }
    if((result->f_embedfn = embedfn)){ // embedding
        embed_gc(master, ps__C, 0); // in case of missing 'restore'
        if(elsefile_isloading(result) || elsefile_ispasting(result))
            pd_bind(master, ps__C);
    }
    if(readfn || writefn){ // panels
        t_elsefile *f;
        char buf[64];
        sprintf(buf, "ELSEFILE.%lx", (unsigned long)result);
        result->f_bindname = gensym(buf);
        pd_bind((t_pd *)result, result->f_bindname);
        result->f_currentdir = result->f_inidir = canvas_getdir(result->f_canvas);
        result->f_panelfn = readfn;
        result->f_panelclock = clock_new(result, (t_method)panel_tick);
        f = (t_elsefile *)pd_new(elsefile_class);
        f->f_master = master;
        f->f_canvas = result->f_canvas;
        sprintf(buf, "ELSEFILE.%lx", (unsigned long)f);
        f->f_bindname = gensym(buf);
        pd_bind((t_pd *)f, f->f_bindname);
        f->f_currentdir = f->f_inidir = result->f_currentdir;
        f->f_panelfn = writefn;
        f->f_panelclock = clock_new(f, (t_method)panel_tick);
        result->f_savepanel = f;
    }
    else
        result->f_savepanel = 0;
    if((result->f_editorfn = updatefn)){ // Text editor
        result->f_editorclock = clock_new(result, (t_method )else_editor_tick);
        if(!result->f_bindname){
            char buf[64];
            sprintf(buf, "ELSEFILE.%lx", (unsigned long)result);
            result->f_bindname = gensym(buf);
            pd_bind((t_pd *)result, result->f_bindname);
        }
    }
    return(result);
}

void elsefile_setup(t_class *c, int embeddable){
    if(embeddable){
        class_setsavefn(c, embed_save);
        class_addmethod(c, (t_method)embed_restore, gensym("restore"), 0);
    }
    if(!elsefile_class){
        ps__C = gensym("#C");
        elsefile_class = class_new(gensym("_elsefile"), 0, 0,sizeof(t_elsefile), CLASS_PD | CLASS_NOINLET, 0);
        class_addmethod(elsefile_class, (t_method)panel_path,gensym("path"), A_SYMBOL, A_DEFSYM, 0);
        class_addmethod(elsefile_class, (t_method)else_editor_clear, gensym("clear"), 0);
        class_addmethod(elsefile_class, (t_method)else_editor_addline, gensym("addline"), A_GIMME, 0);
        class_addmethod(elsefile_class, (t_method)else_editor_end, gensym("end"), 0);
// LATER find a way of ensuring these are not defined yet
        elsefile_text_window_editor_guidefs();
        elsefile_panel_guidefs();
    }
}
