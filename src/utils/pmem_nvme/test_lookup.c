/*
 * Test program for pmem_nvme_table.h
 * This is a userspace-only test program and should not be compiled in kernel context
 */

#ifdef __KERNEL__
/*
 * If being compiled in kernel context, create an empty file
 * This allows the build to proceed without errors
 */
#else /* Userspace compilation */

#include <stdio.h>
#include <stdint.h>
#include "pmem_nvme_table.h"

void find_best_split_ratio(int io_depth, int num_job)
{
    int best_split = 0;
    int best_bandwidth = 0;
    int split;
    int bandwidth;

    printf("\nTesting lookup for IO_Depth=%d, NumJob=%d\n", io_depth, num_job);

    /* Try all split ratios from 0 to 100 in steps of 5 */
    for (split = 0; split <= 100; split += 5)
    {
        bandwidth = lookup_bandwidth(io_depth, num_job, split);
        printf("  Split %d:%d -> Bandwidth: %d\n", split, 100 - split, bandwidth);

        if (bandwidth > best_bandwidth)
        {
            best_bandwidth = bandwidth;
            best_split = split;
        }
    }

    printf("\nBest split ratio for IO_Depth=%d, NumJob=%d is %d:%d with bandwidth %d\n",
           io_depth, num_job, best_split, 100 - best_split, best_bandwidth);
}

int main()
{
    /* Test variables */
    uint64_t device_a_iops;
    uint64_t device_b_iops;
    uint64_t split_ratio;
    uint64_t combined_iops;
    uint64_t device_a_actual;
    uint64_t device_b_actual;
    float a_percent;
    float b_percent;

    /* Test case from the original question */
    find_best_split_ratio(32, 16);

    /* Test a few more combinations */
    find_best_split_ratio(1, 1);
    find_best_split_ratio(1, 8);
    find_best_split_ratio(4, 4);
    find_best_split_ratio(8, 16);
    find_best_split_ratio(16, 32);

    /* Now test the combined_iops calculation */
    printf("\nTesting combined_iops calculation:\n");
    device_a_iops = 53449;
    device_b_iops = 14847;
    split_ratio = 80; /* 80% to device A, 20% to device B */

    combined_iops = calculate_combined_iops(device_a_iops, device_b_iops, split_ratio);

    printf("Device A IOPS: %lu\n", device_a_iops);
    printf("Device B IOPS: %lu\n", device_b_iops);
    printf("Split Ratio: %lu:%lu\n", split_ratio, 100 - split_ratio);
    printf("Combined IOPS: %lu\n", combined_iops);

    /* Calculate how much each device handles in the combined system */
    device_a_actual = (combined_iops * split_ratio) / 100;
    device_b_actual = (combined_iops * (100 - split_ratio)) / 100;
    a_percent = (float)device_a_actual / device_a_iops * 100;
    b_percent = (float)device_b_actual / device_b_iops * 100;

    printf("Device A handles: %lu IOPS (%.2f%% of its capacity)\n",
           device_a_actual, a_percent);
    printf("Device B handles: %lu IOPS (%.2f%% of its capacity)\n",
           device_b_actual, b_percent);

    return 0;
}

#endif /* __KERNEL__ */