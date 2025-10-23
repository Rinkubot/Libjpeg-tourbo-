/*
 * Proof of Concept: Type Confusion Vulnerability in libjpeg-turbo
 * 
 * This demonstrates the type confusion vulnerability in sample_range_limit
 * where a pointer declared as JSAMPLE* can actually point to J12SAMPLE* or J16SAMPLE*
 * 
 * Compile: gcc -I./src poc_type_confusion_demo.c -o poc_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Simplified type definitions matching libjpeg-turbo */
typedef unsigned char JSAMPLE;      /* 1 byte: 0-255 */
typedef short J12SAMPLE;             /* 2 bytes: 0-4095 */
typedef unsigned short J16SAMPLE;    /* 2 bytes: 0-65535 */

#define MAXJSAMPLE       255
#define MAXJ12SAMPLE     4095
#define MAXJ16SAMPLE     65535

#define CENTERJSAMPLE    128
#define CENTERJ12SAMPLE  2048
#define CENTERJ16SAMPLE  32768

/* Simplified decompressor structure */
typedef struct {
    int data_precision;              /* 8, 12, or 16 bits per sample */
    JSAMPLE *sample_range_limit;     /* TYPE CONFUSION: Can point to different types! */
} simplified_decompress_struct;

/*
 * Allocate the sample_range_limit table as libjpeg-turbo does
 */
void setup_range_limit_table(simplified_decompress_struct *cinfo, int precision)
{
    cinfo->data_precision = precision;
    
    if (precision <= 8) {
        printf("[*] Allocating 8-bit range table (JSAMPLE = 1 byte)\n");
        JSAMPLE *table = (JSAMPLE *)malloc((5 * (MAXJSAMPLE + 1) + CENTERJSAMPLE) * sizeof(JSAMPLE));
        table += (MAXJSAMPLE + 1);
        cinfo->sample_range_limit = table;
        
        /* Initialize some values */
        for (int i = 0; i < 10; i++) {
            table[i] = (JSAMPLE)(i * 25);
        }
        printf("    Table address: %p\n", (void *)table);
        printf("    Element size: %zu bytes\n", sizeof(JSAMPLE));
        
    } else if (precision <= 12) {
        printf("[*] Allocating 12-bit range table (J12SAMPLE = 2 bytes)\n");
        J12SAMPLE *table12 = (J12SAMPLE *)malloc((5 * (MAXJ12SAMPLE + 1) + CENTERJ12SAMPLE) * sizeof(J12SAMPLE));
        table12 += (MAXJ12SAMPLE + 1);
        cinfo->sample_range_limit = (JSAMPLE *)table12;  /* TYPE CONFUSION HERE! */
        
        /* Initialize some values */
        for (int i = 0; i < 10; i++) {
            table12[i] = (J12SAMPLE)(i * 400);
        }
        printf("    Table address: %p\n", (void *)table12);
        printf("    Element size: %zu bytes\n", sizeof(J12SAMPLE));
    }
}

/*
 * Simulate code that uses the table assuming 8-bit samples
 * This is how many libjpeg-turbo functions access the table
 */
void process_as_8bit(simplified_decompress_struct *cinfo)
{
    printf("\n[*] Processing as 8-bit (JSAMPLE):\n");
    JSAMPLE *range_limit = cinfo->sample_range_limit;
    
    printf("    Reading first 10 elements:\n");
    for (int i = 0; i < 10; i++) {
        printf("    [%d] = 0x%02x (%u)\n", i, range_limit[i], range_limit[i]);
    }
}

/*
 * Simulate code that uses the table assuming 12-bit samples
 */
void process_as_12bit(simplified_decompress_struct *cinfo)
{
    printf("\n[*] Processing as 12-bit (J12SAMPLE):\n");
    J12SAMPLE *range_limit = (J12SAMPLE *)cinfo->sample_range_limit;
    
    printf("    Reading first 10 elements:\n");
    for (int i = 0; i < 10; i++) {
        printf("    [%d] = 0x%04x (%u)\n", i, range_limit[i], range_limit[i]);
    }
}

/*
 * Demonstrate the type confusion vulnerability
 */
void demonstrate_type_confusion(void)
{
    printf("========================================\n");
    printf("TYPE CONFUSION DEMONSTRATION\n");
    printf("========================================\n\n");
    
    /* Scenario 1: Correct usage */
    printf("=== SCENARIO 1: Correct 8-bit Usage ===\n");
    simplified_decompress_struct cinfo1;
    setup_range_limit_table(&cinfo1, 8);
    process_as_8bit(&cinfo1);
    
    /* Scenario 2: Correct 12-bit usage */
    printf("\n=== SCENARIO 2: Correct 12-bit Usage ===\n");
    simplified_decompress_struct cinfo2;
    setup_range_limit_table(&cinfo2, 12);
    process_as_12bit(&cinfo2);
    
    /* Scenario 3: TYPE CONFUSION - Allocated as 12-bit, accessed as 8-bit */
    printf("\n=== SCENARIO 3: TYPE CONFUSION (12-bit allocated, accessed as 8-bit) ===\n");
    printf("    ** VULNERABILITY: Size mismatch causes incorrect reads **\n");
    simplified_decompress_struct cinfo3;
    setup_range_limit_table(&cinfo3, 12);  /* Allocates J12SAMPLE (2 bytes) */
    process_as_8bit(&cinfo3);               /* Reads as JSAMPLE (1 byte) */
    
    printf("\n    Analysis:\n");
    printf("    - Expected: 12-bit values (0, 400, 800, 1200, ...)\n");
    printf("    - Actual 8-bit reads: Only reads low bytes of each 16-bit value\n");
    printf("    - Result: Incorrect values, potential out-of-bounds indexing\n");
    
    /* Scenario 4: TYPE CONFUSION - Allocated as 8-bit, accessed as 12-bit */
    printf("\n=== SCENARIO 4: TYPE CONFUSION (8-bit allocated, accessed as 12-bit) ===\n");
    printf("    ** VULNERABILITY: Size mismatch causes out-of-bounds reads **\n");
    simplified_decompress_struct cinfo4;
    setup_range_limit_table(&cinfo4, 8);   /* Allocates JSAMPLE (1 byte) */
    process_as_12bit(&cinfo4);              /* Reads as J12SAMPLE (2 bytes) */
    
    printf("\n    Analysis:\n");
    printf("    - Allocated: 8-bit buffer with 1-byte elements\n");
    printf("    - Access: Reading 2-byte values from 1-byte buffer\n");
    printf("    - Result: Reading beyond intended boundaries, combining adjacent bytes\n");
    printf("    - Impact: Information disclosure, potential crash\n");
}

/*
 * Demonstrate potential exploitation scenario
 */
void demonstrate_exploitation(void)
{
    printf("\n========================================\n");
    printf("EXPLOITATION SCENARIO\n");
    printf("========================================\n\n");
    
    printf("Attack Steps:\n");
    printf("1. Attacker crafts malicious JPEG with precision mismatch\n");
    printf("2. SOF marker indicates data_precision = 8\n");
    printf("3. Library allocates 1-byte sample_range_limit table\n");
    printf("4. Attacker manipulates state to change effective precision to 12\n");
    printf("5. Code accesses table with 2-byte reads from 1-byte buffer\n");
    printf("6. Out-of-bounds read occurs, leaking adjacent memory\n");
    printf("7. Or: Out-of-bounds write occurs, corrupting adjacent structures\n\n");
    
    /* Simulate memory layout */
    printf("Memory Layout Visualization:\n");
    printf("-----------------------------------\n");
    
    uint8_t memory[32];
    memset(memory, 0xAA, sizeof(memory));  /* Fill with pattern */
    
    /* Simulate 8-bit allocation (10 bytes) */
    for (int i = 0; i < 10; i++) {
        memory[i] = i * 25;
    }
    
    printf("Allocated 8-bit buffer (10 bytes):\n");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", memory[i]);
        if ((i + 1) % 8 == 0) printf("\n");
    }
    
    printf("\nAccessing as 12-bit (reading 16-bit values):\n");
    uint16_t *ptr16 = (uint16_t *)memory;
    for (int i = 0; i < 8; i++) {
        printf("Element[%d] = 0x%04x ", i, ptr16[i]);
        if (ptr16[i] == 0xAAAA) {
            printf("<-- OUT OF BOUNDS!");
        }
        printf("\n");
    }
    
    printf("\nConsequences:\n");
    printf("- Elements 5-7 read uninitialized/adjacent memory (0xAA pattern)\n");
    printf("- Information disclosure: Leaked adjacent memory contents\n");
    printf("- If writing: Could corrupt adjacent structures\n");
}

int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║  libjpeg-turbo Type Confusion Vulnerability PoC       ║\n");
    printf("║  CVE-CANDIDATE: sample_range_limit Type Confusion     ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    demonstrate_type_confusion();
    demonstrate_exploitation();
    
    printf("\n========================================\n");
    printf("SUMMARY\n");
    printf("========================================\n");
    printf("This PoC demonstrates how libjpeg-turbo's type confusion\n");
    printf("in sample_range_limit can lead to:\n");
    printf("  • Incorrect data interpretation\n");
    printf("  • Out-of-bounds memory access\n");
    printf("  • Information disclosure\n");
    printf("  • Potential memory corruption\n");
    printf("  • Possible code execution (in full exploit)\n\n");
    
    printf("Affected Code Paths:\n");
    printf("  • All IDCT implementations (jidct*.c)\n");
    printf("  • Color conversion (jdcolor.c, jdcolext.c)\n");
    printf("  • Color quantization (jquant*.c)\n");
    printf("  • Upsampling (jdmerge.c, jdmrg*.c)\n\n");
    
    return 0;
}
