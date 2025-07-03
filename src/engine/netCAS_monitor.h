/*
 * netCAS monitor module
 */

#ifndef __NETCAS_MONITOR_H__
#define __NETCAS_MONITOR_H__

#include "ocf/ocf.h"
#include "../ocf_request.h"

/* External function declaration */
extern int lookup_bandwidth(int io_depth, int num_job, int split_ratio);

/* RDMA metrics structure */
struct rdma_metrics
{
    uint64_t latency;
    uint64_t throughput;
};

/* Function declarations */
uint64_t measure_iops_using_opencas_stats(struct ocf_request *req, uint64_t elapsed_time);
uint64_t measure_iops_using_disk_stats(struct ocf_request *req, uint64_t elapsed_time);
struct rdma_metrics read_rdma_metrics(struct ocf_request *req);
struct rdma_metrics measure_performance(struct ocf_request *req, uint64_t elapsed_time);

#endif /* __NETCAS_MONITOR_H__ */