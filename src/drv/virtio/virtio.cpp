
// This file is a part of Simple-XX/SimpleKernel
// (https://github.com/Simple-XX/SimpleKernel).
//
// virtio.cpp for Simple-XX/SimpleKernel.

#include "stdint.h"
#include "string.h"
#include "stddef.h"
#include "stdio.h"
#include "io.h"
#include "common.h"
#include "assert.h"
#include "vmm.h"
#include "virtio.h"

VIRTIO::virtio_queue_t::virtio_queue_t(size_t _size) {
    // virtio-v1.1#2.5
    // virtio-v1.1#2.6
    // _size 应该为 2 的幂
    assert((_size & (_size - 1)) == 0);
    // TODO: 解释
    // 计算偏移
    // desc 偏移为 64
    off_desc = COMMON::ALIGN(sizeof(virtq_t), virtq_desc_t::ALIGN);
    // avail 偏移为在 desc 基地址+desc 个数*desc 长度，最后进行对齐
    off_avail = COMMON::ALIGN(off_desc + _size * sizeof(virtq_desc_t),
                              virtq_avail_t::ALIGN);
    off_used_event =
        (off_avail + sizeof(virtq_avail_t) + _size * sizeof(uint16_t));
    off_used =
        COMMON::ALIGN(off_used_event + sizeof(uint16_t), virtq_used_t::ALIGN);
    off_avail_event =
        (off_used + sizeof(virtq_used_t) + _size * sizeof(virtq_used_elem_t));
    off_desc_virt =
        COMMON::ALIGN(off_avail_event + sizeof(uint16_t), sizeof(void *));
    // 如果需要的内存大于1页则出错
    if (off_desc_virt + _size * sizeof(void *) > COMMON::PAGE_SIZE) {
        printf("virtq_create: error, too big for two pagess\n");
        return;
    }
    uint8_t *page_virt = (uint8_t *)malloc(COMMON::PAGE_SIZE);
    virtq              = (virtq_t *)page_virt;
    virtq->phys        = VMM_LA_PA((ptrdiff_t)page_virt);
    virtq->len         = _size;

    virtq->desc        = (virtq_desc_t *)(page_virt + off_desc);
    virtq->avail       = (virtq_avail_t *)(page_virt + off_avail);
    virtq->used_event  = (uint16_t *)(page_virt + off_used_event);
    virtq->used        = (virtq_used_t *)(page_virt + off_used);
    virtq->avail_event = (uint16_t *)(page_virt + off_avail_event);
    virtq->desc_virt   = (void **)(page_virt + off_desc_virt);

    virtq->avail->idx = 0;
    virtq->used->idx  = 0;
    virtq->seen_used  = virtq->used->idx;
    virtq->free_desc  = 0;

    for (size_t i = 0; i < _size; i++) {
        virtq->desc[i].next = i + 1;
    }

    return;
}

VIRTIO::virtio_queue_t::~virtio_queue_t(void) {
    free(virtq);
    return;
}

uint32_t VIRTIO::virtio_queue_t::alloc_desc(void *_addr) {
    uint32_t desc = virtq->free_desc;
    uint32_t next = virtq->desc[desc].next;
    if (desc == virtq->len) {
        printf("ERROR: ran out of virtqueue descriptors\n");
    }
    virtq->free_desc = next;

    virtq->desc[desc].addr = VMM_LA_PA((ptrdiff_t)_addr);
    virtq->desc_virt[desc] = _addr;
    return desc;
}

void VIRTIO::virtio_queue_t::free_desc(uint32_t desc) {
    virtq->desc[desc].next = virtq->free_desc;
    virtq->free_desc       = desc;
    virtq->desc_virt[desc] = nullptr;
    return;
}

uint32_t VIRTIO::read(void *_off) {
    return io.read32(_off);
}

uint64_t VIRTIO::read64(void *_off) {
    return io.read64(_off);
}

void VIRTIO::write(void *_off, uint32_t _val) {
    io.write32(_off, _val);
    return;
}

bool VIRTIO::set_features(const mystl::vector<feature_t> &_features) {
    // 首先获取硬件信息
    // 0~31 位
    write(&regs->device_features_sel, 0);
    uint32_t device_feature_low = read(&regs->device_features);
    // 32~63 位
    write(&regs->device_features_sel, 1);
    uint32_t device_feature_high = read(&regs->device_features);

    // TODO: 如果有设备不支持的 feature，需要有错误反馈
    for (auto i : _features) {
        // 如果 i.bit 大于 31，那么意味着需要 DeviceFeatures 的 32~63 位，
        // 将 DeviceFeaturesSel 置位即可
        if (i.bit > 31) {
            // 如果设备支持且需要此属性
            if ((i.status == true)) {
                if ((device_feature_high & (1 << (i.bit - 32))) == true) {
                    // 选择写高位
                    write(&regs->device_features_sel, 1);
                    // 写入
                    write(&regs->driver_features,
                          read(&regs->driver_features) | (1 << (i.bit - 32)));
                }
                else {
                    err("Not support: %s\n", i.name.c_str());
                }
            }
        }
        else {
            // 如果设备支持且需要此属性
            if ((i.status == true)) {
                if (((device_feature_low & (1 << i.bit)) == true)) {
                    // 选择写低位
                    write(&regs->driver_features_sel, 0);
                    // 写入
                    write(&regs->driver_features,
                          read(&regs->driver_features) | (1 << i.bit));
                }
                else {
                    err("Not support: %s\n", i.name.c_str());
                }
            }
        }
    }
    return true;
}

void VIRTIO::add_to_device(uint32_t _queue_sel) {
    write(&regs->queue_sel, _queue_sel);
    write(&regs->queue_num, queue->virtq->len);
    write(&regs->queue_desc_low,
          queue->virtq->phys +
              ((uint8_t *)queue->virtq->desc - (uint8_t *)queue->virtq));
    write(&regs->queue_desc_high, 0);
    write(&regs->queue_driver_low,
          queue->virtq->phys +
              ((uint8_t *)queue->virtq->avail - (uint8_t *)queue->virtq));
    write(&regs->queue_driver_high, 0);
    write(&regs->queue_device_low,
          queue->virtq->phys +
              ((uint8_t *)queue->virtq->used - (uint8_t *)queue->virtq));
    write(&regs->queue_device_high, 0);
    write(&regs->queue_ready, 1);
    return;
}

VIRTIO::VIRTIO(void *_addr, virt_device_type_t _type)
    : regs((virtio_regs_t *)_addr) {
    // 检查相关值
    assert(read(&regs->magic) == MAGIC_VALUE);
    assert(read(&regs->version) == VERSION);
    // 检查类型是否符合
    assert(read(&regs->device_id) == _type);
    // 初始化
    // virtio-v1.1#3.1.1
    // 重置设备
    write(&regs->status, 0);
    // 置位 ACKNOWLEDGE
    VIRTIO::write(&regs->status,
                  read(&regs->status) | DEVICE_STATUS_ACKNOWLEDGE);
    // 置位 DRIVER
    VIRTIO::write(&regs->status, read(&regs->status) | DEVICE_STATUS_DRIVER);
    // 接下来设置设备相关 feature，交给特定设备进行
    // 跳转到 virtio_blk.cpp 的构造函数
    return;
}

VIRTIO::~VIRTIO(void) {
    return;
}
