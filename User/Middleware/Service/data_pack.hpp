#ifndef __DATA_PACK_HPP__
#define __DATA_PACK_HPP__

#include "cmsis_os2.h"

/** @brief 单帧数据区最大字节数。 */
#define DATA_PACK_MAX_LENGTH 100

/**
 * @brief 数据打包格式。
 */
enum DataFormat_t {
    HEX, /**< 以二进制字节流方式打包与发送。 */
    STR, /**< 以字符串方式打包与发送（当前未实现）。 */
};

/**
 * @brief 单帧数据包。
 *
 * @details
 * DataPack 负责管理变量地址、打包与解包，以及通过 USB CDC 收发单帧数据。
 */
class DataPack 
{
private:
    /** @brief 数据缓存区。 */
    uint8_t data[DATA_PACK_MAX_LENGTH];
    /** @brief 当前缓存区有效字节数。 */
    uint32_t dataLength;

    /**
     * @brief 支持的数据类型标签。
     */
    enum VarType {
            VarType_uint8,
            VarType_uint16,
            VarType_uint32,
            VarType_uint64,
            VarType_int8,
            VarType_int16,
            VarType_int32,
            VarType_int64,
            VarType_float,
            VarType_double,
            VarType_string,
    };

    /**
     * @brief 单个数据源条目。
     */
    struct VarEntry
    {
        void* addr;   /**< 变量地址。 */
        VarType type; /**< 变量类型。 */
    };

    /** @brief 固定帧头（由构造函数指定）。 */
    uint8_t header;

    /** @brief 本包绑定的数据源数组。 */
    VarEntry dataSource[DATA_PACK_MAX_LENGTH];

    /** @brief 已绑定的数据源数量。 */
    uint32_t dataSourceLength;

    /** @brief 关联的数据包源数组。 */
    DataPack* dataPackSource[DATA_PACK_MAX_LENGTH];

    /** @brief 已关联的数据包源数量。 */
    uint8_t dataPackSourceLength;

    /** @brief 全局打包格式。 */
    static DataFormat_t dataFormat;

    /** @brief 清空缓存数据并复位长度。 */
    void ClearData();

    /**
     * @brief 添加一个数据源条目。
     * @param addr 变量地址。
     * @param type 变量类型。
     * @return osStatus_t 添加结果。
     */
    osStatus_t LinkDataEntry_(void* addr, VarType type);

public:
    /**
     * @brief 构造函数。
     * @param dataFormat 数据格式。
     */
    DataPack(uint8_t header = 0xAA, DataFormat_t dataFormat = HEX);

    /** @brief 析构函数。 */
    ~DataPack();

    /** @brief 绑定 uint8_t 变量地址。 */
    osStatus_t LinkData(uint8_t* dataSource);
    /** @brief 绑定 uint16_t 变量地址。 */
    osStatus_t LinkData(uint16_t* dataSource);
    /** @brief 绑定 uint32_t 变量地址。 */
    osStatus_t LinkData(uint32_t* dataSource);
    /** @brief 绑定 uint64_t 变量地址。 */
    osStatus_t LinkData(uint64_t* dataSource);
    /** @brief 绑定 int8_t 变量地址。 */
    osStatus_t LinkData(int8_t* dataSource);
    /** @brief 绑定 int16_t 变量地址。 */
    osStatus_t LinkData(int16_t* dataSource);
    /** @brief 绑定 int32_t 变量地址。 */
    osStatus_t LinkData(int32_t* dataSource);
    /** @brief 绑定 int64_t 变量地址。 */
    osStatus_t LinkData(int64_t* dataSource);
    /** @brief 绑定 float 变量地址。 */
    osStatus_t LinkData(float* dataSource);
    /** @brief 绑定 double 变量地址。 */
    osStatus_t LinkData(double* dataSource);

    /**
     * @brief 绑定字符串地址。
     * @param str 以 '\0' 结束的字符串地址。
     * @return osStatus_t 绑定结果。
     */
    osStatus_t LinkData(const char* str);

    /**
     * @brief 关联一个 DataPack，并复制其数据源条目到当前对象。
     * @param packSource 源包对象。
     * @return osStatus_t 关联结果。
     */
    osStatus_t LinkDataPack(DataPack *packSource);

    /**
     * @brief 从已绑定变量读取值并打包到内部缓存。
     * @return osStatus_t 打包结果。
     */
    osStatus_t GetData();

    /**
     * @brief 将内部缓存按绑定顺序解包并写回变量。
     * @return osStatus_t 解包结果。
     */
    osStatus_t DistributeData();

    /**
     * @brief 打包并通过 USB CDC 发送数据。
     *
     * @details
     * 发送帧格式：`[header][payload][tail_hash]`，其中 `tail_hash` 为 payload 的哈希值。
     * @return osStatus_t 发送结果。
     */
    osStatus_t SendData();

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
    osStatus_t ReceiveData();
};

#endif // __DATA_PACK_HPP__