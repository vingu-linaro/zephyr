/**
 * @file
 *
 * @brief Generic low-level inter-processor mailbox communication API.
 */

/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_virtio_H_
#define ZEPHYR_INCLUDE_DRIVERS_virtio_H_

/**
 * @brief virtio Interface
 * @defgroup virtio_interface virtio Interface
 * @ingroup io_interfaces
 * @{
 */

#include <kernel.h>
#include <device.h>
#include <openamp/virtqueue.h>

#ifdef __cplusplus
extern "C" {
#endif

struct virtio_buf {
    unsigned int index;
    unsigned int out_num;
    unsigned int in_num;
    struct virtqueue_buf *buffers;
};

/**
 * @typedef virtio_callback_t
 * @brief Callback API for incoming virtio messages
 *
 * These callbacks execute in interrupt context. Therefore, use only
 * interrupt-safe APIS. Registration of callbacks is done via
 * @a virtio_get_vqs
 *
 */
typedef void (*virtio_callback_t)(struct virtqueue *vqueue);

/**
 * @typedef virtio_state_callback_t
 * @brief Callback API for virtio device state change
 *
 * These callbacks execute in interrupt context. Therefore, use only
 * interrupt-safe APIS. Registration of callbacks is done via
 * @a virtio_register_callback
 *
 */

typedef void (*virtio_state_callback_t)(const struct device *virtiodev,
					void *user_data,
					int state);

/**
 * @typedef virtio_push_t
 * @brief Callback API to send virtio request
 *
 * @param virtiodev  Driver instance
 * @param vq         virqueue to use to get requests
 * @param vqbuf      request buffer
 */
typedef int (*virtio_push_t)(const struct device *virtiodev,
                             struct virtqueue *vq,
			     struct virtio_buf *vqbuf);

/**
 * @typedef virtio_pop_t
 * @brief Callback API to get virtio request
 *
 * @param virtiodev  Driver instance
 * @param vq         virqueue to use to get requests
 * @param vqbuf      request buffer
 */
typedef int (*virtio_pop_t)(const struct device *virtiodev,
                             struct virtqueue *vq,
			     struct virtio_buf *vqbuf);

/**
 * @typedef virtio_max_data_size_get_t
 * @brief Callback API to get maximum data size
 *
 * @param virtiodev  Driver instance
 */
typedef int (*virtio_max_data_size_get_t)(const struct device *virtiodev);

/**
 * @typedef virtio_get_vqs_t
 * @brief Callback API to create and get virqueues
 *
 * @param virtiodev  Driver instance
 * @param nvqs       Number of virtqueue to create
 * @param vqs        Array to fill with virtqueues pointer
 * @param callbacks  Array of callbacks to run when incoming notificiation
 * @param names      Array of virtqueues' name
 */
typedef int (*virtio_get_vqs_t)(const struct device *virtiodev,
		                    unsigned int nvqs,
				    struct virtqueue *vqs[],
				    vq_callback callbacks[],
				    const char *names[]);

/**
 * @typedef virtio_get_buffer_t
 * @brief Callback API to get a buffer in the reserved memory
 *
 * @param virtiodev  Driver instance
 * @param vq         Targeted virtqueue for the buffer
 * @param idx        Index in the list of buffer
 *
 * @return pointer to a buffer
 */
typedef uint8_t *(*virtio_get_buffer_t)(const struct device *virtiodev,
				    struct virtqueue *vq,
				    unsigned int idx);

/**
 * @typedef virtio_set_cookie
 * @brief Callback API to register user's data
 *
 * @param virtiodev  Driver instance
 * @param user_data  Argument for notificiation CB
 */
typedef void (*virtio_register_callback)(const struct device *virtiodev,
					virtio_state_callback_t cb,
					void *user_data);

/**
 * @typedef virtio_set_enabled_t
 * @brief Callback API to enable Notification interrupts
 *
 * @param virtiodev Driver instance
 * @param enable    Boolean to enable/disable interrupt
 */
typedef int (*virtio_set_enabled_t)(const struct device *virtiodev, int enable);

/**
 * @typedef virtio_set_device_feature_t
 * @brief set virtio supported features
 *
 * @param virtiodev Driver instance
 * @param features  Bitmask of supported features
 */
typedef int (*virtio_set_device_features_t)(const struct device *virtiodev,
					uint32_t features);


__subsystem struct virtio_driver_api {
	virtio_set_device_features_t set_device_features;
	virtio_get_vqs_t get_vqs;
	virtio_get_buffer_t get_buffer;
	virtio_max_data_size_get_t max_data_size_get;
	virtio_pop_t pop;
	virtio_push_t push;
	virtio_register_callback set_callback;
	virtio_set_enabled_t set_enabled;
};

/**
 * @brief Try to send a message over the virtio device.
 *
 * A message is considered consumed once the remote interrupt handler
 * finishes. If there is deferred processing on the remote side,
 * or if outgoing messages must be queued and wait on an
 * event/semaphore, a high-level driver can implement that.
 *
 * There are constraints on how much data can be sent or the maximum value
 * of id. Use the @a virtio_max_data_size_get and @a virtio_max_id_val_get routines
 * to determine them.
 *
 * The @a size parameter is used only on the sending side to determine
 * the amount of data to put in the message registers. It is not passed along
 * to the receiving side. The upper-level protocol dictates the amount of
 * data read back.
 *
 * @param virtiodev Driver instance
 * @param wait If nonzero, busy-wait for remote to consume the message. The
 * to determine them.
 *
 * The @a size parameter is used only on the sending side to determine
 * the amount of data to put in the message registers. It is not passed along
 * to the receiving side. The upper-level protocol dictates the amount of
 * data read back.
 *
 * @param virtiodev Driver instance
 * @param wait If nonzero, busy-wait for remote to consume the message. The
 *	       message is considered consumed once the remote interrupt handler
 *	       finishes. If there is deferred processing on the remote side,
 *	       or you would like to queue outgoing messages and wait on an
 *	       event/semaphore, you can implement that in a high-level driver
 * @param id Message identifier. Values are constrained by
 *        @a virtio_max_data_size_get since many boards only allow for a
 *        subset of bits in a 32-bit register to store the ID.
 * @param data Pointer to the data sent in the message.
 * @param size Size of the data.
 *
 * @retval -EBUSY    If the remote hasn't yet read the last data sent.
 * @retval -EMSGSIZE If the supplied data size is unsupported by the driver.
 * @retval -EINVAL   If there was a bad parameter, such as: too-large id value.
 *                   or the device isn't an outbound virtio channel.
 * @retval 0         On success.
 */
__syscall int virtio_push(const struct device *virtiodev, struct virtqueue *vq, struct virtio_buf *vqbuf);

static inline int z_impl_virtio_push(const struct device *virtiodev,
                                     struct virtqueue *vq,
				     struct virtio_buf *vqbuf)
{
	const struct virtio_driver_api *api =
		(const struct virtio_driver_api *)virtiodev->api;

	return api->push(virtiodev, vq, vqbuf);
}

/**
 * @brief Try to get a message over the virtio device.
 *
 * A message is considered consumed once the remote interrupt handler
 * finishes. If there is deferred processing on the remote side,
 * or if outgoing messages must be queued and wait on an
 * event/semaphore, a high-level driver can implement that.
 *
 * There are constraints on how much data can be sent or the maximum value
 * of id. Use the @a virtio_max_data_size_get and @a virtio_max_id_val_get routines
 * to determine them.
 *
 * The @a size parameter is used only on the sending side to determine
 * the amount of data to put in the message registers. It is not passed along
 * to the receiving side. The upper-level protocol dictates the amount of
 * data read back.
 *
 * @param virtiodev Driver instance
 * @param wait If nonzero, busy-wait for remote to consume the message. The
 *	       message is considered consumed once the remote interrupt handler
 *	       finishes. If there is deferred processing on the remote side,
 *	       or you would like to queue outgoing messages and wait on an
 *	       event/semaphore, you can implement that in a high-level driver
 * @param id Message identifier. Values are constrained by
 *        @a virtio_max_data_size_get since many boards only allow for a
 *        subset of bits in a 32-bit register to store the ID.
 * @param data Pointer to the data sent in the message.
 * @param size Size of the data.
 *
 * @retval -EBUSY    If the remote hasn't yet read the last data sent.
 * @retval -EMSGSIZE If the supplied data size is unsupported by the driver.
 * @retval -EINVAL   If there was a bad parameter, such as: too-large id value.
 *                   or the device isn't an outbound virtio channel.
 * @retval 0         On success.
 */
__syscall int virtio_pop(const struct device *virtiodev, struct virtqueue *vq, struct virtio_buf *vqbuf);

static inline int z_impl_virtio_pop(const struct device *virtiodev,
                                     struct virtqueue *vq,
				     struct virtio_buf *vqbuf)
{
	const struct virtio_driver_api *api =
		(const struct virtio_driver_api *)virtiodev->api;

	return api->pop(virtiodev, vq, vqbuf);
}

/**
 * @brief Return the maximum number of bytes possible in an outbound message.
 *
 * virtio implementations vary on the amount of data that can be sent in a
 * single message since the data payload is typically stored in registers.
 *
 * @param virtiodev Driver instance pointer.
 *
 * @return Maximum possible size of a message in bytes.
 */
__syscall int virtio_max_data_size_get(const struct device *virtiodev);

static inline int z_impl_virtio_max_data_size_get(const struct device *virtiodev)
{
	const struct virtio_driver_api *api =
		(const struct virtio_driver_api *)virtiodev->api;

	return api->max_data_size_get(virtiodev);
}

/**
 * @brief Return virtqueues of the virtio device.
 *
 * @param virtiodev Driver instance pointer.
 *
 * @retval 0       On success.
 * @retval -EINVAL On error.
 */
__syscall int virtio_get_vqs(const struct device *virtiodev,
					unsigned int nvqs, struct virtqueue **vqs,
					void *callbacks, const char **names);

static inline int z_impl_virtio_get_vqs(const struct device *virtiodev,
					unsigned int nvqs, struct virtqueue **vqs,
					void *callbacks, const char **names)
{
	const struct virtio_driver_api *api =
		(const struct virtio_driver_api *)virtiodev->api;

	return api->get_vqs(virtiodev, nvqs, vqs, (vq_callback *)callbacks, names);
}

/**
 * @brief Return a buffer from the reserved memory.
 *
 * @param virtiodev Driver instance pointer.
 *
 * @retval buffer's pointer
 */
__syscall uint8_t *virtio_get_buffer(const struct device *virtiodev,
				     struct virtqueue *vq,
				     unsigned int idx);

static inline uint8_t *z_impl_virtio_get_buffer(const struct device *virtiodev,
						struct virtqueue *vq,
						unsigned int idx)
{
	const struct virtio_driver_api *api =
		(const struct virtio_driver_api *)virtiodev->api;

	return api->get_buffer(virtiodev, vq, idx);
}

/**
 * @brief Enable interrupts on virtio device.
 *
 * @param virtiodev Driver instance pointer.
 * @param enable Set to 0 to disable and to nonzero to enable.
 *
 * @retval 0       On success.
 * @retval -EINVAL If it isn't an inbound channel.
 */
__syscall int virtio_set_enabled(const struct device *virtiodev, int enable);

static inline int z_impl_virtio_set_enabled(const struct device *virtiodev,
					    int enable)
{
	const struct virtio_driver_api *api =
		(const struct virtio_driver_api *)virtiodev->api;

	return api->set_enabled(virtiodev, enable);
}

/**
 * @brief Set a cookie that will be used by callback function for incoming messages.
 *
 * @param virtiodev Driver instance pointer.
 * @param user_data Application-specific data pointer which will be passed
 *        to the callback function when executed.
 */
__syscall void virtio_set_callback(const struct device *virtiodev,
                                   virtio_state_callback_t cb,
				   void *user_data);

static inline void z_impl_virtio_set_callback(const struct device *virtiodev,
					      virtio_state_callback_t cb,
					      void *user_data)
{
	const struct virtio_driver_api *api =
		(const struct virtio_driver_api *)virtiodev->api;

	api->set_callback(virtiodev, cb, user_data);
}

/**
 * @brief Set the features supported but the virtio driver.
 *
 * @param virtiodev Driver instance pointer.
 * @param features Bitmask of the features supported by the vritio device.
 */
__syscall int virtio_set_device_features(const struct device *virtiodev,
				  uint32_t features);

static inline int z_impl_virtio_set_device_features(const struct device *virtiodev,
					     uint32_t features)
{
	const struct virtio_driver_api *api =
		(const struct virtio_driver_api *)virtiodev->api;

	return api->set_device_features(virtiodev, features);
}


#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#include <syscalls/virtio.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_virtio_H_ */
