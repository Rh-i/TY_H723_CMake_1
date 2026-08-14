#include "bsp_usb.hpp"
#include "tusb.h" // IWYU pragma: keep
#include <string.h>


#define USB_VID 0xCafe
#define USB_PID 0x4001
#define USB_BCD 0x0200

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82

#define CDC_RX_POLL_CHUNK 64u

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

enum
{
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
  ITF_NUM_TOTAL
};

/**
 * @brief 获取 USB BSP 单例对象。
 * @return BspUsb& 单例引用。
 */
BspUsb& BspUsb::instance()
{
  static BspUsb s_instance;
  return s_instance;
}

/**
 * @brief 初始化 TinyUSB 设备栈。
 */
void BspUsb::init()
{
  tud_init(0);
}

/**
 * @brief USB 周期任务。
 *
 * @details
 * 调用 TinyUSB 主任务并轮询处理 CDC 接收数据。
 */
void BspUsb::task()
{
  tud_task();
  process_rx();
}

/**
 * @brief 查询 USB CDC 是否就绪可通信。
 * @return true 已准备好；false 未准备好。
 */
bool BspUsb::is_ready() const
{
  if (!tud_mounted())
  {
    return false;
  }

  if (_require_dtr)
  {
    return tud_cdc_connected();
  }

  return true;
}

/**
 * @brief 设置发送前是否要求 DTR。
 * @param enable true 要求 DTR；false 不要求 DTR。
 */
void BspUsb::set_require_dtr(bool enable)
{
  _require_dtr = enable;
}

/**
 * @brief 查询 USB 是否已被主机挂载。
 * @return true 已挂载；false 未挂载。
 */
bool BspUsb::mounted() const
{
  return tud_mounted();
}

/**
 * @brief 通过 CDC 写数据。
 * @param data 数据指针。
 * @param len 数据长度。
 * @return true 全部写入成功；false 未连接或未全部写入。
 */
bool BspUsb::cdc_write(const uint8_t* data, uint32_t len)
{
  if ((data == NULL) || (len == 0u))
  {
    return false;
  }

  if (!is_ready())
  {
    return false;
  }

  uint32_t written = tud_cdc_write(data, len);
  tud_cdc_write_flush();
  return written == len;
}

/**
 * @brief 从 CDC 读取数据。
 * @param data 输出缓冲区。
 * @param len 最多读取长度。
 * @return uint32_t 实际读取字节数。
 */
uint32_t BspUsb::cdc_read(uint8_t* data, uint32_t len)
{
  if ((data == NULL) || (len == 0u))
  {
    return 0u;
  }

  if (!mounted())
  {
    return 0u;
  }

  return tud_cdc_read(data, len);
}

/**
 * @brief 获取 CDC 可读字节数。
 * @return uint32_t 可读字节数。
 */
uint32_t BspUsb::cdc_available() const
{
  if (!mounted())
  {
    return 0u;
  }

  return tud_cdc_available();
}

/**
 * @brief 设置 CDC 接收回调。
 * @param cb 回调函数，传入 nullptr 则关闭回调。
 * @param user_ctx 用户上下文。
 */
void BspUsb::set_rx_callback(RxCallback cb, void* user_ctx)
{
  _rx_callback = cb;
  _rx_user_ctx = user_ctx;
}

/**
 * @brief 轮询 CDC 接收并分发回调。
 */
void BspUsb::process_rx()
{
  if ((_rx_callback == nullptr) || !mounted())
  {
    return;
  }

  uint8_t rx_buf[CDC_RX_POLL_CHUNK];

  while (tud_cdc_available() > 0u)
  {
    uint32_t read_len = tud_cdc_read(rx_buf, sizeof(rx_buf));
    if (read_len == 0u)
    {
      break;
    }

    _rx_callback(rx_buf, read_len, _rx_user_ctx);
  }
}

static const tusb_desc_device_t desc_device =
  {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01};

/**
 * @brief TinyUSB 设备描述符回调。
 * @return uint8_t const* 设备描述符指针。
 */
extern "C" uint8_t const* tud_descriptor_device_cb(void)
{
  return (uint8_t const*)&desc_device;
}

static const uint8_t desc_configuration[] =
  {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, CFG_TUD_CDC_EP_BUFSIZE)};

/**
 * @brief TinyUSB 配置描述符回调。
 * @param index 配置索引（当前仅支持 0）。
 * @return uint8_t const* 配置描述符指针。
 */
extern "C" uint8_t const* tud_descriptor_configuration_cb(uint8_t index)
{
  (void)index;
  return desc_configuration;
}

enum
{
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_CDC
};

static const char* string_desc_arr[] =
  {
    (const char[]) {0x09, 0x04},
    "RoboMaster",
    "TY H723 TinyUSB CDC",
    "0001",
    "TinyUSB CDC"};

static uint16_t _desc_str[32];

/**
 * @brief TinyUSB 字符串描述符回调。
 * @param index 字符串索引。
 * @param langid 语言 ID。
 * @return uint16_t const* UTF-16LE 字符串描述符指针。
 */
extern "C" uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void)langid;
  uint8_t chr_count;

  if (index == 0)
  {
    _desc_str[1] = 0x0409;
    chr_count    = 1;
  }
  else
  {
    if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))
    {
      return NULL;
    }

    const char* str = string_desc_arr[index];
    chr_count       = (uint8_t)strlen(str);
    if (chr_count > 31)
    {
      chr_count = 31;
    }

    for (uint8_t i = 0; i < chr_count; i++)
    {
      _desc_str[1 + i] = str[i];
    }
  }

  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
  return _desc_str;
}