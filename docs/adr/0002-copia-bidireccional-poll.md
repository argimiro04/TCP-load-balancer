# ADR 0002: Copia bidireccional con poll()

## Estado

Aceptado.

## Contexto

En capa 4, cada sesión tiene dos conexiones TCP: cliente ↔ balanceador y balanceador ↔ backend. El balanceador tiene que copiar los bytes que llegan por cada una a la otra, sin saber de antemano quién habla primero ni cuánto. En HTTP habla primero el cliente, pero en otros protocolos (SMTP, SSH) es el servidor.

Con sockets bloqueantes, un `recv` sobre un lado se queda parado hasta que ese lado envía algo, aunque el otro lado tenga datos pendientes. Hace falta una forma de esperar a cualquiera de los dos a la vez.

Además, el cierre de TCP es por sentidos: un lado puede terminar de enviar (EOF) y seguir recibiendo. Muchos protocolos dependen de este *half-close*.

## Decisión

- La copia la hace una función que recibe los dos descriptores y usa `poll()` sobre ambos, esperando `POLLIN` en cada uno.
- Lo que se lee de un lado se reenvía al otro con `send_all`, que repite `send` hasta enviarlo todo (un `send` puede enviar solo una parte) y usa `MSG_NOSIGNAL`, para que escribir en un socket cerrado devuelva `EPIPE` en vez de matar el proceso con `SIGPIPE`.
- Cuando `recv` devuelve 0 en un lado (EOF), se propaga el half-close con `shutdown(otro, SHUT_WR)` y se deja de vigilar ese lado para lectura. El otro sentido sigue funcionando.
- La sesión termina cuando ambos lados han llegado a EOF o ante cualquier error (`POLLERR`, `ECONNRESET`, fallo de `send`...). Al terminar se cierran los dos descriptores.

## Alternativas consideradas

- **Dos hilos por sesión, uno por sentido, con `recv`/`send` bloqueantes.** Es sencillo, pero duplica los hilos por conexión y obliga a coordinar ambos hilos para el cierre y los errores.
- **`select()`.** Equivalente para dos descriptores, pero limitado a `FD_SETSIZE` (1024): un descriptor con un número mayor no se puede vigilar, lo que con un hilo por conexión es fácil de alcanzar.
- **`epoll`.** Más eficiente con muchos descriptores, pero para dos por sesión no aporta nada y es específico de Linux. Encaja con un futuro modelo dirigido por eventos, no con v1.
- **Cerrar ambas conexiones en cuanto un lado envía EOF.** Más simple, pero rompe los protocolos que usan half-close: se perdería la respuesta que el backend aún estuviera enviando.

## Consecuencias

- Una sesión ocupa un único hilo y no se bloquea esperando al lado equivocado.
- El half-close se propaga correctamente, así que el balanceador es transparente para el protocolo.
- Los sockets siguen siendo bloqueantes: un `send_all` hacia un lado lento bloquea la sesión hasta que ese lado lee. Eso aplica contrapresión de forma natural, pero un cliente que no lee nunca retiene la sesión indefinidamente, porque de momento no hay timeouts.
