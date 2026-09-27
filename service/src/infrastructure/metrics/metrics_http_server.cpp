/**
 * @file metrics_http_server.cpp
 * @brief Реализация минималистичного HTTP-сервера для экспозиции.
 * @since 4.1.0
 * @date 28.09.2026
 */
#include "metrics_http_server.hpp"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <string_view>

namespace stc {

MetricsHttpServer::MetricsHttpServer(uint16_t port, 
                                     std::function<std::string()> payload_provider)
    : port_(port), payload_provider_(std::move(payload_provider)) {
    if (port_ == 0) {
        throw std::invalid_argument("MetricsHttpServer: port cannot be 0");
    }
    if (!payload_provider_) {
        throw std::invalid_argument("MetricsHttpServer: payload_provider cannot be null");
    }
}

MetricsHttpServer::~MetricsHttpServer() {
    Stop();
}

void MetricsHttpServer::Start() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("MetricsHttpServer: socket creation failed");
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(fd);
        throw std::runtime_error("MetricsHttpServer: bind failed on port " + std::to_string(port_));
    }

    if (listen(fd, 10) < 0) {
        close(fd);
        throw std::runtime_error("MetricsHttpServer: listen failed");
    }

    server_fd_.store(fd, std::memory_order_release);

    worker_thread_ = std::jthread([this](std::stop_token stoken) {
        WorkerLoop(stoken);
    });
}

void MetricsHttpServer::Stop() noexcept {
    worker_thread_.request_stop();
    
    int fd = server_fd_.exchange(-1, std::memory_order_acq_rel);
    if (fd >= 0) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void MetricsHttpServer::WorkerLoop(std::stop_token stoken) {
    struct pollfd pfd{};
    pfd.fd = server_fd_.load(std::memory_order_acquire);
    pfd.events = POLLIN;

    while (!stoken.stop_requested()) {
        // Таймаут 500 мс для периодической проверки stoken
        int poll_result = poll(&pfd, 1, 500);

        if (poll_result < 0) {
            if (errno == EINTR) continue;
            break; // Сокет закрыт или произошла ошибка
        }

        if (poll_result > 0 && (pfd.revents & POLLIN)) {
            struct sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(pfd.fd, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd >= 0) {
                HandleClient(client_fd);
            }
        }
    }
}

void MetricsHttpServer::HandleClient(int client_fd) {
    // Установка таймаута на чтение, чтобы избежать зависания на медленных клиентах
    struct timeval tv{1, 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char buffer[1024];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    
    std::string response;

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::string_view request(buffer, bytes_read);

        // Минималистичный парсинг: проверяем только первую строку
        if (request.starts_with("GET /metrics") && 
            (request.size() < 13 || request[12] == ' ' || request[12] == '?')) {
            try {
                std::string payload = payload_provider_();
                response = BuildHttpResponse(200, "OK", payload);
            } catch (...) {
                response = BuildHttpResponse(500, "Internal Server Error", "Metrics generation failed");
            }
        } else {
            response = BuildHttpResponse(404, "Not Found", "Use GET /metrics");
        }
    } else {
        response = BuildHttpResponse(400, "Bad Request", "Empty request");
    }

    send(client_fd, response.c_str(), response.size(), MSG_NOSIGNAL);
    close(client_fd);
}

std::string MetricsHttpServer::BuildHttpResponse(int status_code, 
                                                 const std::string& status_text, 
                                                 const std::string& body) {
    std::string response;
    response.reserve(128 + body.size());
    
    response.append("HTTP/1.1 ");
    response.append(std::to_string(status_code));
    response.append(" ");
    response.append(status_text);
    response.append("\r\n");
    
    response.append("Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n");
    response.append("Content-Length: ");
    response.append(std::to_string(body.size()));
    response.append("\r\n");
    response.append("Connection: close\r\n");
    response.append("\r\n");
    
    response.append(body);
    return response;
}

} // namespace stc