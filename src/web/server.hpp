#pragma once
#include "core/session.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace training::web {

struct Config {
    bool enabled{true};
    int port{27182};
};

class Server {
public:
    explicit Server(Session& session);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void configure(const Config& config);
    void start();
    void stop();

    bool running() const { return running_.load(); }
    int port() const { return port_; }
    std::string listen_address() const; // 例如 "0.0.0.0:27182"

private:
    Session& session_;
    int port_{27182};
    std::atomic<bool> should_run_{false};
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> thread_;

    // 实际的 httplib::Server 放在 .cpp 里，避免头文件依赖
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void run();
};

// 全局单例（和 preferences 一样的用法）
Server& web_server();
void initialize_web_server(Session& session, const Config& config);

} // namespace training::web