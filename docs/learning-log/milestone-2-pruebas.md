# Milestone 2: terminal tests

## 1. Basic forwarding

**What it shows:** that the load balancer started correctly and does a basic forward.

```powershell
PS C:\Users\User\desktop\proyectos\balanceador_de_carga_l4> curl.exe http://localhost:8080 ## Here we check that it started correctly with a basic forward
Hostname: backend1
IP: 127.0.0.1
IP: ::1
IP: 172.19.0.4
RemoteAddr: 172.19.0.5:60490
GET / HTTP/1.1
Host: localhost:8080
User-Agent: curl/8.21.0
Accept: */*

```

## 2. Large transfer (10 MB)

**What it shows:** a large transfer, 10 MB, through the load balancer.

```powershell
PS C:\Users\User\desktop\proyectos\balanceador_de_carga_l4> curl.exe "http://localhost:8080/data?size=10&unit=MB" -o NUL -w "%{size_download} bytes`n" ## Let's test a large transfer, 10 MB
  % Total    % Received % Xferd  Average Speed  Time    Time    Time   Current
                                 Dload  Upload  Total   Spent   Left   Speed
  0      0   0      0   0      0      0      0       100 10.00M   0 10.00M   0      0 115.9M      0       100 10.00M   0 10.00M   0      0 105.4M      0       100 10.00M   0 10.00M   0      0 97.72M      0                              0
10485760 bytes
```

## 3. Backend down and recovery

**What it shows:** that the load balancer works with a backend down and that it is then able to resume that connection once the backend is back.

```powershell
PS C:\Users\User\desktop\proyectos\balanceador_de_carga_l4> ## Now let's check that the load balancer works with a backend down and that it is then able to resume that connection once the backend is back
PS C:\Users\User\desktop\proyectos\balanceador_de_carga_l4> docker compose stop backend1
[+] stop 1/1
 ✔ Container balanceador_de_carga_l4-... Stopped 0.3s
PS C:\Users\User\desktop\proyectos\balanceador_de_carga_l4> curl.exe http://localhost:8080
curl: (52) Empty reply from server
PS C:\Users\User\desktop\proyectos\balanceador_de_carga_l4> ## In the load balancer terminal we can see that it accepts the connection but then is not able to forward it
PS C:\Users\User\desktop\proyectos\balanceador_de_carga_l4> docker compose start backend1
[+] start 1/1
 ✔ Container balanceador_de_carga_l4-... Started 0.2s
PS C:\Users\User\desktop\proyectos\balanceador_de_carga_l4> curl.exe http://localhost:8080
Hostname: backend1
IP: 127.0.0.1
IP: ::1
IP: 172.19.0.4
RemoteAddr: 172.19.0.5:36678
GET / HTTP/1.1
Host: localhost:8080
User-Agent: curl/8.21.0
Accept: */*

PS C:\Users\User\desktop\proyectos\balanceador_de_carga_l4> ## It is back up and connects without problems
```

**What to look for:**

- In the load balancer terminal (section 4), it accepts the connection but then is not able to forward it:
  ```
  Conexión aceptada desde 172.19.0.1:35106
  getaddrinfo(backend1:80): No address associated with hostname
  No se pudo conectar con backend1:80; se cierra la conexión del cliente
  ```
- After `docker compose start backend1`, it is back up and connects without problems: `Hostname: backend1`.

## 4. Environment and load balancer terminal

**What it shows:** that `docker compose ps` prints 4 containers: `lb` and `backend1..3`.

```powershell
S C:\Users\User\Desktop\proyectos\balanceador_de_carga_l4> docker compose up -d
PS C:\Users\User\Desktop\proyectos\balanceador_de_carga_l4> docker compose ps ## it should print 4 containers: lb and backend 1-3
NAME                                 IMAGE                        COMMAND            SERVICE    CREATED      STATUS          PORTS
balanceador_de_carga_l4-backend1-1   traefik/whoami               "/whoami"          backend1   4 days ago   Up 42 seconds   80/tcp
balanceador_de_carga_l4-backend2-1   traefik/whoami               "/whoami"          backend2   4 days ago   Up 42 seconds   80/tcp
balanceador_de_carga_l4-backend3-1   traefik/whoami               "/whoami"          backend3   4 days ago   Up 42 seconds   80/tcp
balanceador_de_carga_l4-lb-1         balanceador_de_carga_l4-lb   "sleep infinity"   lb         4 days ago   Up 42 seconds   0.0.0.0:8080->8080/tcp, [::]:8080->8080/tcp
PS C:\Users\User\Desktop\proyectos\balanceador_de_carga_l4> docker compose exec lb bash
```

```bash
root@aab9ab0f8efb:/app# cmake -S . -B build && cmake --build build && ./build/lb
-- Configuring done (0.1s)
-- Generating done (0.4s)
-- Build files have been written to: /app/build
[100%] Built target lb
Escuchando en 0.0.0.0:8080, backend backend1:80
Conexión aceptada desde 172.19.0.1:59406
Backend conectado: 172.19.0.5:60490 -> 172.19.0.4:80
Sesión cerrada: cliente->backend 78 B, backend->cliente 281 B; motivo: ambos lados cerraron (EOF)
Conexión aceptada desde 172.19.0.1:50236
Backend conectado: 172.19.0.5:44346 -> 172.19.0.4:80
Sesión cerrada: cliente->backend 98 B, backend->cliente 10488457 B; motivo: ambos lados cerraron (EOF)
Conexión aceptada desde 172.19.0.1:35106
getaddrinfo(backend1:80): No address associated with hostname
No se pudo conectar con backend1:80; se cierra la conexión del cliente
Conexión aceptada desde 172.19.0.1:47416
Backend conectado: 172.19.0.5:36678 -> 172.19.0.4:80
Sesión cerrada: cliente->backend 78 B, backend->cliente 281 B; motivo: ambos lados cerraron (EOF)
```

**What to look for:**

- The 4 container lines from `docker compose ps`: `backend1`, `backend2`, `backend3` and `lb`.
