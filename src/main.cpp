#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
constexpr uint16_t kListenPort = 8080;
// Nombre del servicio en docker-compose: lo resuelve el DNS interno de Docker.
constexpr const char* kBackendHost = "backend1";
// getaddrinfo recibe el puerto como texto (admite también nombres como "http").
constexpr const char* kBackendPort = "80";

// Devuelve "ip:puerto" para una dirección IPv4 o IPv6.
std::string format_address(const sockaddr* addr) {
    char ip[INET6_ADDRSTRLEN] = "?";
    uint16_t port = 0;
    if (addr->sa_family == AF_INET) {
        const auto* v4 = reinterpret_cast<const sockaddr_in*>(addr);
        inet_ntop(AF_INET, &v4->sin_addr, ip, sizeof(ip));
        port = ntohs(v4->sin_port);
    } else if (addr->sa_family == AF_INET6) {
        const auto* v6 = reinterpret_cast<const sockaddr_in6*>(addr);
        inet_ntop(AF_INET6, &v6->sin6_addr, ip, sizeof(ip));
        port = ntohs(v6->sin6_port);
    }
    return std::string(ip) + ":" + std::to_string(port);
}

// Crea el socket de escucha. Devuelve el descriptor o -1 si algo falla.
int create_listener(uint16_t port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return -1;
    }

    // Sin SO_REUSEADDR, al reiniciar el LB el puerto sigue ocupado mientras
    // las conexiones anteriores estén en TIME_WAIT (hasta ~60 s en Linux) y
    // bind falla con "Address already in use".
    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        close(listen_fd);
        return -1;
    }

    // 0.0.0.0 (INADDR_ANY) y no 127.0.0.1: dentro del contenedor, el tráfico
    // publicado por Docker llega por la interfaz de red, no por loopback.
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);  // los puertos van en orden de red (big-endian)

    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    // SOMAXCONN deja que el kernel use su tamaño máximo de cola de conexiones
    // pendientes, para no rechazar clientes en ráfagas mientras aceptamos.
    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen");
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}

// Resuelve host:port y abre una conexión TCP bloqueante. Devuelve el
// descriptor conectado o -1 si no se pudo resolver ni conectar.
int connect_to_backend(const char* host, const char* port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;  // IPv4 o IPv6, lo que devuelva el DNS
    hints.ai_socktype = SOCK_STREAM;

    // Se resuelve en cada conexión en vez de una vez al arrancar: si el
    // backend se reinicia, Docker puede darle otra IP, y así la seguimos.
    addrinfo* results = nullptr;
    int rc = getaddrinfo(host, port, &hints, &results);
    if (rc != 0) {
        // getaddrinfo no usa errno: su código de error se traduce con gai_strerror.
        std::cerr << "getaddrinfo(" << host << ":" << port << "): " << gai_strerror(rc) << std::endl;
        return -1;
    }

    // Un nombre puede resolver a varias direcciones: probamos en orden hasta
    // que una conecte.
    int fd = -1;
    for (addrinfo* ai = results; ai != nullptr; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            perror("socket");
            continue;
        }
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            break;
        }
        perror(("connect " + format_address(ai->ai_addr)).c_str());
        close(fd);
        fd = -1;
    }

    freeaddrinfo(results);
    return fd;
}

// Registra la dirección local (puerto efímero que eligió el kernel en
// connect) y la remota de la conexión con el backend.
void log_backend_connection(int backend_fd) {
    sockaddr_storage local{};
    sockaddr_storage remote{};
    socklen_t local_len = sizeof(local);
    socklen_t remote_len = sizeof(remote);
    if (getsockname(backend_fd, reinterpret_cast<sockaddr*>(&local), &local_len) < 0) {
        perror("getsockname");
        return;
    }
    if (getpeername(backend_fd, reinterpret_cast<sockaddr*>(&remote), &remote_len) < 0) {
        perror("getpeername");
        return;
    }
    std::cout << "Backend conectado: " << format_address(reinterpret_cast<sockaddr*>(&local)) << " -> "
              << format_address(reinterpret_cast<sockaddr*>(&remote)) << std::endl;
}
}  // namespace

int main() {
    int listen_fd = create_listener(kListenPort);
    if (listen_fd < 0) {
        return EXIT_FAILURE;
    }
    std::cout << "Escuchando en 0.0.0.0:" << kListenPort << ", backend " << kBackendHost << ":" << kBackendPort
              << std::endl;

    while (true) {
        // sockaddr_storage tiene tamaño para cualquier familia de direcciones.
        sockaddr_storage client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            // Un fallo en accept suele afectar solo a esa conexión (cliente que
            // abortó, señal, límite de descriptores temporal): seguimos sirviendo.
            perror("accept");
            continue;
        }
        std::cout << "Conexión aceptada desde " << format_address(reinterpret_cast<sockaddr*>(&client_addr))
                  << std::endl;

        int backend_fd = connect_to_backend(kBackendHost, kBackendPort);
        if (backend_fd < 0) {
            // Sin backend no hay nada que servir: cerrar enseguida hace que el
            // cliente falle rápido en vez de quedarse esperando.
            std::cerr << "No se pudo conectar con " << kBackendHost << ":" << kBackendPort
                      << "; se cierra la conexión del cliente" << std::endl;
            close(client_fd);
            continue;
        }
        log_backend_connection(backend_fd);

        if (close(backend_fd) < 0) {
            perror("close(backend)");
        }
        if (close(client_fd) < 0) {
            perror("close(cliente)");
        }
    }
}
