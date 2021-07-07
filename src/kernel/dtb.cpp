
// This file is a part of Simple-XX/SimpleKernel
// (https://github.com/Simple-XX/SimpleKernel).
//
// dtb.cpp for Simple-XX/SimpleKernel.

#include "string.h"
#include "stdio.h"
#include "assert.h"
#include "endian.h"
#include "dtb.h"
#include "iostream"
#include "common.h"

dtb_prop_node_t::dtb_prop_node_t(const mystl::string &_name) : name(_name) {
    return;
}

dtb_prop_node_t::~dtb_prop_node_t(void) {
    return;
}

// TODO: 父节点的属性需要拷贝给子节点
void dtb_prop_node_t::add_prop(mystl::string _name, uint32_t *_addr,
                               uint32_t _len) {
    // TODO: 只处理了标准属性
    // 根据字段设置
    if (_name == mystl::string(standard_props_t::COMPATIBLE)) {
        standard.compatible = mystl::string((char *)_addr);
// #define DEBUG
#ifdef DEBUG
        printf("standard.compatible: %s\n", standard.compatible.c_str());
#endif
    }
    if (_name == mystl::string(standard_props_t::MODEL)) {
        standard.model = mystl::string((char *)_addr);
#ifdef DEBUG
        printf("standard.model: %s\n", standard.model.c_str());
#endif
    }
    if (_name == mystl::string(standard_props_t::PHANDLE)) {
        standard.phandle = be32toh(*_addr);
    }
    if (_name == mystl::string(standard_props_t::STATUS)) {
        standard.status = mystl::string((char *)_addr);
#ifdef DEBUG
        printf("standard.status: %s\n", standard.status.c_str());
#endif
    }
    if (_name == mystl::string(standard_props_t::ADDR_CELLS)) {
        standard.addr_cells = be32toh(*_addr);
#ifdef DEBUG
        printf("standard.addr_cells: 0x%X\n", standard.addr_cells);
#endif
    }
    if (_name == mystl::string(standard_props_t::SIZE_CELLS)) {
        standard.size_cells = be32toh(*_addr);
    }
    if (_name == mystl::string(standard_props_t::REG)) {
#ifdef DEBUG
        printf("reg: ");
#endif
        for (uint32_t i = 0; i < _len; i++) {
            standard.reg.push_back(be32toh(*(_addr + i)));
#ifdef DEBUG
            printf("0x%X ", standard.reg.at(i));
#endif
        }
#ifdef DEBUG
        printf("\n");
#endif
    }
    if (_name == mystl::string(standard_props_t::VIRTUAL_REG)) {
        standard.virt_reg = *_addr;
    }
    if (_name == mystl::string(standard_props_t::RANGES)) {
        for (uint32_t i = 0; i < _len; i++) {
            standard.ranges.push_back(be32toh(*(_addr + i)));
        }
    }
    if (_name == mystl::string(standard_props_t::DMA_RANGES)) {
        for (uint32_t i = 0; i < _len; i++) {
            standard.dma_ranges.push_back(be32toh(*(_addr + i)));
        }
    }
    // TODO: 设备中断只处理了一项
    if (_name == mystl::string(interrupt_device_props_t::INTERRUPTS)) {
        for (uint32_t i = 0; i < _len; i++) {
            interrupt_device.interrupts = be32toh(*_addr);
#ifdef DEBUG
            printf("interrupt_device.interrupts: 0x%X\n",
                   interrupt_device.interrupts);
#endif
        }
    }
#ifdef DEBUG
#undef DEBUG
#endif
    return;
}

void dtb_prop_node_t::add_child(dtb_prop_node_t *_child) {
    children.push_back(_child);
    return;
}

void *dtb_addr;

// reserve 区域初始化
void DTB::reserve_init(void) {
    // 遍历保留区
    // devicetree-specification-v0.3.pdf#5.3.2
    fdt_reserve_entry_t *tmp1 = ((fdt_reserve_entry_t *)reserve_addr);
    fdt_reserve_entry_t  tmp2;
    while (1) {
        // 设置数据
        tmp2.address = be32toh(tmp1->address);
        tmp2.size    = be32toh(tmp1->size);
        // 如果为零则结束
        if (tmp2.size == 0 && tmp2.address == 0) {
            break;
        }
        // 不为零则添加到向量中
        // 保留区信息
        reserve.push_back(tmp2);
        // 下一项
        tmp1 += 1;
    }
}
// data 区域初始化
void DTB::data_init(void) {
    // 开始处理数据区
    uint32_t *       pos  = (uint32_t *)data_addr;
    uint32_t         tag  = be32toh(*pos);
    dtb_prop_node_t *node = nullptr;
    while (1) {
        printf("tag: 0x%X\n", tag);
        // BUG: 无法获取根节点 name  “/”
        switch (tag) {
            // 新建节点
            case FDT_BEGIN_NODE: {
                // 新建节点
                mystl::string name((char *)(pos + 1));
                node = new dtb_prop_node_t(name);
                assert(node != nullptr);
                nodes.push_back(node);
                // 跳过 tag
                pos++;
                // 跳过 name
                pos += COMMON::ALIGN(strlen((char *)pos) + 1, 4) / 4;
                break;
            }
            // 节点结束
            case FDT_END_NODE: {
                node = nullptr;
                // 跳过 tag
                pos++;
                break;
            }
            // 属性节点
            case FDT_PROP: {
                assert(node != nullptr);
                fdt_property_t *prop = (fdt_property_t *)pos;
                node->add_prop(get_string(be32toh(prop->nameoff)), prop->data,
                               be32toh(prop->len) / 4);
                // TODO: 将属性内存传递给处理函数
                // 跳过 tag
                pos++;
                // 跳过 len
                pos++;
                // 跳过 nameoff
                pos++;
                // 跳过属性数据
                pos += COMMON::ALIGN(be32toh(prop->len), 4) / 4;
                break;
            }
            // 直接跳过
            case FDT_NOP: {
                pos++;
                break;
            }
            // dtb 结束，直接返回
            case FDT_END: {
                return;
            }
            // 不是以上类型则出错
            default: {
                assert(0);
                return;
            }
        }
        // 更新 tag
        tag = be32toh(*pos);
    }
    return;
}

DTB::DTB(void) : addr(dtb_addr) {
    fdt_header_t header = *((fdt_header_t *)addr);
    // dtb 为大端，可能需要转换
    assert(be32toh(header.version) == 0x11);
    assert(header.magic == be32toh(FDT_MAGIC));
    size         = be32toh(header.totalsize);
    data_addr    = (ptrdiff_t)addr + be32toh(header.off_dt_struct);
    data_size    = be32toh(header.size_dt_struct);
    string_addr  = (ptrdiff_t)addr + be32toh(header.off_dt_strings);
    string_size  = be32toh(header.size_dt_strings);
    reserve_addr = (ptrdiff_t)addr + be32toh(header.off_mem_rsvmap);
    reserve_init();
    data_init();
    printf("dtb init.\n");
    return;
}

DTB::~DTB(void) {
    return;
}

char *DTB::get_string(uint64_t _off) {
    return (char *)string_addr + _off;
}

const mystl::vector<resource_t *> DTB::find(mystl::string _name) {
    mystl::vector<resource_t *> res;
    resource_t *                tmp = nullptr;
    // 遍历所有节点
    for (auto i : nodes) {
        // 找到属性中 kv 为 compatible:_name 的节点
        if (i->name == _name) {
            // TODO: 这里可以优化
            // 根据 prop 分别设置
            // 中断
            // TODO: 这里简单的根据中断号是否为零来判断，需要优化
            if (i->interrupt_device.interrupts != 0x00) {
                tmp = new resource_t();
                // 填充字段
                tmp->type   = resource_t::INTR;
                tmp->name   = "INTR";
                tmp->irq_no = i->interrupt_device.interrupts;
                // 加入向量
                // BUG: 会造成 data_init 执行出错
                res.push_back(tmp);
            }
            // 内存
            // TODO: 这里简单的根据 reg 长度是否为零进行判断，需要优化
            if (i->standard.reg.size() != 0) {
                // 新建 resource_t 对象
                tmp = new resource_t;
                // 填充字段
                tmp->type = resource_t::MEMORY;
                tmp->name = "MEMORY";
                // TODO: 这里需要根据 addr_cells 与 size_cells 计算
                tmp->mem.start =
                    (void *)(((uint64_t)i->standard.reg.at(0) << 32) +
                             i->standard.reg.at(1));
                tmp->mem.end =
                    (void *)(((uint64_t)i->standard.reg.at(2) << 32) +
                             i->standard.reg.at(3));
                // 加入向量
                res.push_back(tmp);
            }
        }
    }
    return res;
}

void dtb_preinit(uint32_t, void *_addr) {
    dtb_addr = _addr;
    return;
}
