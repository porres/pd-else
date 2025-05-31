// stream via icecast2
// For now, an overhaul of Olaf Matthes' [oggcast~], deeply revised, structurally changed and modernized
// in the process of adapting to use ffmpeg instead and support many codeds/options

#include "m_pd.h"
#include "g_canvas.h"

/*
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#include <libavutil/error.h>  // for av_strerror()*/

#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#ifdef WIN32
# include <io.h>	// for 'write' in pute-function only
# include <winsock.h>
# include <winbase.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <sys/time.h>
# include <unistd.h>
# define SOCKET_ERROR -1
#endif

#ifdef _MSC_VER
# pragma warning( disable : 4244 )
# pragma warning( disable : 4305 )
#endif

#ifdef WIN32
# define     sys_closesocket closesocket
# define     pdogg_strdup(s) _strdup(s)
#else
# define     sys_closesocket close
# define     pdogg_strdup(s) strdup(s)
#endif

/************************* streamout~ object ******************************/

/* Each instance of streamout~ owns a "child" thread for doing the data
transfer.  The parent thread signals the child each time:
    (1) a connection wants opening or closing;
    (2) we've eaten another 1/16 of the shared buffer (so that the
    	child thread should check if it's time to receive some more.)
The child signals the parent whenever a receive has completed.  Signalling
is done by setting "conditions" and putting data in mutex-controlled common
areas.
*/

#define     REQUEST_NOTHING 0
#define     REQUEST_CONNECT 1
#define     REQUEST_CLOSE 2
#define     REQUEST_QUIT 3
#define     REQUEST_BUSY 4
#define     REQUEST_DATA 5
#define     REQUEST_REINIT 6

#define     STATE_IDLE 0
#define     STATE_STARTUP 1             // connecting and filling the buffer
#define     STATE_STREAM 2              // streaming aund audio output

#define     READ             4096       // amount of data we pass on to encoder
#define     DEFBUFPERCHAN    131072     // 262144  // default output buffer (128k used to be 256k)
#define     MINBUFSIZE       65536
#define     MAXBUFSIZE       16777216   // arbitrary; just don't want to hang malloc
#define     STRBUF_SIZE      1024       // char received from server on startup
#define		MAXSTREAMCHANS   256        // maximum number of channels: restricted by Pd?
#define     UPDATE_INTERVAL  250        // time in milliseconds between updates of output values

#ifdef __linux__	// 'real' linux only, not for OS X !
#define SEND_OPT MSG_DONTWAIT|MSG_NOSIGNAL
#else
#define SEND_OPT 0
#endif

static t_class *streamout_class;

typedef struct _streamout{
    t_object x_obj;
    t_float x_f;
    t_float x_unused;
    t_symbol *x_ignore;
    t_clock *x_clock_connect;
    t_clock *x_clock_pages;
    t_outlet *x_connection;    // outlet for connection state
    t_outlet *x_info;	       // outlet for no. of ogg pages

    t_float *x_buf;    	    	    	    // audio data buffer
    t_int x_bufsize;  	    	    	    // buffer size in bytes
    t_int x_ninlets; 	    	    	    // number of audio outlets
    t_sample **x_outvec;	// audio vectors
    t_int x_vecsize;  	    	    	    // vector size for transfers
    t_int x_state;    	    	    	    // opened, running, or idle
    	// parameters to communicate with subthread
    t_int x_requestcode;	   // pending request from parent to I/O thread
    t_int x_connecterror;	   // slot for "errno" return
		// buffer stuff
    t_int x_fifosize; 	       // buffer size appropriately rounded down
    t_int x_fifohead; 	       // index of next byte to get from file
    t_int x_fifotail; 	       // index of next byte the ugen will read
    t_int x_sigcountdown;      // counter for signalling child for more data
    t_int x_sigperiod;	       // number of ticks per signal
	t_int x_siginterval;       // number of times per buffer (depends on data rate)
		// ogg/vorbis related stuff
	ogg_stream_state x_os;    // take physical pages, weld into a logical stream of packets
	ogg_page         x_og;    // one Ogg bitstream page.  Vorbis packets are inside
	ogg_packet       x_op;    // one raw packet of data for decode
	vorbis_info      x_vi;    // struct that stores all the static vorbis bitstream settings
	vorbis_comment   x_vc;    // struct that stores all the user comments
	vorbis_dsp_state x_vd;    // central working state for the packet->PCM decoder
	vorbis_block     x_vb;    // local working space for packet->PCM decode
	t_int            x_eos;   // end of stream
	t_float   x_pages;        // number of pages that have been output to server
	t_float   x_lastpages;
        // ringbuffer stuff
    t_float *x_buffer;        // data to be buffered (ringbuffer)
    t_int    x_bytesbuffered; // number of unprocessed bytes in buffer
        // ogg/vorbis format stuff
    t_int    x_samplerate;    // samplerate of stream (default = getsr() )
	t_int    x_skip;          // samples from input to skip (for resampling)
	t_float  x_quality;       // desired quality level from 0.0 to 1.0 (lo to hi)
    t_int    x_br_max;        // max. bitrate of ogg/vorbis stream
    t_int    x_br_nom;        // nom. bitrate of ogg/vorbis stream
    t_int    x_br_min;        // min. bitrate of ogg/vorbis stream
    t_int    x_channels;      // number of channels (1 or 2)
	t_int    x_vbr;
        // IceCast server stuff
    char*    x_passwd;        // server password
    char*    x_bcname;        // stream name
    char*    x_bcurl;         // stream url
    char*    x_bcgenre;       // stream genre
	char*    x_bcdescription; // stream description
	char*    x_bcartist;      // stream artist
	char*    x_bclocation;
	char*    x_bccopyright;
	char*    x_bcperformer;
	char*    x_bccontact;
    
	char*    x_bcdate;        // system date when broadcast started
	char*    x_hostname;      // name or IP of host to connect to
	char*    x_mountpoint;    // mountpoint for IceCast server
	t_float  x_port;          // port number on which the connection is made
    t_int    x_bcpublic;      // do(n't) publish broadcast on www.oggcast.com

	t_int    x_connectstate;   // indicates to state of socket connection
	t_int    x_outvalue;       // value that has last been output via outlet
    t_int    x_fd;             // the socket number
	t_resample    x_resample;  // resampling unit
	t_int    x_recover;        // indicate how to behave on buffer underruns
		// thread stuff
    pthread_mutex_t   x_mutex;
    pthread_cond_t    x_requestcondition;
    pthread_cond_t    x_answercondition;
    pthread_t         x_childthread;
}t_streamout;

static char base64table[65] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
    'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
    'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/',
};

// This isn't efficient, but it doesn't need to be
// for encoding stuff like authentication credentials/metadata when communicating with the server

char *streamout_util_base64_encode(char *data){
	int len = strlen(data);
	char *out = (char *)getbytes(len*4/3 + 4);
	char *result = out;
	int chunk;
	while(len > 0){
		chunk = (len >3)?3:len;
		*out++ = base64table[(*data & 0xFC)>>2];
		*out++ = base64table[((*data & 0x03)<<4) | ((*(data+1) & 0xF0) >> 4)];
		switch(chunk){
            case 3:
                *out++ = base64table[((*(data+1) & 0x0F)<<2) | ((*(data+2) & 0xC0)>>6)];
                *out++ = base64table[(*(data+2)) & 0x3F];
                break;
            case 2:
                *out++ = base64table[((*(data+1) & 0x0F)<<2)];
                *out++ = '=';
                break;
            case 1:
                *out++ = '=';
                *out++ = '=';
                break;
		}
		data += chunk;
		len -= chunk;
	}
	*out = 0;
	return(result);
}

	// check server for writeability
static int streamout_checkserver(t_int sock){
    struct timeval  ztout;
	fd_set writeset;
	fd_set exceptset;
	FD_ZERO(&writeset);
	FD_ZERO(&exceptset);
	FD_SET(sock, &writeset );
	FD_SET(sock, &exceptset );
	if(select(sock+1, NULL, &writeset, &exceptset, &ztout) > 0){
		if(!FD_ISSET(sock, &writeset)){
			post("[streamout~]: can not write data to the server, quitting");
			return(-1);
		}
        if(FD_ISSET(sock, &exceptset)){
			post("[streamout~]: socket returned an error, quitting");
			return(-1);
		}
	}
	return(0);
}

static int safe_send(int fd, const char *buf, size_t len, int flags) {
    size_t total_sent = 0;
    while(total_sent < len){
        ssize_t sent = send(fd, buf + total_sent, len - total_sent, flags);
        if(sent < 0){
            perror("safe_send: send error");
            return -1;
        }
        if(sent == 0){
            fprintf(stderr, "safe_send: send returned 0 (socket closed)\n");
            return -1;
        }
        total_sent += sent;
    }
    return((int)total_sent);
}

// stream ogg/vorbis to IceCast2 server using chunked encoding
static int streamout_stream(t_streamout *x, t_int fd){
    int err = -1;            // error return code
    int pages = 0;
    char chunk_header[32];   // buffer for chunk size header
    char chunk_footer[] = "\r\n";
    size_t total_chunk_size;
    // write out pages (if any)
    while(!x->x_eos){
        int result = ogg_stream_pageout(&(x->x_os), &(x->x_og));
        if(result == 0)
            break;
        // Calculate total size of this chunk (header + body)
        total_chunk_size = x->x_og.header_len + x->x_og.body_len;
        
        // Send chunk size in hexadecimal format
        if(sprintf(chunk_header, "%zx\r\n", total_chunk_size) == -1) {
            pd_error(x, "[streamout~]: error formatting chunk header");
            x->x_eos = 1;
            return -1;
        }
        err = safe_send(fd, chunk_header, strlen(chunk_header), SEND_OPT);
        if(err < 0){
            pd_error(x, "[streamout~]: could not send chunk header to server (%d)", err);
            x->x_eos = 1;
            return err;
        }
        // Send ogg header
        err = safe_send(fd, (const char *)x->x_og.header, x->x_og.header_len, SEND_OPT);
        if(err < 0){
            pd_error(x, "[streamout~]: could not send ogg header to server (%d)", err);
            x->x_eos = 1;
            return err;
        }
        // Send ogg body
        err = safe_send(fd, (const char *)x->x_og.body, x->x_og.body_len, SEND_OPT);
        if(err < 0){
            pd_error(x, "[streamout~]: could not send ogg body to server (%d)", err);
            x->x_eos = 1;
            return err;
        }
        // Send chunk footer (CRLF after each chunk)
        err = safe_send(fd, chunk_footer, strlen(chunk_footer), SEND_OPT);
        if(err < 0){
            pd_error(x, "[streamout~]: could not send chunk footer to server (%d)", err);
            x->x_eos = 1;
            return err;
        }
        pages++; // count number of pages
        // there might be more than one pages we have to send
        if(ogg_page_eos(&(x->x_og)))
            x->x_eos = 1;
    }
    return(pages);
}

// Call this function when closing the stream to properly end chunked encoding
static int streamout_close_chunked_stream(t_int fd){
    // Send final chunk (size 0) to indicate end of stream
    const char final_chunk[] = "0\r\n\r\n";
    int err = safe_send(fd, final_chunk, strlen(final_chunk), SEND_OPT);
    if(err < 0)
        return(err);
    return(0);
}

// ENCODING /////////////////////////////////////////////////////////////////////////////////

// initialize ogg/vorbis ecoding

static int streamout_start_ogg_encoding(t_streamout *x){
/*    // create an "output format context" in ogg
    AVFormatContext *fmt_ctx = NULL; // fmt_ctx will hold the whole output stream setup
    const char *oggfilename = "pd.ogg";
    // use the Ogg container
    int err = avformat_alloc_output_context2(&fmt_ctx, NULL, "ogg", oggfilename);
    if((0) && (fmt_ctx && err >= 0)) // verbose
        post(" created an output format context in ogg successfully");
    else{
        // handle error
    }*/
    
    x->x_eos = 0;
    x->x_skip = 1;  // assume no resampling
    
    // Initialize vorbis info
    vorbis_info_init(&(x->x_vi));
    
    // Handle sample rate conversion
    if(x->x_samplerate != sys_getsr()){
        float sr_ratio = sys_getsr() / (float)x->x_samplerate;
        // Check for supported downsampling ratios
        if(sr_ratio == 2.0f) {
            post("[streamout~]: downsampling from %.0f to %d Hz", sys_getsr(), x->x_samplerate);
            x->x_skip = 2;
        } else if(sr_ratio == 3.0f) {
            post("[streamout~]: downsampling from %.0f to %d Hz", sys_getsr(), x->x_samplerate);
            x->x_skip = 3;
        } else if(sr_ratio == 4.0f) {
            post("[streamout~]: downsampling from %.0f to %d Hz", sys_getsr(), x->x_samplerate);
            x->x_skip = 4;
        } else {
            post("[streamout~]: warning: resampling from %.0f to %d not supported",
                 sys_getsr(), x->x_samplerate);
        }
    }
    // Modern Vorbis encoder setup
    int ret;
    if(x->x_vbr == 1){
        // Quality-based VBR encoding (modern approach)
        ret = vorbis_encode_setup_vbr(&(x->x_vi), x->x_channels, x->x_samplerate, x->x_quality);
        if(ret != 0){
            pd_error(x, "[streamout~]: vorbis VBR setup failed with code %d", ret);
            goto cleanup_info;
        }
    }
    else{ // Bitrate-based encoding (modern approach)
        ret = vorbis_encode_setup_managed(&(x->x_vi), x->x_channels, x->x_samplerate,
            x->x_br_max * 1000, x->x_br_nom * 1000, x->x_br_min * 1000);
        if(ret != 0){
            pd_error(x, "[streamout~]: vorbis managed setup failed with code %d", ret);
            goto cleanup_info;
        }
    }
    // Finalize encoder setup
    ret = vorbis_encode_setup_init(&(x->x_vi));
    if(ret != 0){
        pd_error(x, "[streamout~]: vorbis encoder init failed with code %d", ret);
        goto cleanup_info;
    }
    
    // Initialize comment structure
    vorbis_comment_init(&(x->x_vc));
    // Add metadata tags (check for null strings to avoid crashes)
    if(x->x_bcname && strlen(x->x_bcname) > 0)
        vorbis_comment_add_tag(&(x->x_vc), "TITLE", x->x_bcname);
    if(x->x_bcartist && strlen(x->x_bcartist) > 0)
        vorbis_comment_add_tag(&(x->x_vc), "ARTIST", x->x_bcartist);
    if(x->x_bcgenre && strlen(x->x_bcgenre) > 0)
        vorbis_comment_add_tag(&(x->x_vc), "GENRE", x->x_bcgenre);
    if(x->x_bcdescription && strlen(x->x_bcdescription) > 0)
        vorbis_comment_add_tag(&(x->x_vc), "DESCRIPTION", x->x_bcdescription);
    if(x->x_bclocation && strlen(x->x_bclocation) > 0)
        vorbis_comment_add_tag(&(x->x_vc), "LOCATION", x->x_bclocation);
    if(x->x_bcperformer && strlen(x->x_bcperformer) > 0)
        vorbis_comment_add_tag(&(x->x_vc), "PERFORMER", x->x_bcperformer);
    if(x->x_bccopyright && strlen(x->x_bccopyright) > 0)
        vorbis_comment_add_tag(&(x->x_vc), "COPYRIGHT", x->x_bccopyright);
    if(x->x_bccontact && strlen(x->x_bccontact) > 0)
        vorbis_comment_add_tag(&(x->x_vc), "CONTACT", x->x_bccontact);
    if(x->x_bcdate && strlen(x->x_bcdate) > 0)
        vorbis_comment_add_tag(&(x->x_vc), "DATE", x->x_bcdate);
    vorbis_comment_add_tag(&(x->x_vc), "ENCODER", "[streamout~]");
    
    // Initialize analysis state and encoding blocks
    ret = vorbis_analysis_init(&(x->x_vd), &(x->x_vi));
    if(ret != 0) {
        pd_error(x, "[streamout~]: vorbis analysis init failed");
        goto cleanup_comment;
    }
    ret = vorbis_block_init(&(x->x_vd), &(x->x_vb));
    if(ret != 0) {
        pd_error(x, "[streamout~]: vorbis block init failed");
        goto cleanup_analysis;
    }
    // Initialize OGG stream with better random seed
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
    ogg_stream_init(&(x->x_os), rand());
    
    // Generate and send headers
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;
    
    vorbis_analysis_headerout(&(x->x_vd), &(x->x_vc), &header, &header_comm, &header_code);
    ogg_stream_packetin(&(x->x_os), &header);
    ogg_stream_packetin(&(x->x_os), &header_comm);
    ogg_stream_packetin(&(x->x_os), &header_code);
    
    // Flush headers to server
    while (!x->x_eos) {
        int result = ogg_stream_flush(&(x->x_os), &(x->x_og));
        if(result == 0) break;
        
        ssize_t sent = safe_send(x->x_fd, (const char *)x->x_og.header, x->x_og.header_len, SEND_OPT);
        if(sent < 0 || sent != (ssize_t)x->x_og.header_len) {
            pd_error(x, "[streamout~]: failed to send ogg header to server (%zd/%ld)",
                    sent, x->x_og.header_len);
            goto cleanup_stream;
        }
        sent = safe_send(x->x_fd, (const char *)x->x_og.body, x->x_og.body_len, SEND_OPT);
        if(sent < 0 || sent != (ssize_t)x->x_og.body_len) {
            pd_error(x, "[streamout~]: failed to send ogg body to server (%zd/%ld)",
                    sent, x->x_og.body_len);
            goto cleanup_stream;
        }
    }
    return(0);
// Error cleanup path
cleanup_stream:
    ogg_stream_clear(&(x->x_os));
    vorbis_block_clear(&(x->x_vb));
cleanup_analysis:
    vorbis_dsp_clear(&(x->x_vd));
cleanup_comment:
    vorbis_comment_clear(&(x->x_vc));
cleanup_info:
    vorbis_info_clear(&(x->x_vi));
    x->x_eos = 1;
    return(-1);
}

// finish encoding
static void ifstreamout_finish_ogg_encoding(t_streamout *x){
	vorbis_analysis_wrote(&(x->x_vd),0);
		/* get rid of remaining data in encoder, if any */
	while(vorbis_analysis_blockout(&(x->x_vd),&(x->x_vb)) == 1){
		vorbis_analysis(&(x->x_vb),NULL);
		vorbis_bitrate_addblock(&(x->x_vb));
		while(vorbis_bitrate_flushpacket(&(x->x_vd),&(x->x_op))){
			ogg_stream_packetin(&(x->x_os),&(x->x_op));
			streamout_stream(x, x->x_fd);
		}
	} 
		/* clean up and exit.  vorbis_info_clear() must be called last */
	ogg_stream_clear(&(x->x_os));
	vorbis_block_clear(&(x->x_vb));
	vorbis_dsp_clear(&(x->x_vd));
	vorbis_comment_clear(&(x->x_vc));
	vorbis_info_clear(&(x->x_vi));
}

// encode ogg/vorbis and stream new data
static int streamout_encode(t_streamout *x, float *buf, int channels, int fifosize, int fd){
    x->x_unused = fifosize;
    unsigned short ch;
    int err = 0;
    int n, pages = 0;
		// expose the buffer to submit data
	float **inbuffer=vorbis_analysis_buffer(&(x->x_vd),READ * channels);
		// read from buffer
    for(n = 0; n < READ; n++){		             /* fill encode buffer */
		for(ch = 0; ch < channels; ch++)
			inbuffer[ch][n] = *buf++;
	}
		/* tell the library how much we actually submitted */
	vorbis_analysis_wrote(&(x->x_vd), n);
		/* vorbis does some data preanalysis, then divvies up blocks for
		   more involved (potentially parallel) processing.  Get a single
		   block for encoding now */
	while(vorbis_analysis_blockout(&(x->x_vd),&(x->x_vb)) == 1){
			/* analysis, assume we want to use bitrate management */
		vorbis_analysis(&(x->x_vb),NULL);
		vorbis_bitrate_addblock(&(x->x_vb));
		while(vorbis_bitrate_flushpacket(&(x->x_vd),&(x->x_op))){
				/* weld the packet into the bitstream */
			ogg_stream_packetin(&(x->x_os),&(x->x_op));
			err = streamout_stream(x, fd);	/* stream packet to server */
			if(err >= 0)
				pages += err;       /* count pages */
			else
                return(err);
		}
	}
	return(pages);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  
// connect to icecast2 server
static int streamout_child_connect(t_streamout *x, char *hostname, char *mountpoint, t_int portno,
char *passwd, char *bcname, char *bcgenre, char *bcdescription, char *bcurl, t_int bcpublic, t_int br_nom){
    struct          sockaddr_in server;
    struct          hostent *hp;
        /* variables used for communication with server */
    const char      * buf = 0;
    char            resp[STRBUF_SIZE];
//    unsigned int    len;
    fd_set          fdset;
    struct timeval  tv;
    t_int           sockfd;                         /* our internal handle for the socket */
    t_int           ret;
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0){
        post("[streamout~]: internal error while attempting to open socket");
        return (-1);
    }
        /* connect socket using hostname provided in command line */
    server.sin_family = AF_INET;
    hp = gethostbyname(hostname);
    if(hp == 0){
        post("[streamout~]: bad host?");
        streamout_close_chunked_stream(sockfd);
        sys_closesocket(sockfd);
        return(-1);
    }
    memcpy((char *)&server.sin_addr, (char *)hp->h_addr, hp->h_length);
        /* assign client port number */
    server.sin_port = htons((unsigned short)portno);
        /* try to connect.  */
    if(0) // verbose
        post("[streamout~]: connecting to port %d", portno);
    if(connect(sockfd, (struct sockaddr *) &server, sizeof (server)) < 0){
        post("[streamout~]: connection failed!\n");
        streamout_close_chunked_stream(sockfd);
        sys_closesocket(sockfd);
        return(-1);
    }
        /* sheck if we can read/write from/to the socket */
    FD_ZERO( &fdset);
    FD_SET( sockfd, &fdset);
    tv.tv_sec  = 0;            /* seconds */
    tv.tv_usec = 500;        /* microseconds */
    ret = select(sockfd + 1, &fdset, NULL, NULL, &tv);
    if(ret != 0){
        post("[streamout~]: can not read from socket");
        streamout_close_chunked_stream(sockfd);
        sys_closesocket(sockfd);
        return(-1);
    }
    if(0) // verbose
        post("[streamout~]: logging in to IceCast2 server");
    
    { // Modern Icecast2 server using HTTP/1.1 PUT with chunked encoding
        char auth_string[512];
        char *encoded_auth;
        char response[1024];
        int bytes_received;
        
        // Create authentication string (source:password)
        if(sprintf(auth_string, "source:%s", passwd) == -1) {
            post("[streamout~]: error creating auth string");
            return -1; // or however you handle errors in your code
        }
        encoded_auth = streamout_util_base64_encode(auth_string);
        
        // Send HTTP PUT request line
        buf = "PUT /";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = mountpoint;
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = " HTTP/1.1\r\n";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // Send Host header (you'll need to pass hostname/port to this function)
        // For now, assuming you have a hostname variable or can add it
        sprintf(resp, "Host: %s:%ld\r\n", hostname, portno); // You'll need these variables
        if(safe_send(sockfd, resp, strlen(resp), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // Send authorization header
        sprintf(resp, "Authorization: Basic %s\r\n", encoded_auth);
        if(safe_send(sockfd, resp, strlen(resp), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // Send User-Agent
        buf = "User-Agent: PD-streamout/2.0\r\n";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // Send content type (corrected for modern standards)
        buf = "Content-Type: audio/ogg\r\n"; // or "audio/mpeg" for MP3
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        
        // Enable chunked encoding
        buf = "Transfer-Encoding: chunked\r\n";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // Send connection keep-alive
        buf = "Connection: keep-alive\r\n";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // Send ice headers (no ice-password needed with proper HTTP auth)
        // name
        buf = "Ice-Name: ";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = bcname;
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = "\r\n";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // url
        buf = "Ice-URL: ";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = bcurl;
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = "\r\n";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // genre
        buf = "Ice-Genre: ";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = bcgenre;
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = "\r\n";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // description
        buf = "Ice-Description: ";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = bcdescription;
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = "\r\n";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // public
        buf = "Ice-Public: ";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        if(sprintf(resp, "%ld", bcpublic) == -1)
            post("[streamout~]: wrong public flag");
        if(safe_send(sockfd, resp, strlen(resp), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = "\r\n";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // bitrate
        buf = "Ice-Bitrate: ";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        if(sprintf(resp, "%ld", br_nom) == -1)
            post("[streamout~]: wrong bitrate");
        if(safe_send(sockfd, resp, strlen(resp), SEND_OPT) < 0)
            post("[streamout~]: send error");
        buf = "\r\n";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // End of headers
        buf = "\r\n";
        if(safe_send(sockfd, buf, strlen(buf), SEND_OPT) < 0)
            post("[streamout~]: send error");
        
        // Clean up encoded auth
        free(encoded_auth);
        
        // Wait for server response and check if connection was successful
        bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
        if(bytes_received > 0){
            response[bytes_received] = '\0';

            char http_version[16] = {0};
            int status_code = 0;
            char status_text[64] = {0};

            // Parse status line: e.g. "HTTP/1.1 200 OK"
            if(sscanf(response, "%15s %d %63[^\r\n]", http_version, &status_code, status_text) == 3){
                if(status_code == 200){
                    if(0){ // verbose
                        post("[streamout~]: Successfully connected to Icecast2 server");
                        post("  response = %s", response);
                    }
                }
                else if(status_code == 401){
                    pd_error(x, "[streamout~]: Unauthorized (401) - check username/password");
                    return -1;
                }
                else if(status_code >= 300 && status_code < 400){
                    pd_error(x, "[streamout~]: Redirected (%d) - streaming may not work", status_code);
                    return -1;
                }
                else {
                    pd_error(x, "[streamout~]: Server responded with error %d: %s", status_code, status_text);
                    return -1;
                }
            }
            else {
                pd_error(x, "[streamout~]: Could not parse server response: %.100s", response);
                return -1;
            }
        }
        else{
            post("[streamout~]: No response from server");
            // Handle error
            return -1;
        }
        // end login for IceCast2 using modern HTTP/1.1 scheme
    }
    // check if we can write to server
	if(streamout_checkserver(sockfd)!= 0){
		pd_error(x, "[streamout~]: error: server refused to receive data");
		return(-1);
	}
    if(0) // verbose
        post("[streamout~]: logged in to http://%s:%d/%s", hp->h_name, portno, mountpoint);
	return(sockfd);
}

static void streamout_child_disconnect(t_int fd){
    streamout_close_chunked_stream(fd);
    sys_closesocket(fd);
    if(0) // verbose
        post("[streamout~]: connection closed");
}
/************** the child thread which performs data I/O ***********/

#if 0			/* set this to 1 to get debugging output */
static void pute(char *s){   /* debug routine */
    write(2, s, strlen(s));
}
#else
#define pute(x)
#endif

#if 1
#define streamout_cond_wait pthread_cond_wait
#define streamout_cond_signal pthread_cond_signal
#else
#include <sys/time.h>    /* debugging version... */
#include <sys/types.h>

static void streamout_fakewait(pthread_mutex_t *b){
    struct timeval timout;
    timout.tv_sec = 0;
    timout.tv_usec = 1000000;
    pthread_mutex_unlock(b);
    select(0, 0, 0, 0, &timout);
    pthread_mutex_lock(b);
}

void streamout_banana( void){
    struct timeval timout;
    timout.tv_sec = 0;
    timout.tv_usec = 200000;
    pute("banana1\n");
    select(0, 0, 0, 0, &timout);
    pute("banana2\n");
}

#define streamout_cond_wait(a,b) streamout_fakewait(b)
#define streamout_cond_signal(a) 
#endif

static void *streamout_child_main(void *zz){
    t_streamout *x = zz;
	time_t         now; /* to get the time */
    pute("1\n");
    pthread_mutex_lock(&x->x_mutex);
    while(1){
    	int fd, fifotail;
		pute("0\n");
		if(x->x_requestcode == REQUEST_NOTHING){
            pute("wait 2\n");
			streamout_cond_signal(&x->x_answercondition);
			streamout_cond_wait(&x->x_requestcondition, &x->x_mutex);
            pute("3\n");
		}
		else if(x->x_requestcode == REQUEST_CONNECT){ // connect to Icecast2 server
    		char boo[100];
            int sysrtn;
           // int wantbytes;
			
            /* copy connect stuff out of the data structure so we can
			relinquish the mutex while we're connecting to server. */
			char *hostname = x->x_hostname;
			char *mountpoint = x->x_mountpoint;
			t_int portno = x->x_port;
			char *passwd = x->x_passwd;
			char *bcname = x->x_bcname;
			char *bcgenre = x->x_bcgenre;
            char *bcdescription = x->x_bcdescription;
			char *bcurl = x->x_bcurl;
			t_int bcpublic = x->x_bcpublic;
			t_int br_nom = x->x_br_nom;
            // alter the request code so that an ensuing "open" will get noticed.
    			pute("4\n");
			x->x_requestcode = REQUEST_BUSY;
			x->x_connecterror = 0;
            // open socket with mutex unlocked if we're not already connected
			if(x->x_fd < 0){
				pthread_mutex_unlock(&x->x_mutex);
				fd = streamout_child_connect(x, hostname, mountpoint, portno, passwd, bcname,
                    bcgenre, bcdescription, bcurl, bcpublic, br_nom);
				pthread_mutex_lock(&x->x_mutex);
    			pute("5\n");
    	    		/* copy back into the instance structure. */
				x->x_connectstate = 1;
				clock_delay(x->x_clock_connect, 0);
				x->x_fd = fd;
				if(fd < 0){
    	    		x->x_connecterror = fd;
					x->x_connectstate = 0;
					clock_delay(x->x_clock_connect, 0);
    	    		pute("connect failed\n");
					goto lost;
				}
				else{ // get the time for the DATE comment
					now = time(NULL);
					x->x_bcdate = pdogg_strdup(ctime(&now)); /*--moo*/
					x->x_pages = 0;
					clock_delay(x->x_clock_pages, 0);
						/* initialise the encoder */
					if(streamout_start_ogg_encoding(x) == 0){
                        if(0) // verbose
                            post("[streamout~]: ogg/vorbis encoder initialised");
						x->x_eos = 0;
						x->x_state = STATE_STREAM;
					}
					else{
						post("[streamout~]: could not init encoder");
						streamout_child_disconnect(fd);
						post("[streamout~]: connection closed due to initialisation error");
						x->x_fd = -1;
						x->x_connectstate = 0;
						clock_delay(x->x_clock_connect, 0);
    	    			pute("[streamout~]: initialisation failed\n");
						goto lost;
					} 
				}
    			x->x_fifotail = fifotail = 0;
	    			/* set fifosize from bufsize.  fifosize must be a
				multiple of the number of bytes eaten for each DSP
				tick.  We pessimistically assume MAXVECSIZE samples
				per tick since that could change.  There could be a
				problem here if the vector size increases while a
				stream is being played...  */
				x->x_fifosize = x->x_bufsize - (x->x_bufsize % (x->x_channels * READ));
					/* arrange for the "request" condition to be signalled x->x_siginterval
					times per buffer */
    			sprintf(boo, "fifosize %ld\n", x->x_fifosize);
    			pute(boo);
				x->x_sigcountdown = x->x_sigperiod = (x->x_fifosize / (x->x_siginterval * x->x_channels * x->x_vecsize));
			}
	    		/* check if another request has been made; if so, field it */
			if(x->x_requestcode != REQUEST_BUSY)
	    		goto lost;
    		pute("6\n");
			while(x->x_requestcode == REQUEST_BUSY){
	    		int fifosize = x->x_fifosize, channels;
				float *buf = x->x_buf;
    	    	pute("77\n");
				/* if the head is < the tail, we can immediately write
				from tail to end of fifo to disk; otherwise we hold off
				writing until there are at least WRITESIZE bytes in the
				buffer */
				if(x->x_fifohead < x->x_fifotail ||
					x->x_fifohead >= x->x_fifotail + (READ * x->x_channels)
					|| (x->x_requestcode == REQUEST_CLOSE &&
		    			x->x_fifohead != x->x_fifotail))
    	    	{	/* encode audio and send to server */
    	    		pute("8\n");
					fifotail = x->x_fifotail;
					channels = x->x_channels;
					fd = x->x_fd;
	    			pthread_mutex_unlock(&x->x_mutex);
					sysrtn = streamout_encode(x, buf + fifotail, channels, fifosize, fd);
	    			pthread_mutex_lock(&x->x_mutex);
					if(x->x_requestcode != REQUEST_BUSY &&
	    					x->x_requestcode != REQUEST_CLOSE)
		    				break;
					if(sysrtn < 0){
						post("[streamout~]: closing due to error...");
						goto lost;
					}
					else{
						x->x_fifotail += (READ * x->x_channels);
						x->x_pages += sysrtn;
						if(x->x_fifotail >= fifosize)
    	    	    				x->x_fifotail = 0;
    	    		}
    	    		sprintf(boo, "after: head %ld, tail %ld, pages %d\n", x->x_fifohead, x->x_fifotail, sysrtn);
					pute(boo);
				}
                else{	/* just wait... */
    	    		pute("wait 7a ...\n");
	    			streamout_cond_signal(&x->x_answercondition);
					pute("signalled\n");
					streamout_cond_wait(&x->x_requestcondition,
					&x->x_mutex);
    	    		pute("7a done\n");
					continue;
				}
				/* signal parent in case it's waiting for data */
				streamout_cond_signal(&x->x_answercondition);
			}
		}
			/* reinit encoder (settings have changed) */
		else if(x->x_requestcode == REQUEST_REINIT){
	    	pthread_mutex_unlock(&x->x_mutex);
			ifstreamout_finish_ogg_encoding(x);
    	    streamout_start_ogg_encoding(x);
   	    	pthread_mutex_lock(&x->x_mutex);
			post("[streamout~]: ogg/vorbis encoder reinitialised");
			x->x_state = STATE_STREAM;
			if(x->x_requestcode == REQUEST_REINIT)
	    		x->x_requestcode = REQUEST_CONNECT;
			streamout_cond_signal(&x->x_answercondition);
		}
			/* close connection to server (disconnect) */
		else if(x->x_requestcode == REQUEST_CLOSE){
lost:
			x->x_state = STATE_IDLE;
    		if(x->x_fd >= 0){
	    		fd = x->x_fd;
	    		pthread_mutex_unlock(&x->x_mutex);
				ifstreamout_finish_ogg_encoding(x);
    	    	streamout_child_disconnect(fd);
    	    	pthread_mutex_lock(&x->x_mutex);
	    		x->x_fd = -1;
			}
			if(x->x_requestcode == REQUEST_CLOSE)
	    		x->x_requestcode = REQUEST_NOTHING;
			if(x->x_requestcode == REQUEST_BUSY)	/* disconnect due to error */
	    		x->x_requestcode = REQUEST_NOTHING;
			x->x_connectstate = 0;
			clock_delay(x->x_clock_connect, 0);
			x->x_eos = 1;
			streamout_cond_signal(&x->x_answercondition);
		}
        // quit everything
		else if(x->x_requestcode == REQUEST_QUIT){
			x->x_state = STATE_IDLE;
			if(x->x_fd >= 0){
	    		fd = x->x_fd;
	    		pthread_mutex_unlock(&x->x_mutex);
				ifstreamout_finish_ogg_encoding(x);
    	    	streamout_child_disconnect(fd);
    	    	pthread_mutex_lock(&x->x_mutex);
				x->x_fd = -1;
			}
			x->x_connectstate = 0;
			clock_delay(x->x_clock_connect, 0);
			x->x_requestcode = REQUEST_NOTHING;
			streamout_cond_signal(&x->x_answercondition);
			break;
		}
		else
			pute("13\n");
    }
    pute("thread exit\n");
    pthread_mutex_unlock(&x->x_mutex);
    return(0);
}

/******** the object proper runs in the calling (parent) thread ****/
static void streamout_tick_connect(t_streamout *x){
	pthread_mutex_lock(&x->x_mutex);
	outlet_float(x->x_connection, x->x_connectstate);
	pthread_mutex_unlock(&x->x_mutex);
}

static void streamout_tick_pages(t_streamout *x){
		/* output new no. of pages if anything changed */
	t_float pages;
    pthread_mutex_lock(&x->x_mutex);
	pages = x->x_pages;	/* get current value with mutex locked */
    pthread_mutex_unlock(&x->x_mutex);
	if(pages != x->x_lastpages){
        t_atom at[1];
        SETFLOAT(at, pages);
		outlet_anything(x->x_info, gensym("oggpages"), 1, at);
		x->x_lastpages = pages;
	}
	clock_delay(x->x_clock_pages, UPDATE_INTERVAL);	/* come back again... */
}

static t_int *streamout_perform(t_int *w){
    t_streamout *x = (t_streamout *)(w[1]);
    int vecsize = x->x_vecsize, ninlets = x->x_ninlets;
    int channels = x->x_channels, i, j, skip = x->x_skip;
	float *sp = x->x_buf;
	pthread_mutex_lock(&x->x_mutex);
    if(x->x_state != STATE_IDLE){
    	int wantbytes;
			// get 'wantbytes' bytes from inlet
		wantbytes = channels * vecsize / skip;	// we'll get vecsize bytes per channel
			// check if there is enough space in buffer to write all samples
		while(x->x_fifotail > x->x_fifohead &&
        x->x_fifotail < x->x_fifohead + wantbytes + 1){
			pute("wait...\n");
			streamout_cond_signal(&x->x_requestcondition);
			streamout_cond_wait(&x->x_answercondition, &x->x_mutex);
			pute("done\n");
		}
        // output audio
		sp += x->x_fifohead;
		if(ninlets >= channels){
			for(j = 0; j < vecsize; j += skip){
				for(i = 0; i < channels; i++)
					*sp++ = x->x_outvec[i][j];
			}
		}
        else if(channels == ninlets * 2){	// convert mono -> stereo
			for(j = 0; j < vecsize; j += skip){
				for(i = 0; i < ninlets; i++){
					*sp++ = x->x_outvec[i][j];
					*sp++ = x->x_outvec[i][j];
				}
			}
		}
		x->x_fifohead += wantbytes;
		if(x->x_fifohead >= x->x_fifosize)
			x->x_fifohead = 0;
			/* signal the child thread */
		if((--x->x_sigcountdown) <= 0){
		    pute("signal 1\n");
    		streamout_cond_signal(&x->x_requestcondition);
			x->x_sigcountdown = x->x_sigperiod;
		}
    }
	pthread_mutex_unlock(&x->x_mutex);
    return(w+2);
}

static void streamout_dsp(t_streamout *x, t_signal **sp){
    int i, ninlets = x->x_ninlets;
    pthread_mutex_lock(&x->x_mutex);
    x->x_vecsize = sp[0]->s_n;
    x->x_sigperiod = (x->x_fifosize / (x->x_siginterval * ninlets * x->x_vecsize));
    for(i = 0; i < ninlets; i++)
    	x->x_outvec[i] = sp[i]->s_vec;
    pthread_mutex_unlock(&x->x_mutex);
    dsp_add(streamout_perform, 1, x);
}

static void streamout_disconnect(t_streamout *x){
    /* LATER rethink whether you need the mutex just to set a variable? */
    pthread_mutex_lock(&x->x_mutex);
    if(x->x_fd >= 0){
        x->x_state = STATE_IDLE;
        x->x_requestcode = REQUEST_CLOSE;
        streamout_cond_signal(&x->x_requestcondition);
    }
    else
        post("[streamout~]: not connected");
    pthread_mutex_unlock(&x->x_mutex);
}

// connect <hostname/IP> <portnumber>
static void streamout_connect(t_streamout *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    t_symbol *hostsym = gensym("x->x_hostname");
    t_float portno = x->x_port;
    if(ac){
        hostsym = atom_getsymbolarg(0, ac, av);
        if(ac > 1)
            portno = atom_getfloatarg(1, ac, av);
    }
    if(!*hostsym->s_name)    // check for hostname
        return;
    if(!portno)                // check wether the portnumber is specified
        portno = 8000;        // ...assume port 8000 as standard
    pthread_mutex_lock(&x->x_mutex);
    if(x->x_fd >= 0)
        post("[streamout~]: already connected");
    else{
        x->x_requestcode = REQUEST_CONNECT;
        x->x_hostname = (char *)hostsym->s_name;
        x->x_port = portno;
        x->x_fifotail = 0;
        x->x_fifohead = 0;
        // if(x->x_recover != 1)x->x_fifobytes = 0;
        x->x_connecterror = 0;
        x->x_state = STATE_STARTUP;
        streamout_cond_signal(&x->x_requestcondition);
    }
    pthread_mutex_unlock(&x->x_mutex);
}

static void streamout_float(t_streamout *x, t_floatarg f){
    if(f != 0){
        pthread_mutex_lock(&x->x_mutex);
        if(x->x_fd >= 0)
            post("[streamout~]: already connected");
        else{
            if(x->x_recover != 1){
                x->x_fifotail = 0;
                x->x_fifohead = 0;
            }
            x->x_requestcode = REQUEST_CONNECT;
            x->x_fifotail = 0;
            x->x_fifohead = 0;
            x->x_connecterror = 0;
            x->x_state = STATE_STARTUP;
            streamout_cond_signal(&x->x_requestcondition);
        }
        pthread_mutex_unlock(&x->x_mutex);
    }
    else
        streamout_disconnect(x);
}

// set mountpoint for IceCast server
static void streamout_mount(t_streamout *x, t_symbol *mount){
    x->x_mountpoint = (char *)mount->s_name;
}

// set password for streamout server
static void streamout_password(t_streamout *x, t_symbol *password){
    pthread_mutex_lock(&x->x_mutex);
    x->x_passwd = (char *)password->s_name;
    pthread_mutex_unlock(&x->x_mutex);
}

// set comment fields for header (reads in just anything)
static void streamout_set(t_streamout *x, t_symbol *s, t_int ac, t_atom* av){
	t_binbuf *b = binbuf_new();
	char* comment;
	int length;
	binbuf_add(b, ac-1, av+1);
	binbuf_gettext(b, &comment, &length);
    pthread_mutex_lock(&x->x_mutex);
    
    t_symbol *info = atom_getsymbol(av);
    if(info == gensym("title")){
            free(x->x_bcname);
        x->x_bcname = pdogg_strdup(comment);
        if(0) // verbose
            post("[streamout~]: TITLE = %s", x->x_bcname);
    }
    else if(info == gensym("artist")){
		if(x->x_bcartist) free(x->x_bcartist);
		x->x_bcartist = pdogg_strdup(comment); /*-- moo: added strdup() */
        if(0) // verbose
            post("[streamout~]: ARTIST = %s", x->x_bcartist);
	}
    else if(info == gensym("performer")){
            free(x->x_bcperformer);
        x->x_bcperformer = pdogg_strdup(comment);
        if(0) // verbose
            post("[streamout~]: PERFORMER = %s",x->x_bcperformer);
    }
    else if(info == gensym("descrition")){
            free(x->x_bcdescription);
        x->x_bcdescription = pdogg_strdup(comment);
        if(0) // verbose
            post("[streamout~]: DESCRIPTION = %s", x->x_bcdescription);
    }
	else if(info == gensym("genre")){
	        free(x->x_bcgenre);
		x->x_bcgenre = pdogg_strdup(comment);
        if(0) // verbose
            post("[streamout~]: GENRE = %s", x->x_bcgenre);
	}
    else if(info == gensym("url")){
            free(x->x_bcurl);
        x->x_bcurl = pdogg_strdup(comment);
        if(0) // verbose
            post("[streamout~]: URL = %s", x->x_bcurl);
    }
    else if(info == gensym("location")){
	        free(x->x_bclocation);
		x->x_bclocation = pdogg_strdup(comment);
        if(0) // verbose
            post("[streamout~]: LOCATION = %s", x->x_bclocation);
	}
    else if(info == gensym("contact")){
	        free(x->x_bccontact);
		x->x_bccontact = pdogg_strdup(comment);
        if(0) // verbose
            post("[streamout~]: CONTACT = %s", x->x_bccontact);
	}
/*    else if(info == gensym("date")){
	        free(x->x_bcdate);
		x->x_bcdate = pdogg_strdup(comment);
        if(0) // verbose
            post("[streamout~]: DATE = %s", x->x_bcdate);
	}*/
    else if(info == gensym("copyright")){
            free(x->x_bccopyright);
        x->x_bccopyright = pdogg_strdup(comment);
        if(0) // verbose
            post("[streamout~]: COPYRIGHT = %s", x->x_bccopyright);
    }
	else
        post("[streamout~]: no info method for %s", s->s_name);
	if(x->x_state == STATE_STREAM){
		x->x_state = STATE_IDLE;
		x->x_requestcode = REQUEST_REINIT;
		streamout_cond_signal(&x->x_requestcondition);
	}
    pthread_mutex_unlock(&x->x_mutex);
	freebytes(comment, strlen(comment));
	binbuf_free(b);
}

// set codec mode
static void streamout_codec(t_streamout *x, t_symbol *s, t_int ac, t_atom* av){
    if(!ac)
        return;
    x->x_ignore = s;
    t_symbol *codedtype = atom_getsymbol(av);
    float quality;
    int min, nom, max;
    if(codedtype == gensym("vorbis")){
        if(ac == 2){
            x->x_vbr = 1;
            quality = atom_getfloat(av+1);
            if(0) // verbose
                post("[streamout~]: codec: Vorbis VBR mode (quality %.2f)", quality);
        }
        else if(ac == 4){
            x->x_vbr = 0;
            min = atom_getint(av+1);
            nom = atom_getint(av+2);
            max = atom_getint(av+3);
            if(0) // verbose
                post("[streamout~]: codec: Vorbis Managed Bitrate mode (min %d / nom %d / max %d)",
                    min, nom, max);
        }
        else{
            post("[streamout~]: wrong number of arguments for 'vorbis' codec");
            return;
        }
    }
    else{
        post("[streamout~]: unsupported codec (%s)", codedtype->s_name);
        return;
    }
    pthread_mutex_lock(&x->x_mutex);
    if(x->x_vbr)
        x->x_quality = quality;
    else{
        x->x_br_min = min;
        x->x_br_nom = nom;
        x->x_br_max = max;
    }
    if(x->x_state == STATE_STREAM){
        x->x_state = STATE_IDLE;
        x->x_requestcode = REQUEST_REINIT;
        streamout_cond_signal(&x->x_requestcondition);
    }
    pthread_mutex_unlock(&x->x_mutex);
}

    // print into on console window
static void streamout_print(t_streamout *x){
    pthread_mutex_lock(&x->x_mutex);
    post("[streamout~] PRINT:");
    post("  server type is Icecast2");
    post("  mountpoint: %s", x->x_mountpoint);
	if(x->x_vbr == 1){
		post("  encoder: VBR / ch = %d / SR = %d / quality = %.2f", x->x_channels, x->x_samplerate, x->x_quality);
	}
	else{
		post("  encoder: CBR: ch = %d / SR = %d / bitrates: min %d, nom %d, max %d",
             x->x_channels, x->x_samplerate, x->x_br_min, x->x_br_nom, x->x_br_max);
	}
	post("Header:");
	post("    NAME = %s", x->x_bcname);
	post("    ARTIST = %s", x->x_bcartist);
	post("    PERFORMER = %s", x->x_bcperformer);
	post("    GENRE = %s", x->x_bcgenre);
	post("    LOCATION = %s", x->x_bclocation);
    post("    URL = %s", x->x_bcurl);
	post("    COPYRIGHT = %s", x->x_bccopyright);
	post("    CONTACT = %s", x->x_bccontact);
	post("    DESCRIPTION = %s", x->x_bcdescription);
	post("    DATE = %s", x->x_bcdate);
    pthread_mutex_unlock(&x->x_mutex);
}

static void streamout_free(t_streamout *x){
    // request QUIT and wait for acknowledge
    void *threadrtn;
    pthread_mutex_lock(&x->x_mutex);
    x->x_requestcode = REQUEST_QUIT;
    if(0) // verbose
        post("[streamout~]: stopping thread...");
    streamout_cond_signal(&x->x_requestcondition);
    while(x->x_requestcode != REQUEST_NOTHING){
        if(0) // verbose
            post("signalling...");
		streamout_cond_signal(&x->x_requestcondition);
    	streamout_cond_wait(&x->x_answercondition, &x->x_mutex);
    }
    pthread_mutex_unlock(&x->x_mutex);
    if(pthread_join(x->x_childthread, &threadrtn))
        pd_error(x, "streamout_free: join failed");
    if(0) // verbose
        post("... done.");
    pthread_cond_destroy(&x->x_requestcondition);
    pthread_cond_destroy(&x->x_answercondition);
    pthread_mutex_destroy(&x->x_mutex);
    freebytes(x->x_buf, x->x_bufsize*sizeof(t_float));
	freebytes(x->x_outvec, x->x_ninlets*sizeof(t_sample *));
	clock_free(x->x_clock_connect);
	clock_free(x->x_clock_pages);
	/*-- moo: free dynamically allocated comment strings --*/
	free(x->x_bcname);
	free(x->x_bcurl);
	free(x->x_bcgenre);
	free(x->x_bcdescription);
	free(x->x_bcartist);
	free(x->x_bclocation);
	free(x->x_bccopyright);
	free(x->x_bcperformer);
	free(x->x_bccontact);
	free(x->x_bcdate);
}

static void *streamout_new(t_floatarg fnchannels, t_floatarg fbufsize){
    t_streamout *x;
    int nchannels = fnchannels, bufsize = fbufsize * 1024, i;
    float *buf;
    if(nchannels < 1)
        nchannels = 2;        /* two channels as default */
    else if(nchannels > MAXSTREAMCHANS)
        nchannels = MAXSTREAMCHANS;
        /* check / set buffer size */
    if(bufsize <= 0)
        bufsize = DEFBUFPERCHAN * nchannels;
    else if(bufsize < MINBUFSIZE)
        bufsize = MINBUFSIZE;
    else if(bufsize > MAXBUFSIZE)
        bufsize = MAXBUFSIZE;
    buf = getbytes(bufsize*sizeof(t_float));
    if(!buf)
        return(0);
    x = (t_streamout *)pd_new(streamout_class);
    x->x_hostname = "localhost";
    x->x_port = 8000;
    for(i = 1; i < nchannels; i++)
        inlet_new (&x->x_obj, &x->x_obj.ob_pd, gensym ("signal"), gensym ("signal"));
    x->x_connection = outlet_new(&x->x_obj, gensym("float"));
    x->x_info = outlet_new(&x->x_obj, &s_symbol);
    x->x_ninlets = nchannels;
    x->x_outvec = getbytes(nchannels*sizeof(t_sample *));
    x->x_clock_connect = clock_new(x, (t_method)streamout_tick_connect);
    x->x_clock_pages = clock_new(x, (t_method)streamout_tick_pages);
    pthread_mutex_init(&x->x_mutex, 0);
    pthread_cond_init(&x->x_requestcondition, 0);
    pthread_cond_init(&x->x_answercondition, 0);
    x->x_vecsize = 2;
    x->x_state = STATE_IDLE;
    x->x_buf = buf;
    x->x_bufsize = bufsize;
    x->x_siginterval = 32;  /* signal 32 times per buffer */
                            /* I found this to be most efficient on my machine */
    x->x_fifosize = x->x_fifohead = x->x_fifotail = x->x_requestcode = 0;
    x->x_connectstate = 0;  /* indicating state of connection */
    x->x_outvalue = 0;      /* value at output currently is 0 */
    x->x_samplerate = sys_getsr();  // default to Pd's sampling rate
    x->x_resample.upsample = x->x_resample.downsample = 1;   // don't resample
    x->x_fd = -1;
    x->x_eos = 0;
    x->x_vbr = 1;                   // use the vbr setting by default
    x->x_skip = 1;                  // no resampling supported
    x->x_quality = 0.4;             // quality 0.4 gives roughly 128kbps VBR stream
    x->x_channels = nchannels;
    x->x_br_min = 96;
    x->x_br_nom = 128;
    x->x_br_max = 192;
    x->x_pages = x->x_lastpages = 0;
    x->x_passwd = "hackme";
    x->x_bcname = pdogg_strdup("live streaming patch");
    x->x_bcurl = pdogg_strdup("http://www.puredata.info/");
    x->x_bcgenre = pdogg_strdup("");
    x->x_bcdescription = pdogg_strdup("Pd stream with [streamout~]");
    x->x_bcartist = pdogg_strdup("");
    x->x_bclocation = pdogg_strdup("");
    x->x_bccopyright = pdogg_strdup("");
    x->x_bcperformer = pdogg_strdup("");
    x->x_bccontact = pdogg_strdup("");
    x->x_bcdate = pdogg_strdup("");
    x->x_bcpublic = 1;
    x->x_mountpoint = "pd.ogg";
    if(0){ // verbose
        post("[streamout~]: set buffer to %dk bytes", bufsize / 1024);
        post("[streamout~]: encoding %d chans / %d Hz", x->x_channels, x->x_samplerate);
    }
    clock_delay(x->x_clock_pages, 0);
    pthread_create(&x->x_childthread, 0, streamout_child_main, x);
    
/*    post("[streamout~] FFmpeg version: %s", av_version_info());
    post("FFmpeg configured with: %s", avcodec_configuration());
    post("libavcodec version: %u", avcodec_version());
    post("libavformat version: %u", avformat_version());
    post("libavutil version: %u", avutil_version());
    void *opaque = NULL;
    const AVInputFormat *ifmt = NULL;

    post("Demuxers:");
    while ((ifmt = av_demuxer_iterate(&opaque))) {
        post("  %s", ifmt->name);
    }
    opaque = NULL;
    const AVOutputFormat *ofmt = NULL;

    post("Muxers:");
    while ((ofmt = av_muxer_iterate(&opaque))) {
        post("  %s", ofmt->name);
    }*/

    return(x);
}

void streamout_tilde_setup(void){
    streamout_class = class_new(gensym("streamout~"), (t_newmethod)streamout_new, 
    	(t_method)streamout_free, sizeof(t_streamout), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(streamout_class, t_streamout, x_f); // ????????????????????/
    class_addmethod(streamout_class, (t_method)streamout_dsp, gensym("dsp"), 0);
    class_addfloat(streamout_class, (t_method)streamout_float);
    class_addmethod(streamout_class, (t_method)streamout_connect, gensym("connect"), A_GIMME, 0);
    class_addmethod(streamout_class, (t_method)streamout_disconnect, gensym("disconnect"), 0);
    class_addmethod(streamout_class, (t_method)streamout_password, gensym("password"), A_SYMBOL, 0);
    class_addmethod(streamout_class, (t_method)streamout_mount, gensym("mount"), A_SYMBOL, 0);
    class_addmethod(streamout_class, (t_method)streamout_print, gensym("print"), 0);
    class_addmethod(streamout_class, (t_method)streamout_set, gensym("set"), A_GIMME, 0);
    class_addmethod(streamout_class, (t_method)streamout_codec, gensym("codec"), A_GIMME, 0);
}
