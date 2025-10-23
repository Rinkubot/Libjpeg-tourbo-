/*
 * Test program to trigger type confusion vulnerability in libjpeg-turbo
 * Compile with ASan-enabled library
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
 * Test function that processes a JPEG and triggers the vulnerability
 * by forcing access to sample_range_limit with different type assumptions
 */
int test_jpeg_type_confusion(const char *filename)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  FILE *infile;
  JSAMPARRAY buffer;
  int row_stride;
  
  printf("\n[*] Testing: %s\n", filename);
  
  if ((infile = fopen(filename, "rb")) == NULL) {
    fprintf(stderr, "Can't open %s\n", filename);
    return 0;
  }
  
  /* Setup error handling */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  
  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return 0;
  }
  
  /* Initialize decompressor */
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, infile);
  
  /* Read header */
  (void)jpeg_read_header(&cinfo, TRUE);
  
  printf("    Image size: %d x %d\n", cinfo.image_width, cinfo.image_height);
  printf("    Components: %d\n", cinfo.num_components);
  printf("    Data precision: %d bits\n", cinfo.data_precision);
  printf("    Color space: %d\n", cinfo.jpeg_color_space);
  
  /* Start decompression */
  (void)jpeg_start_decompress(&cinfo);
  
  row_stride = cinfo.output_width * cinfo.output_components;
  buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);
  
  printf("    Output size: %d x %d\n", cinfo.output_width, cinfo.output_height);
  printf("    Output components: %d\n", cinfo.output_components);
  
  /* This is where the vulnerability can be triggered */
  /* The sample_range_limit is accessed during IDCT and color conversion */
  printf("    Processing scanlines...\n");
  
  int scanlines_read = 0;
  while (cinfo.output_scanline < cinfo.output_height) {
    (void)jpeg_read_scanlines(&cinfo, buffer, 1);
    scanlines_read++;
    
    /* Print progress every 100 lines */
    if (scanlines_read % 100 == 0) {
      printf("    ... processed %d scanlines\n", scanlines_read);
    }
  }
  
  printf("    Total scanlines processed: %d\n", scanlines_read);
  
  /* Finish decompression */
  (void)jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  
  fclose(infile);
  
  printf("    [✓] Completed successfully\n");
  return 1;
}

/*
 * Test that specifically tries to trigger type confusion
 * by manipulating the decompressor state
 */
int test_precision_manipulation(const char *filename)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  FILE *infile;
  
  printf("\n[*] Testing precision manipulation: %s\n", filename);
  
  if ((infile = fopen(filename, "rb")) == NULL) {
    fprintf(stderr, "Can't open %s\n", filename);
    return 0;
  }
  
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  
  if (setjmp(jerr.setjmp_buffer)) {
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return 0;
  }
  
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, infile);
  
  printf("    Initial data_precision: %d\n", cinfo.data_precision);
  
  (void)jpeg_read_header(&cinfo, TRUE);
  
  printf("    After header data_precision: %d\n", cinfo.data_precision);
  printf("    sample_range_limit: %p\n", (void *)cinfo.sample_range_limit);
  
  /* Try to access sample_range_limit before it's initialized */
  /* This could trigger issues if the precision is mismatched */
  
  (void)jpeg_start_decompress(&cinfo);
  
  printf("    After start data_precision: %d\n", cinfo.data_precision);
  printf("    sample_range_limit: %p\n", (void *)cinfo.sample_range_limit);
  
  /* Now try to read some data */
  JSAMPARRAY buffer;
  int row_stride = cinfo.output_width * cinfo.output_components;
  buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);
  
  /* Read a few scanlines to trigger IDCT/color conversion */
  int lines_to_read = (cinfo.output_height < 10) ? cinfo.output_height : 10;
  for (int i = 0; i < lines_to_read && cinfo.output_scanline < cinfo.output_height; i++) {
    (void)jpeg_read_scanlines(&cinfo, buffer, 1);
    
    /* Try to directly access sample_range_limit as different types */
    JSAMPLE *limit8 = cinfo.sample_range_limit;
    printf("    Scanline %d: limit8[128] = 0x%02x\n", i, limit8[128]);
    
    /* This is the type confusion: accessing as 12-bit when it might be 8-bit */
    if (cinfo.data_precision > 8) {
      typedef short J12SAMPLE;
      J12SAMPLE *limit12 = (J12SAMPLE *)cinfo.sample_range_limit;
      printf("    Scanline %d: limit12[128] = 0x%04x (interpreted as 12-bit)\n", 
             i, limit12[128]);
    }
  }
  
  jpeg_destroy_decompress(&cinfo);
  fclose(infile);
  
  printf("    [✓] Test completed\n");
  return 1;
}

int main(int argc, char **argv)
{
  printf("========================================\n");
  printf("libjpeg-turbo Type Confusion Test\n");
  printf("========================================\n");
  printf("Testing with AddressSanitizer enabled\n");
  printf("========================================\n");
  
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <jpeg_file> [jpeg_file2 ...]\n", argv[0]);
    return 1;
  }
  
  for (int i = 1; i < argc; i++) {
    test_jpeg_type_confusion(argv[i]);
    test_precision_manipulation(argv[i]);
  }
  
  printf("\n========================================\n");
  printf("All tests completed\n");
  printf("========================================\n");
  
  return 0;
}
