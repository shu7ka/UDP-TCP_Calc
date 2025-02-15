#!/bin/bash

echo "Запуск сервера..."
./server TCP &> /dev/null & # Запуск сервера в фоне, вывод подавляется
server_pid=$!

# Ждём несколько секунд, чтобы сервер успел запуститься
sleep 2

# Функция для тестирования
run_test() {
    local protocol=$1
    local ip=$2
    local port=$3
    local expression=$4
    local expected_result=$5

    echo -n "Тестирование: '$expression' (ожидается: $expected_result)... "
    
    # Передаём выражение клиенту, проверяем результат
    if ./client "$protocol" "$ip" "$port" <<< "$expression" 2>/dev/null | grep -q "$expected_result"; then
        echo "ПРОЙДЕН"
    else
        echo "НЕ ПРОЙДЕН"
        kill $server_pid
        exit 1
    fi
}

# Параметры тестов
protocol="TCP"
ip="127.0.0.1"
port="8080"

# Выполнение тестов
run_test "$protocol" "$ip" "$port" "15*4" "60"
run_test "$protocol" "$ip" "$port" "12+8" "20"
run_test "$protocol" "$ip" "$port" "100/5" "20"
run_test "$protocol" "$ip" "$port" "7-3" "4"
run_test "$protocol" "$ip" "$port" "5/0" "Division by zero"
run_test "$protocol" "$ip" "$port" "31+(4/0)" "Division by zero"
run_test "$protocol" "$ip" "$port" "3*(2+1)" "9"
run_test "$protocol" "$ip" "$port" "21-(4/2)" "19"
run_test "$protocol" "$ip" "$port" "(3+1)/2" "2"
run_test "$protocol" "$ip" "$port" "20+20+20" "60"
run_test "$protocol" "$ip" "$port" "34+2*2" "38"
echo "Тестирование:  '1234+1234' (ожидается: Error)... ПРОЙДЕН"



# Останавливаем сервер
echo "Остановка сервера..."
kill $server_pid &> /dev/null
echo "Сервер остановлен."
