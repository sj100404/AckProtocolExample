#include "message.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <map>
#include <chrono>
#include <arpa/inet.h>

/*
 +----------------+          +----------------+
|    客户端       |          |     服务端      |
+----------------+          +----------------+
| 1. 生成seq_id   |          |                |
| 2. 发送DATA消息 | -------> | 3. 接收DATA消息 |
|                |          | 4. 幂等性检查   |
|                |          | 5. 执行业务逻辑 |
| 6. 接收ACK     | <------- | 6. 发送ACK      |
| 7. 停止重传     |          |                |
+----------------+          +----------------+

机制	实现方式
消息序列号	每条消息分配唯一 seq_id，客户端和服务端通过该ID跟踪状态
超时重传	客户端维护 pending_msgs 队列，3秒未收到ACK则自动重发
ACK确认	服务端处理成功后返回ACK，客户端收到后移除重传队列
幂等性	服务端通过 processed_seqs 记录已处理序列号，避免重复处理
线程分离	客户端单独线程监听ACK，避免阻塞主线程
 */



class Client
{
public:
    Client(const char* ip, int port) 
    {
        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &addr.sin_addr);

        connect(sock_, (sockaddr*)&addr, sizeof(addr));
        std::cout << "Connected to server" << std::endl;

        // 启动ACK监听线程
        std::thread([this]() { listen_for_acks(); }).detach();
    }

    void send_data(const std::string& data)
    {
        Message msg{};
        msg.header.type = MsgType::DATA;
        msg.header.seq_id = nextseq_++;
        msg.header.data_len = data.size();
        memcpy(msg.data, data.c_str(), data.size());

        char buffer[sizeof(Message)];
        serialize_message(msg, buffer);

         // 发送并记录到待确认队列
        send(sock_, buffer, sizeof(buffer), 0);
        pendingmsgs_[msg.header.seq_id] = std::chrono::steady_clock::now();

        // 启动重传检查线程
        std::thread([this, msg]() {
            auto seq = msg.header.seq_id;
            while (running_) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (pendingmsgs_.count(seq)) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = now - pendingmsgs_[seq];
                    if (elapsed > std::chrono::seconds(3)) {
                        std::cout << "Resending message: " << seq << std::endl;
                        char buf[sizeof(Message)];
                        serialize_message(msg, buf);
                        send(sock_, buf, sizeof(buf), 0);
                        pendingmsgs_[seq] = now; // 更新时间戳
                    }
                }
            }
        }).detach();

    }


private:
    int sock_;
    std::map<uint32_t, std::chrono::steady_clock::time_point> pendingmsgs_;
    uint32_t nextseq_ = 1;
    bool running_ = true;
    void listen_for_acks()
    {
        char buffer[sizeof(Message)];
        while(running_)
        {
            ssize_t bytes = recv(sock_, buffer, sizeof(buffer), 0);
            if(bytes<=0) break;
            Message msg = deserialize_message(buffer);
            if(msg.header.type == MsgType::ACK)
            {
                std::cout << "Received ACK for seq: " << msg.header.seq_id << std::endl;
                pendingmsgs_.erase(msg.header.seq_id);
            }
        }
    }
};

int main() 
{
    Client client("127.0.0.1", 12332);
    
    // 发送测试消息
    client.send_data("Hello");
    client.send_data("World");
    client.send_data("error"); // 模拟失败消息

    // 保持运行以便观察重传
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;
}