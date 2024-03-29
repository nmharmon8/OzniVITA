/*
 * Generate signal and write VRT IF data packet to file. Note that this will not generate a big endian-format, i.e.
 * standard conforming, packet on a little endian platform.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vrt/vrt_init.h>
#include <vrt/vrt_string.h>
#include <vrt/vrt_types.h>
#include <vrt/vrt_util.h>
#include <vrt/vrt_write.h>

/* Size of buffer in 32-bit words */
#define SIZE 515
/* Sample rate [Hz] */
#define SAMPLE_RATE 44100.0F
/* Center frequency [Hz] */
#define CENTER_FREQUENCY 10000.0F
/* M_PI in math.h is nonstandard :( */
#define PI 3.1415926F

int main() {
    /* Packet data buffer */
    uint32_t b[SIZE];

    /* Generate signal data */
    float s[SIZE - 3];
    for (int i = 0; i < SIZE - 3; ++i) {
        s[i] = sinf(2.0F * PI * CENTER_FREQUENCY * (float)i / SAMPLE_RATE);
    }

    /* Initialize to reasonable values */
    struct vrt_header  h;
    struct vrt_fields  f;
    struct vrt_trailer t;
    vrt_init_header(&h);
    vrt_init_fields(&f);
    vrt_init_trailer(&t);

    /* Configure */
    h.packet_type        = VRT_PT_IF_DATA_WITH_STREAM_ID;
    h.has.trailer        = true;
    h.packet_size        = SIZE;
    f.stream_id          = 0xDEADBEEF;
    t.has.reference_lock = true;
    t.reference_lock     = true;

    /* Write header to buffer */
    int32_t offset = 0;
    int32_t rv     = vrt_write_header(&h, b + offset, SIZE - offset, true);
    if (rv < 0) {
        fprintf(stderr, "Failed to write header: %s\n", vrt_string_error(rv));
        return EXIT_FAILURE;
    }
    offset += rv;

    /* Write fields to buffer */
    rv = vrt_write_fields(&h, &f, b + offset, SIZE - offset, true);
    if (rv < 0) {
        fprintf(stderr, "Failed to write fields section: %s\n", vrt_string_error(rv));
        return EXIT_FAILURE;
    }
    offset += rv;

    /* Copy signal data from signal to packet buffer.
     * This could also have been written directly into the write buffer. */
    memcpy(b + offset, s, sizeof(float) * (SIZE - 3));
    offset += SIZE - 3;

    /* Write trailer to buffer */
    rv = vrt_write_trailer(&t, b + offset, SIZE - offset, true);
    if (rv < 0) {
        fprintf(stderr, "Failed to write trailer: %s\n", vrt_string_error(rv));
        return EXIT_FAILURE;
    }

    /* Write generated packet to file */
    const char* file_path = "signal.vrt";
    FILE*       fp        = fopen(file_path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file '%s'\n", file_path);
        return EXIT_FAILURE;
    }
    if (fwrite(b, sizeof(uint32_t) * SIZE, 1, fp) != 1) {
        fprintf(stderr, "Failed to write to file '%s'\n", file_path);
        fclose(fp);
        return EXIT_FAILURE;
    }
    fclose(fp);

    /* Warn if not standards compliant */
    if (vrt_is_platform_little_endian()) {
        fprintf(stderr, "Warning: Written packet is little endian. It is NOT compliant with the VRT standard.\n");
    }

    return EXIT_SUCCESS;
}
