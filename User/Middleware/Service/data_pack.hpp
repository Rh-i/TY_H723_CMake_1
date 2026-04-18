#ifndef __DATA_PACK_HPP__
#define __DATA_PACK_HPP__

#include "cmsis_os2.h"

/** @brief 单帧数据区最大字节数。 */
#define DATA_PACK_MAX_LENGTH 100

/**
 * @brief 数据打包格式。
 */
enum data_format {
    DATA_FORMAT_HEX, /**< 以二进制字节流方式打包与发送。 */
    DATA_FORMAT_STR, /**< 以字符串方式打包与发送（当前未实现）。 */
};

/**
 * @brief 单帧数据包。
 *
 * @details
 * data_pack 负责管理变量地址、打包与解包，以及通过 USB CDC 收发单帧数据。
 */
class data_pack 
{
private:
    /** @brief 数据缓存区。 */
    uint8_t data[DATA_PACK_MAX_LENGTH];
    /** @brief 当前缓存区有效字节数。 */
    uint32_t data_length;

    /**
     * @brief 支持的数据类型标签。
     */
    enum var_type {
            VAR_TYPE_UINT8,
            VAR_TYPE_UINT16,
            VAR_TYPE_UINT32,
            VAR_TYPE_UINT64,
            VAR_TYPE_INT8,
            VAR_TYPE_INT16,
            VAR_TYPE_INT32,
            VAR_TYPE_INT64,
            VAR_TYPE_FLOAT,
            VAR_TYPE_DOUBLE,
            VAR_TYPE_STRING,
    };

    /**
     * @brief 单个数据源条目。
     */
    struct var_entry
    {
        void* addr;   /**< 变量地址。 */
        var_type type; /**< 变量类型。 */
    };

    /** @brief 固定帧头（由构造函数指定）。 */
    uint8_t header;

    /** @brief 本包绑定的数据源数组。 */
    var_entry data_source[DATA_PACK_MAX_LENGTH];

    /** @brief 已绑定的数据源数量。 */
    uint32_t data_source_length;

    /** @brief 关联的数据包源数组。 */
    data_pack* data_pack_source[DATA_PACK_MAX_LENGTH];

    /** @brief 已关联的数据包源数量。 */
    uint8_t data_pack_source_length;

    /** @brief 全局打包格式。 */
    static data_format data_format_;

    /** @brief 清空缓存数据并复位长度。 */
    void clear_data();

    /**
     * @brief 添加一个数据源条目。
     * @param addr 变量地址。
     * @param type 变量类型。
     * @return osStatus_t 添加结果。
     */
    osStatus_t link_data_entry_(void* addr, var_type type);

public:
    /**
     * @brief 构造函数。
     * @param header 帧头。
     * @param data_format 数据格式。
     */
    data_pack(uint8_t header = 0xAA, data_format data_format = DATA_FORMAT_HEX);

    /** @brief 析构函数。 */
    ~data_pack();

    /** @brief 绑定 uint8_t 变量地址。 */
    osStatus_t link_data(uint8_t* data_source);
    /** @brief 绑定 uint16_t 变量地址。 */
    osStatus_t link_data(uint16_t* data_source);
    /** @brief 绑定 uint32_t 变量地址。 */
    osStatus_t link_data(uint32_t* data_source);
    /** @brief 绑定 uint64_t 变量地址。 */
    osStatus_t link_data(uint64_t* data_source);
    /** @brief 绑定 int8_t 变量地址。 */
    osStatus_t link_data(int8_t* data_source);
    /** @brief 绑定 int16_t 变量地址。 */
    osStatus_t link_data(int16_t* data_source);
    /** @brief 绑定 int32_t 变量地址。 */
    osStatus_t link_data(int32_t* data_source);
    /** @brief 绑定 int64_t 变量地址。 */
    osStatus_t link_data(int64_t* data_source);
    /** @brief 绑定 float 变量地址。 */
    osStatus_t link_data(float* data_source);
    /** @brief 绑定 double 变量地址。 */
    osStatus_t link_data(double* data_source);

    /**
     * @brief 绑定字符串地址。
     * @param str 以 '\0' 结束的字符串地址。
     * @return osStatus_t 绑定结果。
     */
    osStatus_t link_data(const char* str);

    /**
     * @brief 关联一个 data_pack，并复制其数据源条目到当前对象。
     * @param pack_source 源包对象。
     * @return osStatus_t 关联结果。
     */
    osStatus_t link_data_pack(data_pack *pack_source);

    /**
     * @brief 从已绑定变量读取值并打包到内部缓存。
     * @return osStatus_t 打包结果。
     */
    osStatus_t get_data();

    /**
     * @brief 将内部缓存按绑定顺序解包并写回变量。
     * @return osStatus_t 解包结果。
     */
    osStatus_t distribute_data();

    /**
     * @brief 打包并通过 USB CDC 发送数据。
     *
     * @details
     * 发送帧格式：`[header][payload][tail_hash]`，其中 `tail_hash` 为 payload 的哈希值。
     * @return osStatus_t 发送结果。
     */
    osStatus_t send_data();

    /**
     * @brief 从 USB CDC 接收数据并分发到已绑定变量。
     *
     * @details
     * 接收后按 `[header][payload][tail_hash]` 校验：
     * 1) 帧头匹配；
     * 2) 尾部哈希与 payload 计算值一致；
     * 3) STR 模式下字符串字段内容一致。
     * @return osStatus_t 接收与分发结果。
     */
    osStatus_t receive_data();
};

#endif // __DATA_PACK_HPP__