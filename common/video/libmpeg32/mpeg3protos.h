#ifndef MPEG3PROTOS_H
#define MPEG3PROTOS_H


/* CSS */

mpeg3_css_t* mpeg3_new_css();

/* DEMUX */

mpeg3_demuxer_t* mpeg3_new_demuxer(mpeg3_t *file, int do_audio, int do_video, int stream_id);
int mpeg3_delete_demuxer(mpeg3_demuxer_t *demuxer);
int mpeg3demux_read_data(mpeg3_demuxer_t *demuxer, unsigned char *output, long size);
unsigned int mpeg3demux_read_int32(mpeg3_demuxer_t *demuxer);
unsigned int mpeg3demux_read_int24(mpeg3_demuxer_t *demuxer);
unsigned int mpeg3demux_read_int16(mpeg3_demuxer_t *demuxer);
double mpeg3demux_length(mpeg3_demuxer_t *demuxer);
mpeg3_demuxer_t* mpeg3_get_demuxer(mpeg3_t *file);
int64_t mpeg3demux_tell(mpeg3_demuxer_t *demuxer);
double mpeg3demux_tell_percentage(mpeg3_demuxer_t *demuxer);
int mpeg3demux_tell_title(mpeg3_demuxer_t *demuxer);
double mpeg3demux_get_time(mpeg3_demuxer_t *demuxer);
int mpeg3demux_eof(mpeg3_demuxer_t *demuxer);
int mpeg3demux_bof(mpeg3_demuxer_t *demuxer);
void mpeg3demux_start_reverse(mpeg3_demuxer_t *demuxer);
void mpeg3demux_start_forward(mpeg3_demuxer_t *demuxer);
int mpeg3demux_seek_byte(mpeg3_demuxer_t *demuxer, int64_t byte);
int64_t mpeg3demuxer_total_bytes(mpeg3_demuxer_t *demuxer);
unsigned char mpeg3demux_read_char_packet(mpeg3_demuxer_t *demuxer);
unsigned char mpeg3demux_read_prev_char_packet(mpeg3_demuxer_t *demuxer);
int mpeg3demux_read_program(mpeg3_demuxer_t *demuxer);

/* TITLE */

mpeg3_title_t* mpeg3_new_title(mpeg3_t *file, const char *path);
void mpeg3_new_timecode(mpeg3_title_t *title, 
		long start_byte, 
		double start_time,
		long end_byte,
		double end_time,
		int program);
mpeg3demux_timecode_t* mpeg3_append_timecode(mpeg3_demuxer_t *demuxer, 
		mpeg3_title_t *title, 
		long prev_byte, 
		double prev_time, 
		long start_byte, 
		double start_time,
		int dont_store,
		int program);


/* ATRACK */

mpeg3_atrack_t* mpeg3_new_atrack(mpeg3_t *file, 
	int stream_id, 
	int is_ac3, 
	mpeg3_demuxer_t *demuxer,
	int number);
int mpeg3_delete_atrack(mpeg3_t *file, mpeg3_atrack_t *atrack);

/* VTRACK */

mpeg3_vtrack_t* mpeg3_new_vtrack(mpeg3_t *file, 
	int stream_id, 
	mpeg3_demuxer_t *demuxer,
	int number);
int mpeg3_delete_vtrack(mpeg3_t *file, mpeg3_vtrack_t *vtrack);

/* AUDIO */
mpeg3audio_t* mpeg3audio_new(mpeg3_atrack_t *track, int is_ac3);
int mpeg3audio_delete(mpeg3audio_t *audio);


/* VIDEO */
mpeg3video_t* mpeg3video_new(int have_mmx,
			     int is_video_stream, int cpus);
int mpeg3video_delete(mpeg3video_t *video);
int mpeg3video_read_frame(mpeg3video_t *video, 
			  unsigned char *input,
			  long input_size,
			  unsigned char **output_rows,
			  int in_x, 
			  int in_y, 
			  int in_w, 
			  int in_h, 
			  int out_w, 
			  int out_h, 
			  int color_model);
int mpeg3video_get_header(mpeg3video_t *video, int dont_repeat);
int mpeg3video_initdecoder(mpeg3video_t *video);
int mpeg3video_read_yuvframe_ptr(mpeg3video_t *video, 
				 unsigned char *input,
				 long input_size,
				 char **y_output,
				 char **u_output,
				 char **v_output);

/* FILESYSTEM */

mpeg3_fs_t* mpeg3_new_fs(const char *path);
int mpeg3_delete_fs(mpeg3_fs_t *fs);
int mpeg3io_open_file(mpeg3_fs_t *fs);
int mpeg3io_close_file(mpeg3_fs_t *fs);
int mpeg3io_seek(mpeg3_fs_t *fs, int64_t byte);
int mpeg3io_read_data(unsigned char *buffer, long bytes, mpeg3_fs_t *fs);
int mpeg3io_next_code(mpeg3_fs_t *fs, uint32_t code, int count);
int mpeg3io_prev_code(mpeg3_fs_t *fs, uint32_t code, int count);

/* BITSTREAM */
#ifndef MPEG4IP
mpeg3_bits_t* mpeg3bits_new_stream(mpeg3_t *file, mpeg3_demuxer_t *demuxer);
#else
mpeg3_bits_t* mpeg3bits_new_stream(mpeg3_demuxer_t *demuxer);
#endif
unsigned int mpeg3bits_getbits(mpeg3_bits_t* stream, int n);
int mpeg3bits_seek_byte(mpeg3_bits_t* stream, int64_t position);
int mpeg3bits_open_title(mpeg3_bits_t* stream, int title);
int64_t mpeg3bits_tell(mpeg3_bits_t* stream);
int mpeg3bits_use_ptr_len(mpeg3_bits_t *stream,
			  unsigned char *buffer,
			  long buflen);



#endif
