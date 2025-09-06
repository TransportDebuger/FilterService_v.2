#pragma once
#include <string>
#include <optional>
#include <cstdio>

/// Утилита для управления PID-файлом через RAII и явные методы.
class PidFileManager {
public:
    /// Конструктор принимает путь к PID-файлу.
    explicit PidFileManager(std::string path);
    ~PidFileManager();

    /// Записать текущий PID процесса в файл. Перезаписывает файл.
    /// @throws std::runtime_error при ошибке записи.
    void write();

    /// Прочитать PID из файла. Если файл не существует или не может быть прочитан,
    /// возвращает пустую optional.
    std::optional<pid_t> read() const;

    /// Проверить, существует ли PID-файл и он содержит корректное число.
    bool exists() const;

    /// Удалить PID-файл. Без ошибок, если файла нет.
    void remove();

    const std::string& path() const { return path_; }

private:
    std::string path_;
};