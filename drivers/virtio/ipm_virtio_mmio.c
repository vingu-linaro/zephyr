/*
 * Copyright (c) 2019 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT virtio_mmio

#include <zephyr.h>
#include <errno.h>
#include <device.h>
#include <soc.h>
#include <metal/device.h>

#include <linker/linker-defs.h>

#include <drivers/virtio.h>
#include <drivers/virtio_mmio.h>
#include "virtio_mmio_uapi.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(virtio_mmio);

#define DEV_CFG(dev) \
    ((const struct virtio_mmio_device_config * const)(dev)->config)
#define DEV_DATA(dev) \
    ((struct virtio_mmio_data *)(dev)->data)
#define virtio_mmio_REGS(dev) \
    ((volatile uint32_t *)(DEV_CFG(dev))->base)

/*
 * Virtio device dispatch fops
 */

static inline void virtio_mmio_write(struct virtio_device *vdev, int offset, uint32_t value)
{
    struct device *dev = vdev->priv;
    mem_addr_t base = (mem_addr_t)DEVICE_MMIO_GET(dev) + offset;

    sys_write32(value, base);
}

static inline uint32_t virtio_mmio_read(struct virtio_device *vdev, int offset)
{
    struct device *dev = vdev->priv;
    mem_addr_t base = (mem_addr_t)DEVICE_MMIO_GET(dev) + offset;

    return sys_read32(base);
}

static void virtio_mmio_set_status(struct virtio_device *vdev,
                       uint8_t status)
{
    virtio_mmio_write(vdev, VIRTIO_MMIO_STATUS, status);
}

static uint8_t virtio_mmio_get_status(struct virtio_device *vdev)
{
    return virtio_mmio_read(vdev, VIRTIO_MMIO_STATUS) & 0xff;
}

static void _virtio_mmio_set_features(struct virtio_device *vdev,
                       uint32_t features, int idx)
{
    virtio_mmio_write(vdev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, idx);
    virtio_mmio_write(vdev, VIRTIO_MMIO_DRIVER_FEATURES, features);
}

static void virtio_mmio_set_features(struct virtio_device *vdev,
                       uint32_t features)
{
    _virtio_mmio_set_features(vdev, features, 0);
}

static uint32_t _virtio_mmio_get_features(struct virtio_device *vdev, int idx)
{
    virtio_mmio_write(vdev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, idx);
    return virtio_mmio_read(vdev, VIRTIO_MMIO_DEVICE_FEATURES) & 0xffffffff;
}

static uint32_t virtio_mmio_get_features(struct virtio_device *vdev)
{
    return _virtio_mmio_get_features(vdev, 0);
}

static void virtio_mmio_read_config(struct virtio_device *vdev,
             uint32_t offset, void *dst, int length)
{
    LOG_WRN("virtio_mmio_read_config not supported");
    return;
}

static void virtio_mmio_write_config(struct virtio_device *vdev,
             uint32_t offset, void *dst, int length)
{
    LOG_WRN("virtio_mmio_write_config not supported");
    return;
}

static void virtio_mmio_reset_device(struct virtio_device *vdev)
{
    virtio_mmio_write(vdev, VIRTIO_MMIO_STATUS, 0);
}

static void virtio_mmio_notify(struct virtqueue *vq)
{
    virtio_mmio_write(vq->vq_dev, VIRTIO_MMIO_QUEUE_NOTIFY, vq->vq_queue_index);
    return;
}

const struct virtio_dispatch virtio_mmio_dispatch = {
    .get_status = virtio_mmio_get_status,
    .set_status = virtio_mmio_set_status,
    .get_features = virtio_mmio_get_features,
    .set_features = virtio_mmio_set_features,
    .read_config = virtio_mmio_read_config,
    .write_config = virtio_mmio_write_config,
    .reset_device = virtio_mmio_reset_device,
    .notify = virtio_mmio_notify,
};

/*
 * virtio mmio reserved memory management
 */

static int virtio_mmio_get_metal_io(const struct device *dev)
{
	const struct virtio_mmio_device_config *config = DEV_CFG(dev);
	struct virtio_mmio_data *driver_data = DEV_DATA(dev);
	struct metal_init_params metal_params = METAL_INIT_DEFAULTS;
	struct metal_device     *device;
	uintptr_t virt_mem_ptr;
	int32_t err;
	int device_idx = 0;

	/* Map dedicated mem region */
	device_map(&virt_mem_ptr, (uintptr_t)config->shared_mem,
                   config->shared_size, K_MEM_CACHE_NONE);

	/* Setup shared memory device */
	driver_data->shm_physmap = (metal_phys_addr_t) config->shared_mem;
	driver_data->shm_device.regions[0].physmap = &config->shared_mem;

	driver_data->shm_device.regions[0].virt = (void *)virt_mem_ptr;
	driver_data->shm_device.regions[0].size = config->shared_size;

	/* Libmetal setup */
	err = metal_init(&metal_params);
	if (err) {
		LOG_ERR("metal_init: failed - error code %d", err);
		return err;
	}

	err = metal_register_generic_device(&driver_data->shm_device);
	if (err) {
		LOG_ERR("Couldn't register shared memory device: %d", err);
		return err;
	}

	err = metal_device_open("generic", driver_data->shm_device.name, &device);
	if (err) {
		LOG_ERR("metal_device_open failed: %d", err);
		return err;
	}

	driver_data->shm_io = metal_device_io_region(device, device_idx);
	if (!driver_data->shm_io) {
		LOG_ERR("metal_device_io_region failed to get region");
		return err;
	}

	return 0;
}

static int virtio_mmio_init_mem(unsigned int role, struct virtio_vring_info *vring_info, int offset)
{
    if (role == VIRTIO_DEV_SLAVE)
       return offset;

    /* Master preallocate Buffer before vring */
    offset += vring_info->info.num_descs * VIRTIO_MMIO_MAX_DATA_SIZE;

    /* Align base address */
    offset = (offset + vring_info->info.align - 1) & ~(vring_info->info.align - 1);

    /* Set vring base address */
    vring_info->info.vaddr = metal_io_virt(vring_info->io, offset);

    /* Move to end of vring */
    offset += vring_size(vring_info->info.num_descs, vring_info->info.align);

    return offset;
}

/*
 * virtio mmio management
 */

static int virtio_mmio_register(struct virtio_device *vdev, uint64_t driver_features)
{
    if (vdev->role == VIRTIO_DEV_MASTER) {
        /* Virtio mmio driver init sequence */

        /* Reset the device */
        virtio_reset(vdev);

        /* Acknowledge that we've seen the device. */
        virtio_set_status(vdev, virtio_get_status(vdev) | VIRTIO_CONFIG_STATUS_ACK);

        /* We have a driver! */
        virtio_set_status(vdev, virtio_get_status(vdev) | VIRTIO_CONFIG_STATUS_DRIVER);

        /* Figure out what features the device supports. */
        /* lower 28 bits are device specific */
        vdev->features = virtio_mmio_get_features(vdev);
        /* other upper bits are reserved for future use or transport layer */
        vdev->features |= (uint64_t)_virtio_mmio_get_features(vdev, 1) << 32;

        /* Support only V1 */
        driver_features |= (uint64_t)1 << VIRTIO_F_VERSION_1;

        /*
         * Support limited access to memory :
         * Only the reserved memory region assigned to the virtio-mmio device
         */
        driver_features |= (uint64_t)1 << VIRTIO_F_ACCESS_PLATFORM;

        /* Check what is supported by the virtio driver */
        vdev->features &= driver_features;

        virtio_set_features(vdev, vdev->features);
        _virtio_mmio_set_features(vdev, vdev->features >> 32, 1);

        if (!(vdev->features & (uint64_t)(1) << VIRTIO_F_VERSION_1)) {
            /* V1 not supported. Reset and abort */
            virtio_reset(vdev);
            return -1;
        }
    } else {
        /* Virtio mmio device init sequence */

        /* Support only V1 */
        driver_features |= (uint64_t)1 << VIRTIO_F_VERSION_1;

        /*
         * Support limited access to memory :
         * Only the reserved memory region assigned to the virtio-mmio device
         */
        driver_features |= (uint64_t)1 << VIRTIO_F_ACCESS_PLATFORM;
        vdev->features = driver_features;

        virtio_mmio_write(vdev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
        virtio_mmio_write(vdev, VIRTIO_MMIO_DEVICE_FEATURES, vdev->features & 0xFFFFFFFF);
        virtio_mmio_write(vdev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
        virtio_mmio_write(vdev, VIRTIO_MMIO_DEVICE_FEATURES, vdev->features  >>  32);
    }

    /* Acknowledge that we've agreed on the features. */
    virtio_set_status(vdev, virtio_get_status(vdev) | VIRTIO_CONFIG_FEATURES_OK);

    return 0;
}

uint32_t virtio_mmio_get_max_elem(struct virtio_device *vdev, int idx)
{
    /* Select the queue we're interested in */
    virtio_mmio_write(vdev, VIRTIO_MMIO_QUEUE_SEL, idx);

    return virtio_mmio_read(vdev,VIRTIO_MMIO_QUEUE_NUM_MAX);
}

static int virtio_mmio_set_vq(struct virtio_device *vdev, struct virtqueue *vq)
{
    uint64_t addr;
    unsigned int num;

    if (vdev->role == VIRTIO_DEV_SLAVE)
        return 0;

    /* Select the queue we're interested in */
    virtio_mmio_write(vdev, VIRTIO_MMIO_QUEUE_SEL, vq->vq_queue_index);

    /* Queue shouldn't already be set up. */
    if (virtio_mmio_read(vdev, VIRTIO_MMIO_QUEUE_READY)) {
        LOG_WRN("virtio_mmio_set_vq %s already setup", vq->vq_name);
        return 0;
    }

    num = virtio_mmio_read(vdev,VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (num == 0) {
        LOG_WRN("virtio_mmio_set_vq %s max queue is NULL", vq->vq_name);
        return 0;
    }

    if (num < vq->vq_nentries) {
        LOG_WRN("virtio_mmio_set_vq %s nb entries %d exceeds max %d", vq->vq_name, vq->vq_nentries, num);
        return 0;
    }

    /* Activate the queue */
    virtio_mmio_write(vdev, VIRTIO_MMIO_QUEUE_NUM, vq->vq_nentries);

    addr = (uint64_t)metal_io_virt_to_phys(vq->shm_io, vq->vq_ring.desc);
    virtio_mmio_write(vdev, VIRTIO_MMIO_QUEUE_DESC_LOW, addr);
    virtio_mmio_write(vdev, VIRTIO_MMIO_QUEUE_DESC_HIGH, addr >> 32);

    addr = (uint64_t)metal_io_virt_to_phys(vq->shm_io, vq->vq_ring.avail);
    virtio_mmio_write(vdev, VIRTIO_MMIO_QUEUE_AVAIL_LOW, addr);
    virtio_mmio_write(vdev, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, addr >> 32);

    addr = (uint64_t)metal_io_virt_to_phys(vq->shm_io, vq->vq_ring.used);
    virtio_mmio_write(vdev, VIRTIO_MMIO_QUEUE_USED_LOW, addr);
    virtio_mmio_write(vdev, VIRTIO_MMIO_QUEUE_USED_HIGH, addr >> 32);

    virtio_mmio_write(vdev, VIRTIO_MMIO_QUEUE_READY, 1);

    return 1;
}

static int virtio_mmio_get_vq(struct virtio_device *vdev, struct virtqueue *vq)
{
    uint64_t addr;
    uint32_t num;

    if (vdev->role == VIRTIO_DEV_MASTER)
        return 0;

    /* Driver and virtqueues should already be set up. */
    if (!(virtio_mmio_read(vdev, VIRTIO_MMIO_STATUS) & VIRTIO_CONFIG_STATUS_DRIVER_OK)) {
        LOG_WRN("virtio_mmio_get_vq %s device not ready", vq->vq_name);
        return 0;
    }

    /* Select the queue, we're interested in */
    virtio_mmio_write(vdev, VIRTIO_MMIO_QUEUE_SEL, vq->vq_queue_index);

    /* Queue should already be set up. */
    if (!virtio_mmio_read(vdev, VIRTIO_MMIO_QUEUE_READY)) {
        LOG_WRN("virtio_mmio_set_vq %s not already setup", vq->vq_name);
        return 0;
    }

    num = virtio_mmio_read(vdev,VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (num == 0) {
        LOG_WRN("virtio_mmio_get_vq %s max queue is NULL", vq->vq_name);
        return 0;
    }

    /* Get the vring configuration */
    vq->vq_ring.num = vq->vq_nentries = virtio_mmio_read(vdev, VIRTIO_MMIO_QUEUE_NUM);

    addr = (uint64_t)virtio_mmio_read(vdev, VIRTIO_MMIO_QUEUE_DESC_HIGH) << 32;
    addr |= virtio_mmio_read(vdev, VIRTIO_MMIO_QUEUE_DESC_LOW);
    vq->vq_ring.desc = metal_io_phys_to_virt(vq->shm_io, addr);

    addr = (uint64_t)virtio_mmio_read(vdev, VIRTIO_MMIO_QUEUE_AVAIL_HIGH) << 32;
    addr |= virtio_mmio_read(vdev, VIRTIO_MMIO_QUEUE_AVAIL_LOW);
    vq->vq_ring.avail = metal_io_phys_to_virt(vq->shm_io, addr);

    addr = (uint64_t)virtio_mmio_read(vdev, VIRTIO_MMIO_QUEUE_USED_HIGH) << 32;
    addr |= virtio_mmio_read(vdev, VIRTIO_MMIO_QUEUE_USED_LOW);
    vq->vq_ring.used = metal_io_phys_to_virt(vq->shm_io, addr);

    /* Reset internal slave virtqueue idx */
    vq->vq_available_idx = vq->vq_ring.avail->idx;
    vq->vq_ring.used->idx = 0;

    return 1;
}

static struct virtqueue *virtio_mmio_init_vq(struct virtio_device *vdev, struct virtqueue *vq)
{
    if (virtio_mmio_set_vq(vdev, vq))
        return vq;

    virtio_mmio_get_vq(vdev, vq);

    if (vdev->role == VIRTIO_DEV_SLAVE)
        return vq;
    else
        return NULL;
}

static int virtio_mmio_ready(const struct device *dev, struct virtqueue *vq)
{
    struct virtio_mmio_data *driver_data = DEV_DATA(dev);
    struct virtio_device *vdev = &driver_data->device;
    uint32_t status;

    status = virtio_mmio_read(vdev, VIRTIO_MMIO_STATUS);

    if (!(status & VIRTIO_CONFIG_STATUS_DRIVER_OK)) {
        /*
	 * Driver is not yet ready or no more ready.
	 * Anyway, clear the slave_init variable
	 */
	driver_data->slave_init = 0;
        return 0;
    }

    /* The virtqueue is ready */
    if ((driver_data->slave_init >> vq->vq_queue_index) & 0x1)
        return 1;

    /* Otherwise, try to get vring config */
    if (virtio_mmio_get_vq(vdev, vq))
        driver_data->slave_init |= 1 << vq->vq_queue_index;

    return (driver_data->slave_init >> vq->vq_queue_index) & 0x1;
}

/*
 * virtio mmio device management
 */

static int virtio_mmio_init(const struct device *dev)
{
    const struct virtio_mmio_device_config *config = DEV_CFG(dev);
    struct virtio_mmio_data *driver_data = DEV_DATA(dev);
    struct virtio_device *vdev = &driver_data->device;
    uint32_t magic;

    /* Init virtio_device role */
    vdev->role = config->slave;

    /* Save ptr to zephyr device */
    vdev->priv = (void *)dev;

    /* Set mmio func */
    vdev->func = &virtio_mmio_dispatch;

    /* Set Interrupt handler */
    config->irq_config_func(dev);

    /* Map mmio device */
    device_map(DEVICE_MMIO_RAM_PTR(dev), (uintptr_t)config->base,
               config->size, K_MEM_CACHE_NONE);

    /* Detect the virtio_mmio device */

    /* Check magic value : "virt"*/
    magic = virtio_mmio_read(vdev, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != 0x74726976) {
        LOG_WRN("Wrong magic value 0x%08x!", magic);
        return -1;
    }

    /* Check device version */
    vdev->id.version = virtio_mmio_read(vdev, VIRTIO_MMIO_VERSION);
    if (vdev->id.version != 0x2) {
        LOG_WRN("Version %d not supported!", vdev->id.version);
        return -1;
    }

    /* Get device id */
    vdev->id.device = virtio_mmio_read(vdev, VIRTIO_MMIO_DEVICE_ID);
    if (vdev->id.device == 0) {
        LOG_WRN("Device ID %d not supported!", vdev->id.device);
        return -1;
    }

    /* Get vendor id */
    vdev->id.vendor = virtio_mmio_read(vdev, VIRTIO_MMIO_VENDOR_ID);

    /* Set metal io mem ops */
    virtio_mmio_get_metal_io(dev);

    return 0;
}

static int virtio_mmio_set_device_features(const struct device *dev, uint32_t features)
{
    struct virtio_mmio_data *driver_data = DEV_DATA(dev);

    driver_data->features = features;

    return 0;
}

static int virtio_mmio_max_data_size_get(const struct device *d)
{
    ARG_UNUSED(d);

    return VIRTIO_MMIO_MAX_DATA_SIZE;
}

static int virtio_mmio_get_vqs(const struct device *dev, unsigned int nvqs, struct virtqueue *vqs[],
        vq_callback callbacks[], const char *names[])
{
    struct virtio_mmio_data *driver_data = DEV_DATA(dev);
    struct virtio_device *vdev = &driver_data->device;
    struct virtio_vring_info *vrings_info;
    int i, offset = 0;

    /* register driver to virtio */
    if (virtio_mmio_register(vdev, (uint64_t)driver_data->features))
        return -1;

    if (nvqs > SCMI_VIRTQUEUE_MAX)
        return -1;

    /*
     * vrings info array is statically allocated
     * virtio scmi has 2 queues max
     */
    vrings_info = driver_data->vrings; //k_malloc(nvqs * sizeof(struct virtio_vring_info));
    vdev->vrings_num = nvqs;
    vdev->vrings_info = vrings_info;

    /* Map the virqueues in virtio mmio device */
    for (i = 0; i < nvqs; ++i) {
        struct virtio_vring_info *vring_info;
        struct vring_alloc_info *vring_alloc;

        if (!names[i]) {
            vqs[i] = NULL;
            continue;
        }

	vring_info = &vrings_info[i];

        /* Prepare vrings */
        vring_info->io = driver_data->shm_io;
        vring_info->info.num_descs = virtio_mmio_get_max_elem(vdev, i);
        vring_info->info.align = VRING_ALIGNMENT;

	/* Update memory allocation */
        offset = virtio_mmio_init_mem(vdev->role, vring_info, offset);

	/* Allocate virtqueue struct */
        vring_info->vq = k_malloc(sizeof(struct virtqueue) + vring_info->info.num_descs * sizeof(struct vq_desc_extra));

        vring_alloc = &vring_info->info;

        if (virtqueue_create(vdev, i, names[i], vring_alloc,
                             callbacks[i], vdev->func->notify,
                             vring_info->vq))
            return -1;

        virtqueue_set_shmem_io(vring_info->vq, driver_data->shm_io);

        vqs[i] = virtio_mmio_init_vq(vdev, vring_info->vq);
    }

    if (vdev->role == VIRTIO_DEV_MASTER)
        virtio_set_status(vdev, virtio_get_status(vdev) | VIRTIO_CONFIG_STATUS_DRIVER_OK);

    return 0;
}

static uint8_t *virtio_mmio_get_buffer(const struct device *dev, struct virtqueue *vq,
        unsigned int idx)
{
    struct virtio_mmio_data *driver_data = DEV_DATA(dev);
    struct virtio_device *vdev = &driver_data->device;

    if ((vdev->role == VIRTIO_DEV_MASTER) && (idx < vq->vq_nentries)) {
        /* Master mode allocate buffer just before descriptors */
        return (uint8_t *)(vq->vq_ring.desc) - (vq->vq_nentries - idx) * virtio_mmio_max_data_size_get(dev);
    } else {
        return NULL;
    }
}

static int virtio_mmio_pop(const struct device *dev, struct virtqueue *vq, struct virtio_buf *vqbuf)
{
    struct virtio_mmio_data *driver_data = DEV_DATA(dev);
    struct virtio_device *vdev = &driver_data->device;
    uint16_t index, in_idx;
    void *buffer;
    int lenght, status = 0;

    if (vdev->role == VIRTIO_DEV_MASTER) {
        /*
	 * Cookie has been set to 1st input buffer idx : writable by other side
	 */
	in_idx = (uint16_t) virtqueue_get_buffer(vq, &lenght, &index);
        if (lenght) {
            vqbuf->buffers[in_idx].len = lenght;
            vqbuf->index = (unsigned int)index;
            status = 1;
        }
    } else {
        int in_idx = 0;
        int out_idx = 0;

	/* Check if master has init the vring */
        if (!virtio_mmio_ready(dev, vq))
            return 0;

        /* Get 1st buffer and request descriptor index */
        buffer = virtqueue_get_available_buffer(vq, &index, &lenght);
        vqbuf->index = index;

        while (buffer) {
	    /* Add buffer in the user's buffer list */
            if (virtqueue_buffer_writable(vq, index)) {
                if (out_idx < vqbuf->out_num) {
                    vqbuf->buffers[out_idx].buf = buffer;
                    vqbuf->buffers[out_idx++].len = lenght;
		}
            } else if (in_idx < vqbuf->in_num) {
                vqbuf->buffers[vqbuf->out_num+in_idx].buf = buffer;
                vqbuf->buffers[vqbuf->out_num+in_idx++].len = lenght;
            }

	    /* Get next buffer */
            buffer = virtqueue_get_next_buffer(vq, &index, &lenght);
        }

	status = in_idx + out_idx;
    }

    return status;
}

static int virtio_mmio_push(const struct device *dev, struct virtqueue *vq, struct virtio_buf *vqbuf)
{
    struct virtio_mmio_data *driver_data = DEV_DATA(dev);
    struct virtio_device *vdev = &driver_data->device;
    int status;

    if (vdev->role == VIRTIO_DEV_MASTER) {
	/*
	 * cookie set to the idx of the 1st input buffer (after output buffer)
	 * master:
	 * 1st buffers are output : readable by other side
	 * then input buffers : writable by other side
	 */
        status = virtqueue_add_buffer(vq, vqbuf->buffers, vqbuf->out_num, vqbuf->in_num, (void *) vqbuf->out_num);
    } else {
        if (vqbuf->index > vq->vq_nentries)
            return ERROR_INVLD_DESC_IDX;
        /*
	 * 1st buffers are output : readable by other side
	 * then input buffers : writable by other side
	 * Use this order to stay aligned with master mode
	 * but we don't really care of the order because
	 * only the vring descriptor idx is used.
	 */
        status = virtqueue_add_consumed_buffer(vq, vqbuf->index, vqbuf->buffers[0].len);
    }

    virtqueue_kick(vq);
    return status;
}

static void virtio_mmio_set_callback(const struct device *dev,
                                   virtio_state_callback_t cb,
                                   void *user_data)
{
    struct virtio_mmio_data *driver_data = DEV_DATA(dev);

    driver_data->state_callback = cb;
    driver_data->user_data = user_data;
}

static int virtio_mmio_set_enabled(const struct device *dev, int enable)
{
    const struct virtio_mmio_device_config *config = DEV_CFG(dev);

    if (enable)
         irq_enable(config->irq);
    else
         irq_disable(config->irq);

    return 0;
}

static void virtio_mmio_reset(const struct device *dev, struct virtio_mmio_data *driver_data)
{
    if (driver_data->state_callback)
        driver_data->state_callback(dev, driver_data->user_data, 0);
}

static void virtio_mmio_isr(const struct device *dev)
{
    struct virtio_mmio_data *driver_data = DEV_DATA(dev);
    struct virtio_device *vdev = &driver_data->device;
    int i, irq, status;

    irq = virtio_mmio_read(vdev,VIRTIO_MMIO_INTERRUPT_STATUS);
    if (irq) {
        virtio_mmio_write(vdev,VIRTIO_MMIO_INTERRUPT_ACK, irq);
    }

    status = virtio_mmio_read(vdev, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_CONFIG_STATUS_DRIVER_OK)) {
        /*
	 * Driver is not yet ready or no more ready.
	 * Anyway, clear the slave_init variable
	 */
	if(driver_data->slave_init)
            virtio_mmio_reset(dev, driver_data);
        /*
	 * Driver is not yet ready or no more ready.
	 * Anyway, clear the slave_init variable
	 */
	driver_data->slave_init = 0;
    }

    if (irq & VIRTIO_MMIO_INT_CONFIG) {
        LOG_INF("virtio_mmio_isr config not supported");
    }

    if (irq & VIRTIO_MMIO_INT_VRING) {
        for (i = 0; i < vdev->vrings_num; i++) {
            virtqueue_notification(vdev->vrings_info[i].vq);
        }
    }
}

static const struct virtio_driver_api virtio_mmio_driver_api = {
    .set_device_features = virtio_mmio_set_device_features,
    .max_data_size_get = virtio_mmio_max_data_size_get,
    .get_vqs = virtio_mmio_get_vqs,
    .get_buffer = virtio_mmio_get_buffer,
    .pop = virtio_mmio_pop,
    .push = virtio_mmio_push,
    .set_callback = virtio_mmio_set_callback,
    .set_enabled = virtio_mmio_set_enabled,
};

#define CREATE_MY_DEVICE(inst)                                       \
     static void virtio_mmio_irq_config_func_##inst(const struct device *d) \
     {                                                               \
         ARG_UNUSED(d);                                              \
         IRQ_CONNECT(DT_INST_IRQN(inst),                             \
            DT_INST_IRQ(inst, priority),                             \
            virtio_mmio_isr,                                         \
            DEVICE_DT_INST_GET(inst),                                \
            0);                                                      \
     }                                                               \
     static struct virtio_mmio_data my_data_##inst = {               \
		.vq_callback = NULL,                                 \
		.state_callback = NULL,                              \
		.user_data = 0,                                      \
                .shm_device = {                                      \
		    .name = DT_PROP(DT_NODELABEL(virtio_mmio##inst), label), \
		    .bus = NULL,                                     \
		    .num_regions = 1,                                \
		    {                                                \
			{                                            \
				.virt       = (void *) NULL,         \
				.physmap    = NULL,                  \
				.size       = 0,                     \
				.page_shift = 0xffffffffffffffff,    \
				.page_mask  = 0xffffffffffffffff,    \
				.mem_flags  = 0,                     \
				.ops        = { NULL },              \
			},                                           \
		    },                                               \
                    .node = { NULL },                                \
                    .irq_num = 0,                                    \
                    .irq_info = NULL                                 \
	        }                                                    \
     };                                                              \
     static const struct virtio_mmio_device_config my_cfg_##inst = { \
             /* initialize ROM values as needed. */                  \
	    .base = (uint8_t *)DT_INST_REG_ADDR(inst),               \
	    .size = (unsigned int)DT_INST_REG_SIZE(inst),            \
	    .irq = DT_INST_IRQN(inst),                               \
	    .irq_config_func = virtio_mmio_irq_config_func_##inst,   \
	    .shared_mem = (uint8_t *)LINKER_DT_RESERVED_MEM_GET_PTR_BY_PHANDLE(DT_NODELABEL(virtio_mmio##inst), memory_region), \
	    .shared_size = (unsigned int)LINKER_DT_RESERVED_MEM_GET_SIZE_BY_PHANDLE(DT_NODELABEL(virtio_mmio##inst), memory_region), \
	    .slave = DT_PROP(DT_INST(inst, DT_DRV_COMPAT),device_mode) \
     };                                                              \
     DEVICE_DT_INST_DEFINE(inst,                                     \
                           virtio_mmio_init,                         \
                           device_pm_control_nop,                    \
                           &my_data_##inst,                          \
                           &my_cfg_##inst,                           \
                           PRE_KERNEL_1,                             \
		           CONFIG_KERNEL_INIT_PRIORITY_DEVICE,       \
                           &virtio_mmio_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_MY_DEVICE)

