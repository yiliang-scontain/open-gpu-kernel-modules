/*
 * SPDX-License-Identifier: MIT
 */

#ifndef __NV_SCONE_H__
#define __NV_SCONE_H__

#include <nvtypes.h>
#include <nvstatus.h>

struct nv_state_t;

/* Keep these values synchronized with enum gpu_security_operation. */
enum nv_scone_gpu_operation {
    NV_SCONE_GPU_OPERATION_DISCOVER = 1,
    NV_SCONE_GPU_OPERATION_COMPUTE = 2,
    NV_SCONE_GPU_OPERATION_MEMORY = 3,
    NV_SCONE_GPU_OPERATION_CONTROL = 4,
    NV_SCONE_GPU_OPERATION_RESET = 5,
    NV_SCONE_GPU_OPERATION_DEBUG = 6,
    NV_SCONE_GPU_OPERATION_PEER = 7,
};

NV_STATUS NV_API_CALL nv_scone_gpu_authorize(struct nv_state_t *nv,
                                              NvU32 operation);
NV_STATUS NV_API_CALL nv_scone_gpu_authorize_uuid(const NvU8 *uuid,
                                                   NvU32 operation);
NV_STATUS NV_API_CALL nv_scone_gpu_acquire(struct nv_state_t *nv,
                                           NvBool *host_lease);
NV_STATUS NV_API_CALL nv_scone_gpu_acquire_uuid(const NvU8 *uuid,
                                                NvBool *host_lease);
void NV_API_CALL nv_scone_gpu_release(struct nv_state_t *nv,
                                      NvBool host_lease);
void NV_API_CALL nv_scone_gpu_release_uuid(const NvU8 *uuid,
                                           NvBool host_lease);
NvBool NV_API_CALL nv_scone_gpu_exclusive_enabled(void);

#endif /* __NV_SCONE_H__ */
