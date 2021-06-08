/*
 * Copyright (c) 2019 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_virtio_mmio_H_
#define ZEPHYR_DRIVERS_virtio_mmio_H_

#include <kernel.h>
#include <drivers/virtio.h>
#include <device.h>

#include <openamp/open_amp.h>
#include <openamp/virtqueue.h>
#include <openamp/virtio.h>

#include <metal/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT        12
#define PAGE_SIZE        (1 << PAGE_SHIFT)
#define PAGE_MASK        (~(PAGE_SIZE-1))

#define VIRTIO_MMIO_MAX_DATA_SIZE        128

#define VRING_ALIGNMENT      4096
#define SCMI_VIRTQUEUE_MAX        2


struct virtio_mmio_device_config {
    uint8_t *base;
    unsigned int size;
    uint8_t *shared_mem;
    unsigned int shared_size;
    unsigned int slave;
    unsigned int irq;
    void (*irq_config_func)(const struct device *d);
};

/* Device data structure */
struct virtio_mmio_data {
    virtio_callback_t vq_callback;
    virtio_state_callback_t state_callback;
    void *user_data;

    struct virtio_device device;
    uint32_t features;

    uint32_t slave_init;
    struct virtqueue *vqs;
    struct virtio_vring_info vrings[SCMI_VIRTQUEUE_MAX];

    struct metal_io_region *shm_io;
    metal_phys_addr_t shm_physmap;
    struct metal_device shm_device;
};

static inline unsigned int
virtio_get_role(struct virtio_device *vdev)
{
    return vdev->role;
}

static inline void virtio_set_status(struct virtio_device *vdev,
                       uint8_t status)
{
    vdev->func->set_status(vdev, status);
}

static inline uint8_t virtio_get_status(struct virtio_device *vdev)
{
    return vdev->func->get_status(vdev);
}

static inline uint32_t
virtio_get_features(struct virtio_device *vdev)
{
    return vdev->func->get_features(vdev);
}

static inline void
virtio_set_features(struct virtio_device *vdev, uint32_t features)
{
    return vdev->func->set_features(vdev, features);
}

static inline void
virtio_read_config(struct virtio_device *vdev,
             uint32_t offset, void *dst, int length)
{
    vdev->func->read_config(vdev, offset, dst, length);
}

static inline void
virtio_write_config(struct virtio_device *vdev,
             uint32_t offset, void *dst, int length)
{
    vdev->func->write_config(vdev, offset, dst, length);
}

static inline void
virtio_reset(struct virtio_device *vdev)
{
    vdev->func->reset_device(vdev);
}



#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_IPM_virtio_mmio_H_ */
