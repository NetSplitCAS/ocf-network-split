/*
 * netCAS monitor module
 */

#ifndef __NETCAS_MONITOR_H__
#define __NETCAS_MONITOR_H__

#include "ocf/ocf.h"
#include "../ocf_request.h"

/* RDMA metrics structure */
struct rdma_metrics
{
    uint64_t latency;
    uint64_t throughput;
};

/* Performance metrics structure */
struct performance_metrics
{
    uint64_t rdma_latency;
    uint64_t rdma_throughput;
    uint64_t opencas_iops;
    uint64_t disk_iops;
};

/* Function declarations */
uint64_t measure_iops_using_opencas_stats(struct ocf_request *req, uint64_t elapsed_time);
uint64_t measure_iops_using_disk_stats(uint64_t elapsed_time);
struct rdma_metrics read_rdma_metrics(void);
// struct performance_metrics measure_performance(struct ocf_request *req, uint64_t elapsed_time);
struct rdma_metrics measure_performance(void);

#endif /* __NETCAS_MONITOR_H__ */