/**
 * @file metrics_http_server.hpp
 * @brief Минималистичный HTTP-сервер для экспозиции метрик.
 * @since 4.1.0
 * @date 28.09.2026
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace stc {

/**
 * @class MetricsHttpServer
 * @brief Инкапсулирует сетевой ввод-вывод для отдачи текстового
 *        дампа метрик в формате Prometheus Text Exposition.
 * @since 4.1.0
 */
class MetricsHttpServer {
public:
    /**
     * @brief Конструирует экземпляр HTTP-сервера.
     * @param[in] port TCP-порт для прослушивания.
     * @param[in] payload_provider Делегат для получения текста.
     * @throws std::invalid_argument Если port равен 0 или
     *         payload_provider пуст.
     * @since 4.1.0
     */
    MetricsHttpServer(uint16_t port, std::function<std::string()> payload_provider);

    /// @brief Деструктор. Останавливает поток и закрывает сокет.
    ~MetricsHttpServer();

    MetricsHttpServer(const MetricsHttpServer&) = delete;
    MetricsHttpServer& operator=(const MetricsHttpServer&) = delete;

    /**
     * @brief Инициализирует сокет и запускает фоновый поток.
     * @throws std::runtime_error При ошибках создания сокета,
     *         bind или listen.
     * @since 4.1.0
     */
    void Start();

    /// @brief Останавливает фоновый поток и освобождает ресурсы.
    void Stop() noexcept;

private:
    /**
     * @brief Основной цикл обработки входящих соединений.
     * @param[in] stoken Токен кооперативной остановки потока.
     * @private
     * @since 4.1.0
     */
    void WorkerLoop(std::stop_token stoken);

    /**
     * @brief Обрабатывает один HTTP-запрос от клиента.
     * @param[in] client_fd Файловый дескриптор соединения.
     * @private
     * @since 4.1.0
     */
    void HandleClient(int client_fd);

    /**
     * @brief Формирует валидный HTTP-ответ.
     * @param[in] status_code Код состояния HTTP.
     * @param[in] status_text Текстовое представление статуса.
     * @param[in] body Тело ответа.
     * @return std::string Сформированная строка HTTP-ответа.
     * @private
     * @since 4.1.0
     */
    static std::string BuildHttpResponse(int status_code, 
                                         const std::string& status_text, 
                                         const std::string& body);

    /// @brief TCP-порт для прослушивания.
    uint16_t port_;
    
    /// @brief Делегат для получения payload.
    std::function<std::string()> payload_provider_;
    
    /// @brief Файловый дескриптор слушающего сокета.
    std::atomic<int> server_fd_{-1};
    
    /// @brief Фоновый поток обработки соединений.
    std::jthread worker_thread_;
};

} // namespace stc