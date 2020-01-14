#ifndef __GPU_KPI_WRAP_H
#define __GPU_KPI_WRAP_H

#include <stdint.h>
#include <ged/ged_kpi.h>

typedef void* GED_KPI_HANDLE;

extern "C"
{
	GED_KPI_HANDLE ged_kpi_create_wrap(uint64_t BBQ_id);
	void ged_kpi_destroy_wrap(GED_KPI_HANDLE hKPI);
	GED_ERROR ged_kpi_dequeue_buffer_tag_wrap(GED_KPI_HANDLE hKPI, int32_t BBQ_api_type, int32_t fence, int32_t pid, intptr_t buffer_addr);
	GED_ERROR ged_kpi_queue_buffer_tag_wrap(GED_KPI_HANDLE hKPI, int32_t BBQ_api_type, int32_t fence, int32_t pid, int32_t QedBuffer_length, intptr_t buffer_addr);
	GED_ERROR ged_kpi_acquire_buffer_tag_wrap(GED_KPI_HANDLE hKPI, int32_t pid, intptr_t buffer_addr);
}
#endif
