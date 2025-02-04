
#include <m_pd.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

static t_class *sfinfo_class;

typedef struct _sfinfo {
    t_object        x_obj;
    t_outlet       *x_info_outlet;
    AVFormatContext *x_ic;
    t_canvas       *x_canvas;
    char            x_path[MAXPDSTRING];
    t_atom          x_info[1];
    int             x_opened;
}t_sfinfo;

void* sfinfo_nchs(t_sfinfo *x){
    if(!x->x_opened){
        pd_error(x, "[sfinfo]: No file loaded");
        return(NULL);
    }
    x->x_ic = avformat_alloc_context();
    if(avformat_open_input(&x->x_ic, x->x_path, NULL, NULL) != 0){
        pd_error(x, "[sfinfo]: Could not open file '%s'", x->x_path);
        return(NULL);
    }
    if (avformat_find_stream_info(x->x_ic, NULL) < 0) {
        pd_error(x, "[sfinfo]: Could not find stream information");
        avformat_close_input(&x->x_ic);
        return(NULL);
    }
    int audio_stream_index = -1;
    for(unsigned int i = 0; i < x->x_ic->nb_streams; i++){
        if(x->x_ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audio_stream_index = i;
            break;
        }
    }
    if(audio_stream_index == -1){
        pd_error(x, "[sfinfo]: Could not find any audio stream in the file");
        avformat_close_input(&x->x_ic);
        return(NULL);
    }
    AVStream *audio_stream = x->x_ic->streams[audio_stream_index];
    // Use AVChannelLayout to determine the number of channels
    AVChannelLayout layout = {0}; // Initialize the layout
    if(audio_stream->codecpar->ch_layout.u.mask)
        av_channel_layout_from_mask(&layout, audio_stream->codecpar->ch_layout.u.mask);
    else
        av_channel_layout_default(&layout, audio_stream->codecpar->ch_layout.nb_channels);
    unsigned int num_channels = layout.nb_channels;
    // Clean up
    av_channel_layout_uninit(&layout);
    avformat_close_input(&x->x_ic);
    // Output the number of channels
    SETFLOAT(x->x_info + 0, (t_float)num_channels);
    outlet_anything(x->x_info_outlet, gensym("channels"), 1, x->x_info);
    return(NULL);
}

static int sfinfo_find_file(t_sfinfo *x, t_symbol *file, char *dir_out) {
    static char fname[MAXPDSTRING];
    char *bufptr;
    int fd = canvas_open(x->x_canvas, file->s_name, "", fname, &bufptr, MAXPDSTRING, 1);
    if(fd < 0){
        pd_error(x, "[sfinfo] file '%s' not found", file->s_name);
        return(0);
    }
    else if(bufptr > fname){
        bufptr[-1] = '/';
        strcpy(dir_out, fname);
    }
    return(1);
}

void sfinfo_read(t_sfinfo* x, t_symbol* file){
    x->x_opened = sfinfo_find_file(x, file, x->x_path);
}

static void sfinfo_free(t_sfinfo *x) {
    if(x->x_ic)
        avformat_close_input(&x->x_ic);
}

static void *sfinfo_new(t_symbol *s, int ac, t_atom *av) {
    t_sfinfo *x = (t_sfinfo *)pd_new(sfinfo_class);
    x->x_ic = NULL;
    x->x_canvas = canvas_getcurrent();
    x->x_info_outlet = outlet_new(&x->x_obj, &s_list);
    return(x);
}

void sfinfo_setup(void) {
    sfinfo_class = class_new(gensym("sfinfo"), (t_newmethod)sfinfo_new,
        (t_method)sfinfo_free, sizeof(t_sfinfo), 0, A_GIMME, 0);
    class_addmethod(sfinfo_class, (t_method)sfinfo_read, gensym("read"), A_SYMBOL, 0);
    class_addmethod(sfinfo_class, (t_method)sfinfo_nchs, gensym("channels"), 0);
}
