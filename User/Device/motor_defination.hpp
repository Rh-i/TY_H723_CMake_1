/**
 * @file motor_defination.hpp
 * @author ChoseB
 * @brief 电机反馈、运动学数据和观测器输出的通用数据定义
 * @version 0.1
 * @date 2026-09-01
 *
 * @note 本文件只保存可复制的数据结构，不包含硬件所有权、在线检查或 CAN 资源。
 * @todo 文件名中的 defination 为历史拼写，接口稳定后可统一重命名为 motor_definition.hpp。
 */
#include <stdint.h>

#ifndef MOTOR_DEFINATION_HPP
#define MOTOR_DEFINATION_HPP

/**
 * @brief 单个电机的反馈数据、运动学换算结果和固定参数
 *
 * @note 各字段由具体电机设备对象初始化和更新；默认构造的裸结构不保证字段有效。
 */
struct MotorData
{
    /**
     * @brief 从电调反馈帧直接解析得到的原始数据
     */
    struct RawData
    {
        int16_t ecd;           ///< 编码器原始位置计数
        int16_t rpm;           ///< 电调反馈的有符号转速，单位：rpm
        int16_t TorqueCurrent; ///< 电调反馈的有符号原始力矩电流
        uint8_t Temperature;   ///< 电调反馈温度，单位：摄氏度
    } rawData;

    /**
     * @brief 基于原始反馈和减速比换算后的 SI 制运动量
     */
    struct Radian
    {
        float angle_SingleRound; ///< 单圈机械角度，单位：rad
        float angle_MultiRound;  ///< 累计多圈机械角度，单位：rad
        float velocity;          ///< 输出轴角速度，单位：rad/s
        float acceleration;      ///< 输出轴角加速度，单位：rad/s^2
    } radianData;

    /**
     * @brief 解析反馈和限制输出所需的固定参数
     */
    struct Param
    {
        uint16_t ecdOffset;    ///< 机械零位对应的编码器原始值
        uint16_t ecdFullRange; ///< 编码器一圈的计数总数，例如 8192
        uint16_t currentLimit; ///< 原始控制指令允许的绝对值上限
        float ratio;           ///< 转子转速与输出轴转速之比
    } param;
};

/**
 * @brief 可选的 Luenberger 电机状态观测结果
 *
 * @note 该结构属于可选能力，并非每个电机实例都必须启用或使用观测结果。
 */
struct LuenbergerMotorData
{
    float angle;        ///< 观测得到的累计机械角度，单位：rad
    float velocity;     ///< 观测得到的角速度，单位：rad/s
    float acceleration; ///< 观测得到的角加速度，单位：rad/s^2
};

#endif // MOTOR_DEFINATION_HPP
