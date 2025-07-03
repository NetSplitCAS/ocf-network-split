/**
 * netCAS split ratio management module.
 *
 * Dynamically monitors and adjusts the optimal split ratio
 * between cache and backend storage.
 */

#ifndef NETCAS_SPLIT_H_
#define NETCAS_SPLIT_H_

#include "ocf/ocf.h"

/* Function declarations */
int netcas_query_optimal_split_ratio(void);
int netcas_mngt_split_monitor_start(ocf_core_t core);
void netcas_mngt_split_monitor_stop(void);

#endif /* NETCAS_SPLIT_H_ */