#include "../include/daemonizer.hpp"
#include <stdexcept>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void Daemonizer::daemonize() {
    // 1. Первый fork
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("Daemonizer: Failed to fork (errno=" + std::to_string(errno) + ")");
    }
    if (pid > 0) {
        // Родитель завершает работу
        exit(EXIT_SUCCESS);
    }

    // 2. Создать новую сессию
    if (setsid() < 0) {
        throw std::runtime_error("Daemonizer: Failed to create new session (setsid)");
    }

    // 3. Игнорировать SIGHUP и SIGCHLD
    signal(SIGHUP, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    // 4. Второй fork
    pid = fork();
    if (pid < 0) {
        throw std::runtime_error("Daemonizer: Failed to fork second time (errno=" + std::to_string(errno) + ")");
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // 5. Сбросить маску прав доступа
    umask(0);

    // 6. Перейти в корневой каталог
    if (chdir("/") < 0) {
        throw std::runtime_error("Daemonizer: Failed to chdir to /");
    }

    // 7. Закрыть стандартные файловые дескрипторы
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // 8. Перенаправить stdin, stdout, stderr в /dev/null
    int fd0 = open("/dev/null", O_RDONLY);
    int fd1 = open("/dev/null", O_WRONLY);
    int fd2 = open("/dev/null", O_RDWR);

    // Гарантируем, что дескрипторы 0, 1, 2 заняты
    if (fd0 != STDIN_FILENO)  dup2(fd0, STDIN_FILENO);
    if (fd1 != STDOUT_FILENO) dup2(fd1, STDOUT_FILENO);
    if (fd2 != STDERR_FILENO) dup2(fd2, STDERR_FILENO);

    // Не забываем закрыть лишние дескрипторы, если они не 0/1/2
    if (fd0 > 2) close(fd0);
    if (fd1 > 2) close(fd1);
    if (fd2 > 2) close(fd2);
}