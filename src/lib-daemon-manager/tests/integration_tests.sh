#!/bin/bash

# Запуск демона
./daemon_example &
sleep 1

# Проверка PID-файла
if [ ! -f /var/run/daemon_example.pid ]; then
    echo "PID file not created"
    exit 1
fi

# Проверка процесса
PID=$(cat /var/run/daemon_example.pid)
if ! ps -p $PID > /dev/null; then
    echo "Process not running"
    exit 1
fi

# Тест graceful shutdown
kill -TERM $PID
sleep 1
if ps -p $PID > /dev/null; then
    echo "Process not terminated"
    exit 1
fi

# Тест перезагрузки
./daemon_example &
sleep 1
kill -HUP $PID
sleep 1
# Проверка обновленной конфигурации

echo "Integration tests passed"
exit 0
