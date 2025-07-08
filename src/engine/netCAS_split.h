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

/* Constants */
#define RDMA_WINDOW_SIZE 20
#define MONITOR_INTERVAL_MS 100        /* Check every 0.1 second */
#define LOG_INTERVAL_MS 1000           /* Log every 1 second */
#define WARMUP_PERIOD_NS 3000000000ULL /* 3 seconds in nanoseconds */
#define RDMA_THRESHOLD 100             /* Threshold for starting warmup */
#define CONGESTION_THRESHOLD 90        /* 9.0% drop threshold for congestion mode */
#define RDMA_LATENCY_THRESHOLD 1000000 /* 1ms in nanoseconds */
#define IOPS_THRESHOLD 1000            /* 1000 IOPS */

/* Scale constants for split ratio (0-10000 where 10000 = 100%) */
#define SPLIT_RATIO_SCALE 10000 /* Scale factor for split ratio */
#define SPLIT_RATIO_MAX 10000   /* Maximum split ratio value */
#define SPLIT_RATIO_MIN 0       /* Minimum split ratio value */

/* Test app parameters */
extern const uint64_t IO_DEPTH;
extern const uint64_t NUM_JOBS;

/* netCAS operation modes */
typedef enum
{
    NETCAS_MODE_IDLE = 0,
    NETCAS_MODE_WARMUP = 1,
    NETCAS_MODE_STABLE = 2,
    NETCAS_MODE_CONGESTION = 3,
    NETCAS_MODE_FAILURE = 4
} netCAS_mode_t;

/* Function declarations */

/**
 * Query the current optimal split ratio.
 * @return Current optimal split ratio (0-10000 where 10000 = 100%)
 */
uint64_t netcas_query_optimal_split_ratio(void);

/**
 * Query the current data admit switch value.
 * @return Current data admit value (true/false)
 */
bool netcas_query_data_admit(void);

/**
 * Set the data admit switch value.
 * @param data_admit New data admit value
 */
void netcas_set_data_admit(bool data_admit);

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