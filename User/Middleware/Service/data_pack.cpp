#include "data_pack.hpp"
#include "bsp_usb.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file data_pack.cpp
 * @brief DataPack 的打包、解包与 USB 传输实现。
 */

DataFormat_t DataPack::dataFormat = HEX;

namespace
{
uint8_t calc_payload_hash(const uint8_t* payload, uint32_t len)
{
    uint8_t hash = 0x5Au;
    for (uint32_t i = 0; i < len; ++i)
    {
        hash = static_cast<uint8_t>((hash * 131u) ^ payload[i]);
    }
    return hash;
}

template <typename T>
osStatus_t append_value(uint8_t* buffer, uint32_t capacity, uint32_t* used, const T* value)
{
    if ((buffer == nullptr) || (used == nullptr) || (value == nullptr))
    {
        return osErrorParameter;
    }

    const uint32_t size = static_cast<uint32_t>(sizeof(T));
    if ((*used + size) > capacity)
    {
        return osErrorResource;
    }

    memcpy(&buffer[*used], value, size);
    *used += size;
    return osOK;
}

template <typename T>
osStatus_t extract_value(const uint8_t* buffer, uint32_t length, uint32_t* offset, T* out)
{
    if ((buffer == nullptr) || (offset == nullptr) || (out == nullptr))
    {
        return osErrorParameter;
    }

    const uint32_t size = static_cast<uint32_t>(sizeof(T));
    if ((*offset + size) > length)
    {
        return osErrorResource;
    }

    memcpy(out, &buffer[*offset], size);
    *offset += size;
    return osOK;
}
}

DataPack::DataPack(uint8_t header, DataFormat_t dataFormat) : 
dataLength(0),
header(header),
dataSourceLength(0),
dataPackSourceLength(0)
{
    this->dataFormat = dataFormat;
    ClearData();
    data[dataLength++] = header;
}

/**
 * @brief 析构函数。
 */
DataPack::~DataPack()
{
    ClearData();
    dataSourceLength = 0;
    dataPackSourceLength = 0;
}

/**
 * @brief 清空内部缓存数据。
 */
void DataPack::ClearData()
{
    memset(data, 0, sizeof(data));
    dataLength = 0;
    data[dataLength++] = header;
}

/**
 * @brief 添加一个数据源条目。
 * @param addr 变量地址。
 * @param type 变量类型。
 * @return osStatus_t 添加结果。
 */
osStatus_t DataPack::LinkDataEntry_(void* addr, VarType type)
{
    if (addr == nullptr)
    {
        return osErrorParameter;
    }

    if (dataSourceLength >= DATA_PACK_MAX_LENGTH)
    {
        return osErrorResource;
    }

    this->dataSource[dataSourceLength++] = {addr, type};
    return osOK;
}

#define DATA_PACK_LINK_IMPL(_ctype, _vartype)            \
osStatus_t DataPack::LinkData(_ctype* dataSource)        \
{                                                         \
    return LinkDataEntry_(dataSource, _vartype);         \
}

DATA_PACK_LINK_IMPL(uint8_t, VarType_uint8)
DATA_PACK_LINK_IMPL(uint16_t, VarType_uint16)
DATA_PACK_LINK_IMPL(uint32_t, VarType_uint32)
DATA_PACK_LINK_IMPL(uint64_t, VarType_uint64)
DATA_PACK_LINK_IMPL(int8_t, VarType_int8)
DATA_PACK_LINK_IMPL(int16_t, VarType_int16)
DATA_PACK_LINK_IMPL(int32_t, VarType_int32)
DATA_PACK_LINK_IMPL(int64_t, VarType_int64)
DATA_PACK_LINK_IMPL(float, VarType_float)
DATA_PACK_LINK_IMPL(double, VarType_double)

#undef DATA_PACK_LINK_IMPL

osStatus_t DataPack::LinkData(const char* str)
{
    return LinkDataEntry_(const_cast<char*>(str), VarType_string);
}

/**
 * @brief 关联一个数据包并复制其数据源条目。
 * @param packSource 源数据包。
 * @return osStatus_t 关联结果。
 */
osStatus_t DataPack::LinkDataPack(DataPack* packSource)
{
    if (packSource == nullptr)
    {
        return osErrorParameter;
    }

    if (dataPackSourceLength >= DATA_PACK_MAX_LENGTH)
    {
        return osErrorResource;
    }

    if ((dataSourceLength + packSource->dataSourceLength) > DATA_PACK_MAX_LENGTH)
    {
        return osErrorResource;
    }

    dataPackSource[dataPackSourceLength++] = packSource;

    for (uint32_t i = 0; i < packSource->dataSourceLength; ++i)
    {
        dataSource[dataSourceLength++] = packSource->dataSource[i];
    }

    return osOK;
}

/**
 * @brief 从绑定变量打包到内部缓存。
 * @return osStatus_t 打包结果。
 */
osStatus_t DataPack::GetData()
{
    ClearData();

    for (uint32_t i = 0; i < dataSourceLength; ++i)
    {
        const VarEntry& entry = dataSource[i];
        osStatus_t status = osError;

        if (dataFormat == STR)
        {
            char token[64];
            token[0] = '\0';

            switch (entry.type)
            {
                case VarType_uint8:
                    (void)snprintf(token, sizeof(token), "%u", static_cast<unsigned>(*static_cast<uint8_t*>(entry.addr)));
                    break;
                case VarType_uint16:
                    (void)snprintf(token, sizeof(token), "%u", static_cast<unsigned>(*static_cast<uint16_t*>(entry.addr)));
                    break;
                case VarType_uint32:
                    (void)snprintf(token, sizeof(token), "%lu", static_cast<unsigned long>(*static_cast<uint32_t*>(entry.addr)));
                    break;
                case VarType_uint64:
                    (void)snprintf(token, sizeof(token), "%llu", static_cast<unsigned long long>(*static_cast<uint64_t*>(entry.addr)));
                    break;
                case VarType_int8:
                    (void)snprintf(token, sizeof(token), "%d", static_cast<int>(*static_cast<int8_t*>(entry.addr)));
                    break;
                case VarType_int16:
                    (void)snprintf(token, sizeof(token), "%d", static_cast<int>(*static_cast<int16_t*>(entry.addr)));
                    break;
                case VarType_int32:
                    (void)snprintf(token, sizeof(token), "%ld", static_cast<long>(*static_cast<int32_t*>(entry.addr)));
                    break;
                case VarType_int64:
                    (void)snprintf(token, sizeof(token), "%lld", static_cast<long long>(*static_cast<int64_t*>(entry.addr)));
                    break;
                case VarType_float:
                    (void)snprintf(token, sizeof(token), "%.6g", static_cast<double>(*static_cast<float*>(entry.addr)));
                    break;
                case VarType_double:
                    (void)snprintf(token, sizeof(token), "%.12g", *static_cast<double*>(entry.addr));
                    break;
                case VarType_string:
                {
                    if (entry.addr == nullptr)
                    {
                        return osErrorParameter;
                    }
                    const char* str = static_cast<const char*>(entry.addr);
                    uint32_t len = static_cast<uint32_t>(strlen(str));
                    if ((dataLength + len + ((i + 1u < dataSourceLength) ? 1u : 0u)) > DATA_PACK_MAX_LENGTH)
                    {
                        return osErrorResource;
                    }
                    memcpy(&data[dataLength], str, len);
                    dataLength += len;
                    if (i + 1u < dataSourceLength)
                    {
                        data[dataLength++] = ',';
                    }
                    status = osOK;
                    break;
                }
                default:
                    return osError;
            }

            if (entry.type != VarType_string)
            {
                uint32_t len = static_cast<uint32_t>(strlen(token));
                if ((dataLength + len + ((i + 1u < dataSourceLength) ? 1u : 0u)) > DATA_PACK_MAX_LENGTH)
                {
                    return osErrorResource;
                }
                memcpy(&data[dataLength], token, len);
                dataLength += len;
                if (i + 1u < dataSourceLength)
                {
                    data[dataLength++] = ',';
                }
                status = osOK;
            }

            if (status != osOK)
            {
                return status;
            }

            continue;
        }

        switch (entry.type)
        {
            case VarType_uint8:
                status = append_value<uint8_t>(data, DATA_PACK_MAX_LENGTH, &dataLength, static_cast<uint8_t*>(entry.addr));
                break;
            case VarType_uint16:
                status = append_value<uint16_t>(data, DATA_PACK_MAX_LENGTH, &dataLength, static_cast<uint16_t*>(entry.addr));
                break;
            case VarType_uint32:
                status = append_value<uint32_t>(data, DATA_PACK_MAX_LENGTH, &dataLength, static_cast<uint32_t*>(entry.addr));
                break;
            case VarType_uint64:
                status = append_value<uint64_t>(data, DATA_PACK_MAX_LENGTH, &dataLength, static_cast<uint64_t*>(entry.addr));
                break;
            case VarType_int8:
                status = append_value<int8_t>(data, DATA_PACK_MAX_LENGTH, &dataLength, static_cast<int8_t*>(entry.addr));
                break;
            case VarType_int16:
                status = append_value<int16_t>(data, DATA_PACK_MAX_LENGTH, &dataLength, static_cast<int16_t*>(entry.addr));
                break;
            case VarType_int32:
                status = append_value<int32_t>(data, DATA_PACK_MAX_LENGTH, &dataLength, static_cast<int32_t*>(entry.addr));
                break;
            case VarType_int64:
                status = append_value<int64_t>(data, DATA_PACK_MAX_LENGTH, &dataLength, static_cast<int64_t*>(entry.addr));
                break;
            case VarType_float:
                status = append_value<float>(data, DATA_PACK_MAX_LENGTH, &dataLength, static_cast<float*>(entry.addr));
                break;
            case VarType_double:
                status = append_value<double>(data, DATA_PACK_MAX_LENGTH, &dataLength, static_cast<double*>(entry.addr));
                break;
            case VarType_string:
            {
                if (entry.addr == nullptr)
                {
                    status = osErrorParameter;
                    break;
                }

                const char* str = static_cast<const char*>(entry.addr);
                uint32_t len = static_cast<uint32_t>(strlen(str)) + 1u;
                if ((dataLength + len) > DATA_PACK_MAX_LENGTH)
                {
                    status = osErrorResource;
                    break;
                }
                memcpy(&data[dataLength], str, len);
                dataLength += len;
                status = osOK;
                break;
            }
            default:
                status = osError;
                break;
        }

        if (status != osOK)
        {
            return status;
        }
    }

    return osOK;
}

osStatus_t DataPack::DistributeData()
{
    if (dataFormat == STR)
    {
        uint32_t offset = 0;

        for (uint32_t i = 0; i < dataSourceLength; ++i)
        {
            const VarEntry& entry = dataSource[i];
            uint32_t start = offset;

            while ((offset < dataLength) && (data[offset] != ','))
            {
                offset++;
            }

            uint32_t token_len = offset - start;
            char token[DATA_PACK_MAX_LENGTH + 1];
            if (token_len > DATA_PACK_MAX_LENGTH)
            {
                return osErrorResource;
            }

            memcpy(token, &data[start], token_len);
            token[token_len] = '\0';

            char* endptr = nullptr;
            switch (entry.type)
            {
                case VarType_uint8:
                    *static_cast<uint8_t*>(entry.addr) = static_cast<uint8_t>(strtoul(token, &endptr, 10));
                    break;
                case VarType_uint16:
                    *static_cast<uint16_t*>(entry.addr) = static_cast<uint16_t>(strtoul(token, &endptr, 10));
                    break;
                case VarType_uint32:
                    *static_cast<uint32_t*>(entry.addr) = static_cast<uint32_t>(strtoul(token, &endptr, 10));
                    break;
                case VarType_uint64:
                    *static_cast<uint64_t*>(entry.addr) = static_cast<uint64_t>(strtoull(token, &endptr, 10));
                    break;
                case VarType_int8:
                    *static_cast<int8_t*>(entry.addr) = static_cast<int8_t>(strtol(token, &endptr, 10));
                    break;
                case VarType_int16:
                    *static_cast<int16_t*>(entry.addr) = static_cast<int16_t>(strtol(token, &endptr, 10));
                    break;
                case VarType_int32:
                    *static_cast<int32_t*>(entry.addr) = static_cast<int32_t>(strtol(token, &endptr, 10));
                    break;
                case VarType_int64:
                    *static_cast<int64_t*>(entry.addr) = static_cast<int64_t>(strtoll(token, &endptr, 10));
                    break;
                case VarType_float:
                    *static_cast<float*>(entry.addr) = strtof(token, &endptr);
                    break;
                case VarType_double:
                    *static_cast<double*>(entry.addr) = strtod(token, &endptr);
                    break;
                case VarType_string:
                {
                    // STR 模式字符串校验：接收内容必须与本地绑定字符串一致。
                    const char* expected = static_cast<const char*>(entry.addr);
                    if ((expected == nullptr) || (strcmp(expected, token) != 0))
                    {
                        return osError;
                    }
                    endptr = token + token_len;
                    break;
                }
                default:
                    return osError;
            }

            if ((entry.type != VarType_string) && ((endptr == nullptr) || (*endptr != '\0')))
            {
                return osError;
            }

            if (offset < dataLength)
            {
                offset++; // skip comma
            }
        }

        if (offset < dataLength)
        {
            return osError;
        }

        return osOK;
    }

    uint32_t offset = 0;

    for (uint32_t i = 0; i < dataSourceLength; ++i)
    {
        const VarEntry& entry = dataSource[i];
        osStatus_t status = osError;

        switch (entry.type)
        {
            case VarType_uint8:
                status = extract_value<uint8_t>(data, dataLength, &offset, static_cast<uint8_t*>(entry.addr));
                break;
            case VarType_uint16:
                status = extract_value<uint16_t>(data, dataLength, &offset, static_cast<uint16_t*>(entry.addr));
                break;
            case VarType_uint32:
                status = extract_value<uint32_t>(data, dataLength, &offset, static_cast<uint32_t*>(entry.addr));
                break;
            case VarType_uint64:
                status = extract_value<uint64_t>(data, dataLength, &offset, static_cast<uint64_t*>(entry.addr));
                break;
            case VarType_int8:
                status = extract_value<int8_t>(data, dataLength, &offset, static_cast<int8_t*>(entry.addr));
                break;
            case VarType_int16:
                status = extract_value<int16_t>(data, dataLength, &offset, static_cast<int16_t*>(entry.addr));
                break;
            case VarType_int32:
                status = extract_value<int32_t>(data, dataLength, &offset, static_cast<int32_t*>(entry.addr));
                break;
            case VarType_int64:
                status = extract_value<int64_t>(data, dataLength, &offset, static_cast<int64_t*>(entry.addr));
                break;
            case VarType_float:
                status = extract_value<float>(data, dataLength, &offset, static_cast<float*>(entry.addr));
                break;
            case VarType_double:
                status = extract_value<double>(data, dataLength, &offset, static_cast<double*>(entry.addr));
                break;
            case VarType_string:
            {
                if (offset >= dataLength)
                {
                    return osErrorResource;
                }

                while ((offset < dataLength) && (data[offset] != '\0'))
                {
                    offset++;
                }
                if (offset >= dataLength)
                {
                    return osErrorResource;
                }
                offset++; // 跳过 '\0'
                status = osOK;
                break;
            }
            default:
                status = osError;
                break;
        }

        if (status != osOK)
        {
            return status;
        }
    }

    return osOK;
}

/**
 * @brief 打包后发送到 USB CDC。
 * @return osStatus_t 发送结果。
 */
osStatus_t DataPack::SendData()
{
    osStatus_t status = GetData();
    if (status != osOK)
    {
        return status;
    }

    if (dataLength == 0u)
    {
        return osErrorResource;
    }

    const uint8_t tail_hash = calc_payload_hash(data, dataLength);

    data[dataLength++] = tail_hash;

    if (!bsp_usb.cdc_write(data, dataLength))
    {
        return osError;
    }

    return osOK;
}

/**
 * @brief 从 USB CDC 接收后分发到绑定变量。
 * @return osStatus_t 接收与分发结果。
 */
osStatus_t DataPack::ReceiveData()
{
    uint32_t available = bsp_usb.cdc_available();
    if (available == 0u)
    {
        return osErrorResource;
    }

    if (available > (DATA_PACK_MAX_LENGTH ))
    {
        available = DATA_PACK_MAX_LENGTH ;
    }

    uint8_t frame[DATA_PACK_MAX_LENGTH];
    uint32_t read_len = bsp_usb.cdc_read(frame, available);
    if (read_len == 0u)
    {
        return osErrorResource;
    }

    if (read_len < 3u)
    {
        return osError;
    }

    uint32_t start = 0;
    while ((start < read_len) && (frame[start] != header))
    {
        start++;
    }
    if (start >= read_len)
    {
        return osError;
    }

    // 协议固定为：[header][payload][tail_hash]
    const uint32_t end = read_len - 1u;
    if (end <= start)
    {
        return osError;
    }

    uint32_t payload_len = end - start - 0u;
    payload_len -= 1u;
    if (payload_len > DATA_PACK_MAX_LENGTH)
    {
        return osErrorResource;
    }

    memcpy(data, &frame[start + 1u], payload_len);

    const uint8_t recv_tail_hash = frame[end];
    const uint8_t calc_tail_hash = calc_payload_hash(data, payload_len);
    if (recv_tail_hash != calc_tail_hash)
    {
        return osError;
    }

    dataLength = payload_len;

    // 通信校验：帧头与哈希合法后，再通过 STR 模式字符串一致性校验。
    return DistributeData();
}


