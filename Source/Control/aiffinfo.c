// extract info from AIFF's header

#include "m_pd.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

static t_class *aiffinfo_class;

typedef struct _aiff_marker {
    uint16_t id;           // Marker ID
    uint32_t position;     // Position in sample frames
    char     name[256];    // Marker name (Pascal string)
} t_aiff_marker;

typedef struct _aiff_loops {
    uint16_t playMode;     // 0=no loop, 1=forward, 2=forward/backward
    uint16_t beginLoop;    // Marker ID for loop start
    uint16_t endLoop;      // Marker ID for loop end
} t_aiff_loops;

typedef struct _aiff_INST {
    int8_t   baseNote;     // MIDI note number 0-127
    int8_t   detune;       // Detune in cents (-50 to +50)
    int8_t   lowNote;      // MIDI note number 0-127
    int8_t   highNote;     // MIDI note number 0-127
    int8_t   lowVelocity;  // MIDI velocity 1-127
    int8_t   highVelocity; // MIDI velocity 1-127
    int16_t  gain;         // Gain in dB
    t_aiff_loops sustainLoop;
    t_aiff_loops releaseLoop;
} t_aiff_INST;

typedef struct _aiffinfo {
    t_object        x_obj;
    t_canvas       *x_canvas;
    t_aiff_marker  *x_markers;
    uint16_t        x_nummarkers;
    t_aiff_INST     x_instrument;
    int             x_nchans;
    int             x_nsamps;
    int             x_bitsize;
    t_float         x_sample_rate;
} t_aiffinfo;

static uint16_t swap16(uint16_t x) {
    return ((x & 0xff) << 8) | ((x & 0xff00) >> 8);
}

static uint32_t swap32(uint32_t x) {
    return ((x & 0xff) << 24) | ((x & 0xff00) << 8) |
           ((x & 0xff0000) >> 8) | ((x & 0xff000000) >> 24);
}

static int read_markers(t_aiffinfo *x, FILE *fp, uint32_t chunk_size) {
    uint16_t num_markers;
    if(fread(&num_markers, 2, 1, fp) != 1) {
        pd_error(x, "[aiffinfo~]: error reading number of markers");
        return(0);
    }
    num_markers = swap16(num_markers);
//    post("chunk_size = %u", chunk_size);
//    post("file contains %d markers", num_markers);
    size_t expected_size = 2 + (num_markers * (2 + 4 + 1)); // Base size without names
    if (chunk_size < expected_size) {
        pd_error(x, "[aiffinfo~]: invalid MARK chunk size (%u)", chunk_size);
        return 0;
    }
    x->x_markers = getbytes(num_markers * sizeof(t_aiff_marker));
    x->x_nummarkers = num_markers;
    for(size_t i = 0; i < num_markers; i++){
        t_aiff_marker *m = &x->x_markers[i];
        unsigned char id_bytes[2];
        unsigned char pos_bytes[4];
        // Read marker ID (2 bytes)
        if(fread(id_bytes, 2, 1, fp) != 1) {
            pd_error(x, "[aiffinfo~]: error reading marker id");
            return 0;
        }
//        post("marker %d - ID: %02x %02x", i, id_bytes[0], id_bytes[1]);
        // Read position (4 bytes)
        if(fread(pos_bytes, 4, 1, fp) != 1) {
            pd_error(x, "[aiffinfo~]: error reading marker position");
            return 0;
        }
//        post("marker %d - position bytes: %02x %02x %02x %02x",
//             i, pos_bytes[0], pos_bytes[1], pos_bytes[2], pos_bytes[3]);
        // Set marker ID (1-based index as per AIFF spec)
//        m->id = i + 1;
        m->id = swap16(*(uint16_t *)id_bytes);
        // Calculate position using standard big-endian 32-bit format
        // This is the same for ALL markers - no special cases
        m->position = swap32(*(uint32_t *)pos_bytes);
        // Read Pascal string (pstring):
        // First byte is the length count
        uint8_t namelen;
        if(fread(&namelen, 1, 1, fp) != 1){
            pd_error(x, "[aiffinfo~]: error reading marker name length");
            return(0);
        }
        // Safety check on name length
        if(namelen > 100)
            namelen = 100;
        // Read the text bytes
        if(fread(m->name, namelen, 1, fp) != 1){
            pd_error(x, "[aiffinfo~]: error reading marker name");
            return(0);
        }
        m->name[namelen] = '\0';  // Null-terminate the string
        // Handle padding according to AIFF-1.3 spec:
        // - A pstring consists of: [1 byte count][N bytes text][optional pad byte]
        // - Total length (count byte + text bytes + optional pad) must be even
        // - Therefore: if text length is even, we need a pad byte
        //             if text length is odd, we don't need a pad byte
        if((namelen % 2) == 0){
//            post("marker %d - skipping padding byte (text length %d is even)", i, namelen);
            fseek(fp, 1, SEEK_CUR);
        }
//        post("marker: id = %d / name = '%s' / pos = %d", m->id, m->name, m->position);
    }
    return(1);
}

// Helper function to convert loop mode to a human-readable string
const char *loop_mode_to_string(int mode) {
    switch (mode) {
        case 0: return "No Loop";
        case 1: return "Forward Loop";
        case 2: return "Alternating Loop";
        default: return "Invalid Mode";
    }
}

// Function to find the index of a marker by its ID
int find_index(t_aiffinfo *x, int marker) {
    for (uint16_t i = 0; i < x->x_nummarkers; i++) {
        if (marker == x->x_markers[i].id) {
            return i; // Return the index if the ID matches
        }
    }
    return -1; // Return -1 if no match is found
}

static void read_output(t_aiffinfo *x){
    post("----------------------------------");
    post("Sample Size: %d", x->x_nsamps);
    post("Channels: %d", x->x_nchans);
    post("Bits: %d", x->x_bitsize);
    post("Sample Size: %.1f", x->x_sample_rate);
    post("INST DATA -------------------------");
    post("Base Note: %d", x->x_instrument.baseNote);
    post("Detune: %d cents", x->x_instrument.detune);
    post("MIDI Note Range: %d - %d", x->x_instrument.lowNote, x->x_instrument.highNote);
    post("Velocity Range: %d - %d", x->x_instrument.lowVelocity, x->x_instrument.highVelocity);
    post("Gain: %d dB", x->x_instrument.gain);
    post("Sustain Loop: Mode = %s, Begin marker = %d, End marker = %d",
         loop_mode_to_string(x->x_instrument.sustainLoop.playMode),
         x->x_instrument.sustainLoop.beginLoop,
         x->x_instrument.sustainLoop.endLoop);
    int idx1 = 0, idx2 = 0, idx3 = 0, idx4 = 0;
    if(x->x_instrument.sustainLoop.playMode > 0){
        idx1 = find_index(x, x->x_instrument.sustainLoop.beginLoop);
        idx2 = find_index(x, x->x_instrument.sustainLoop.endLoop);
        post("name: %s | pos: %d", x->x_markers[idx1].name, x->x_markers[idx1].position);
        post("name: %s | pos: %d", x->x_markers[idx2].name, x->x_markers[idx2].position);
    }
    post("Release Loop: Mode = %s, Begin marker = %d, End marker = %d",
         loop_mode_to_string(x->x_instrument.releaseLoop.playMode),
         x->x_instrument.releaseLoop.beginLoop,
         x->x_instrument.releaseLoop.endLoop);
    if(x->x_instrument.releaseLoop.playMode > 0){
        idx3 = x->x_instrument.sustainLoop.beginLoop;
        idx4 = x->x_instrument.sustainLoop.endLoop;
        post("name: %s | pos: %d", x->x_markers[idx3].name, x->x_markers[idx3].position);
        post("name: %s | pos: %d", x->x_markers[idx4].name, x->x_markers[idx4].position);
    }
    post("----------------------------------");
//
    t_atom header[4];
    SETFLOAT(header+0, x->x_nsamps);
    SETFLOAT(header+1, x->x_nchans);
    SETFLOAT(header+2, x->x_bitsize);
    SETFLOAT(header+3, x->x_sample_rate);
    outlet_anything(x->x_obj.ob_outlet, gensym("header"), 4, header);
//
    t_atom inst[13];
    SETFLOAT(inst+0, x->x_instrument.baseNote);
    SETFLOAT(inst+1, x->x_instrument.detune);
    SETFLOAT(inst+2, x->x_instrument.lowNote);
    SETFLOAT(inst+3, x->x_instrument.highNote);
    SETFLOAT(inst+4, x->x_instrument.lowVelocity);
    SETFLOAT(inst+5, x->x_instrument.highVelocity);
    SETFLOAT(inst+6, x->x_instrument.gain);
    int susloop = x->x_instrument.sustainLoop.playMode > 0;
    SETFLOAT(inst+7, susloop);
    SETFLOAT(inst+8, susloop ? x->x_markers[idx1].position : 0);
    SETFLOAT(inst+9, susloop ? x->x_markers[idx2].position : 0);
    int relloop = x->x_instrument.releaseLoop.playMode > 0;
    SETFLOAT(inst+10, relloop);
    SETFLOAT(inst+11, relloop ? x->x_markers[idx3].position : 0);
    SETFLOAT(inst+12, relloop ? x->x_markers[idx4].position : 0);
    outlet_anything(x->x_obj.ob_outlet, gensym("inst"), 13, inst);
}

static int read_instrument(t_aiffinfo *x, FILE *fp) {
    t_aiff_INST *inst = &x->x_instrument;
    // Parse basic parameters
    if (fread(&inst->baseNote, 1, 1, fp) != 1 ||
        fread(&inst->detune, 1, 1, fp) != 1 ||
        fread(&inst->lowNote, 1, 1, fp) != 1 ||
        fread(&inst->highNote, 1, 1, fp) != 1 ||
        fread(&inst->lowVelocity, 1, 1, fp) != 1 ||
        fread(&inst->highVelocity, 1, 1, fp) != 1 ||
        fread(&inst->gain, 2, 1, fp) != 1) {
        pd_error(x, "[aiffinfo~]: error reading INST chunk basic parameters");
        return 0;
    }
    unsigned char raw_data[12]; // Adjusted to 12 bytes
    if (fread(raw_data, 12, 1, fp) != 1) { // Read 12 bytes
        pd_error(x, "[aiffinfo~]: error reading sustain loop data");
        return 0;
    }
    // Sustain Loop
    inst->sustainLoop.playMode = (raw_data[0] << 8) | raw_data[1];
    inst->sustainLoop.beginLoop = (raw_data[2] << 8) | raw_data[3];
    inst->sustainLoop.endLoop = (raw_data[4] << 8) | raw_data[5];
    // Release Loop
    inst->releaseLoop.playMode = (raw_data[6] << 8) | raw_data[7];
    inst->releaseLoop.beginLoop = (raw_data[8] << 8) | raw_data[9];
    inst->releaseLoop.endLoop = (raw_data[10] << 8) | raw_data[11];
    return 1;
}

// Helper function to convert 80-bit extended float to double
static double extended_to_double(const uint8_t *bytes) {
    // Log raw bytes of the 80-bit extended float
    // Extract exponent (first 2 bytes, big-endian)
    uint16_t exponent = (bytes[0] << 8) | bytes[1];
//    post("Exponent (raw): %u", exponent);
    // Extract mantissa (next 8 bytes, big-endian)
    uint64_t mantissa = 0;
    for (int i = 0; i < 8; i++) {
        mantissa |= ((uint64_t)bytes[i + 2]) << ((7 - i) * 8);
    }
//    post("Mantissa (raw): %llu", mantissa);
    // Handle special case: exponent == 0 && mantissa == 0
    if (exponent == 0 && mantissa == 0) {
//        post("Exponent and mantissa are both zero");
        return 0.0;
    }
    // Bias for 80-bit extended float
    int bias = 16383;
    int actual_exponent = exponent - bias;
//    post("Actual exponent: %d", actual_exponent);
    // Compute the value: mantissa * 2^(actual_exponent - 63)
    double value = (double)mantissa * pow(2.0, actual_exponent - 63);
//    post("Computed sample rate: %f", value);
    return(value);
}

// Function to parse the COMM chunk
static void read_comm_chunk(t_aiffinfo *x, FILE *fp, uint32_t chunk_size){
//            post("found COMM chunk");
    // Validate the chunk size
    size_t expected_data_size = 18; // Fixed size of the COMM data portion
    if (chunk_size != expected_data_size) {
        pd_error(x, "[aiffinfo~]: invalid COMM chunk size (%u)", chunk_size);
        return;
    }
    uint16_t num_channels;
    uint32_t num_sample_frames;
    uint16_t sample_size;
    uint8_t sample_rate_bytes[10];
    // Read Num Channels (2 bytes)
    if (fread(&num_channels, 2, 1, fp) != 1) {
        pd_error(x, "[aiffinfo~]: error reading COMM chunk (num channels)");
        return;
    }
    num_channels = swap16(num_channels);
//    post("Num channels: %u", num_channels);
    // Read Num Sample Frames (4 bytes)
    if (fread(&num_sample_frames, 4, 1, fp) != 1) {
        pd_error(x, "[aiffinfo~]: error reading COMM chunk (num sample frames)");
        return;
    }
    num_sample_frames = swap32(num_sample_frames);
//    post("Num sample frames: %u", num_sample_frames);
    // Read Sample Size (2 bytes)
    if (fread(&sample_size, 2, 1, fp) != 1) {
        pd_error(x, "[aiffinfo~]: error reading COMM chunk (sample size)");
        return;
    }
    sample_size = swap16(sample_size);
//    post("Sample size (bits): %u", sample_size);
    // Read Sample Rate (10 bytes)
    if (fread(sample_rate_bytes, 10, 1, fp) != 1) {
        pd_error(x, "[aiffinfo~]: error reading COMM chunk (sample rate)");
        return;
    }
    // Convert sample rate from 80-bit extended float to double
    double sample_rate = extended_to_double(sample_rate_bytes);
    // Output the metadata to Pd outlets
/*    post("COMM chunk:  nsamples = %u / sample rate = %.1f / channels = %d / bits = %d",
         num_sample_frames, sample_rate, num_channels, sample_size);*/
    x->x_nchans = num_channels;
    x->x_nsamps = num_sample_frames;
    x->x_bitsize = sample_size;
    x->x_sample_rate = sample_rate;
}

// Main file parsing function
static void aiffinfo_read(t_aiffinfo *x, t_symbol *file) {
    char fname[MAXPDSTRING];
    char *bufptr;
    int fd = canvas_open(x->x_canvas, file->s_name, "", fname, &bufptr, MAXPDSTRING, 1);
    if(fd < 0){
        pd_error(x, "[aiffinfo~]: file '%s' not found", file->s_name);
        return;
    }
    else if(bufptr > fname)
        bufptr[-1] = '/';
    FILE *fp = fopen(fname, "rb");
    if(!fp){
        pd_error(x, "[aiffinfo~]: error opening '%s'", fname);
        return;
    }
    char id[4];
    uint32_t size;
    char formtype[4];
    // Read FORM chunk
    if(fread(id, 4, 1, fp) != 1 || strncmp(id, "FORM", 4)){
        pd_error(x, "[aiffinfo~]: %s: not an AIFF file (no FORM chunk)", fname);
        fclose(fp);
        return;
    }
    if(fread(&size, 4, 1, fp) != 1){
        pd_error(x, "[aiffinfo~]: %s: corrupt AIFF file (no chunk size)", fname);
        fclose(fp);
        return;
    }
    size = swap32(size);
    if(fread(formtype, 4, 1, fp) != 1 || (strncmp(formtype, "AIFF", 4)
    && strncmp(formtype, "AIFC", 4))){
        pd_error(x, "[aiffinfo~]: %s: not an AIFF/AIFC file", fname);
        fclose(fp);
        return;
    }
    // Iterate through chunks
    while(!feof(fp)){
        char chunk_id[4];
        uint32_t chunk_size;
        // Read chunk ID and size
        if(fread(chunk_id, 4, 1, fp) != 1)
            break;
        if(fread(&chunk_size, 4, 1, fp) != 1)
            break;
        chunk_size = swap32(chunk_size);
        // Handle known chunks
        if(!strncmp(chunk_id, "COMM", 4)){
            read_comm_chunk(x, fp, chunk_size);
        }
        else if(!strncmp(chunk_id, "MARK", 4)){
//            post("found MARK chunk");
            if(x->x_markers){
                freebytes(x->x_markers, x->x_nummarkers * sizeof(t_aiff_marker));
                x->x_markers = NULL;
                x->x_nummarkers = 0;
            }
            if(!read_markers(x, fp, chunk_size))
                break;
        }
        else if(!strncmp(chunk_id, "INST", 4)){
//            post("found INST chunk");
            if(!read_instrument(x, fp)){
                pd_error(x, "[aiffinfo~]: error reading instrument chunk");
                break;
            }
        }
        else // Skip unknown chunks
            fseek(fp, chunk_size + (chunk_size & 1), SEEK_CUR);
    }
    read_output(x);
    fclose(fp);
}

static void aiffinfo_free(t_aiffinfo *x) {
    if(x->x_markers)
        freebytes(x->x_markers, x->x_nummarkers * sizeof(t_aiff_marker));
}

static void *aiffinfo_new(void) {
    t_aiffinfo *x = (t_aiffinfo *)pd_new(aiffinfo_class);
    x->x_canvas = canvas_getcurrent();
    outlet_new(&x->x_obj, &s_list);
    x->x_markers = NULL;
    x->x_nummarkers = 0;
    return(x);
}

void aiffinfo_setup(void) {
    aiffinfo_class = class_new(gensym("aiffinfo"), (t_newmethod)aiffinfo_new,
        (t_method)aiffinfo_free, sizeof(t_aiffinfo), CLASS_DEFAULT, 0);
    class_addmethod(aiffinfo_class, (t_method)aiffinfo_read, gensym("read"), A_SYMBOL, 0);
}
