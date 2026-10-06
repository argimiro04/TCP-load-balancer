 Load balancer L4 desde cero

Proyecto de portfolio: un balanceador de carga / reverse proxy TCP (capa 4) escrito en C++. El objetivo es entender y demostrar conceptos de sistemas distribuidos: reparto de carga, detección de fallos y failover.

## Decisiones de arquitectura (no cambiar sin que se pida explícitamente)
- **Capa 4 (TCP):** el LB termina la conexión del cliente, abre otra al backend y copia bytes en ambos sentidos. No parsea HTTP.
- **Lenguaje:** C++20, sockets POSIX (Linux). Sin librerías externas de red (nada de Boost/Asio en v1).
- **Entorno:** todo corre en Docker. El contenedor `lb` (gcc + cmake) monta el repositorio en `/app`; los backends son `traefik/whoami` (backend1..3, puerto 80) en la red de docker-compose.
- **Concurrencia v1:** sockets bloqueantes, un hilo por conexión.
- **Componentes previstos:** Listener, ProxySession, BackendPool, BalancingStrategy (patrón Strategy; RoundRobin como primera implementación) y HealthChecker.

## Reglas de trabajo
- Implementa **solo el alcance de la sesión pedida**. No anticipes componentes de sesiones futuras.
- Prioriza la legibilidad sobre la optimización. Comenta el *porqué* de las decisiones no evidentes (llamadas a sockets, sincronización), no el *qué*.
- Compila con `-Wall -Wextra -Wpedantic` sin warnings.
- Al terminar, resume qué archivos has creado o cambiado y cómo probarlo.
- `docs/learning-log.md` lo escribo yo y solo yo. No lo edites, no lo rellenes, no lo reformatees y no propongas texto para él, aunque una tarea parezca pedirlo. Solo puedes leerlo si te lo pido.

## Flujo de Git
- Nunca hagas commits en `main`. Trabaja en la rama que te indique.
- Commits pequeños, uno por paso lógico, con mensajes en inglés (Conventional Commits).
- No hagas merge: el merge lo hago yo tras revisar el PR.

## Compilar y ejecutar
```powershell
docker compose up -d --build
docker compose exec lb bash
```
Dentro del contenedor:
```bash
cmake -S . -B build && cmake --build build && ./build/lb
```
Desde Windows: `curl.exe http://localhost:8080`