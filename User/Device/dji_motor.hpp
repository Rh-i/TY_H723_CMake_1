/**
 * @file dji_motor.hpp
 * @author ChoseB
 * @brief DJI 电机反馈解析、在线状态和控制帧聚合框架
 * @version 0.1
 * @date 2026-09-01
 *
 * @details 一个 DjiMotor 实例表示一台电机。用户只需提供所用 CAN 和电机 ID；
 *          类内部根据电机型号计算控制帧 ID 与帧内槽位，并从固定池中复用或启用
 *          对应的 DjiMotorBus。Bus 以 (BspCan*, std_id) 为唯一键，因此采用相同
 *          控制帧协议的不同型号电机可以共享一帧发送。
 *
 * @note 构造阶段只登记软件对象关系，不初始化或访问 CAN 硬件。
 * @warning FillData() 与 DjiMotorSendAll() 若运行在不同任务或中断上下文，实现时
 *          必须保护共享输出槽位，避免 C++ 数据竞争。
 * @todo 当前文件仅定义对象框架，协议映射、反馈解析、固定池管理和发送逻辑待实现。
 */
#ifndef DJI_MOTOR_HPP
#define DJI_MOTOR_HPP

#include "motor_defination.hpp"
#include "bsp_can.hpp"
#include "online_check.hpp"

/** @todo 将以下临时协议常量迁移到按 MotorType 特化的 traits，随后删除宏。 */
#define K_ECD_TO_ANGLE 0.043945f       ///< 编码器计数到机械角度的临时换算系数，单位：度/count
#define ECD_RANGE_FOR_3508 8191        ///< M3508 编码器原始值上限（包含 0，共 8192 个计数）
#define CURRENT_LIMIT_FOR_3508 16000   ///< M3508/C620 软件限制使用的原始电流指令绝对值
#define ECD_RANGE_FOR_6020 8191        ///< GM6020 编码器原始值上限（包含 0，共 8192 个计数）
#define CURRENT_LIMIT_FOR_6020 29000   ///< GM6020 软件限制使用的原始控制指令绝对值
#define ECD_RANGE_FOR_2006 8191        ///< M2006 编码器原始值上限（包含 0，共 8192 个计数）
#define CURRENT_LIMIT_FOR_2006 10000   ///< M2006/C610 软件限制使用的原始电流指令绝对值

/**
 * @brief 框架支持的 DJI 电机型号
 *
 * @note 电机型号用于选择反馈格式、合法 ID 范围、控制帧 ID、槽位和默认减速比。
 */
enum MotorType
{
    Motor3508 = 0x00U, ///< M3508 电机配合 C620 电调
    Motor6020 = 0x01U, ///< GM6020 电机
    Motor2006 = 0x02U, ///< M2006 电机配合 C610 电调
};

template <MotorType type>
class DjiMotor;

/**
 * @brief 发送固定池中所有已启用的 DJI 电机控制帧
 *
 * @return Status::OK 所有控制帧均成功交给 CAN BSP；其他状态表示至少一帧发送失败。
 *
 * @note 应在固定频率（通常为 1 kHz）的高优先级任务中调用。
 * @note 未占用的帧槽位必须保持为零，防止向不存在的电机输出指令。
 */
Status DjiMotorSendAll(void);

/**
 * @brief DJI 电机控制帧的内部聚合对象
 *
 * @note 用户无需创建 Bus。DjiMotor 构造时按 (BspCan*, std_id) 从固定池中
 *       自动获取或启用 Bus，并注册对应槽位。构造阶段只登记关系，不操作 CAN 硬件。
 */
class DjiMotorBus
{
private:
    static const uint8_t MAX_BUS_COUNT = 15; ///< 固定池容量：3 路 CAN × 每路最多 5 个控制帧

    /**
     * @brief 一个控制帧中的 16 位电机指令槽位
     */
    struct Slot
    {
        int16_t output; ///< 待发送的有符号原始控制量
        bool occupied; ///< true 表示已有一个 DjiMotor 占用该槽位
    };

    CanTxMsg _txMsg;       ///< 本组独占的 8 字节标准帧及其标识符
    BspCan* _canItem;      ///< 关联的物理 CAN；池项未启用时为 nullptr
    Slot _slots[4];        ///< 按协议顺序排列的四个 16 位输出槽位
    uint8_t _attachedCount;///< 当前已注册的电机数量，范围 0~4
    bool _used;            ///< 固定池项是否已绑定有效的 (CAN, std_id)
    Status _statu;         ///< 最近一次池管理或发送操作的结果

    static DjiMotorBus _pool[MAX_BUS_COUNT]; ///< 所有 Bus 的静态存储，不使用运行时堆

    /**
     * @brief 构造一个未启用的池项
     *
     * @note 构造函数仅供静态固定池使用，不允许业务代码直接创建 Bus。
     */
    DjiMotorBus();

    /**
     * @brief 销毁固定池项
     *
     * @note 正常嵌入式运行期间固定池不会销毁；声明析构函数用于明确对象生命周期。
     */
    ~DjiMotorBus();

    /**
     * @brief 获取控制帧 Bus 并为一台电机占用槽位
     *
     * @param canItem 电机连接的物理 CAN BSP 对象
     * @param std_id  DJI 协议规定的 11 位控制帧标准标识符
     * @param slot    帧内槽位，合法范围为 0~3
     * @param bus     成功时写入获取到的 Bus 指针；失败时必须写入 nullptr
     * @return Status::OK 获取并占用成功；Status::BAD_ARG 参数非法；
     *         Status::BUSY 槽位已被其他电机占用；Status::FULL 固定池已耗尽。
     *
     * @details 先查找相同的 (canItem, std_id)，不存在时再启用空闲池项。
     *          相同控制帧中的不同电机型号可共享同一个 Bus，但同一槽位只能注册一次。
     */
    static Status acquire(BspCan& canItem, uint32_t std_id, uint8_t slot, DjiMotorBus** bus);

    /**
     * @brief 释放一台电机占用的控制帧槽位
     *
     * @param bus  DjiMotor 构造时获取到的 Bus
     * @param slot 待释放的槽位，合法范围为 0~3
     * @return Status::OK 释放成功；Status::BAD_ARG 指针或槽位非法。
     *
     * @note 释放前必须将该槽位输出清零；最后一个槽位释放后可回收整个池项。
     */
    static Status release(DjiMotorBus* bus, uint8_t slot);

    /**
     * @brief 更新一个控制帧槽位的待发送输出
     *
     * @param slot   帧内槽位，合法范围为 0~3
     * @param output 已由具体电机型号完成限幅的原始控制量
     * @return Status::OK 更新成功；Status::BAD_ARG 槽位非法；
     *         Status::NOT_INIT Bus 未启用或槽位未注册。
     */
    Status setOutput(uint8_t slot, int16_t output);

    /**
     * @brief 组合四个槽位并发送本 Bus 对应的 8 字节 CAN 帧
     *
     * @return BspCan::send() 返回的统一状态码。
     *
     * @note 每个 16 位输出按 DJI 协议使用高字节在前的顺序写入帧。
     */
    Status send(void);

    /**
     * @brief 遍历固定池并发送所有已启用 Bus
     * @return Status::OK 全部成功；其他状态表示至少一个 Bus 发送失败。
     */
    static Status sendAll(void);

    // 禁止复制/移动，保护固定池内对象的身份。
    DjiMotorBus(const DjiMotorBus&) = delete;
    DjiMotorBus& operator=(const DjiMotorBus&) = delete;
    DjiMotorBus(DjiMotorBus&&) = delete;
    DjiMotorBus& operator=(DjiMotorBus&&) = delete;

    template <MotorType type>
    friend class DjiMotor;
    friend Status DjiMotorSendAll(void);
};

/**
 * @brief 单个 DJI 电机设备对象
 *
 * @tparam type 电机型号，决定合法 ID、反馈解析、控制帧映射、限幅和默认减速比
 *
 * @details 对象负责保存单电机反馈和目标输出。多个电机的目标输出由内部
 *          DjiMotorBus 聚合后通过 DjiMotorSendAll() 周期发送。
 * @note 对象不可复制或移动，以保证在线检查节点和控制帧槽位的身份稳定。
 */
template <MotorType type>
class DjiMotor
{
private:
    MotorData _data;                  ///< 原始反馈、换算后的运动量和固定参数
    Online _online;                   ///< 由有效反馈帧刷新的在线检查对象
    LuenbergerMotorData _lvboData;    ///< 可选的 Luenberger 观测结果

    DjiMotorBus* _bus; ///< 内部控制帧 Bus；构造注册失败时为 nullptr
    uint8_t _motorID;  ///< 电调配置的 DJI 协议 ID
    uint8_t _busSlot;  ///< 当前电机在控制帧中的槽位，范围 0~3
    Status _statu;     ///< 构造注册或最近一次公开操作的状态

public:
    /**
     * @brief 获取只读电机数据
     * @return 内部 MotorData 的常量引用。
     * @warning 返回的是实时引用；若反馈更新与读取不在同一任务，实现时需提供同步或快照机制。
     */
    const MotorData& data(void) const;

    /**
     * @brief 获取本电机的在线检查对象
     * @return Online 的常量引用，可用于调用 isOnline() 查询状态。
     */
    const Online& online(void) const;

    /**
     * @brief 获取可选的 Luenberger 观测数据
     * @return 内部 LuenbergerMotorData 的常量引用。
     * @note 观测器未启用时，该数据不应作为有效控制依据。
     */
    const LuenbergerMotorData& lvboData(void) const;

    /**
     * @brief 查询对象最近一次初始化或操作状态
     * @return Status::OK 表示内部 Bus 注册成功且最近操作成功；其他值表示具体错误。
     */
    Status statu(void) const;

    /**
     * @brief 构造单个 DJI 电机对象并注册控制帧槽位
     *
     * @param canItem   电机连接的物理 CAN BSP 对象
     * @param motorID   电调设置的 DJI 协议 ID
     * @param ratio     电机转子到输出轴的减速比；传 0 时按型号选择默认值：
     *                  M3508 为 3591/187，GM6020 为 1，M2006 为 36
     * @param ecdOffset 机械零位对应的编码器原始值
     *
     * @note 构造函数不能返回错误；ID 非法、槽位冲突或池耗尽可通过 statu() 查询。
     * @note 构造阶段不会调用 BspCan::init() 或发送 CAN 帧。
     */
    DjiMotor(BspCan& canItem, uint8_t motorID, float ratio = 0, uint16_t ecdOffset = 0);

    /**
     * @brief 注销控制帧槽位并销毁电机对象
     * @note 注销时应先将输出清零，避免回收后保留旧控制量。
     */
    ~DjiMotor();

    /**
     * @brief 解析一帧属于本电机的 DJI CAN 反馈
     *
     * @param rx 待解析的 CAN 标准帧
     * @return Status::OK 反馈有效且数据已更新；Status::BAD_ARG 标识符或帧格式不匹配；
     *         Status::NOT_INIT 电机未成功完成内部注册。
     *
     * @note 只有完整且有效的反馈才能刷新 _online。
     */
    Status DataUnpack(CanRxMsg rx);

    /**
     * @brief 设置下一周期发送的原始控制量
     *
     * @param output 有符号原始控制指令；实现时应按具体型号的限制进行饱和限幅
     * @return Status::OK 写入成功；Status::NOT_INIT 未取得内部 Bus；
     *         Status::BAD_ARG 或其他状态表示参数/Bus 操作失败。
     *
     * @note 本函数只更新内部 Bus 槽位，实际发送由 DjiMotorSendAll() 统一完成。
     */
    Status FillData(int16_t output);

    // 禁止复制/移动
    DjiMotor(const DjiMotor&) = delete;
    DjiMotor& operator=(const DjiMotor&) = delete;
    DjiMotor(DjiMotor&&) = delete;
    DjiMotor& operator=(DjiMotor&&) = delete;

private:
    /** @brief 检查 motorID 是否位于当前电机型号支持的范围内。 */
    static bool MotorIDValid(uint8_t motorID);

    /** @brief 根据电机型号和 motorID 计算控制帧标准标识符。 */
    static uint32_t ControlStdID(uint8_t motorID);

    /** @brief 根据电机型号和 motorID 计算 8 字节控制帧中的槽位。 */
    static uint8_t ControlSlot(uint8_t motorID);

    /**
     * @brief 根据最新反馈更新 Luenberger 观测结果
     * @return Status::OK 更新成功；Status::NOT_SUPPORTED 当前型号或配置未启用观测器。
     * @todo 当前阶段不需要观测器，保留接口供后续控制算法接入。
     */
    Status LvboDataUpdate(void);
};



#endif // DJI_MOTOR_HPP
