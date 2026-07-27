/**
@file workercontainer.hpp
@brief Потокобезопасный контейнер для управления объектами Worker.
@version 2.0.0
@date 2026-07-17
*/
#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "worker.hpp"

namespace stc {

/**
@class WorkersContainer
@brief Обеспечивает потокобезопасный доступ к вектору рабочих потоков.
*/
class WorkersContainer {
public:
    /**
    @brief Выполняет произвольную операцию с внутренним вектором воркеров.
    @param[in] func Функтор, принимающий ссылку на вектор воркеров.
    */
    template <typename Func>
    void access(Func &&func) {
        std::lock_guard<std::mutex> lock(mutex_);
        func(workers_);
    }

    /**
    @brief Возвращает текущий размер контейнера воркеров.
    @return size_t Число элементов в контейнере.
    */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return workers_.size();
    }

    /**
    @brief Атомарно заменяет содержимое двух контейнеров.
    @param[in,out] other Контейнер, с которым будет выполнен обмен.
    */
    void swap(WorkersContainer &other) {
        std::unique_lock<std::mutex> lock1(mutex_, std::defer_lock);
        std::unique_lock<std::mutex> lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
        std::swap(workers_, other.workers_);
    }

private:
    /// @private Мьютекс для защиты доступа к внутреннему вектору.
    mutable std::mutex mutex_;

    /// @private Вектор умных указателей на объекты Worker.
    std::vector<std::unique_ptr<Worker>> workers_;
};

} // namespace stc