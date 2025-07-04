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
#include "../utils/pmem_nvme/pmem_nvme_table.h"

/** Enable kernel verbose logging? */
static const bool SPLIT_VERBOSE_LOG = true;

// Test app parameters
const uint64_t IO_DEPTH = 16;
const uint64_t NUM_JOBS = 1;
const bool CACHING_FAILED = false;

// Moving average window for RDMA throughput
static uint64_t rdma_throughput_window[RDMA_WINDOW_SIZE] = {0};
static uint64_t rdma_window_index = 0;
static uint64_t rdma_window_sum = 0;
static uint64_t rdma_window_count = 0;
static uint64_t rdma_window_average = 0;
static uint64_t max_average_rdma_throughput = 0;

// Mode management variables
static uint64_t last_nonzero_transition_time = 0; // Time when RDMA throughput changed from 0 to non-zero
static bool netCAS_initialized = false;
static bool split_ratio_calculated_in_stable = false; // Track if split ratio was calculated in stable mode

/** Optimal split ratio, protected by a global rwlock. */
static uint64_t optimal_split_ratio = 100; // Default 100% to cache

/** `data_admit` switch, protected by a global rwlock. */
static bool global_data_admit = true;

/** Reader-writer lock to protect optimal_split_ratio. */
static env_rwlock split_ratio_lock;

/** Reader-writer lock to protect `data_admit`. */
static env_rwlock data_admit_lock;

/**
 * Set split ratio value with writer lock.
 */
static void
split_set_optimal_ratio(uint64_t ratio)
{
    env_rwlock_write_lock(&split_ratio_lock);
    optimal_split_ratio = ratio;
    env_rwlock_write_unlock(&split_ratio_lock);
}

/**
 * For OCF engine to query the optimal split ratio.
 */
uint64_t netcas_query_optimal_split_ratio(void)
{
    uint64_t ratio;

    env_rwlock_read_lock(&split_ratio_lock);
    ratio = optimal_split_ratio;
    env_rwlock_read_unlock(&split_ratio_lock);

    return ratio;
}

/**
 * Set data admit value with writer lock.
 */
void netcas_set_data_admit(bool data_admit)
{
    env_rwlock_write_lock(&data_admit_lock);
    global_data_admit = data_admit;
    env_rwlock_write_unlock(&data_admit_lock);
}

/**
 * For OCF engine to query the data admit switch value.
 */
bool netcas_query_data_admit(void)
{
    bool data_admit;

    env_rwlock_read_lock(&data_admit_lock);
    data_admit = global_data_admit;
    env_rwlock_read_unlock(&data_admit_lock);

    return data_admit;
}

/**
 * Calculate split ratio using the formula A/(A+B) * 100.
 * This is the core formula for determining optimal split ratio.
 */
static uint64_t
calculate_split_ratio_formula(uint64_t bandwidth_cache_only, uint64_t bandwidth_backend_only)
{
    uint64_t calculated_split;

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
static uint64_t
find_best_split_ratio(ocf_core_t core, uint64_t io_depth, uint64_t numjob, uint64_t curr_rdma_throughput, uint64_t drop_permil)
{
    uint64_t bandwidth_cache_only;   /* A: IOPS when split ratio is 100% (all to cache) */
    uint64_t bandwidth_backend_only; /* B: IOPS when split ratio is 0% (all to backend) */
    uint64_t calculated_split;       /* Calculated optimal split ratio */

    /* Get bandwidth for cache only (split ratio 100%) */
    bandwidth_cache_only = (uint64_t)lookup_bandwidth(io_depth, numjob, 100);
    /* Get bandwidth for backend only (split ratio 0%) */
    bandwidth_backend_only = (uint64_t)lookup_bandwidth(io_depth, numjob, 0);

    if (max_average_rdma_throughput == 0)
    {
        return 100;
    }

    // If current RDMA throughput is less than 9% of max_average_rdma_throughput,
    // change netCAS_mode to NETCAS_MODE_Congestion
    if (curr_rdma_throughput > RDMA_THRESHOLD)
    {
        bandwidth_backend_only = (uint64_t)((bandwidth_backend_only * (1000 - drop_permil)) / 1000);
    }

    /* Calculate optimal split ratio using the formula */
    calculated_split = calculate_split_ratio_formula(bandwidth_cache_only, bandwidth_backend_only);

    if (SPLIT_VERBOSE_LOG)
    {
        printk(KERN_ALERT "NETCAS_SPLIT: Optimal split ratio for IO_Depth=%llu, NumJob=%llu is %llu:%llu (cache_iops=%llu, adjusted_backend_iops=%llu)",
               io_depth, numjob, calculated_split, 100 - calculated_split,
               bandwidth_cache_only, bandwidth_backend_only);
    }

    return calculated_split;
}

static void init_netCAS(void)
{
    // Initialize RDMA throughput window
    uint64_t i;
    for (i = 0; i < RDMA_WINDOW_SIZE; ++i)
        rdma_throughput_window[i] = 0;
    rdma_window_sum = 0;
    rdma_window_index = 0;
    rdma_window_count = 0;
    rdma_window_average = 0;
    max_average_rdma_throughput = 0;

    // Initialize data admit
    netcas_set_data_admit(true);

    // Initialize split ratio
    split_set_optimal_ratio(100);

    // Initialize netCAS variables
    last_nonzero_transition_time = 0;
    netCAS_initialized = true;
    split_ratio_calculated_in_stable = false;
}

static netCAS_mode_t determine_netcas_mode(uint64_t curr_rdma_throughput, uint64_t drop_permil)
{
    static netCAS_mode_t current_mode = NETCAS_MODE_IDLE;
    uint64_t curr_time = env_get_tick_count();

    // No Active RDMA traffic, set netCAS_mode to IDLE
    if (curr_rdma_throughput <= RDMA_THRESHOLD)
    {
        current_mode = NETCAS_MODE_IDLE;
        last_nonzero_transition_time = 0;
    }
    // Active RDMA traffic, determine the mode
    else
    {
        // First time active RDMA traffic, set netCAS_mode to WARMUP
        if (current_mode == NETCAS_MODE_IDLE)
        {
            // Idle -> Warmup
            current_mode = NETCAS_MODE_WARMUP;
            last_nonzero_transition_time = curr_time;
            netCAS_initialized = false;
        }
        else if (current_mode == NETCAS_MODE_WARMUP && curr_time - last_nonzero_transition_time >= WARMUP_PERIOD_NS)
        {
            // Still in warmup, return
            ;
        }
        else if (current_mode == NETCAS_MODE_WARMUP)
        {
            // Warmup -> Stable
            current_mode = NETCAS_MODE_STABLE;
            split_ratio_calculated_in_stable = false; // Reset flag when entering stable mode
        }
        else if (current_mode == NETCAS_MODE_CONGESTION && drop_permil < CONGESTION_THRESHOLD)
        {
            // Congestion -> Stable
            current_mode = NETCAS_MODE_STABLE;
            split_ratio_calculated_in_stable = false; // Reset flag when entering stable mode
        }
        else if (current_mode == NETCAS_MODE_STABLE && drop_permil > CONGESTION_THRESHOLD)
        {
            // Stable -> Congestion
            current_mode = NETCAS_MODE_CONGESTION;
            split_ratio_calculated_in_stable = true; // Set flag when entering congestion
        }
        else if (CACHING_FAILED)
        {
            current_mode = NETCAS_MODE_FAILURE;
        }
    }
    return current_mode;
}

static void update_rdma_window(uint64_t curr_rdma_throughput)
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

/**
 * Split ratio monitor thread logic.
 */
static int
split_monitor_func(void *core_ptr)
{
    ocf_core_t core = core_ptr;
    uint64_t split_ratio;
    uint64_t drop_permil = 0;
    netCAS_mode_t netCAS_mode = NETCAS_MODE_IDLE;
    uint64_t curr_rdma_throughput;
    struct rdma_metrics current_rdma_metrics;

    if (SPLIT_VERBOSE_LOG)
        printk(KERN_ALERT "NETCAS_SPLIT: Monitor thread started\n");

    while (1)
    {
        if (kthread_should_stop())
        {
            env_rwlock_destroy(&split_ratio_lock);
            env_rwlock_destroy(&data_admit_lock);
            if (SPLIT_VERBOSE_LOG)
                printk(KERN_ALERT "NETCAS_SPLIT: Monitor thread stopping\n");
            break;
        }

        // Get current time and RDMA metrics
        current_rdma_metrics = measure_performance();
        curr_rdma_throughput = current_rdma_metrics.throughput;
        if (max_average_rdma_throughput > 0)
        {
            drop_permil = ((max_average_rdma_throughput - rdma_window_average) * 1000) / max_average_rdma_throughput;
        }

        // Mode management logic
        netCAS_mode = determine_netcas_mode(curr_rdma_throughput, drop_permil);

        switch (netCAS_mode)
        {
        case NETCAS_MODE_IDLE:
            if (SPLIT_VERBOSE_LOG)
                printk(KERN_ALERT "NETCAS_SPLIT: Idle mode\n");
            if (!netCAS_initialized)
            {
                init_netCAS();
            }
            break;

        case NETCAS_MODE_WARMUP:
            if (SPLIT_VERBOSE_LOG)
                printk(KERN_ALERT "NETCAS_SPLIT: Warmup mode\n");
            netcas_set_data_admit(false);
            break;

        case NETCAS_MODE_STABLE:
            if (SPLIT_VERBOSE_LOG)
                printk(KERN_ALERT "NETCAS_SPLIT: Stable mode\n");
            netcas_set_data_admit(false);
            update_rdma_window(curr_rdma_throughput);

            // Only calculate split ratio once in stable mode
            if (!split_ratio_calculated_in_stable && rdma_window_count >= RDMA_WINDOW_SIZE)
            {
                split_ratio = find_best_split_ratio(core, IO_DEPTH, NUM_JOBS, curr_rdma_throughput, drop_permil);
                optimal_split_ratio = split_ratio;
                split_set_optimal_ratio(optimal_split_ratio);
                split_ratio_calculated_in_stable = true; // Mark as calculated
                if (SPLIT_VERBOSE_LOG)
                {
                    printk(KERN_ALERT "NETCAS_SPLIT: Split ratio calculated once in stable mode: %llu\n",
                           split_ratio);
                }
            }
            break;

        case NETCAS_MODE_CONGESTION:
            if (SPLIT_VERBOSE_LOG)
                printk(KERN_ALERT "NETCAS_SPLIT: Congestion mode\n");
            netcas_set_data_admit(false);
            update_rdma_window(curr_rdma_throughput);

            // Continuously calculate split ratio in congestion mode
            if (rdma_window_count >= RDMA_WINDOW_SIZE)
            {
                split_ratio = find_best_split_ratio(core, IO_DEPTH, NUM_JOBS, curr_rdma_throughput, drop_permil);

                // Update the split ratio if it changed
                if (split_ratio != optimal_split_ratio)
                {
                    optimal_split_ratio = split_ratio;
                    split_set_optimal_ratio(split_ratio);
                    if (SPLIT_VERBOSE_LOG)
                    {
                        printk(KERN_ALERT "NETCAS_SPLIT: Split ratio updated in congestion mode: %llu\n",
                               split_ratio);
                    }
                }
            }
            break;

        case NETCAS_MODE_FAILURE:
            if (SPLIT_VERBOSE_LOG)
                printk(KERN_ALERT "NETCAS_SPLIT: Failure mode\n");
            break;
        }
    }

    // Sleep for the monitoring interval
    msleep(MONITOR_INTERVAL_MS);

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

    init_netCAS();

    env_rwlock_init(&split_ratio_lock);
    env_rwlock_init(&data_admit_lock);

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
