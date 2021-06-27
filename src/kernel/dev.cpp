
// This file is a part of Simple-XX/SimpleKernel
// (https://github.com/Simple-XX/SimpleKernel).
//
// dev.cpp for Simple-XX/SimpleKernel.

#include "dev.h"
#include "virtio_blk.h"
#include "virtio_scsi.h"
#include "dtb.h"

DEV::DEV(void) {
    // 根据 dtb 获取硬件信息
    DTB dtb = DTB();
    // 获取块设备
    auto blk_dev = dtb.find("virtio_mmio@10001000");
    // 初始化 virtio
    // TODO: 这里的参数应该从 blk_dev 获取
    // TODO: 逻辑上应该是遍历 dtb，根据设备信息进行注册，
    // 而不是预设有什么设备主动注册
    // TODO: 在设备初始化之前，virtio queue 应该已经初始化了
    VIRTIO_BLK  blk  = VIRTIO_BLK((void *)0x10001000);
    VIRTIO_SCSI scsi = VIRTIO_SCSI((void *)0x10002000);
    return;
}

DEV::~DEV(void) {
    return;
}
