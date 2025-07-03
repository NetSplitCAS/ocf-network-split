/**
 * netCAS split ratio management module.
 *
 * Dynamically monitors and adjusts the optimal split ratio
 * between cache and backend storage.
 */

#include <linux/fs.h>
#include <asm/segment.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include "ocf/ocf.h"
#include "cache_engine.h"
#include "engine_debug.h"
#include "../ocf_stats_priv.h"
#include "../ocf_core_priv.h"
#include "netCAS_split.h"
#include "netCAS_monitor.h"

/** Enable kernel verbose logging? */
static const bool SPLIT_VERBOSE_LOG = true;

// Test app parameters
const uint64_t IO_DEPTH = 16;
const uint64_t NUM_JOBS = 1;

// Moving average window for RDMA throughput
static uint64_t rdma_throughput_window[RDMA_WINDOW_SIZE] = {0};
static int rdma_window_index = 0;
static uint64_t rdma_window_sum = 0;
static int rdma_window_count = 0;
static uint64_t rdma_window_average = 0;
static uint64_t max_average_rdma_throughput = 0;

// Mode management variables
static uint64_t last_nonzero_transition_time = 0; // Time when RDMA throughput changed from 0 to non-zero
static bool in_warmup = false;
static bool just_initialized = false;

static netCAS_mode_t netCAS_mode = NETCAS_MODE_IDLE;

/** Optimal split ratio, protected by a global rwlock. */
static int optimal_split_ratio = 100; // Default 100% to cache

/** Reader-writer lock to protect optimal_split_ratio. */
static env_rwlock split_ratio_lock;

/**
 * Set split ratio value with writer lock.
 */
static void
split_set_optimal_ratio(int ratio)
{
    env_rwlock_write_lock(&split_ratio_lock);
    optimal_split_ratio = ratio;
    env_rwlock_write_unlock(&split_ratio_lock);
}

/**
 * For OCF engine to query the optimal split ratio.
 */
int netcas_query_optimal_split_ratio(void)
{
    int ratio;

    env_rwlock_read_lock(&split_ratio_lock);
    ratio = optimal_split_ratio;
    env_rwlock_read_unlock(&split_ratio_lock);

    return ratio;
}

/**
 * Calculate split ratio using the formula A/(A+B) * 100.
 * This is the core formula for determining optimal split ratio.
 */
static int
calculate_split_ratio_formula(int bandwidth_cache_only, int bandwidth_backend_only)
{
    int calculated_split;

    /* Calculate optimal split ratio using formula A/(A+B) * 100 */
    calculated_split = (bandwidth_cache_only * 100) / (bandwidth_cache_only + bandwidth_backend_only);

    /* Ensure the result is within valid range (0-100) */
    if (calculated_split < 0)
        calculated_split = 0;
    if (calculated_split > 100)
        calculated_split = 100;

    return calculated_split;
}

/**
 * Function to find the best split ratio for given IO depth and NumJob.
 * Based on the algorithm from engine_fast.c
 */
static int
find_best_split_ratio(ocf_core_t core, int io_depth, int num_job, uint64_t curr_rdma_throughput)
{
    int bandwidth_cache_only;   /* A: IOPS when split ratio is 100% (all to cache) */
    int bandwidth_backend_only; /* B: IOPS when split ratio is 0% (all to backend) */
    int calculated_split;       /* Calculated optimal split ratio */
    uint64_t drop_permil = 0;

    /* Get bandwidth for cache only (split ratio 100%) */
    bandwidth_cache_only = lookup_bandwidth(io_depth, num_job, 100);
    /* Get bandwidth for backend only (split ratio 0%) */
    bandwidth_backend_only = lookup_bandwidth(io_depth, num_job, 0);

    if (max_average_rdma_throughput == 0)
    {
        return;
    }

    // Calculate how much RDMA throughput is dropped
    drop_permil = ((max_average_rdma_throughput - rdma_window_average) * 1000) / max_average_rdma_throughput;

    // If current RDMA throughput is less than 9% of max_average_rdma_throughput,
    // change netCAS_mode to NETCAS_MODE_Congestion
    if (drop_permil > CONGESTION_THRESHOLD)
    {
        bandwidth_backend_only = (int)((bandwidth_backend_only * (1000 - drop_permil)) / 1000);
    }

    /* Calculate optimal split ratio using the formula */
    calculated_split = calculate_split_ratio_formula(bandwidth_cache_only, bandwidth_backend_only);

    /* Store the calculated optimal split ratio in the global variable */
    split_set_optimal_ratio(calculated_split);

    if (SPLIT_VERBOSE_LOG)
    {
        printk(KERN_ALERT "NETCAS_SPLIT: Optimal split ratio for IO_Depth=%d, NumJob=%d is %d:%d (cache_iops=%d, adjusted_backend_iops=%d)",
               io_depth, num_job, calculated_split, 100 - calculated_split,
               bandwidth_cache_only, bandwidth_backend_only);
    }

    return calculated_split;
}

/**
 * Split ratio monitor thread logic.
 */
static int
split_monitor_func(void *core_ptr)
{
    ocf_core_t core = core_ptr;
    int split_ratio;
    uint64_t curr_time;
    uint64_t curr_rdma_throughput;

    if (SPLIT_VERBOSE_LOG)
        printk(KERN_ALERT "NETCAS_SPLIT: Monitor thread started\n");

    while (1)
    {
        if (kthread_should_stop())
        {
            env_rwlock_destroy(&split_ratio_lock);
            if (SPLIT_VERBOSE_LOG)
                printk(KERN_ALERT "NETCAS_SPLIT: Monitor thread stopping\n");
            break;
        }

        // Get current time and RDMA metrics
        curr_time = env_get_tick_count();
        struct rdma_metrics rdma_metrics = measure_performance();
        curr_rdma_throughput = rdma_metrics.throughput;

        // Mode management logic
        if (curr_rdma_throughput == 0)
        {
            netCAS_mode = NETCAS_MODE_IDLE;
            just_initialized = true;
            in_warmup = false;
            last_nonzero_transition_time = 0;
        }
        else if (curr_rdma_throughput > RDMA_THRESHOLD && just_initialized)
        {
            // Start warmup
            netCAS_mode = NETCAS_MODE_WARMUP;
            in_warmup = true;
            last_nonzero_transition_time = curr_time;
            if (SPLIT_VERBOSE_LOG)
                printk(KERN_ALERT "NETCAS_SPLIT: Starting warmup\n");
            just_initialized = false; // Only start warmup once per initialization
        }
        else if (in_warmup)
        {
            // If in warmup, check for end condition
            if (curr_time - last_nonzero_transition_time >= WARMUP_PERIOD_NS)
            {
                in_warmup = false;
                if (SPLIT_VERBOSE_LOG)
                    printk(KERN_ALERT "NETCAS_SPLIT: Warmup period over\n");
            }
        }
        else if (!in_warmup && !just_initialized)
        {
            netCAS_mode = NETCAS_MODE_STABLE;
        }

        // Only calculate split ratio in stable mode
        if (netCAS_mode == NETCAS_MODE_STABLE)
        {
            /* Update moving average window before using it */
            if (curr_rdma_throughput == 0)
            {
                // Reset window
                int i;
                for (i = 0; i < RDMA_WINDOW_SIZE; ++i)
                    rdma_throughput_window[i] = 0;
                rdma_window_sum = 0;
                rdma_window_index = 0;
                rdma_window_count = 0;
            }
            else
            {
                // Update window
                if (rdma_window_count < RDMA_WINDOW_SIZE)
                {
                    rdma_window_count++;
                }
                else
                {
                    rdma_window_sum -= rdma_throughput_window[rdma_window_index];
                }
                rdma_throughput_window[rdma_window_index] = curr_rdma_throughput;
                rdma_window_sum += curr_rdma_throughput;
                rdma_window_average = rdma_window_sum / rdma_window_count;
                rdma_window_index = (rdma_window_index + 1) % RDMA_WINDOW_SIZE;

                if (max_average_rdma_throughput < rdma_window_average)
                {
                    max_average_rdma_throughput = rdma_window_average;
                    if (SPLIT_VERBOSE_LOG)
                        printk(KERN_ALERT "NETCAS_SPLIT: max_average_rdma_throughput: %llu\n", max_average_rdma_throughput);
                }
            }

            // Only calculate split ratio if we have enough data
            if (rdma_window_count >= RDMA_WINDOW_SIZE)
            {
                int new_split_ratio;
                new_split_ratio = find_best_split_ratio(core, IO_DEPTH, NUM_JOBS, curr_rdma_throughput);

                // Update the local split ratio if it changed
                if (split_ratio != new_split_ratio)
                {
                    split_ratio = new_split_ratio;
                    split_set_optimal_ratio(split_ratio);
                    if (SPLIT_VERBOSE_LOG)
                    {
                        printk(KERN_ALERT "NETCAS_SPLIT: Split ratio updated to %d\n",
                               split_ratio);
                    }
                }
            }
        }

        // Sleep for the monitoring interval
        msleep(MONITOR_INTERVAL_MS);
    }

    return 0;
}

static struct task_struct *split_monitor_thread_st = NULL;

/**
 * Setup split ratio management and start the monitor thread.
 */
int netcas_mngt_split_monitor_start(ocf_core_t core)
{
    if (split_monitor_thread_st != NULL) // Already started.
        return 0;

    optimal_split_ratio = 49; // Default 49% to cache

    env_rwlock_init(&split_ratio_lock);

    /** Create the monitor thread. */
    split_monitor_thread_st = kthread_run(split_monitor_func, (void *)core,
                                          "netcas_split_monitor_thread");
    if (split_monitor_thread_st == NULL)
        return -1; // Error creating thread

    printk(KERN_ALERT "NETCAS_SPLIT: Thread %d started running\n",
           split_monitor_thread_st->pid);
    return 0;
}

/**
 * For the context to gracefully stop the monitor thread.
 */
void netcas_mngt_split_monitor_stop(void)
{
    if (split_monitor_thread_st != NULL)
    { // Only if started.
        kthread_stop(split_monitor_thread_st);
        printk(KERN_ALERT "NETCAS_SPLIT: Thread %d stop signaled\n",
               split_monitor_thread_st->pid);
        split_monitor_thread_st = NULL;
    }
}