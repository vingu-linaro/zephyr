/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <syscall_handler.h>
#include <drivers/virtio.h>
#include <drivers/virtio_mmio.h>
#include <openamp/virqueue.h>

static inline int z_vrfy_virtio_push(const struct device *dev,
                                     struct virtqueue *vq,
                                     struct virtio_buf *vqbuf)
{
    Z_OOPS(Z_SYSCALL_DRIVER_virtio(dev, push));
    return z_impl_virtio_push((const struct device *)dev, vq, vqbuf);
}
#include <syscalls/virtio_push_mrsh.c>

static inline int z_vrfy_virtio_pop(const struct device *dev,
                                    struct virtqueue *vq,
                                    struct virtio_buf *vqbuf)
{
    Z_OOPS(Z_SYSCALL_DRIVER_virtio(dev, pop));
    return z_impl_virtio_pop((const struct device *)dev, vq, vqbuf);
}
#include <syscalls/virtio_pop_mrsh.c>

static inline int z_vrfy_virtio_max_data_size_get(const struct device *dev)
{
    Z_OOPS(Z_SYSCALL_DRIVER_virtio(dev, max_data_size_get));
    return z_impl_virtio_max_data_size_get((const struct device *)dev);
}
#include <syscalls/virtio_max_data_size_get_mrsh.c>

static inline uint32_t z_vrfy_virtio_get_vqs(const struct device *dev,
                                              unsigned int nvqs, struct virtqueue **vqs,
                                              void *callbacks, const char **names)
{
    Z_OOPS(Z_SYSCALL_DRIVER_virtio(dev, get_vqs));
    return z_impl_virtio_get_vqs((const struct device *)dev, nvqs, vqs, callbacks, names);
}
#include <syscalls/virtio_get_vqs_mrsh.c>

static inline uint32_t z_vrfy_virtio_get_buffer(const struct device *dev,
                                              struct virtqueue *vq,
                                              unsigned int idx)
{
    Z_OOPS(Z_SYSCALL_DRIVER_virtio(dev, get_buffer));
    return z_impl_virtio_get_buffer((const struct device *)dev, vq, idx);
}
#include <syscalls/virtio_get_buffer_mrsh.c>

static inline int z_vrfy_virtio_set_enabled(const struct device *dev, int enable)
{
    Z_OOPS(Z_SYSCALL_DRIVER_virtio(dev, set_enabled));
    return z_impl_virtio_set_enabled((const struct device *)dev, enable);
}
#include <syscalls/virtio_set_enabled_mrsh.c>

static inline int z_vrfy_virtio_set_cookie(const struct device *dev, void *user_data)
{
    Z_OOPS(Z_SYSCALL_DRIVER_virtio(dev, set_cookie));
    return z_impl_virtio_set_cookie((const struct device *)dev, user_data);
}
#include <syscalls/virtio_set_enabled_mrsh.c>

static inline int z_vrfy_virtio_set_device_features(const struct device *dev, uint32_t features)
{
    Z_OOPS(Z_SYSCALL_DRIVER_virtio(dev, set_device_features));
    return z_impl_virtio_set_device_features((const struct device *)dev, features);
}
#include <syscalls/virtio_set_enabled_mrsh.c>
