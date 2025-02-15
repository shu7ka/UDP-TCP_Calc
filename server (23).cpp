#include <iostream>      // Для работы с вводом/выводом
#include <cstring>       // Для функций работы со строками (например, memset)
#include <sys/socket.h>  // Для создания и работы с сокетами
#include <netinet/in.h>  // Для структуры sockaddr_in и функций работы с IP/портами
#include <unistd.h>      // Для системных вызовов close, read и других
#include <fcntl.h>       // Для работы с флагами файлового дескриптора (например, O_NONBLOCK)
#include <errno.h>       // Для получения кодов ошибок системных вызовов
#include <stack>         // Для стека, используемого в вычислении выражений
#include <stdexcept>     // Для обработки исключений
#include <regex>         // Для проверки корректности входного выражения
#include <arpa/inet.h>   // Для преобразования IP-адресов

#define DEFAULT_PORT 8080  // Порт по умолчанию
#define DEFAULT_IP "127.0.0.1"  // IP-адрес по умолчанию (локальный хост)
#define TIMEOUT_SEC 3  // Таймаут в секундах для некоторых операций

// Функция для выполнения арифметических операций
double calculate(char op, double num1, double num2) {
    switch (op) {
        case '+': return num1 + num2; // Сложение
        case '-': return num1 - num2; // Вычитание
        case '*': return num1 * num2; // Умножение
        case '/': // Деление с проверкой на деление на 0
            if (num2 == 0) {
                throw std::runtime_error("Division by zero");
            }
            return num1 / num2;
        default: throw std::runtime_error("Invalid operator");
    }
}

// Функция для определения приоритета операции
int precedence(char op) {
    if (op == '+' || op == '-') return 1; // Низкий приоритет
    if (op == '*' || op == '/') return 2; // Высокий приоритет
    return 0; // Неизвестный оператор
}

// Функция для вычисления выражения в строковом формате
double evaluateExpression(const std::string &expr) {
    std::stack<double> values; // Стек для чисел
    std::stack<char> ops;      // Стек для операторов

    for (size_t i = 0; i < expr.length(); i++) {
        if (isspace(expr[i])) continue; // Пропуск пробелов

        if (isdigit(expr[i])) { // Если это число
            double val = 0;
            while (i < expr.length() && (isdigit(expr[i]) || expr[i] == '.')) {
                if (expr[i] == '.') { // Обработка десятичной точки
                    double decimal_place = 0.1;
                    i++;
                    while (i < expr.length() && isdigit(expr[i])) {
                        val += (expr[i] - '0') * decimal_place;
                        decimal_place *= 0.1;
                        i++;
                    }
                    break;
                }
                val = (val * 10) + (expr[i] - '0');
                i++;
            }
            values.push(val); // Добавление числа в стек
            i--;
        } else if (expr[i] == '(') {
            ops.push(expr[i]); // Открывающая скобка
        } else if (expr[i] == ')') {
            // Закрывающая скобка: вычисляем до открывающей скобки
            while (!ops.empty() && ops.top() != '(') {
                double val2 = values.top(); values.pop();
                double val1 = values.top(); values.pop();
                char op = ops.top(); ops.pop();
                values.push(calculate(op, val1, val2));
            }
            ops.pop(); // Удаляем '('
        } else if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' || expr[i] == '/') {
            // Обработка операторов
            while (!ops.empty() && precedence(ops.top()) >= precedence(expr[i])) {
                if (ops.top() == '(') break;
                double val2 = values.top(); values.pop();
                double val1 = values.top(); values.pop();
                char op = ops.top(); ops.pop();
                values.push(calculate(op, val1, val2));
            }
            ops.push(expr[i]); // Добавляем оператор в стек
        }
    }

    // Обрабатываем оставшиеся операторы
    while (!ops.empty()) {
        double val2 = values.top(); values.pop();
        double val1 = values.top(); values.pop();
        char op = ops.top(); ops.pop();
        values.push(calculate(op, val1, val2));
    }

    return values.top(); // Возвращаем результат
}

// Функция для форматирования результата (целые без десятичных знаков)
std::string formatResult(double result) {
    if (result == static_cast<int>(result)) {
        return std::to_string(static_cast<int>(result));
    } else {
        return std::to_string(result);
    }
}

int main(int argc, char *argv[]) {
    std::string ip = DEFAULT_IP; // IP-адрес сервера
    int port = DEFAULT_PORT;     // Порт сервера

    std::cout << "Using default IP: " << ip << ", Port: " << port << std::endl;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <TCP/UDP>" << std::endl;
        return -1;
    }

    // Определение, использовать ли UDP
    bool use_udp = (std::string(argv[1]) == "UDP");
    int server_fd; // Дескриптор сокета

    // Создание сокета
    if (use_udp) {
        server_fd = socket(AF_INET, SOCK_DGRAM, 0); // UDP
    } else {
        server_fd = socket(AF_INET, SOCK_STREAM, 0); // TCP
    }

    if (server_fd == -1) {
        std::cerr << "Socket creation failed" << std::endl;
        return -1;
    }

    struct sockaddr_in address; // Структура для хранения адреса сервера
    address.sin_family = AF_INET; // Семейство адресов IPv4
    inet_pton(AF_INET, ip.c_str(), &address.sin_addr); // Установка IP-адреса
    address.sin_port = htons(port); // Установка порта (в сетевом порядке байт)

    // Привязка сокета к адресу
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        close(server_fd);
        return -1;
    }

    // Для TCP — переводим сокет в режим прослушивания
    if (!use_udp && listen(server_fd, 3) < 0) {
        std::cerr << "Listen failed" << std::endl;
        close(server_fd);
        return -1;
    }

    std::cout << "Server started on " << ip << ":" << port << " (" << (use_udp ? "UDP" : "TCP") << ")" << std::endl;

    if (use_udp) {
        // Реализация для UDP
        char buffer[1024] = {0};
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        while (true) {
            // Получение данных от клиента
            int valread = recvfrom(server_fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&client_addr, &client_len);
            buffer[valread] = '\0'; // Завершаем строку
            if (valread <= 0) {
                std::cerr << "Error receiving data" << std::endl;
                continue;
            }

            std::string expression(buffer);
            if (expression.empty()) {
                std::cerr << "Received empty input from client. Skipping." << std::endl;
                continue;
            }

            // Проверка на допустимость выражения
            std::regex validExpressionRegex("^[0-9+\\-*/().\\s]+$");
            if (!std::regex_match(expression, validExpressionRegex)) {
                std::cerr << "Invalid expression received: " << expression << std::endl;
                sendto(server_fd, "Invalid expression", strlen("Invalid expression") + 1, 0,   (struct sockaddr*)&client_addr, client_len);
                continue;
            }

            try {
                double result = evaluateExpression(expression);
                std::string resultStr = formatResult(result);
                sendto(server_fd, resultStr.c_str(), resultStr.length() + 1, 0, (struct sockaddr*)&client_addr, client_len);
            } catch (const std::runtime_error &e) {
                std::string error_message = e.what();
                sendto(server_fd, error_message.c_str(), error_message.length() + 1, 0, (struct sockaddr*)&client_addr, client_len);
            }
        }
    } else {
        // Реализация для TCP
        while (true) {
            // Ожидание входящего соединения
            int new_socket = accept(server_fd, nullptr, nullptr);
            if (new_socket < 0) {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }

            std::cout << "New client connected" << std::endl;

            while (true) {
                char buffer[1024] = {0};
                int valread = read(new_socket, buffer, sizeof(buffer));
                if (valread <= 0 || std::string(buffer).empty()) {
                    std::cout << "Client disconnected or sent empty input." << std::endl;
                    break;
                }

                std::string expression(buffer);
                try {
                    double result = evaluateExpression(expression);
                    std::string resultStr = formatResult(result);
                    send(new_socket, resultStr.c_str(), resultStr.length() + 1, 0);
                } catch (const std::runtime_error &e) {
                    std::string error_message = e.what();
                    send(new_socket, error_message.c_str(), error_message.length() + 1, 0);
                }
            }

            close(new_socket); // Закрытие соединения
        }
    }

    close(server_fd); // Закрытие сокета сервера
    return 0;
}
