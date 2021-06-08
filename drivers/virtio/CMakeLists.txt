# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_VIRTIO_MMIO ipm_virtio_mmio.c)


zephyr_library_sources_ifdef(CONFIG_USERSPACE   ipm_handlers.c)
