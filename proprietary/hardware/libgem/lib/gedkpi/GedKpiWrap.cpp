#include <ged/ged_kpi.h>
#include <cutils/log.h>
#include <gedkpi/GedKpiWrap.h>

GED_KPI_HANDLE ged_kpi_create_wrap(uint64_t BBQ_ID)
{
 #ifndef MTK_DO_NOT_USE_GPU_EXT
    return ged_kpi_create(BBQ_ID);
 #else
    ALOGD("Use dummy ged handle");
    return (GED_KPI_HANDLE)BBQ_ID;
 #endif
}

void ged_kpi_destroy_wrap(GED_KPI_HANDLE hKPI)
{
 #ifndef MTK_DO_NOT_USE_GPU_EXT
    ged_kpi_destroy(hKPI);
 #else
    (void)hKPI;
 #endif
}

GED_ERROR ged_kpi_dequeue_buffer_tag_wrap(GED_KPI_HANDLE hKPI, int32_t BBQ_api_type, int32_t fence, int32_t pid, intptr_t buffer_addr)
{
 #ifndef MTK_DO_NOT_USE_GPU_EXT
    return ged_kpi_dequeue_buffer_tag(hKPI, BBQ_api_type, fence, pid, buffer_addr);
 #else
    (void)hKPI;
    (void)BBQ_api_type;
    (void)fence;
    (void)pid;
    (void)buffer_addr;
    return GED_OK;
 #endif
}

GED_ERROR ged_kpi_queue_buffer_tag_wrap(GED_KPI_HANDLE hKPI, int32_t BBQ_api_type, int32_t fence, int32_t pid, int32_t QedBuffer_length, intptr_t buffer_addr)
{
#ifndef MTK_DO_NOT_USE_GPU_EXT
    return ged_kpi_queue_buffer_tag(hKPI, BBQ_api_type, fence, pid, QedBuffer_length, buffer_addr);
#else
    (void)hKPI;
    (void)BBQ_api_type;
    (void)fence;
    (void)pid;
    (void)QedBuffer_length;
    (void)buffer_addr;
    return GED_OK;
#endif
}

GED_ERROR ged_kpi_acquire_buffer_tag_wrap(GED_KPI_HANDLE hKPI, int pid, intptr_t buffer_addr)
{
#ifndef MTK_DO_NOT_USE_GPU_EXT
    return ged_kpi_acquire_buffer_tag(hKPI, pid, buffer_addr);
#else
    (void)hKPI;
    (void)pid;
    (void)buffer_addr;
    return GED_OK;
#endif
}
