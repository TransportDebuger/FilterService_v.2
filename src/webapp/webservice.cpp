#include <httplib.h>
#include <iostream>

int main() {
    httplib::Server svr;

    // REST API для управления сервисом
    svr.Post("/api/pause", [](const httplib::Request&, httplib::Response& res) {
        pauseProcessing();
        res.set_content("Обработка приостановлена", "text/plain");
    });

    svr.Post("/api/resume", [](const httplib::Request&, httplib::Response& res) {
        resumeProcessing();
        res.set_content("Обработка возобновлена", "text/plain");
    });

    svr.Post("/api/reload", [](const httplib::Request&, httplib::Response& res) {
        reloadConfig();
        res.set_content("Конфиг перезагружен", "text/plain");
    });

    std::cout << "Сервер запущен на http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);

    return 0;
}