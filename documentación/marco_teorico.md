
## Proyecto 1: Firmware Controlador Banda Transportadora

Curso: Programación II  
Carrera: Ingeniería Electrónica  
III Cuatrimestre 2026  

Este documento desarrolla los conceptos teóricos que sustentan la implementación del firmware del proyecto, incluyendo concurrencia en FreeRTOS, manejo de interrupciones, acceso seguro a variables compartidas, debouncing mediante CCOUNT, control PWM por hardware y arquitectura de máquina de estados finitos.



# Marco teórico

## 1. Concurrencia en FreeRTOS: Mutex, Spinlocks y Deadlocks

### 1.1 Contexto de una tarea vs. contexto de una ISR

Una tarea de FreeRTOS se ejecuta dentro del flujo normal del sistema operativo y puede ser planificada, suspendida o puesta en espera por el scheduler. Por esta razón, una tarea sí puede utilizar funciones que bloquean temporalmente su ejecución, por ejemplo `vTaskDelay()`, esperando que otra tarea utilice el procesador mientras tanto.

Una ISR, o Rutina de Servicio de Interrupción, funciona de manera diferente. Se ejecuta como respuesta inmediata a un evento de hardware y tiene prioridad sobre el código normal que estaba ejecutándose. Su objetivo debe ser realizar solamente el trabajo mínimo necesario y devolver el control lo más rápido posible.

Por esta razón, una ISR no debe llamar funciones bloqueantes como `vTaskDelay()`. El problema es que una ISR no pertenece al contexto normal de una tarea que el scheduler pueda suspender y reanudar. Intentar bloquear una interrupción puede dejar al sistema en un estado inconsistente y provocar un error crítico o un reinicio del ESP32.

En nuestro proyecto, la ISR del botón de emergencia solamente detecta la interrupción, aplica el filtro de rebote y modifica una bandera. La acción de detener el motor se procesa posteriormente desde el `loop()`, fuera de la ISR.

---

### 1.2 Spinlock vs. Mutex

Un Mutex de FreeRTOS se utiliza principalmente para sincronizar tareas. Cuando una tarea intenta tomar un Mutex que está ocupado, puede quedar bloqueada hasta que el recurso sea liberado. Esto funciona correctamente entre tareas porque el scheduler puede detener temporalmente una tarea y ejecutar otra.

Un Spinlock funciona de una manera más cercana al hardware. En el ESP32 se utiliza mediante estructuras como:

```cpp
portMUX_TYPE
```

y regiones críticas como:

```cpp
portENTER_CRITICAL()
portEXIT_CRITICAL()
```

o, dentro de una ISR:

```cpp
portENTER_CRITICAL_ISR()
portEXIT_CRITICAL_ISR()
```

En el proyecto se necesita un spinlock porque existe información compartida entre una ISR y el código ejecutado desde `loop()`. Una ISR no debe utilizar un Mutex tradicional que pueda bloquearse, ya que no puede quedarse esperando a que otra tarea libere el recurso.

Por eso, para proteger variables como la bandera del paro de emergencia y el contador de pulsaciones utilizamos un `portMUX_TYPE`.

---

### 1.3 Inversión de prioridad

La inversión de prioridad ocurre cuando una tarea de alta prioridad necesita utilizar un recurso que actualmente está siendo utilizado por una tarea de menor prioridad.

Por ejemplo, supongamos que una tarea de baja prioridad toma un Mutex. Después, una tarea de alta prioridad necesita el mismo Mutex y queda bloqueada esperando.

El problema puede empeorar si aparece una tarea de prioridad media. Esta tarea puede ejecutar antes que la tarea de baja prioridad, evitando que la tarea de baja prioridad termine su trabajo y libere el Mutex. Como resultado, indirectamente la tarea de alta prioridad termina esperando por una tarea de prioridad inferior.

Para reducir este problema, los Mutex de FreeRTOS pueden utilizar un mecanismo llamado **Priority Inheritance** o herencia de prioridad. Temporalmente, la tarea de baja prioridad que posee el Mutex hereda la prioridad de la tarea de mayor prioridad que está esperando el recurso.

De esta manera puede terminar su sección crítica más rápidamente, liberar el Mutex y permitir que la tarea de alta prioridad continúe.

---

### 1.4 Condiciones de Coffman y Deadlock

Para que pueda existir un deadlock deben cumplirse simultáneamente cuatro condiciones conocidas como condiciones de Coffman:

1. **Exclusión mutua:** un recurso solamente puede ser utilizado por un proceso o tarea a la vez.
2. **Retención y espera:** una tarea mantiene un recurso mientras espera adquirir otro.
3. **No apropiación:** el sistema no puede quitarle forzosamente el recurso a la tarea que lo posee.
4. **Espera circular:** existe una cadena donde cada tarea espera un recurso que está siendo retenido por otra.

En un escenario incorrecto de nuestro proyecto, podría ocurrir que el código principal mantuviera bloqueado un Mutex mientras se produce una interrupción. Si la ISR intentara adquirir ese mismo Mutex, la ISR quedaría esperando.

Sin embargo, el código principal tampoco podría continuar para liberarlo porque fue interrumpido por la ISR. Se produciría entonces una situación en la que ninguno de los dos contextos podría avanzar.

Esta es una de las razones principales por las que no se utilizan Mutex bloqueantes dentro de una ISR y se utilizan regiones críticas con spinlocks.

---

# 2. Arquitectura de memoria y restricciones del ESP32

## 2.1 Caché de Flash e `IRAM_ATTR`

El ESP32 almacena gran parte del programa en memoria Flash externa. Debido a que la Flash es más lenta que la memoria interna, el procesador utiliza una caché para acceder al código de forma eficiente.

Durante algunas operaciones, como escritura en memoria Flash, actualización OTA o determinadas operaciones con memoria no volátil, la caché de Flash puede quedar temporalmente deshabilitada.

Si en ese instante ocurre una interrupción y la ISR está almacenada en Flash, el procesador podría intentar ejecutar código que temporalmente no está accesible.

Para evitar esto, una ISR crítica puede declararse utilizando:

```cpp
IRAM_ATTR
```

Esto indica que el código debe ubicarse en la memoria interna IRAM del ESP32.

En nuestro proyecto utilizamos:

```cpp
void IRAM_ATTR emergencyButtonISR()
```

de forma que la rutina de interrupción pueda ejecutarse desde memoria interna y no dependa directamente del acceso a Flash.

---

## 2.2 Torn Reads o lecturas fragmentadas

Una lectura fragmentada puede ocurrir cuando una variable es más grande que el tamaño de palabra que el procesador puede manejar de forma atómica.

Si tenemos, por ejemplo:

```cpp
uint64_t contador;
```

la variable ocupa 64 bits.

Si el procesador realiza ciertas operaciones internamente en bloques menores, la lectura completa puede requerir más de una operación. El problema ocurre si una ISR modifica el valor justo entre esas operaciones.

Por ejemplo, el código principal podría leer primero una parte del valor antiguo y después otra parte del valor nuevo. El resultado sería un número que nunca existió realmente.

Eso se conoce como **Torn Read**.

Para evitar este tipo de situaciones se utilizan regiones críticas. El código que lee o modifica datos compartidos queda protegido para impedir que otro contexto los modifique durante la operación.

En nuestro proyecto el contador utilizado actualmente es de 32 bits, pero el concepto sigue siendo importante para comprender por qué los datos compartidos deben protegerse adecuadamente.

---

## 2.3 La palabra clave `volatile`

Los compiladores modernos intentan optimizar el código para hacerlo más rápido. Una de esas optimizaciones consiste en mantener temporalmente una variable dentro de un registro del procesador en lugar de volver a leerla constantemente desde memoria RAM.

Normalmente esto es correcto porque el compilador supone que una variable solamente cambia cuando el código visible la modifica.

Sin embargo, una variable compartida con una ISR puede cambiar de manera asíncrona.

Por ejemplo:

```cpp
bool emergencyEvent;
```

podría ser modificada por una ISR sin que el flujo normal del programa haya ejecutado ninguna asignación.

Si no se utiliza `volatile`, el compilador podría reutilizar una copia antigua almacenada en un registro y no detectar correctamente el nuevo valor escrito por la ISR.

Al declarar:

```cpp
volatile bool emergencyEvent;
```

indicamos al compilador que esa variable puede cambiar fuera del flujo normal y que sus accesos deben tratarse teniendo esto en cuenta.

Es importante aclarar que `volatile` **no evita condiciones de carrera**. En nuestro proyecto también utilizamos un spinlock para proteger el acceso concurrente a las variables.

---

# 3. Física del rebote mecánico y debouncing

## 3.1 Física del contacto

Cuando se presiona un microinterruptor, los contactos metálicos internos no pasan instantáneamente de estar completamente separados a permanecer perfectamente unidos.

Debido a la elasticidad de los materiales, vibraciones y características mecánicas del contacto, estos pueden tocarse y separarse varias veces durante unos pocos milisegundos antes de estabilizarse.

Para una persona esto sigue siendo una única pulsación, pero para un microcontrolador funcionando a millones de ciclos por segundo esos cambios son perfectamente detectables.

Por esta razón, una sola pulsación puede producir múltiples transiciones:

```text
HIGH
LOW
HIGH
LOW
HIGH
LOW
```

aunque físicamente el usuario solamente presionó el botón una vez.

Este fenómeno se conoce como **contact bounce** o rebote mecánico.

---

## 3.2 Debouncing mediante `CCOUNT`

Una forma sencilla de eliminar rebotes sería utilizar `delay()`, pero hacerlo dentro de una ISR sería incorrecto porque bloquearía la ejecución.

En este proyecto utilizamos:

```cpp
XTHAL_GET_CCOUNT()
```

Este mecanismo permite leer directamente el contador de ciclos del procesador.

Cuando ocurre una interrupción se guarda el valor actual del contador:

```cpp
uint32_t currentCycles = XTHAL_GET_CCOUNT();
```

y se compara contra el valor registrado durante la última interrupción válida:

```cpp
uint32_t elapsedCycles =
    currentCycles - isrLastCycle;
```

Solamente aceptamos el evento si se cumplen al menos:

```cpp
elapsedCycles >= 4000000
```

De esta forma no esperamos activamente. Simplemente medimos cuánto tiempo ha transcurrido y descartamos eventos demasiado cercanos entre sí.

Esto produce un debouncing no bloqueante.

---

## 3.3 Aritmética modular y rollover de `CCOUNT`

`CCOUNT` es un contador de 32 bits. Cuando llega al valor máximo:

\[
2^{32}-1
\]

la siguiente cuenta vuelve a:

\[
0
\]

A primera vista esto parece generar un problema cuando calculamos el tiempo transcurrido, pero con enteros sin signo la resta funciona utilizando aritmética modular.

Supongamos de manera simplificada un contador de 8 bits.

El máximo sería:

\[
255
\]

Si el último valor fue:

\[
250
\]

y después del rollover el contador actual es:

\[
5
\]

matemáticamente esperaríamos que hayan transcurrido:

\[
11
\]

cuentas.

En aritmética unsigned:

\[
5-250
\]

se evalúa módulo 256:

\[
(5-250)\mod256
\]

\[
=-245\mod256
\]

\[
=11
\]

El mismo principio se aplica a un contador de 32 bits utilizando módulo:

\[
2^{32}
\]

Por eso podemos utilizar directamente:

```cpp
currentCycles - isrLastCycle
```

sin crear una condición especial para detectar manualmente el rollover.

---

# 4. Periféricos de IdeaBoard y determinismo

## 4.1 Hardware PWM y LEDC

El ESP32 posee el periférico LEDC, que permite generar PWM directamente mediante hardware.

En nuestro proyecto el profesor solicita configurar:

```text
Frecuencia = 1000 Hz
Resolución = 10 bits
```

Con 10 bits tenemos:

\[
2^{10}=1024
\]

niveles posibles de duty cycle:

```text
0 ... 1023
```

Antes de utilizar los canales que controlan IO12 e IO14 debemos configurar un temporizador LEDC porque ese temporizador determina características como la frecuencia y la resolución utilizadas por los canales asociados.

Después los canales pueden modificar el duty sin que el procesador tenga que generar manualmente cada pulso.

Esto es mejor que producir PWM mediante software, porque el periférico de hardware mantiene la señal de forma más estable y libera al CPU para ejecutar otras partes del programa.

---

## 4.2 Uso de IO33 e IO34 con `INPUT_PULLUP`

En nuestro proyecto el microinterruptor se conecta entre:

```text
IO33 ─ botón ─ GND
```

y utilizamos:

```cpp
pinMode(33, INPUT_PULLUP);
```

La resistencia pull-up interna mantiene normalmente el pin en nivel lógico HIGH.

Cuando se presiona el botón, el pin queda conectado directamente a GND y pasa a LOW.

El IO33 permite utilizar esta resistencia interna.

Sin embargo, algunos GPIO del ESP32, específicamente los GPIO de entrada como 34 a 39 en variantes clásicas del ESP32, no disponen de resistencias pull-up/pull-down internas.

Por eso, si se trasladara el botón a IO34, no bastaría con utilizar:

```cpp
INPUT_PULLUP
```

y sería necesaria una resistencia externa para definir correctamente el nivel lógico cuando el botón está abierto.

De lo contrario, el pin podría quedar flotante y generar lecturas impredecibles.

---

# 5. Matriz de transición y complejidad O(1)

Nuestra Máquina de Estados Finitos utiliza una matriz donde:

- las filas representan estados;
- las columnas representan eventos.

Por ejemplo:

```cpp
FSM[state][event]
```

Si el sistema está en:

```text
RUNNING
```

y ocurre:

```text
BTN_PRESS
```

el programa puede acceder directamente a:

```cpp
FSM[RUNNING][BTN_PRESS]
```

y obtener el handler correspondiente.

No necesitamos recorrer elementos ni buscar qué transición corresponde al evento.

Por eso el tiempo de acceso es constante:

\[
O(1)
\]

independientemente de cuál estado o evento esté siendo procesado.

Este enfoque también permite separar claramente la definición de las transiciones de las acciones ejecutadas por cada handler.

En comparación, una implementación basada en muchos `if-else` o `switch-case` anidados puede aumentar la cantidad de comparaciones y saltos condicionales conforme el sistema crece.

Además, una matriz de transición hace que la arquitectura sea más predecible y fácil de analizar, algo especialmente importante en sistemas embebidos donde se busca comportamiento determinista.

---

# 6. Relación de estos conceptos con el proyecto

Los conceptos anteriores no se utilizaron solamente de forma teórica, sino que están directamente relacionados con la arquitectura implementada.

En la HAL del motor se utilizó el periférico LEDC para controlar el L9110S mediante IO12 e IO14.

La entrada de emergencia utiliza IO33 con `INPUT_PULLUP` y genera una interrupción mediante flanco `FALLING`.

La ISR utiliza `IRAM_ATTR` y `XTHAL_GET_CCOUNT()` para realizar un debouncing no bloqueante basado en ciclos.

Las variables compartidas se declararon como `volatile` y se protegieron mediante un `portMUX_TYPE`, utilizando regiones críticas diferentes para contexto normal y contexto ISR.

Finalmente, la lógica principal se organizó mediante una FSM basada en una matriz `constexpr` de handlers. Esto permite procesar cada combinación Estado-Evento mediante acceso directo y mantener un comportamiento determinista.

De esta manera, el proyecto combina control de hardware, concurrencia, interrupciones y arquitectura de software embebido dentro de una misma implementación.