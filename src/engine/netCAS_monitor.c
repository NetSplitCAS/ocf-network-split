/*
netCAS monitor module
*/

#include "ocf/ocf.h"
#include "../ocf_cache_priv.h"
#include "netCAS_monitor.h"
#include "engine_debug.h"
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/atomic.h>

#define OCF_ENGINE_DEBUG_IO_NAME "netCAS_monitor"
#include "engine_debug.h"

// Constants
const uint64_t REQUEST_BLOCK_SIZE = 64;
static const char *CAS_STAT_FILE = "/sys/block/cas1-1/stat";

// Static variables for OpenCAS stats
static uint64_t prev_reads_from_core = 0;
static uint64_t prev_reads_from_cache = 0;
static bool opencas_stats_initialized = false;

// Static variables for disk stats
static uint64_t prev_reads = 0, prev_writes = 0;
static bool disk_stats_initialized = false;

// RDMA metrics structure
struct rdma_metrics
{
    uint64_t latency;
    uint64_t throughput;
};

uint64_t measure_iops_using_opencas_stats(struct ocf_request *req, uint64_t elapsed_time /* ms */)
{
    uint64_t reads_from_core = 0;
    uint64_t reads_from_cache = 0;
    uint64_t curr_IO = 0;
    uint64_t curr_IOPS = 0;

    struct ocf_stats_core stats;
    if (ocf_core_get_stats(req->core, &stats) != 0)
    {
        // Failed to get stats, return 0 or error code
        return 0;
    }

    reads_from_cache = stats.cache_volume.read;
    reads_from_core = stats.core_volume.read;

    if (!opencas_stats_initialized)
    {
        prev_reads_from_core = reads_from_core;
        prev_reads_from_cache = reads_from_cache;
        opencas_stats_initialized = true;
        return 0; // Not enough data to calculate IOPS yet
    }

    curr_IO = (reads_from_core - prev_reads_from_core) + (reads_from_cache - prev_reads_from_cache);

    prev_reads_from_core = reads_from_core;
    prev_reads_from_cache = reads_from_cache;

    // Convert to IOPS per second (assuming elapsed_time is in ms)
    curr_IOPS = (elapsed_time > 0) ? (curr_IO / REQUEST_BLOCK_SIZE) / elapsed_time : 0;

    return curr_IOPS;
}

uint64_t measure_iops_using_disk_stats(struct ocf_request *req, uint64_t elapsed_time /* ms */)
{
    struct file *cas_file;
    uint64_t reads = 0, writes = 0;
    uint64_t delta_reads, delta_writes, iops = 0;
    static char cas_buf[1024];
    char *buf_ptr, *token;
    int count, read_bytes;
    mm_segment_t old_fs;

    /* Prepare for kernel file operations */
    old_fs = get_fs();
    set_fs(KERNEL_DS);

    /* Open CAS stat file */
    cas_file = filp_open(CAS_STAT_FILE, O_RDONLY, 0);
    if (IS_ERR(cas_file))
    {
        OCF_DEBUG_RQ(req, "disk_stats - Failed to open CAS file: %ld", PTR_ERR(cas_file));
        set_fs(old_fs);
        return 0;
    }

    /* Read CAS stats */
    memset(cas_buf, 0, sizeof(cas_buf));
    read_bytes = kernel_read(cas_file, cas_buf, sizeof(cas_buf) - 1, &cas_file->f_pos);
    if (read_bytes <= 0)
    {
        OCF_DEBUG_RQ(req, "disk_stats - Failed to read CAS file: %d", read_bytes);
        filp_close(cas_file, NULL);
        set_fs(old_fs);
        return 0;
    }
    cas_buf[read_bytes] = '\0';

    /* Close file and restore fs */
    filp_close(cas_file, NULL);
    set_fs(old_fs);

    /* Parse CAS stats */
    buf_ptr = cas_buf;
    while (*buf_ptr == ' ')
        buf_ptr++;

    for (count = 0; count < 5; ++count)
    {
        token = buf_ptr;
        while (*buf_ptr != ' ' && *buf_ptr != '\0')
            buf_ptr++;
        if (*buf_ptr == '\0')
            break;
        *buf_ptr++ = '\0';
        while (*buf_ptr == ' ')
            buf_ptr++;

        if (count == 0)
        {
            if (kstrtoull(token, 10, &reads))
                return 0;
        }
        else if (count == 4)
        {
            if (kstrtoull(token, 10, &writes))
                return 0;
        }
    }

    /* Initialize stats on first run */
    if (!disk_stats_initialized)
    {
        prev_reads = reads;
        prev_writes = writes;
        disk_stats_initialized = true;
        return 0;
    }

    /* Calculate deltas */
    delta_reads = reads - prev_reads;
    delta_writes = writes - prev_writes;

    /* Update previous values */
    prev_reads = reads;
    prev_writes = writes;

    /* Calculate IOPS = (reads + writes) / seconds */
    if (elapsed_time > 0)
        iops = ((delta_reads + delta_writes) * 1000) / elapsed_time;

    return iops;
}

struct rdma_metrics read_rdma_metrics(struct ocf_request *req)
{
    struct file *latency_file, *throughput_file;
    char buffer[32];
    int read_bytes;
    mm_segment_t old_fs;
    struct rdma_metrics metrics = {0, 0};

    /* Prepare for kernel file operations */
    old_fs = get_fs();
    set_fs(KERNEL_DS);

    /* Read latency */
    latency_file = filp_open("/sys/kernel/rdma_metrics/latency", O_RDONLY, 0);
    if (!IS_ERR(latency_file))
    {
        memset(buffer, 0, sizeof(buffer));
        read_bytes = kernel_read(latency_file, buffer, sizeof(buffer) - 1, &latency_file->f_pos);
        if (read_bytes > 0)
        {
            buffer[read_bytes] = '\0';
            if (kstrtoull(buffer, 10, &metrics.latency))
            {
                OCF_DEBUG_RQ(req, "Failed to parse RDMA latency");
            }
        }
        filp_close(latency_file, NULL);
    }
    else
    {
        OCF_DEBUG_RQ(req, "Failed to open RDMA latency file: %ld", PTR_ERR(latency_file));
    }

    /* Read throughput */
    throughput_file = filp_open("/sys/kernel/rdma_metrics/throughput", O_RDONLY, 0);
    if (!IS_ERR(throughput_file))
    {
        memset(buffer, 0, sizeof(buffer));
        read_bytes = kernel_read(throughput_file, buffer, sizeof(buffer) - 1, &throughput_file->f_pos);
        if (read_bytes > 0)
        {
            buffer[read_bytes] = '\0';
            if (kstrtoull(buffer, 10, &metrics.throughput))
            {
                OCF_DEBUG_RQ(req, "Failed to parse RDMA throughput");
            }
        }
        filp_close(throughput_file, NULL);
    }
    else
    {
        OCF_DEBUG_RQ(req, "Failed to open RDMA throughput file: %ld", PTR_ERR(throughput_file));
    }

    /* Restore fs */
    set_fs(old_fs);

    // OCF_DEBUG_RQ(req, "RDMA metrics - latency: %llu, throughput: %llu", metrics.latency, metrics.throughput);

    return metrics;
}

struct rdma_metrics measure_performance(struct ocf_request *req, uint64_t elapsed_time)
{
    // TODO: Do we need to measure IOPS?
    // IOPS variables
    uint64_t curr_opencas_IOPS;
    uint64_t curr_disk_IOPS;

    // 1. Measure IOPS
    curr_opencas_IOPS = measure_iops_using_opencas_stats(req, elapsed_time);
    curr_disk_IOPS = measure_iops_using_disk_stats(req, elapsed_time);

    // 2. Read RDMA metrics
    struct rdma_metrics rdma_metrics = read_rdma_metrics(req);

    // Log results
    // OCF_DEBUG_RQ(req, "curr_opencas_IOPS: %llu, curr_disk_IOPS: %llu",
    //              curr_opencas_IOPS, curr_disk_IOPS);

    return rdma_metrics;
}