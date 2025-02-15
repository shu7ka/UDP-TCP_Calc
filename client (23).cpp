#include <iostream>
#include <cstring>
#include <sys/socket.h> // Для работы с сокетами: создание, отправка/прием данных
#include <arpa/inet.h>  // Для преобразования IP-адресов из текстового в числовой формат
#include <unistd.h>     // Для работы с системными вызовами, например, close()
#include <fcntl.h>      // Для управления флагами файлового дескриптора (например, неблокирующий режим)
#include <regex>        // Для проверки формата IP, порта и выражений
#include <chrono>       // Для измерения времени (таймаут)
#include <cmath>        // Для std::abs

#define DEFAULT_PORT 8080 // Порт по умолчанию, если не указан пользователем
#define DEFAULT_IP "127.0.0.1" // Локальный IP-адрес для тестирования (loopback)
#define RETRY_LIMIT 3 // Максимальное количество попыток отправки запроса при использовании UDP
#define TIMEOUT_SEC 3 // Таймаут ожидания ответа в секундах (для UDP)

// Проверка корректности порта
bool isValidPort(const std::string &port) {
    std::regex portRegex(R"(^\d{1,5}$)");
    if (!std::regex_match(port, portRegex)) return false;
    int portNum = std::stoi(port);
    return portNum > 0 && portNum <= 65535;
}

// Проверка корректности IP-адреса
bool isValidIP(const std::string &ip) {
    std::regex ipRegex(
        R"(^((25[0-5]|2[0-4]\d|1\d\d|\d\d|\d)\.){3}(25[0-5]|2[0-4]\d|1\d\d|\d\d|\d)$)");
    return std::regex_match(ip, ipRegex);
}

// Проверка корректности математического выражения
bool isValidExpression(const std::string &expr) {
    std::regex validRegex(R"(^[\d\s\+\-\*/\.\(\)]*$)");
    return std::regex_match(expr, validRegex);
}

// Проверка диапазона чисел в выражении
bool isWithinRange(const std::string &expr) {
    std::regex numberRegex(R"((\b\d+(\.\d+)?\b))"); // Регулярное выражение для поиска чисел
    std::smatch match;

    std::string::const_iterator searchStart(expr.cbegin());
    while (std::regex_search(searchStart, expr.cend(), match, numberRegex)) {
        double value = std::stod(match[1].str());
        if (std::abs(value) >= 1000) { // Проверяем модуль числа
            
            return false;
        }
        searchStart = match.suffix().first;
    }
    return true;
}

// Отправка данных и обработка ответа при работе через UDP
bool sendWithConfirmation(int sock, struct sockaddr_in &serv_addr, const std::string &data, std::string &result) {
    int attempts = 0;
    char buffer[1024] = {0};
    while (attempts < RETRY_LIMIT) {
        sendto(sock, data.c_str(), data.length() + 1, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
        std::cout << "Sent expression to server: " << data << std::endl;

        auto start = std::chrono::steady_clock::now();
        while (true) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= TIMEOUT_SEC) break;

            socklen_t addr_len = sizeof(serv_addr);
            int valread = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&serv_addr, &addr_len);
            if (valread > 0) {
                result = std::string(buffer);
                return true;
            }
        }
        std::cout << "Timeout: No response, retrying..." << std::endl;
        attempts++;
    }
    std::cerr << "Lost connection to server." << std::endl;
    return false;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <TCP/UDP> <IP> <PORT>" << std::endl;
        return -1;
    }

    bool use_udp = (std::string(argv[1]) == "UDP");
    std::string ip = argv[2];
    std::string portStr = argv[3];
    int port = std::stoi(portStr);

    if (!isValidIP(ip)) {
        std::cerr << "Invalid IP address" << std::endl;
        return -1;
    }
    if (!isValidPort(portStr)) {
        std::cerr << "Invalid port" << std::endl;
        return -1;
    }

    int sock = 0;
    struct sockaddr_in serv_addr;

    if (use_udp) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
    } else {
        sock = socket(AF_INET, SOCK_STREAM, 0);
    }

    if (sock < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }

    auto closeSocket = [&]() {
        if (sock >= 0) {
            close(sock);
            sock = -1;
        }
    };

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        closeSocket();
        return -1;
    }

    if (!use_udp && connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        closeSocket();
        return -1;
    }

    if (use_udp) {
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    while (true) {
        std::string expression;
        std::cout << "Enter an expression (or type 'exit' to quit): ";
        std::getline(std::cin, expression);

        if (expression == "exit") {
            std::cout << "Exiting..." << std::endl;
            closeSocket();
            return 0;
        }

        if (!isValidExpression(expression)) {
            std::cerr << "Invalid expression" << std::endl;
            continue;
        }

        if (!isWithinRange(expression)) { // Проверка диапазона
             std::cerr << "Error" << std::endl;
            continue;
        }

        if (use_udp) {
            std::string result;
            if (!sendWithConfirmation(sock, serv_addr, expression, result)) {
                closeSocket();
                return -1;
            }
            std::cout << "Received from server: " << result << std::endl;
        } else {
            if (send(sock, expression.c_str(), expression.length() + 1, 0) < 0) {
                std::cerr << "Error sending data to server." << std::endl;
                closeSocket();
                return -1;
            }
            char buffer[1024] = {0};
            if (recv(sock, buffer, sizeof(buffer), 0) > 0) {
                std::cout << "Received from server: " << buffer << std::endl;
            } else {
                std::cerr << "Error receiving data from server." << std::endl;
                closeSocket();
                return -1;
            }
        }
    }

    closeSocket();
    return 0;
}
