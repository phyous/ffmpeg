/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavcodec API use example.
 *
 * Note that libavcodec only handles codecs (mpeg, mpeg4, etc...),
 * not file formats (avi, vob, mp4, mov, mkv, mxf, flv, mpegts, mpegps, etc...). See library 'libavformat' for the
 * format handling
 * @example doc/examples/decoding_encoding.c
 */

#include <math.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

static void alloc_frame(AVFrame* frame) {
	frame = avcodec_alloc_frame();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
}

static FILE* open_file(const char *file_path) {
	FILE* f = fopen(file_path, "rb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", file_path);
		exit(1);
	}
	return f;
}

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
		char *filename) {
	FILE *f;
	int i;

	f = fopen(filename, "w");
	fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
	for (i = 0; i < ysize; i++)
		fwrite(buf + i * wrap, 1, xsize, f);
	fclose(f);
}

static int decode_write_frame(const char *outfilename, AVCodecContext *avctx,
		AVFrame *frame, int *frame_count, AVPacket *pkt, int last) {
	int len, got_frame;
	char buf[1024];


	len = avcodec_decode_video2(avctx, frame, &got_frame, pkt);
	if (len < 0) {
		fprintf(stderr, "Error while decoding frame %d\n", *frame_count);
		return len;
	}
	if (got_frame) {
		printf("Saving %sframe %3d\n", last ? "last " : "", *frame_count);
		fflush(stdout);

		/* the picture is allocated by the decoder, no need to free it */
		snprintf(buf, sizeof(buf), outfilename, *frame_count);
		pgm_save(frame->data[0], frame->linesize[0], avctx->width,
				avctx->height, buf);
		(*frame_count)++;
	}
	if (pkt->data) {
		pkt->size -= len;
		pkt->data += len;
	}
	return 0;
}

static void video_stitch(const char *input_file1, const char *input_file2,
		const char *output_file) {
	AVCodec *codec;
	AVCodecContext *c = NULL;
	int frame_count1, frame_count2;
	FILE *f1, *f2;
	AVFrame *frame1, *frame2;
	uint8_t inbuf1[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
	uint8_t inbuf2[INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
	AVPacket avpkt1, avpkt2;

	av_init_packet(&avpkt1);
	av_init_packet(&avpkt2);

	/* set end of buffer to 0 (this ensures that no overreading happens for damaged mpeg streams) */
	memset(inbuf1 + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);
	memset(inbuf2 + INBUF_SIZE, 0, FF_INPUT_BUFFER_PADDING_SIZE);

	printf("Stitching video files %s and %s to %s\n", input_file1, input_file2,
			output_file);

	// Decode frames form each file
	/* find the mpeg1 video decoder */
	codec = avcodec_find_decoder(AV_CODEC_ID_MPEG4);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	if (codec->capabilities & CODEC_CAP_TRUNCATED)
		c->flags |= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

	f1 = open_file(input_file1);
	f2 = open_file(input_file2);
	//o1 = open_file(output_file);

	alloc_frame(frame1);
	alloc_frame(frame2);

	frame_count1 = 0;
	frame_count2 = 0;
	int toggle = 0;
	for (;;) {
		printf("frame1:%i frame2:%i", frame_count1, frame_count2);

		avpkt1.size = fread(inbuf1, 1, INBUF_SIZE, f1);
		avpkt2.size = fread(inbuf2, 1, INBUF_SIZE, f2);
		if (avpkt1.size == 0 || avpkt1.size == 0)
			break;

		avpkt1.data = inbuf1;
		avpkt2.data = inbuf2;

		//if (toggle == 0) {
			while (avpkt1.size > 0)
				if (decode_write_frame(output_file, c, frame1, &frame_count1,
						&avpkt1, 0) < 0)
					exit(1);
//			toggle = 1;
//		} else {
//			while (avpkt1.size > 0)
//				if (decode_write_frame(output_file, c, frame2, &frame_count2,
//						&avpkt2, 0) < 0)
//					exit(1);
//			toggle = 0;
//		}
	}
	printf("FINIISHED!");

    avpkt1.data = NULL;
    avpkt1.size = 0;
    decode_write_frame(output_file, c, frame1, &frame_count1, &avpkt1, 1);

    fclose(f1);
    fclose(f2);

    avcodec_close(c);
    av_free(c);
    avcodec_free_frame(&frame1);
    avcodec_free_frame(&frame2);
    printf("\n");
}

int main(int argc, char **argv) {
	const char *input_file1;
	const char *input_file2;
	const char *output_file;

	/* TODO: Support .mp4 files (avcodec doesn't do this for us)
	 int mp4Codec = AV_CODEC_ID_MP4ALS;
	 AVCodec *mp4_codec = avcodec_find_encoder(mp4Codec);
	 avcodec_register(mp4_codec);
	 */
	// HACK: For now, let's just go with the default formats supported
	avcodec_register_all();

	if (argc < 3) {
		printf("usage: input_file1 input_file2 output_file");
		return 1;
	}

	input_file1 = argv[1];
	input_file2 = argv[2];
	output_file = argv[3];

	video_stitch(input_file1, input_file2, output_file);

	return 0;
}
