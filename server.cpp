#include "message.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <unordered_set>

class Server
{
public:
    Server(int port)
    {
        // 创建Socket
        serverfd_ = socket(AF_INET,SOCK_STREAM,0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        //绑定并且监听
        bind(serverfd_,(sockaddr*)&addr,sizeof(addr));
        listen(serverfd_,5);
        std::cout << "Server listening on port " << port << std::endl;
    }

    void run()
    {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(serverfd_, (sockaddr*)&client_addr, &addr_len);
        std::cout << "Client connected" << std::endl;

        char buffer[sizeof(Message)];

        while (true) 
        {
            // 接收消息
            ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
            if (bytes <= 0) break;

            Message msg = deserialize_message(buffer);
            if (msg.header.type == MsgType::DATA) 
            {
                handleData_(client_fd, msg);
            }
        }
        close(client_fd);
    }

private:
    int serverfd_;
    std::unordered_set<uint32_t> processedseqs_;// 已处理的序列号（幂等性）
    void handleData_(int client_fd, const Message& msg)
    {
            //防止重复处理同一消息  例如客户端重传导致的 如果我处理过了直接发送ack
        if(processedseqs_.count(msg.header.seq_id))
        {
            sendAck_(client_fd, msg.header.seq_id);
            return;
        }

        //业务处理
        std::string data(msg.data, msg.header.data_len);
        std::cout << "Processing data: " << data << std::endl;

        bool success = (data != "error");

        if(success)
        {
            processedseqs_.insert(msg.header.seq_id);
            sendAck_(client_fd, msg.header.seq_id);
        }

    }
    void sendAck_(int client_fd, uint32_t seq_id)
    {
     
        Message ack_msg{};
        ack_msg.header.type = MsgType::ACK;
        ack_msg.header.seq_id = seq_id;
        ack_msg.header.data_len = 0;

        char buffer[sizeof(Message)];
        serialize_message(ack_msg, buffer);
        send(client_fd, buffer, sizeof(buffer), 0);
    }
};

int main() 
{
    Server server(12332);
    server.run();
    return 0;
}