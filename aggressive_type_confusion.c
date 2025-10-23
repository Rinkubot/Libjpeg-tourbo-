/*
 * Aggressive type confusion test - access far out of bounds
 * 
 * The sample_range_limit buffer for 8-bit is:
 *   (5 * (MAXJSAMPLE + 1) + CENTERJSAMPLE) * sizeof(JSAMPLE)
 *   = (5 * 256 + 128) * 1 = 1408 bytes
 * 
 * When accessed as J12SAMPLE (2 bytes), we have 704 elements
 * Accessing beyond element 704 should trigger ASan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <setjmp.h>

#define MAXJSAMPLE       255
#define CENTERJSAMPLE    128

struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
  struct my_error_mgr *myerr = (struct my_error_mgr *)cinfo->err;
  (*cinfo->err->output_message)(cinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

void aggressive_oob_test(const char *filename)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  FILE *infile;
  
  printf("\n========================================\n");
  printf("AGGRESSIVE OUT-OF-BOUNDS ACCESS TEST\n");
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
    printf("\n[!] CAUGHT ERROR - Test terminated early\n");
    return;
  }
  
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, infile);
  (void)jpeg_read_header(&cinfo, TRUE);
  (void)jpeg_start_decompress(&cinfo);
  
  printf("[*] Data precision: %d bits\n", cinfo.data_precision);
  printf("[*] sample_range_limit: %p\n", (void *)cinfo.sample_range_limit);
  
  if (cinfo.data_precision == 8 && cinfo.sample_range_limit) {
    typedef short J12SAMPLE;
    
    /* For 8-bit precision:
     * Buffer size = (5 * 256 + 128) * 1 = 1408 bytes
     * Pointer is offset by (MAXJSAMPLE + 1) = 256 bytes
     * So usable range is index -256 to +1152 (1408 bytes total)
     * 
     * When interpreted as J12SAMPLE* (2 bytes each):
     * We have 704 elements (1408 / 2)
     * Offset accounts for 128 elements (256 / 2)
     * So valid range as J12SAMPLE* is index -128 to +576
     */
    
    unsigned char *limit8 = cinfo.sample_range_limit;
    J12SAMPLE *limit12 = (J12SAMPLE *)cinfo.sample_range_limit;
    
    printf("\n[*] Buffer analysis:\n");
    printf("    8-bit buffer size: %d bytes\n", 5 * (MAXJSAMPLE + 1) + CENTERJSAMPLE);
    printf("    When interpreted as 12-bit: %d elements\n", 
           (5 * (MAXJSAMPLE + 1) + CENTERJSAMPLE) / 2);
    printf("    Offset compensation: %d bytes = %d 12-bit elements\n",
           MAXJSAMPLE + 1, (MAXJSAMPLE + 1) / 2);
    
    printf("\n[*] Testing valid 8-bit accesses:\n");
    printf("    limit8[0] = 0x%02x\n", limit8[0]);
    printf("    limit8[255] = 0x%02x\n", limit8[255]);
    printf("    limit8[500] = 0x%02x\n", limit8[500]);
    
    printf("\n[*] Testing 12-bit access (type confusion):\n");
    printf("    limit12[0] = 0x%04x\n", limit12[0]);
    printf("    limit12[255] = 0x%04x\n", limit12[255]);
    
    printf("\n[*] Accessing near the boundary:\n");
    printf("    limit12[700] = 0x%04x\n", limit12[700]);
    
    printf("\n[!] CRITICAL: Accessing beyond allocated buffer!\n");
    printf("    This SHOULD trigger AddressSanitizer error:\n");
    
    /* These should definitely be out of bounds */
    for (int i = 800; i <= 1000; i += 50) {
      printf("    limit12[%d] = 0x%04x\n", i, limit12[i]);
    }
    
    printf("\n[!] Accessing WAY beyond buffer:\n");
    for (int i = 2000; i <= 5000; i += 1000) {
      printf("    limit12[%d] = 0x%04x\n", i, limit12[i]);
    }
    
    printf("\n[!] Attempting write to out-of-bounds location:\n");
    printf("    Writing to limit12[1000]...\n");
    limit12[1000] = 0xDEAD;
    printf("    Write completed!\n");
    
    printf("\n[!] Writing far out of bounds:\n");
    limit12[5000] = 0xBEEF;
    printf("    Wrote to limit12[5000]\n");
  }
  
  jpeg_destroy_decompress(&cinfo);
  fclose(infile);
  
  printf("\n========================================\n");
}

/* Test with negative indices */
void test_negative_indices(const char *filename)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  FILE *infile;
  
  printf("\n========================================\n");
  printf("NEGATIVE INDEX TEST\n");
  printf("========================================\n\n");
  
  if ((infile = fopen(filename, "rb")) == NULL) {
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
    J12SAMPLE *limit12 = (J12SAMPLE *)cinfo.sample_range_limit;
    
    printf("[*] Testing negative indices (underflow):\n");
    
    /* The buffer is offset, so some negative indices are valid */
    printf("    limit12[-10] = 0x%04x\n", limit12[-10]);
    printf("    limit12[-50] = 0x%04x\n", limit12[-50]);
    
    printf("\n[!] Accessing way before buffer start:\n");
    printf("    limit12[-200] = 0x%04x\n", limit12[-200]);
    printf("    limit12[-500] = 0x%04x\n", limit12[-500]);
    printf("    limit12[-1000] = 0x%04x\n", limit12[-1000]);
  }
  
  jpeg_destroy_decompress(&cinfo);
  fclose(infile);
  
  printf("\n========================================\n");
}

int main(int argc, char **argv)
{
  printf("\n");
  printf("╔════════════════════════════════════════════════════════╗\n");
  printf("║  AGGRESSIVE Type Confusion Trigger                    ║\n");
  printf("║  This WILL cause out-of-bounds access                 ║\n");
  printf("╚════════════════════════════════════════════════════════╝\n");
  
  if (argc < 2) {
    fprintf(stderr, "\nUsage: %s <jpeg_file>\n\n", argv[0]);
    return 1;
  }
  
  aggressive_oob_test(argv[1]);
  test_negative_indices(argv[1]);
  
  printf("\n");
  printf("╔════════════════════════════════════════════════════════╗\n");
  printf("║  If ASan didn't trigger, the buffer may have extra    ║\n");
  printf("║  padding or the accesses were within allocated space  ║\n");
  printf("╚════════════════════════════════════════════════════════╝\n");
  printf("\n");
  
  return 0;
}
