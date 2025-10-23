/*
 * Create a JPEG with extreme coefficient values that will cause
 * out-of-bounds access to sample_range_limit during IDCT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>

/*
 * Create a JPEG with extreme DC coefficients that will cause
 * large values to be clamped using sample_range_limit
 */
void create_extreme_dc_jpeg(const char *filename)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *outfile;
    JSAMPROW row_pointer[1];
    int row_stride;
    unsigned char *image_buffer;
    int width = 256;
    int height = 256;
    
    printf("[*] Creating JPEG with extreme DC values: %s\n", filename);
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    
    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "Can't open %s\n", filename);
        return;
    }
    
    jpeg_stdio_dest(&cinfo, outfile);
    
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 1;  /* Grayscale */
    cinfo.in_color_space = JCS_GRAYSCALE;
    cinfo.data_precision = 8;
    
    jpeg_set_defaults(&cinfo);
    
    /* Use highest quality to preserve extreme values */
    jpeg_set_quality(&cinfo, 100, FALSE);  /* Don't force baseline */
    
    /* Disable smoothing to keep sharp transitions */
    cinfo.smoothing_factor = 0;
    
    /* Use integer DCT for predictability */
    cinfo.dct_method = JDCT_ISLOW;
    
    jpeg_start_compress(&cinfo, TRUE);
    
    row_stride = width;
    image_buffer = (unsigned char *)malloc(row_stride);
    
    /* Create pattern with extreme transitions */
    /* This will create large AC coefficients */
    while (cinfo.next_scanline < cinfo.image_height) {
        for (int i = 0; i < width; i++) {
            /* Create sharp black/white transitions */
            /* These will produce extreme DCT coefficients */
            if ((i / 8) % 2 == 0) {
                image_buffer[i] = 0;    /* Black */
            } else {
                image_buffer[i] = 255;  /* White */
            }
            
            /* Also alternate by row for vertical transitions */
            if ((cinfo.next_scanline / 8) % 2 == 1) {
                image_buffer[i] = 255 - image_buffer[i];
            }
        }
        
        row_pointer[0] = image_buffer;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    
    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
    free(image_buffer);
    
    printf("    ✓ Created %s\n", filename);
}

/*
 * Create a JPEG with a repeating extreme pattern
 */
void create_checkerboard_jpeg(const char *filename)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *outfile;
    JSAMPROW row_pointer[1];
    int row_stride;
    unsigned char *image_buffer;
    int width = 512;
    int height = 512;
    
    printf("[*] Creating extreme checkerboard JPEG: %s\n", filename);
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    
    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "Can't open %s\n", filename);
        return;
    }
    
    jpeg_stdio_dest(&cinfo, outfile);
    
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;  /* RGB */
    cinfo.in_color_space = JCS_RGB;
    cinfo.data_precision = 8;
    
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 100, FALSE);
    cinfo.smoothing_factor = 0;
    
    jpeg_start_compress(&cinfo, TRUE);
    
    row_stride = width * 3;
    image_buffer = (unsigned char *)malloc(row_stride);
    
    /* 1-pixel checkerboard - maximum high-frequency content */
    while (cinfo.next_scanline < cinfo.image_height) {
        for (int i = 0; i < row_stride; i += 3) {
            int x = i / 3;
            int y = cinfo.next_scanline;
            
            /* Checkerboard pattern */
            if ((x + y) % 2 == 0) {
                image_buffer[i] = 255;    /* R */
                image_buffer[i+1] = 255;  /* G */
                image_buffer[i+2] = 255;  /* B */
            } else {
                image_buffer[i] = 0;
                image_buffer[i+1] = 0;
                image_buffer[i+2] = 0;
            }
        }
        
        row_pointer[0] = image_buffer;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    
    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
    free(image_buffer);
    
    printf("    ✓ Created %s\n", filename);
}

/*
 * Create JPEG with random noise - statistically likely to have extreme values
 */
void create_noise_jpeg(const char *filename)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *outfile;
    JSAMPROW row_pointer[1];
    int row_stride;
    unsigned char *image_buffer;
    int width = 320;
    int height = 240;
    
    printf("[*] Creating noise JPEG: %s\n", filename);
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    
    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "Can't open %s\n", filename);
        return;
    }
    
    jpeg_stdio_dest(&cinfo, outfile);
    
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;
    cinfo.data_precision = 8;
    
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 95, FALSE);
    
    jpeg_start_compress(&cinfo, TRUE);
    
    row_stride = width;
    image_buffer = (unsigned char *)malloc(row_stride);
    
    /* Random noise */
    srand(12345);
    while (cinfo.next_scanline < cinfo.image_height) {
        for (int i = 0; i < width; i++) {
            image_buffer[i] = rand() % 256;
        }
        row_pointer[0] = image_buffer;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    
    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
    free(image_buffer);
    
    printf("    ✓ Created %s\n", filename);
}

int main(void)
{
    printf("========================================\n");
    printf("Creating Extreme Value JPEG Files\n");
    printf("========================================\n\n");
    
    printf("These JPEGs contain patterns designed to produce\n");
    printf("extreme coefficient values during DCT processing,\n");
    printf("which may cause out-of-bounds access to sample_range_limit.\n\n");
    
    create_extreme_dc_jpeg("extreme_dc.jpg");
    create_checkerboard_jpeg("extreme_checkerboard.jpg");
    create_noise_jpeg("extreme_noise.jpg");
    
    printf("\n========================================\n");
    printf("Test with djpeg (ASan enabled):\n");
    printf("========================================\n");
    printf("  cd build_asan\n");
    printf("  ./djpeg-static -outfile /tmp/out.ppm ../extreme_dc.jpg\n");
    printf("  ./djpeg-static -outfile /tmp/out.ppm ../extreme_checkerboard.jpg\n");
    printf("  ./djpeg-static -outfile /tmp/out.ppm ../extreme_noise.jpg\n");
    printf("\n");
    
    return 0;
}
