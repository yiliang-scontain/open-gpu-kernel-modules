<!-- SPDX-License-Identifier: MIT -->

# SCONE exclusive GPU integration

This tree integrates NVIDIA's open kernel modules with the SCONE kernel's
exclusive whole-GPU admission API.  The integration is active only when the
target kernel enables
`CONFIG_SECURITY_SCONE_DOMAIN_V3_EXCLUSIVE_GPU=y`.

## Patch points

The security boundary has the following driver-side patch points:

1. `nvidia.ko` registers each physical `/dev/nvidiaN` minor with its immutable
   raw GPU UUID and PCI domain/bus/slot/function after probe and before the
   minor is published.  Probe fails closed if registration fails.  Removal
   unregisters the minor before its number can be reused.
2. `nvidia.ko` registers `/dev/nvidiactl` as a global control gateway before
   publishing its character device and unregisters it before deletion.
3. Direct physical-device ioctls authorize that GPU.  Card discovery filters
   unauthorized GPUs, and `NV_ESC_ATTACH_GPUS_TO_FD` authorizes every GPU ID
   before changing the file's attachment set.
4. User-originated RM resource allocation and RM control requests authorize
   the resolved physical `OBJGPU` before executing the operation.  This covers
   the compute/control object paths after RM has resolved a target.  Root RM
   discovery arrays are filtered, GPU-ID parameters and peer groups are
   authorized explicitly, and targetless global mutation or diagnostic paths
   are rejected.  Cross-client object duplication and sharing are rejected.
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

## Security assumptions and unsupported configurations

The patch implements physical, whole-GPU ownership.  MIG, vGPU, display,
NVSwitch, peer-memory, UVM tools, and cross-TD sharing are not supported.
Peer authorization exists for completeness but succeeds only when both whole
GPUs are admitted to the same current TD.

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
