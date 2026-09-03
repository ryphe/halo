#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef int            s32;

#define SUBSAMPLE 8

#pragma pack(push, 1)
typedef struct {
    u16 reserved;
    u16 type;
    u16 count;
} ICONHEADER;

typedef struct {
    u8  width;
    u8  height;
    u8  colors;
    u8  reserved;
    u16 planes;
    u16 bitCount;
    u32 size;
    u32 offset;
} ICONDIRENTRY;

typedef struct {
    u32 biSize;
    s32 biWidth;
    s32 biHeight;
    u16 biPlanes;
    u16 biBitCount;
    u32 biCompression;
    u32 biSizeImage;
    s32 biXPelsPerMeter;
    s32 biYPelsPerMeter;
    u32 biClrUsed;
    u32 biClrImportant;
} BITMAPINFOHEADER_T;
#pragma pack(pop)

typedef struct {
    int size;
    u32 data_size;
    u8* data;
} IconImageBlob;

static double srgb_to_linear(int c) {
    double v = c / 255.0;
    return (v <= 0.04045) ? (v / 12.92) : pow((v + 0.055) / 1.055, 2.4);
}

static u8 linear_to_srgb(double v) {
    if (v <= 0.0) return 0;
    if (v >= 1.0) return 255;
    double s = (v <= 0.0031308) ? (v * 12.92) : (1.055 * pow(v, 1.0 / 2.4) - 0.055);
    int c = (int)(s * 255.0 + 0.5);
    return (u8)(c < 0 ? 0 : (c > 255 ? 255 : c));
}

static double clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

static double cover(double d) {
    return clamp01(0.5 - d * (double)SUBSAMPLE);
}

static IconImageBlob render_halo_icon(int size) {
    IconImageBlob blob;
    blob.size = size;
    blob.data_size = 0;
    blob.data = NULL;

    const double inv_ss = 1.0 / (double)SUBSAMPLE;

    double fill_lin[3] = { srgb_to_linear(18), srgb_to_linear(20), srgb_to_linear(25) };
    double orange_lin[3] = { srgb_to_linear(255), srgb_to_linear(140), srgb_to_linear(25) };
    double inner_lin[3] = { srgb_to_linear(255), srgb_to_linear(190), srgb_to_linear(70) };

    double cx = size * 0.5;
    double cy = size * 0.5;
    double R  = size * 0.46;
    double border_w = (size <= 16) ? 1.0 : ((size <= 32) ? 1.6 : (size * 0.06));
    double inner_r  = size * 0.26;
    double inner_w  = (size <= 16) ? 1.1 : ((size <= 32) ? 1.8 : (size * 0.07));

    int xor_size   = size * size * 4;
    int and_stride = ((size + 31) / 32) * 4;
    int and_size   = and_stride * size;

    blob.data_size = (u32)(sizeof(BITMAPINFOHEADER_T) + (size_t)xor_size + (size_t)and_size);
    blob.data = (u8*)calloc(1, blob.data_size);
    if (!blob.data) return blob;

    BITMAPINFOHEADER_T bih;
    memset(&bih, 0, sizeof(bih));
    bih.biSize = (u32)sizeof(BITMAPINFOHEADER_T);
    bih.biWidth = size;
    bih.biHeight = size * 2;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biSizeImage = (u32)(xor_size + and_size);
    memcpy(blob.data, &bih, sizeof(bih));

    u8* dest_xor = blob.data + sizeof(BITMAPINFOHEADER_T);
    u8* dest_and = dest_xor + xor_size;

    for (int y = 0; y < size; y++) {
        u8* out_row = dest_xor + (size_t)(size - 1 - y) * (size_t)size * 4;
        u8* and_row = dest_and + (size_t)(size - 1 - y) * (size_t)and_stride;

        for (int x = 0; x < size; x++) {
            double acc_r = 0.0, acc_g = 0.0, acc_b = 0.0, acc_a = 0.0;

            for (int sy = 0; sy < SUBSAMPLE; sy++) {
                double py = (double)y + ((double)sy + 0.5) * inv_ss;
                double dy = py - cy;

                for (int sx = 0; sx < SUBSAMPLE; sx++) {
                    double px = (double)x + ((double)sx + 0.5) * inv_ss;
                    double dx = px - cx;
                    double dist = sqrt(dx * dx + dy * dy);

                    /* Outer Disc + Orange Border Ring */
                    double cov_out = cover(dist - R);
                    if (cov_out <= 0.0) continue;

                    double cov_in = cover(dist - (R - border_w));
                    double ring = cov_out - cov_in;
                    if (ring < 0.0) ring = 0.0;

                    double pr = ring * orange_lin[0] + cov_in * fill_lin[0];
                    double pg = ring * orange_lin[1] + cov_in * fill_lin[1];
                    double pb = ring * orange_lin[2] + cov_in * fill_lin[2];
                    double pa = cov_out;

                    /* Concentric Spectral Halo */
                    double halo_d = fabs(dist - inner_r);
                    double cov_halo = cover(halo_d - (inner_w * 0.5));
                    if (cov_halo > 0.0) {
                        pr = cov_halo * inner_lin[0] + (1.0 - cov_halo) * pr;
                        pg = cov_halo * inner_lin[1] + (1.0 - cov_halo) * pg;
                        pb = cov_halo * inner_lin[2] + (1.0 - cov_halo) * pb;
                    }

                    acc_r += pr;
                    acc_g += pg;
                    acc_b += pb;
                    acc_a += pa;
                }
            }

            u8* op = out_row + (size_t)x * 4;
            if (acc_a > 0.0) {
                double inv = 1.0 / acc_a;
                op[0] = linear_to_srgb(acc_b * inv);
                op[1] = linear_to_srgb(acc_g * inv);
                op[2] = linear_to_srgb(acc_r * inv);
                double af = acc_a * (255.0 / (double)(SUBSAMPLE * SUBSAMPLE));
                int a = (int)(af + 0.5);
                if (a > 255) a = 255;
                op[3] = (u8)a;
            } else {
                and_row[x >> 3] |= (u8)(0x80u >> (x & 7));
            }
        }
    }
    return blob;
}

int main(int argc, char* argv[]) {
    const char* target = (argc > 1) ? argv[1] : "halo.ico";
    const int sizes[] = {16, 20, 24, 32, 48, 64, 128, 256};
    const int num_sizes = (int)(sizeof(sizes) / sizeof(sizes[0]));
    IconImageBlob blobs[8];

    for (int i = 0; i < num_sizes; i++) {
        blobs[i] = render_halo_icon(sizes[i]);
        if (!blobs[i].data) {
            for (int j = 0; j < i; j++) free(blobs[j].data);
            return 1;
        }
    }

    FILE* f = fopen(target, "wb");
    if (!f) return 1;

    ICONHEADER header = { 0, 1, (u16)num_sizes };
    fwrite(&header, sizeof(header), 1, f);

    u32 current_offset = (u32)(sizeof(ICONHEADER) + sizeof(ICONDIRENTRY) * num_sizes);

    for (int i = 0; i < num_sizes; i++) {
        ICONDIRENTRY entry;
        entry.width  = (blobs[i].size >= 256) ? 0 : (u8)blobs[i].size;
        entry.height = (blobs[i].size >= 256) ? 0 : (u8)blobs[i].size;
        entry.colors = 0;
        entry.reserved = 0;
        entry.planes = 1;
        entry.bitCount = 32;
        entry.size = blobs[i].data_size;
        entry.offset = current_offset;

        fwrite(&entry, sizeof(entry), 1, f);
        current_offset += blobs[i].data_size;
    }

    for (int i = 0; i < num_sizes; i++) {
        fwrite(blobs[i].data, 1, blobs[i].data_size, f);
        free(blobs[i].data);
    }

    fclose(f);
    printf("Successfully generated orange spectral halo icon: %s\n", target);
    return 0;
}