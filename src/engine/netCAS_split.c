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

/** Optimal split ratio, protected by a global rwlock. */
static int optimal_split_ratio = 49; // Default 49% to cache

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
 * Dummy function to calculate optimal split ratio.
 * For now, returns static value 8000 (80%).
 * TODO: Implement actual algorithm here.
 */
static int
calculate_optimal_split_ratio(ocf_core_t core)
{
    // TODO: Implement your algorithm here
    // For now, return static value 8000 (80% to cache)
    return 8000;
}

/**
 * Split ratio monitor thread logic.
 */
static int
split_monitor_func(void *core_ptr)
{
    ocf_core_t core = core_ptr;
    const int MONITOR_INTERVAL_MS = 1000; // Check every 1 second

    if (SPLIT_VERBOSE_LOG)
        printk(KERN_ALERT "NETCAS_SPLIT: Monitor thread started\n");

    while (1)
    {
        int new_split_ratio;

        if (kthread_should_stop())
        {
            env_rwlock_destroy(&split_ratio_lock);
            if (SPLIT_VERBOSE_LOG)
                printk(KERN_ALERT "NETCAS_SPLIT: Monitor thread stopping\n");
            break;
        }

        // Calculate new optimal split ratio
        new_split_ratio = calculate_optimal_split_ratio(core);

        // Update the global split ratio if it changed
        if (new_split_ratio != optimal_split_ratio)
        {
            split_set_optimal_ratio(new_split_ratio);
            if (SPLIT_VERBOSE_LOG)
            {
                printk(KERN_ALERT "NETCAS_SPLIT: Split ratio updated to %d\n",
                       new_split_ratio);
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