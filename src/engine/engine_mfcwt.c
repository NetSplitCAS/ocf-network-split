/*
 * Multi-Factor Cached Write-Through (MFCWT) engine
 * 독립적으로 동작하며, mfwa/wt의 코드를 복사하여 구현
 */

#include <linux/random.h>
#include "ocf/ocf.h"
#include "../ocf_cache_priv.h"
#include "../ocf_request.h"
#include "../utils/utils_io.h"
#include "../utils/utils_cache_line.h"
#include "../utils/utils_part.h"
#include "../concurrency/ocf_concurrency.h"
#include "../metadata/metadata.h"
#include "engine_common.h"
#include "engine_mfcwt.h"
#include "mf_monitor.h"
#include "engine_pt.h"
#include "engine_inv.h"
#include "engine_bf.h"

// Include netCAS split
#include "netCAS_split.h"

#define OCF_ENGINE_DEBUG 1

#define OCF_ENGINE_DEBUG_IO_NAME "mfcwt"
#include "engine_debug.h"

// Use global flag for netCAS
extern bool USING_NETCAS_SPLIT;

// ====== Multi-Factor Cached Write-Through: READ ======

static inline bool data_admit_allow(void)
{
    if (USING_NETCAS_SPLIT)
    {
        return netcas_query_data_admit();
    }
    else
    {
        return monitor_query_data_admit();
    }
}

static inline bool load_admit_allow(struct ocf_request *req)
{
    // 패턴/쿼터 기반 분배 알고리즘 (engine_fast.c 참고)
    static uint32_t request_counter = 0;
    static uint32_t cache_quota = 0;
    static uint32_t backend_quota = 0;
    static bool last_request_to_cache = false;
    static uint32_t pattern_position = 0;
    static uint32_t pattern_cache = 0;
    static uint32_t pattern_backend = 0;
    static uint32_t pattern_size = 0;
    static uint32_t total_requests = 0;
    static uint32_t cache_requests = 0;
    static uint32_t backend_requests = 0;
    const uint32_t window_size = 10000;
    const uint32_t max_pattern_size = 10;
    bool send_to_backend;
    uint32_t expected_cache_ratio;
    uint32_t expected_backend_ratio;

    uint64_t split_ratio;
    if (USING_NETCAS_SPLIT)
    {
        split_ratio = netcas_query_optimal_split_ratio();
    }
    else
    {
        split_ratio = monitor_query_load_admit();
    }

    if (request_counter % window_size == 0 || pattern_size == 0)
    {
        uint32_t a = split_ratio;
        uint32_t b = window_size - split_ratio;
        uint32_t gcd = 1;
        if (a > 0 && b > 0)
        {
            while (b != 0)
            {
                uint32_t temp = b;
                b = a % b;
                a = temp;
            }
            gcd = a;
        }
        pattern_size = (split_ratio + (window_size - split_ratio)) / gcd;
        if (pattern_size > max_pattern_size)
        {
            pattern_size = max_pattern_size;
        }
        pattern_cache = (split_ratio * pattern_size) / window_size;
        pattern_backend = pattern_size - pattern_cache;
        total_requests = 0;
        cache_requests = 0;
        backend_requests = 0;
        cache_quota = split_ratio;
        backend_quota = window_size - split_ratio;
        pattern_position = 0;
        OCF_DEBUG_RQ(req, "[MFCWT] [load_admit_allow] --- 패턴 초기화: split_ratio=%llu, pattern_size=%u, pattern_cache=%u, pattern_backend=%u", split_ratio, pattern_size, pattern_cache, pattern_backend);
    }

    request_counter++;
    total_requests++;

    expected_cache_ratio = (total_requests * split_ratio) / window_size;
    expected_backend_ratio = total_requests - expected_cache_ratio;
    OCF_DEBUG_RQ(req, "[MFCWT] [load_admit_allow] --- 요청 #%u: split_ratio=%llu, expected_cache_ratio=%u, expected_backend_ratio=%u, cache_requests=%u, backend_requests=%u", total_requests, split_ratio, expected_cache_ratio, expected_backend_ratio, cache_requests, backend_requests);

    if (cache_requests < expected_cache_ratio)
    {
        send_to_backend = false;
        OCF_DEBUG_RQ(req, "[MFCWT] [load_admit_allow] --- cache_requests < expected_cache_ratio: cache로 보냄");
    }
    else if (backend_requests < expected_backend_ratio)
    {
        send_to_backend = true;
        OCF_DEBUG_RQ(req, "[MFCWT] [load_admit_allow] --- backend_requests < expected_backend_ratio: backend로 보냄");
    }
    else
    {
        if (pattern_position < pattern_size)
        {
            send_to_backend = (pattern_position >= pattern_cache);
            OCF_DEBUG_RQ(req, "[MFCWT] [load_admit_allow] --- 패턴 기반 분배: pattern_position=%u, send_to_backend=%d", pattern_position, send_to_backend);
            pattern_position = (pattern_position + 1) % pattern_size;
        }
        else
        {
            if (cache_quota == 0)
            {
                send_to_backend = true;
                OCF_DEBUG_RQ(req, "[MFCWT] [load_admit_allow] --- cache_quota == 0: backend로 보냄");
            }
            else if (backend_quota == 0)
            {
                send_to_backend = false;
                OCF_DEBUG_RQ(req, "[MFCWT] [load_admit_allow] --- backend_quota == 0: cache로 보냄");
            }
            else
            {
                send_to_backend = last_request_to_cache;
                OCF_DEBUG_RQ(req, "[MFCWT] [load_admit_allow] --- 교차 분배: last_request_to_cache=%d, send_to_backend=%d", last_request_to_cache, send_to_backend);
            }
        }
    }

    if (send_to_backend)
    {
        backend_quota--;
        backend_requests++;
        last_request_to_cache = false;
        OCF_DEBUG_RQ(req, "[MFCWT] [load_admit_allow] --- 최종: backend로 보냄 (backend_quota=%u, backend_requests=%u)", backend_quota, backend_requests);
        return false; // backend로 보냄
    }
    else
    {
        cache_quota--;
        cache_requests++;
        last_request_to_cache = true;
        OCF_DEBUG_RQ(req, "[MFCWT] [load_admit_allow] --- 최종: cache로 보냄 (cache_quota=%u, cache_requests=%u)", cache_quota, cache_requests);
        return true; // cache로 보냄
    }
}

static void _ocf_read_mfcwt_to_cache_cmpl(struct ocf_request *req, int error)
{
    if (error)
        req->error |= error;
    if (req->error)
        inc_fallback_pt_error_counter(req->cache);
    if (env_atomic_dec_return(&req->req_remaining) == 0)
    {
        OCF_DEBUG_RQ(req, "TO_CACHE completion");
        if (req->error)
        {
            ocf_core_stats_cache_error_update(req->core, OCF_READ);
            ocf_engine_push_req_front_pt(req);
        }
        else
        {
            ocf_req_unlock(req);
            req->complete(req, req->error);
            ocf_req_put(req);
        }
    }
}

static inline void _ocf_read_mfcwt_submit_to_cache(struct ocf_request *req)
{
    env_atomic_set(&req->req_remaining, ocf_engine_io_count(req));
    ocf_submit_cache_reqs(req->cache, req, OCF_READ, 0, req->byte_length,
                          ocf_engine_io_count(req), _ocf_read_mfcwt_to_cache_cmpl);
}

static void _ocf_read_mfcwt_to_core_cmpl_do_promote(struct ocf_request *req, int error)
{
    struct ocf_cache *cache = req->cache;
    if (error)
        req->error = error;
    if (env_atomic_dec_return(&req->req_remaining) == 0)
    {
        OCF_DEBUG_RQ(req, "TO_CORE completion");
        if (req->error)
        {
            req->complete(req, req->error);
            req->info.core_error = 1;
            ocf_core_stats_core_error_update(req->core, OCF_READ);
            ctx_data_free(cache->owner, req->cp_data);
            req->cp_data = NULL;
            ocf_engine_invalidate(req);
            return;
        }
        ctx_data_cpy(cache->owner, req->cp_data, req->data, 0, 0, req->byte_length);
        req->complete(req, req->error);
        ocf_engine_backfill(req);
    }
}

static void _ocf_read_mfcwt_to_core_cmpl_no_promote(struct ocf_request *req, int error)
{
    struct ocf_cache *cache = req->cache;
    if (error)
        req->error = error;
    if (env_atomic_dec_return(&req->req_remaining) == 0)
    {
        OCF_DEBUG_RQ(req, "TO_CORE completion");
        if (req->error)
        {
            req->complete(req, req->error);
            req->info.core_error = 1;
            ocf_core_stats_core_error_update(req->core, OCF_READ);
            ctx_data_free(cache->owner, req->cp_data);
            req->cp_data = NULL;
            ocf_engine_invalidate(req);
            return;
        }
        req->complete(req, req->error);
        ocf_req_put(req);
    }
}

static inline void _ocf_read_mfcwt_submit_to_core(struct ocf_request *req, bool promote)
{
    struct ocf_cache *cache = req->cache;
    int ret;
    env_atomic_set(&req->req_remaining, 1);
    if (promote)
    {
        req->cp_data = ctx_data_alloc(cache->owner, BYTES_TO_PAGES(req->byte_length));
        if (!req->cp_data)
        {
            _ocf_read_mfcwt_to_core_cmpl_do_promote(req, -OCF_ERR_NO_MEM);
            return;
        }
        ret = ctx_data_mlock(cache->owner, req->cp_data);
        if (ret)
        {
            _ocf_read_mfcwt_to_core_cmpl_do_promote(req, -OCF_ERR_NO_MEM);
            return;
        }
        ocf_submit_volume_req(&req->core->volume, req, _ocf_read_mfcwt_to_core_cmpl_do_promote);
    }
    else
    {
        ocf_submit_volume_req(&req->core->volume, req, _ocf_read_mfcwt_to_core_cmpl_no_promote);
    }
}

static int _ocf_read_mfcwt_do(struct ocf_request *req)
{
    ocf_req_get(req);
    if (req->info.re_part)
    {
        OCF_DEBUG_RQ(req, "Re-Part");
        ocf_req_hash_lock_wr(req);
        ocf_part_move(req);
        ocf_req_hash_unlock_wr(req);
    }
    if (ocf_engine_is_hit(req))
    {
        if (req->load_admit_allowed)
        {
            OCF_DEBUG_RQ(req, "Submit");
            _ocf_read_mfcwt_submit_to_cache(req);
        }
        else
        {
            OCF_DEBUG_RQ(req, "Submit");
            _ocf_read_mfcwt_submit_to_core(req, false);
        }
    }
    else
    {
        if (req->data_admit_allowed)
        {
            if (req->map->rd_locked)
            {
                OCF_DEBUG_RQ(req, "Switching to PT");
                ocf_read_pt_do(req);
                return 0;
            }
            if (req->info.dirty_any)
            {
                ocf_req_hash_lock_rd(req);
                ocf_engine_clean(req);
                ocf_req_hash_unlock_rd(req);
                ocf_req_put(req);
                return 0;
            }
            ocf_req_hash_lock_rd(req);
            ocf_set_valid_map_info(req);
            ocf_req_hash_unlock_rd(req);
            OCF_DEBUG_RQ(req, "Submit");
            _ocf_read_mfcwt_submit_to_core(req, true);
        }
        else
        {
            OCF_DEBUG_RQ(req, "Submit");
            _ocf_read_mfcwt_submit_to_core(req, false);
        }
    }
    ocf_engine_update_request_stats(req);
    ocf_engine_update_block_stats(req);
    ocf_req_put(req);
    return 0;
}

static enum ocf_engine_lock_type ocf_read_mfcwt_get_lock_type(struct ocf_request *req)
{
    if (ocf_engine_is_hit(req))
    {
        if (req->load_admit_allowed)
            return ocf_engine_lock_read;
        else
            return ocf_engine_lock_none;
    }
    else
    {
        if (req->data_admit_allowed)
            return ocf_engine_lock_write;
        else
            return ocf_engine_lock_none;
    }
}

static const struct ocf_io_if _io_if_read_mfcwt_resume = {
    .read = _ocf_read_mfcwt_do,
    .write = _ocf_read_mfcwt_do,
};

static const struct ocf_engine_callbacks _read_mfcwt_engine_callbacks = {
    .get_lock_type = ocf_read_mfcwt_get_lock_type,
    .resume = ocf_engine_on_resume,
};

int ocf_read_mfcwt(struct ocf_request *req)
{
    int lock = OCF_LOCK_NOT_ACQUIRED;
    struct ocf_cache *cache = req->cache;
    ocf_io_start(&req->ioi.io);
    if (env_atomic_read(&cache->pending_read_misses_list_blocked))
    {
        ocf_get_io_if(ocf_cache_mode_pt)->read(req);
        return 0;
    }
    ocf_req_get(req);
    req->data_admit_allowed = data_admit_allow();
    req->load_admit_allowed = load_admit_allow(req);
    req->io_if = &_io_if_read_mfcwt_resume;
    lock = ocf_engine_prepare_clines(req, &_read_mfcwt_engine_callbacks);
    if (!req->info.mapping_error)
    {
        if (lock >= 0)
        {
            if (lock != OCF_LOCK_ACQUIRED)
            {
                OCF_DEBUG_RQ(req, "NO LOCK");
            }
            else
            {
                _ocf_read_mfcwt_do(req);
            }
        }
        else
        {
            OCF_DEBUG_RQ(req, "LOCK ERROR %d", lock);
            req->complete(req, lock);
            ocf_req_put(req);
        }
    }
    else
    {
        ocf_req_clear(req);
        ocf_get_io_if(ocf_cache_mode_pt)->read(req);
    }
    ocf_req_put(req);
    return 0;
}

// ====== Multi-Factor Cached Write-Through: WRITE ======

static void _ocf_write_mfcwt_req_complete(struct ocf_request *req)
{
    if (env_atomic_dec_return(&req->req_remaining))
        return;
    // OCF_DEBUG_RQ(req, "Completion");
    if (req->error)
    {
        req->complete(req, req->info.core_error ? req->error : 0);
        ocf_engine_invalidate(req);
    }
    else
    {
        ocf_req_unlock_wr(req);
        req->complete(req, req->info.core_error ? req->error : 0);
        ocf_req_put(req);
    }
}

static void _ocf_write_mfcwt_cache_complete(struct ocf_request *req, int error)
{
    if (error)
    {
        req->error = req->error ?: error;
        ocf_core_stats_cache_error_update(req->core, OCF_WRITE);
        if (req->error)
            inc_fallback_pt_error_counter(req->cache);
    }
    _ocf_write_mfcwt_req_complete(req);
}

static void _ocf_write_mfcwt_core_complete(struct ocf_request *req, int error)
{
    if (error)
    {
        req->error = error;
        req->info.core_error = 1;
        ocf_core_stats_core_error_update(req->core, OCF_WRITE);
    }
    _ocf_write_mfcwt_req_complete(req);
}

static inline void _ocf_write_mfcwt_submit(struct ocf_request *req)
{
    struct ocf_cache *cache = req->cache;
    env_atomic_set(&req->req_remaining, ocf_engine_io_count(req));
    env_atomic_inc(&req->req_remaining);
    if (req->info.flush_metadata)
    {
        ocf_metadata_flush_do_asynch(cache, req, _ocf_write_mfcwt_cache_complete);
    }
    ocf_submit_cache_reqs(cache, req, OCF_WRITE, 0, req->byte_length,
                          ocf_engine_io_count(req), _ocf_write_mfcwt_cache_complete);
    ocf_submit_volume_req(&req->core->volume, req, _ocf_write_mfcwt_core_complete);
}

static void _ocf_write_mfcwt_update_bits(struct ocf_request *req)
{
    if (ocf_engine_is_miss(req))
    {
        ocf_req_hash_lock_rd(req);
        ocf_set_valid_map_info(req);
        ocf_req_hash_unlock_rd(req);
    }
    if (req->info.dirty_any)
    {
        ocf_req_hash_lock_wr(req);
        ocf_set_clean_map_info(req);
        ocf_req_hash_unlock_wr(req);
    }
    if (req->info.re_part)
    {
        OCF_DEBUG_RQ(req, "Re-Part");
        ocf_req_hash_lock_wr(req);
        ocf_part_move(req);
        ocf_req_hash_unlock_wr(req);
    }
}

static int _ocf_write_mfcwt_do(struct ocf_request *req)
{
    ocf_req_get(req);
    _ocf_write_mfcwt_update_bits(req);
    _ocf_write_mfcwt_submit(req);
    ocf_engine_update_request_stats(req);
    ocf_engine_update_block_stats(req);
    ocf_req_put(req);
    return 0;
}

static const struct ocf_io_if _io_if_mfcwt_resume = {
    .read = _ocf_write_mfcwt_do,
    .write = _ocf_write_mfcwt_do,
};

static enum ocf_engine_lock_type ocf_mfcwt_get_lock_type(struct ocf_request *req)
{
    return ocf_engine_lock_write;
}

static const struct ocf_engine_callbacks _mfcwt_engine_callbacks = {
    .get_lock_type = ocf_mfcwt_get_lock_type,
    .resume = ocf_engine_on_resume,
};

int ocf_write_mfcwt(struct ocf_request *req)
{
    int lock = OCF_LOCK_NOT_ACQUIRED;
    ocf_io_start(&req->ioi.io);
    ocf_req_get(req);
    req->io_if = &_io_if_mfcwt_resume;
    lock = ocf_engine_prepare_clines(req, &_mfcwt_engine_callbacks);
    if (!req->info.mapping_error)
    {
        if (lock >= 0)
        {
            if (lock != OCF_LOCK_ACQUIRED)
            {
                OCF_DEBUG_RQ(req, "NO LOCK");
            }
            else
            {
                _ocf_write_mfcwt_do(req);
            }
        }
        else
        {
            OCF_DEBUG_RQ(req, "LOCK ERROR %d\n", lock);
            req->complete(req, lock);
            ocf_req_put(req);
        }
    }
    else
    {
        ocf_req_clear(req);
        ocf_get_io_if(ocf_cache_mode_pt)->write(req);
    }
    ocf_req_put(req);
    return 0;
}