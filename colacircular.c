#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    int *buffer;   // Cola circular
    int capacity;  // Capacidad máxima de la cola
    int size; 	   // Número actual de elementos en la cola
    int front;     // Indice del frente de la cola
    int back;      // Indice del final de la cola
    pthread_mutex_t mutex;     // Mutex para exclusión mutua
    pthread_cond_t not_empty;  // Variable de condición para cuando la cola no esté llena
    pthread_cond_t not_full;   // Variable de condición para cuando la cola no esté vacía
} CircularQueue;

// Inicializa el monitor, inicializando la cola circular, el mutex y las variables de condición.
CircularQueue* createQueue(int capacity) {
    CircularQueue *queue = (CircularQueue *)malloc(sizeof(CircularQueue));
    queue->buffer = (int *)malloc(capacity * sizeof(int));
    queue->capacity = capacity;
    queue->size = 0;
    queue->front = 0;
    queue->back = -1;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
    return queue;
}

void destroyQueue(CircularQueue *queue) {
    free(queue->buffer);
    pthread_cond_destroy(&queue->not_full);
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}

// Agrega un ítem a la cola
void enqueue(CircularQueue *queue, int item, FILE *log) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->size == queue->capacity) {
        queue->capacity *= 2; // Duplicar tamaño
        queue->buffer = (int *)realloc(queue->buffer, queue->capacity * sizeof(int));
        fprintf(log, "Cola duplicada a tamaño %d\n", queue->capacity); // Registra en el log que se duplicó la cola
    }
    queue->back = (queue->back + 1) % queue->capacity;
    queue->buffer[queue->back] = item;
    queue->size++;
    pthread_cond_signal(&queue->not_empty); // Se señala que la cola no está vacía
    pthread_mutex_unlock(&queue->mutex);
}

// Quita un ítem de la cola
int dequeue(CircularQueue *queue, FILE *log) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->size == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    // Si el tamaño de la cola es menor o igual a 1/4 de la capacidad y la capacidad es mayor a 1
    if (queue->size <= queue->capacity / 4 && queue->capacity > 1) {
        int new_capacity = queue->capacity / 2;
        int* newBuffer = (int*) malloc(new_capacity * sizeof(int));

        // Copiar los elementos a la nueva memoria con la lógica circular
        for (int i = 0; i < queue->size; i++) {
            newBuffer[i] = queue->buffer[(queue->front + i) % queue->capacity];
        }

        // Actualizar la capacidad y la memoria del buffer
        free(queue->buffer);
        queue->buffer = newBuffer;
        queue->capacity = new_capacity;
        queue->front = 0; // Reiniciar 'front' después de la redimensión
        queue->back = queue->size; // 'back' debe ser igual al número de elementos en la cola

        fprintf(log, "Cola reducida a tamaño %d\n", queue->capacity); // Registra en el log que se redujo la cola
    }

    // Extraer el elemento de la cola
    int item = queue->buffer[queue->front];
    queue->front = (queue->front + 1) % queue->capacity; // Mover 'front' al siguiente índice
    queue->size--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    return item;
}

typedef struct {
    int id;
    CircularQueue *queue;
    int items_to_produce;
    FILE *log_file;
} ProducerArgs;

typedef struct {
    int id;
    CircularQueue *queue;
    int max_wait_time;
    FILE *log_file;
} ConsumerArgs;

void* producer(void *arg) {
    ProducerArgs *args = (ProducerArgs *)arg;
    for (int i = 0; i < args->items_to_produce; i++) {
        int item = rand() % 100; // Como no se especifica que item se manejará, utilizaremos un entero con valores aleatorios
        enqueue(args->queue, item, args->log_file);
        fprintf(args->log_file, "Productor %d agregó: %d\n", args->id, item); // Registra en el log lo que agregó el productor a la cola
        usleep(100000); // Espera de 0.1 segundos
    }
    return NULL;
}

void* consumer(void *arg) {
    ConsumerArgs *args = (ConsumerArgs *)arg;
    while (1) {
        int item = dequeue(args->queue, args->log_file);
        fprintf(args->log_file, "Consumidor %d extrajo: %d\n", args->id, item); // Registra en el log lo que extrajo el consumidor a la cola
        usleep(args->max_wait_time * 1000);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 9) {
        printf("Uso: %s -p <productores> -c <consumidores> -s <tamano> -t <espera>\n", argv[0]);
        return -1;
    }

    FILE *log_file = fopen("ejecucion.log", "w");
    if (!log_file) {
        perror("Error al abrir el archivo log");
        exit(EXIT_FAILURE);
    }
    int producers = atoi(argv[2]);
    int consumers = atoi(argv[4]);
    int queue_size = atoi(argv[6]);
    int wait_time = atoi(argv[8]);

    CircularQueue *queue = createQueue(queue_size);

    pthread_t producer_threads[producers];
    pthread_t consumer_threads[consumers];

    ProducerArgs producer_args[producers];
    ConsumerArgs consumer_args[consumers];

    for (int i = 0; i < producers; i++) {
        producer_args[i].id = i + 1;
        producer_args[i].queue = queue;
        producer_args[i].items_to_produce = 1; // Un productor produce 1 item.
        producer_args[i].log_file = log_file;
        pthread_create(&producer_threads[i], NULL, producer, &producer_args[i]);
    }

    for (int i = 0; i < consumers; i++) {
        consumer_args[i].id = i + 1;
        consumer_args[i].queue = queue;
        consumer_args[i].max_wait_time = wait_time;
        consumer_args[i].log_file = log_file;
        pthread_create(&consumer_threads[i], NULL, consumer, &consumer_args[i]);
    }

    for (int i = 0; i < producers; i++) {
        pthread_join(producer_threads[i], NULL);
    }

    sleep(2); // Esperar a que los consumidores terminen

    for (int i = 0; i < consumers; i++) {
        pthread_cancel(consumer_threads[i]);
    }

    fclose(log_file);

    destroyQueue(queue);

    return 0;
}
