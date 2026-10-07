# ADR 0001: Arquitectura base

## Estado

Aceptado.

## Contexto

El proyecto es un balanceador de carga / reverse proxy construido desde cero con fines de aprendizaje y portfolio. El objetivo es entender y demostrar conceptos de sistemas distribuidos: reparto de carga, detección de fallos y failover. Antes de escribir código hay que fijar en qué capa trabaja, con qué lenguaje y herramientas, dónde se ejecuta y cómo atiende varias conexiones a la vez.

El desarrollo se hace en Windows, pero el código usa la API de sockets de Linux.

## Decisión

1. **Capa 4 (TCP).** El balanceador termina la conexión TCP del cliente, abre otra conexión al backend y copia bytes en ambos sentidos. No interpreta el contenido: no parsea HTTP.
2. **C++20 con sockets POSIX, sin librerías externas de red.** Se usan directamente `socket`, `bind`, `listen`, `accept`, `connect`, `poll`, etc. Nada de Boost.Asio ni similares en v1.
3. **Todo corre en Docker.** Un contenedor `lb` (gcc + cmake) monta el repositorio en `/app` y compila y ejecuta allí el balanceador. Los backends son tres contenedores `traefik/whoami` (`backend1..3`, puerto 80) en la red de docker-compose.
4. **Un hilo por conexión con sockets bloqueantes (v1).** Cada conexión de cliente se atiende en su propio hilo, que hace llamadas bloqueantes.

## Alternativas consideradas

- **Capa 7 (HTTP).** Permitiría balancear por ruta o cabeceras, pero obliga a parsear HTTP. Eso desvía el foco de lo que interesa aprender (gestión de conexiones, fallos, failover) hacia el parseo de un protocolo, y limita el balanceador a HTTP.
- **Boost.Asio u otra librería de red.** Reduce código y da un modelo asíncrono probado, pero oculta justo lo que se quiere entender: qué hace cada llamada al sistema y cómo fallan.
- **Otro lenguaje (Go, Rust).** Go esconde los sockets tras su runtime y sus goroutines; Rust añade una curva de aprendizaje que no es el objetivo. C++ con la API POSIX deja ver las llamadas al sistema sin capas intermedias.
- **Desarrollo nativo en Windows o en WSL sin contenedores.** Winsock difiere de POSIX y el entorno no sería reproducible. Docker da una toolchain Linux idéntica en cualquier máquina y, con docker-compose, una red privada con backends reales.
- **Modelo dirigido por eventos (`epoll`, sockets no bloqueantes).** Escala a muchas más conexiones, pero es bastante más difícil de leer y depurar. Se deja como posible evolución, no como objetivo de v1.

## Consecuencias

- El balanceador sirve para cualquier protocolo sobre TCP, pero no puede tomar decisiones basadas en el contenido (rutas, cabeceras, cookies).
- Hay más código y más gestión manual de errores y recursos (descriptores) que con una librería. A cambio, cada comportamiento es visible y explicable.
- Para compilar y probar hace falta Docker. Las IPs de cliente que ve el balanceador son las del gateway de Docker, por el NAT de los puertos publicados.
- El modelo de un hilo por conexión limita la escalabilidad (memoria por hilo, cambios de contexto) y obliga a sincronizar el estado compartido, como el pool de backends y su estado de salud.
