#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_REFERENCIAS 100
#define TAMANO_TABLA_HASH 10

// Nodo para listas enlazadas en la tabla hash
typedef struct Nodo {
    int pagina;
    struct Nodo *siguiente;
} Nodo;

// Estructura para los marcos
typedef struct {
    int pagina;   // Página cargada en el marco (-1 si está vacío)
    int usado;    // A modo de bit de uso para el reloj
    int tiempo;   // Para el LRU
} Marco;

// Funciones para tabla hash
int funcion_hash(int pagina) {
    return pagina % TAMANO_TABLA_HASH;
}

Nodo *crear_nodo(int pagina) {
    Nodo *nuevo = (Nodo *)malloc(sizeof(Nodo));
    nuevo->pagina = pagina;
    nuevo->siguiente = NULL;
    return nuevo;
}

void insertar_en_hash(Nodo *tabla[], int pagina) {
    int posicion = funcion_hash(pagina);
    Nodo *nuevo = crear_nodo(pagina);

    if (tabla[posicion] == NULL) {
        tabla[posicion] = nuevo;
    } else {
        Nodo *temp = tabla[posicion];
        while (temp->siguiente != NULL) {
            temp = temp->siguiente;
        }
        temp->siguiente = nuevo;
    }
}

int buscar_en_hash(Nodo *tabla[], int pagina) {
    int posicion = funcion_hash(pagina);
    Nodo *temp = tabla[posicion];

    while (temp != NULL) {
        if (temp->pagina == pagina) return 1; // Encontrado
        temp = temp->siguiente;
    }
    return 0; // No encontrado
}

void eliminar_de_hash(Nodo *tabla[], int pagina) {
    int posicion = funcion_hash(pagina);
    Nodo *temp = tabla[posicion];
    Nodo *anterior = NULL;

    while (temp != NULL) {
        if (temp->pagina == pagina) {
            if (anterior == NULL) {
                tabla[posicion] = temp->siguiente;
            } else {
                anterior->siguiente = temp->siguiente;
            }
            free(temp);
            return;
        }
        anterior = temp;
        temp = temp->siguiente;
    }
}

// Limpia la tabla hash
void limpiar_tabla(Nodo *tabla[]) {
    for (int i = 0; i < TAMANO_TABLA_HASH; i++) {
        Nodo *temp = tabla[i];
        while (temp != NULL) {
            Nodo *eliminar = temp;
            temp = temp->siguiente;
            free(eliminar);
        }
        tabla[i] = NULL;
    }
}

// Lee las paginas y guarda la cantidad de ellas
void leer_referencias(const char *filename, int referencias[], int *num_referencias) {
    //Verifica si el archivo fue abierto correctamente
    FILE *archivo = fopen(filename, "r");
    if (!archivo) {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }
    // Setea el largo en 0, luego mientras lea paginas suma 1 al largo hasta que no haya mas que leer
    *num_referencias = 0;
    while (fscanf(archivo, "%d", &referencias[(*num_referencias)]) != EOF) {
        (*num_referencias)++;
    }
    fclose(archivo);
}

// Algoritmo FIFO
void fifo(int referencias[], int num_referencias, int num_marcos) {
    Marco marcos[num_marcos];
    Nodo *tabla_hash[TAMANO_TABLA_HASH] = {NULL};
    int puntero = 0, fallos = 0;

    //Setea todos los marcos de pagina como vacios
    for (int i = 0; i < num_marcos; i++) marcos[i].pagina = -1;

    //Pasa por todas las referencias
    for (int i = 0; i < num_referencias; i++) {
        //Busca en la tabla si es que está la pagina
        if (!buscar_en_hash(tabla_hash, referencias[i])) {
            //Si el marco no está vacio, elimina el "primero" que entró de la tabla hash
            if (marcos[puntero].pagina != -1) {
                eliminar_de_hash(tabla_hash, marcos[puntero].pagina);
            }
            //remplaza la pagina del marco e inserta en la tabla hash la pagina.
            marcos[puntero].pagina = referencias[i];
            insertar_en_hash(tabla_hash, referencias[i]);
            //suma 1 para pasar al siguiente que entro, al pasar el numero de marcos se le hace modulo, asi, siempre sigue al "primero" en entrar
            puntero = (puntero + 1) % num_marcos;
            fallos++;
        }
    }
    printf("FIFO - Fallos de página: %d\n", fallos);
}

// Algoritmo LRU
void lru(int referencias[], int num_referencias, int num_marcos) {
    Marco marcos[num_marcos];
    Nodo *tabla_hash[TAMANO_TABLA_HASH] = {NULL};
    int tiempo = 0, fallos = 0;

    for (int i = 0; i < num_marcos; i++) marcos[i].pagina = -1;

    for (int i = 0; i < num_referencias; i++) {
        tiempo++;

        if (!buscar_en_hash(tabla_hash, referencias[i])) {
            int indice_reemplazo = 0, menor_tiempo = INT_MAX;

            for (int j = 0; j < num_marcos; j++) {
                if (marcos[j].pagina == -1) {
                    indice_reemplazo = j;
                    break;
                }
                if (marcos[j].tiempo < menor_tiempo) {
                    menor_tiempo = marcos[j].tiempo;
                    indice_reemplazo = j;
                }
            }

            if (marcos[indice_reemplazo].pagina != -1) {
                eliminar_de_hash(tabla_hash, marcos[indice_reemplazo].pagina);
            }

            marcos[indice_reemplazo].pagina = referencias[i];
            marcos[indice_reemplazo].tiempo = tiempo;
            insertar_en_hash(tabla_hash, referencias[i]);
            fallos++;
        } else {
            for (int j = 0; j < num_marcos; j++) {
                if (marcos[j].pagina == referencias[i]) {
                    marcos[j].tiempo = tiempo;
                    break;
                }
            }
        }
    }
    printf("LRU - Fallos de página: %d\n", fallos);
}

// Algoritmo Optimo
void optimo(int referencias[], int num_referencias, int num_marcos) {
    Marco marcos[num_marcos];
    Nodo *tabla_hash[TAMANO_TABLA_HASH] = {NULL};
    int fallos = 0;

    for (int i = 0; i < num_marcos; i++) marcos[i].pagina = -1;

    for (int i = 0; i < num_referencias; i++) {
        if (!buscar_en_hash(tabla_hash, referencias[i])) {
            int indice_reemplazo = -1, max_distancia = -1;

            for (int j = 0; j < num_marcos; j++) {
                if (marcos[j].pagina == -1) {
                    indice_reemplazo = j;
                    break;
                }

                int distancia = INT_MAX;
                for (int k = i + 1; k < num_referencias; k++) {
                    if (marcos[j].pagina == referencias[k]) {
                        distancia = k - i;
                        break;
                    }
                }

                if (distancia > max_distancia) {
                    max_distancia = distancia;
                    indice_reemplazo = j;
                }
            }

            if (marcos[indice_reemplazo].pagina != -1) {
                eliminar_de_hash(tabla_hash, marcos[indice_reemplazo].pagina);
            }

            marcos[indice_reemplazo].pagina = referencias[i];
            insertar_en_hash(tabla_hash, referencias[i]);
            fallos++;
        }
    }

    printf("OPTIMO - Fallos de página: %d\n", fallos);
}

// Algoritmo Reloj
void lru_reloj(int referencias[], int num_referencias, int num_marcos) {
    Marco marcos[num_marcos];
    Nodo *tabla_hash[TAMANO_TABLA_HASH] = {NULL};
    int puntero = 0, fallos = 0;

    for (int i = 0; i < num_marcos; i++) {
        marcos[i].pagina = -1;
        marcos[i].usado = 0;
    }

    for (int i = 0; i < num_referencias; i++) {
        if (!buscar_en_hash(tabla_hash, referencias[i])) {
            while (marcos[puntero].usado == 1) {
                marcos[puntero].usado = 0;
                puntero = (puntero + 1) % num_marcos;
            }

            if (marcos[puntero].pagina != -1) {
                eliminar_de_hash(tabla_hash, marcos[puntero].pagina);
            }

            marcos[puntero].pagina = referencias[i];
            marcos[puntero].usado = 1;
            insertar_en_hash(tabla_hash, referencias[i]);

            puntero = (puntero + 1) % num_marcos;
            fallos++;
        } else {
            for (int j = 0; j < num_marcos; j++) {
                if (marcos[j].pagina == referencias[i]) {
                    marcos[j].usado = 1;
                    break;
                }
            }
        }
    }

    printf("RELOJ - Fallos de página: %d\n", fallos);
}

int main(int argc, char *argv[]) {
    //Verifica si tiene la estructura correcta, sino falla
    if (argc != 7) {
        fprintf(stderr, "Uso: %s -m <num_marcos> -a <algoritmo> -f <archivo>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_marcos = 0;
    char algoritmo[10];
    char archivo[50];

    //Extrae los parametros
    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "-m") == 0) {
            num_marcos = atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "-a") == 0) {
            strcpy(algoritmo, argv[i + 1]);
        } else if (strcmp(argv[i], "-f") == 0) {
            strcpy(archivo, argv[i + 1]);
        }
    }

    int referencias[MAX_REFERENCIAS], num_referencias;
    //Se calcula el numero de referencias(longitud de la secuencia) para usarla en los algoritmos
    leer_referencias(archivo, referencias, &num_referencias);

    //Usa el algoritmo señalado
    if (strcmp(algoritmo, "FIFO") == 0) {
        fifo(referencias, num_referencias, num_marcos);
    } else if (strcmp(algoritmo, "LRU") == 0) {
        lru(referencias, num_referencias, num_marcos);
    } else if (strcmp(algoritmo, "OPTIMO") == 0) {
        optimo(referencias, num_referencias, num_marcos);
    } else if (strcmp(algoritmo, "RELOJ") == 0) {
        lru_reloj(referencias, num_referencias, num_marcos);
    } else {
        //Si se escribe un algoritmo invalido pasa esto
        fprintf(stderr, "Algoritmo no reconocido: %s\n", algoritmo);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}