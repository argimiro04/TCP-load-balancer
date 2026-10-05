#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace {
constexpr uint16_t kListenPort = 8080;
}

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    // Sin SO_REUSEADDR, al reiniciar el LB el puerto sigue ocupado mientras
    // las conexiones anteriores estén en TIME_WAIT (hasta ~60 s en Linux) y
    // bind falla con "Address already in use".
    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    // 0.0.0.0 (INADDR_ANY) y no 127.0.0.1: dentro del contenedor, el tráfico
    // publicado por Docker llega por la interfaz de red, no por loopback.
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kListenPort);  // los puertos van en orden de red (big-endian)

    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    // SOMAXCONN deja que el kernel use su tamaño máximo de cola de conexiones
    // pendientes, para no rechazar clientes en ráfagas mientras aceptamos.
    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    std::cout << "Escuchando en 0.0.0.0:" << kListenPort << std::endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            // Un fallo en accept suele afectar solo a esa conexión (cliente que
            // abortó, señal, límite de descriptores temporal): seguimos sirviendo.
            perror("accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip)) == nullptr) {
            perror("inet_ntop");
        } else {
            std::cout << "Conexión desde " << ip << ":" << ntohs(client_addr.sin_port) << std::endl;
        }

        if (close(client_fd) < 0) {
            perror("close");
        }
    }
}
