<!-- SPDX-License-Identifier: MIT -->

# SCONE exclusive GPU integration

This tree integrates NVIDIA's open kernel modules with the SCONE kernel's
exclusive whole-GPU admission API.  The integration is active only when the
target kernel enables
`CONFIG_SECURITY_SCONE_DOMAIN_V3_EXCLUSIVE_GPU=y`.

## Patch points

The security boundary has the following driver-side patch points:

1. `nvidia.ko` registers each physical `/dev/nvidiaN` minor with its raw GPU
   UUID and PCI domain/bus/slot/function after probe and before the minor is
   published.  These fields are driver-owned locators, not cryptographic
   device identity.  Probe fails closed if registration fails.  Removal
   unregisters the minor before its number can be reused.
2. `nvidia.ko` registers `/dev/nvidiactl` as a global control gateway before
   publishing its character device and unregisters it before deletion.
3. Direct physical-device ioctls authorize that GPU.  Card discovery filters
   unauthorized GPUs, and `NV_ESC_ATTACH_GPUS_TO_FD` authorizes every GPU ID
   before changing the file's attachment set.
4. User-originated RM resource allocation and RM control requests authorize
   the resolved physical `OBJGPU` before executing the operation.  Allocation
   is classified by the resource's NVOC ancestry: memory, VA-space, and
   context-DMA resources require `MEMORY`; channels, channel groups, and
   channel descendants require `COMPUTE`; peer resources require `PEER`;
   debugger resources require `DEVICE_ADMIN`; and device binding plus every
   other ordinary allocation requires `CONTROL`.  RM controls use the same
   ancestry classification.  Privileged controls and whole-device reset or
   debug controls require `DEVICE_ADMIN`, which the kernel rejects for a
   `SHARE_ACROSS_TD` assignment.  Root RM discovery arrays are filtered, GPU-ID
   parameters and peer groups are authorized explicitly, and targetless global
   mutation or diagnostic paths are rejected.  Cross-client object duplication
   and sharing are rejected.
5. `nvidia-uvm.ko` registers `/dev/nvidia-uvm` and its tools minor as UVM
   gateways.  GPU registration and peer operations authorize UUID targets,
   while every subsequent VA-space ioctl reauthorizes all GPUs retained by
   that VA space before dispatch.
6. UVM tools access is rejected because its cross-process observation model
   is outside whole-GPU admission.  Modeset, DRM, peer-memory, and NVSwitch
   modules also fail initialization while exclusive admission is enabled;
   those paths need separate mediation before they can be enabled safely.

The RM code is built with `kernel-open/common/inc/nv-scone.h`, whose operation
values are compile-time checked against the kernel API in `nvidia.ko`.

The targetless controls denied in exclusive mode include ACPI execution,
global event/debug controls, NVD dumps and logs, global boost/cache/NVLink and
fabric-management mutations, vGPU host mutation, and system-wide telemetry
that cannot be attributed to one admitted GPU.  Adding or regenerating an RM
root control requires classifying it as target-resolved, filtered discovery,
or denied global state before it can be enabled.

Ordinary host clients hold a per-GPU lease from physical-device open or RM/UVM
attachment through completed driver teardown.  TD admission fails while any
host lease exists.  The lease is deliberately released from NVIDIA's actual
close or VA-space destruction path, not the VFS file release path, because
both core RM and UVM can defer teardown to driver kernel threads.

## FD-bound device information

An admitted process can issue `NV_ESC_SCONE_GPU_INFO` on an already-open
physical `/dev/nvidiaN` file descriptor.  The caller initializes
`nv_ioctl_scone_gpu_info_t.version` to `NV_SCONE_GPU_INFO_VERSION` and leaves
every other byte zero.  The driver reconstructs the identity associated with
that descriptor and asks `security_gpu_device_query()` for the current
authorized kernel record.  On success the ioctl returns that record's raw GPU
UUID, PCI function, and access mode.  The only valid modes are
`NV_SCONE_GPU_ACCESS_MODE_SAME_TD_INSTANCE` and
`NV_SCONE_GPU_ACCESS_MODE_SHARE_ACROSS_TD`.

The ioctl accepts only a physical GPU descriptor, is unavailable when SCONE
exclusive-GPU support is disabled, and succeeds only for an active TD member
whose policy and live record both authorize `DISCOVER` with the same mode.  A
host process, kernel thread, stale or draining TD, absent GPU, or mode mismatch
receives no record.  It never accepts a GPU ID, UUID, PCI address, or device
path as an input selector.  Returned UUID and PCI fields are suitable for
correlating the descriptor with NVIDIA evidence collection, but they are not
attestation claims and must not be used as the confidential-computing identity
of the GPU.

## Security assumptions and unsupported configurations

The patch implements physical, whole-GPU ownership.  A policy may explicitly
share the physical assignment across TDs, but every TD must use separate file
descriptions, RM clients, channels, mappings, and UVM VA spaces; cross-TD RM or
UVM context sharing remains unsupported.  MIG, vGPU, display, NVSwitch,
peer-memory, and UVM tools are not supported.  Peer authorization exists for
completeness but succeeds only when both whole GPUs are admitted to the same
current TD.

The SCONE generic device hooks bind every opened physical or gateway file to
the TD generation and activation epoch.  Consequently, RM handles reachable
through a gateway file cannot be passed to a different TD, and the RM checks
ensure that a TD cannot create a resource for an unadmitted GPU.  Driver
entry points that introduce a new physical target must resolve and authorize
that target with `security_gpu_device_use()` before side effects.

The target kernel must export the SCONE security entry points to GPL-compatible
modules.  Module signature enforcement should admit only the qualified driver
build; a valid signature by itself does not prove that another driver version
contains these checks.
