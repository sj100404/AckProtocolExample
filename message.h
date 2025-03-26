#pragma once
#include <cstdint>
#include <string>
#include <cstring>

//消息类型定义
enum class MsgType:uint32_t
{
    DATA = 0,
    ACK = 1
};

// 消息头（固定大小，用于网络传输）
struct MessageHeader
{
    MsgType  type;       // 消息类型
    uint32_t seq_id;     // 消息序列号
    uint32_t data_len;   // 数据长度
};

// 完整消息结构（头 + 数据）
struct Message 
{
    MessageHeader header;
    char data[1024];     // 可变长度数据（根据data_len确定）
};

// 工具函数：序列化消息到缓冲区
void serialize_message(const Message& msg, char* buffer) 
{
    memcpy(buffer, &msg.header, sizeof(MessageHeader));
    memcpy(buffer + sizeof(MessageHeader), msg.data, msg.header.data_len);
}

// 工具函数：从缓冲区反序列化消息
Message deserialize_message(const char* buffer)
{
    Message msg;
    memcpy(&msg.header, buffer, sizeof(MessageHeader));
    memcpy(msg.data, buffer + sizeof(MessageHeader), msg.header.data_len);
    return msg;
}