/**
 * netCAS split ratio management module.
 *
 * Dynamically monitors and adjusts the optimal split ratio
 * between cache and backend storage.
 */

#ifndef NETCAS_SPLIT_H_
#define NETCAS_SPLIT_H_

#include "ocf/ocf.h"
#include "netCAS_monitor.h"

/* External function declaration */
extern int lookup_bandwidth(int io_depth, int num_job, int split_ratio);

/* Constants */
#define RDMA_WINDOW_SIZE 20
#define MONITOR_INTERVAL_MS 1000        /* Check every 1 second */
#define WARMUP_PERIOD_NS 10000000000ULL /* 10 seconds in nanoseconds */
#define RDMA_THRESHOLD 100              /* Threshold for starting warmup */
#define CONGESTION_THRESHOLD 90         /* 90% drop threshold for congestion mode */

/* Test app parameters */
extern const uint64_t IO_DEPTH;
extern const uint64_t NUM_JOBS;

/* netCAS operation modes */
typedef enum
{
    NETCAS_MODE_IDLE = 0,
    NETCAS_MODE_WARMUP = 1,
    NETCAS_MODE_STABLE = 2,
    NETCAS_MODE_CONGESTION = 3
} netCAS_mode_t;

/* Function declarations */

/**
 * Query the current optimal split ratio.
 * @return Current optimal split ratio (0-100)
 */
int netcas_query_optimal_split_ratio(void);

/**
 * Start the split ratio monitoring thread.
 * @param core OCF core handle
 * @return 0 on success, -1 on failure
 */
int netcas_mngt_split_monitor_start(ocf_core_t core);

/**
 * Stop the split ratio monitoring thread.
 */
void netcas_mngt_split_monitor_stop(void);

#endif /* NETCAS_SPLIT_H_ */