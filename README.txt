README – Sistema de Minería Distribuida en C (Linux)

YEIMY VANESSA LOPEZ TERREROS 160004713

.
.
.
.

1.Requisitos previos

   Tener instalado gcc y OpenSSL

   Ejecutar en la terminal:
	   sudo apt update
	   sudo apt install build-essential libssl-dev

   Archivos:
	   server.c → Servidor coordinador de la minería.
	   worker.c → Cliente (worker) que realiza la búsqueda del hash.

.
.
.
.

2. Compilación

En la carpeta donde están los archivos, ejecutar:

gcc server.c -o server -lpthread -lm
gcc worker.c -o worker -lpthread -lssl -lcrypto

Esto generará los ejecutables server y worker.

	Ejecución

	1. Iniciar el servidor

	   ./server

	   Ingresar un texto base cuando se solicite. 
	   
	   TEXTO EJEMPLO: "La tecnología blockchain es un mecanismo avanzado de base de datos  que permite compartir información de forma transparente dentro de una red empresarial."
	   
	   El servidor esperará conexiones de todos los workers.

	2. Iniciar los workers (clientes)
	   En otras terminales locales en la carpeta donde están los archivos,    		ejecutar:

	   ./worker
	   
	   Configuracion establecida para 2 workers, 
	   si se quiere cambiar, modificar la constante NUM_WORKERS en server.c con el numero de workers deseado (El programa usará esa varible para la asignacion de rangos)
	   
	   Cada worker se conectará al servidor (por defecto en 127.0.0.1:5000).
	   Si el servidor está en otro equipo, modificar la constante SERVER_IP en worker.c con la IP del servidor antes de compilar.

.
.
.
.

3. Proceso
El servidor divide el espacio de búsqueda en rangos y los asigna a cada worker.
Cada worker genera combinaciones de 5 caracteres, calcula su hash SHA-256 y verifica si cumple la condición (termina en los dígitos especificados).
Cuando uno encuentra el hash, se envía un mensaje al servidor y este ordena a los demás detenerse.

4. Finalización

Al encontrar un hash válido, el servidor muestra:

     Hash encontrado por worker X
     Relleno: xxxxx
     Hash: ....
 
Luego envía la señal STOP a los demás workers.

Notas

La condición de búsqueda está definida en `CONDITION_SUFFIX` dentro de server.c.
Por defecto se buscan combinaciones de 5 caracteres
Para cambiar la cantidad de workers, modificar NUM_WORKERS en server.c.
Si se ejecuta en diferentes equipos, asegurarse de que el puerto 5000 esté abierto en el firewall. y hacer los cambios de ip indicados.

