#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include "../include/worker.hpp"

/**
 * @class WorkersContainer
 * @brief Потокобезопасный контейнер для управления Worker'ами
 * 
 * @note Обеспечивает безопасный доступ к вектору Worker'ов из нескольких потоков
 *       с использованием std::mutex для синхронизации
 */
class WorkersContainer {
public:
    /**
     * @brief Выполняет операцию с внутренним вектором Worker'ов
     * @tparam Func Тип функтора (auto-deduced)
     * @param func Функтор, принимающий std::vector<std::unique_ptr<Worker>>&
     * 
     * @example
     * container.access([](auto& workers) {
     *     workers.push_back(std::make_unique<Worker>(config));
     * });
     */
    template<typename Func>
    void access(Func&& func) {
        std::lock_guard lock(mutex_);
        func(workers_);
    };

    /**
     * @brief Возвращает текущее количество Worker'ов
     * @return size_t Размер контейнера
     */
    size_t size() const {
        std::lock_guard lock(mutex_);
        return workers_.size();
    };

    /**
     * @brief Атомарно заменяет содержимое контейнера
     * @param other Контейнер для обмена
     */
    void swap(WorkersContainer& other) {
    using std::swap;
        std::unique_lock lock1(mutex_, std::defer_lock);
        std::unique_lock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
    
        swap(workers_, other.workers_);
    };

private:
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<Worker>> workers_;
};