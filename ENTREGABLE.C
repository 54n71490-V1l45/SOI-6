/*
Autores: Adriana Sánchez-Bravo Cuesta y Santiago Vilas Pampín
*/

#include <stdio.h>      //Para printf, perror
#include <stdlib.h>
#include <string.h>
#include <ctype.h>      //toupper
#include <fcntl.h>      //Para open
#include <unistd.h>     //Para close, read
#include <sys/stat.h>   //Para fstat
#include <sys/mman.h>   //Para mmap, munmap
#include <sys/wait.h>
#include <signal.h>

//Variable global para sincronización simple
volatile int señal_recibida = 0;

void manejador(int sig) {
    señal_recibida = 1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <archivo_entrada> <archivo_salida>\n", argv[0]);
        exit(1);
    }

    if (strcmp(argv[1], argv[2]) == 0) {
        printf("Los archivos de entrada y salida deben ser diferentes.\n");
        exit(1);
    }


    int fich1, fich2;
    struct stat sb;
    char *mapa;

    fich1 = open(argv[1], O_RDONLY);
    if(fich1 == -1){
        perror("Error en open del archivo de entrada");
        exit(1);
    }
    if (fstat(fich1, &sb) == -1) { //averigua su longitud con fstat
        perror("Error en la función fstat()");
        close(fich1);
        exit(1);
    }
    //proyeccion de memoria
    mapa= (char*)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fich1, 0);
    close(fich1); //Ya no necesitamos el descriptor
    if(mapa == MAP_FAILED){
        perror("Error en mmap\n");
        exit(1);
    }

    //Calcular tamaño final
    long tam = 0;
    for (int i = 0; i < sb.st_size; i++) {
        if (isdigit((unsigned char)mapa[i])) {
            tam += mapa[i] - '0'; //número de asteriscos
        } else {
            tam += 1; //un carácter normal
        }
    }

    //Buffer compartido
    char *buffer= (char*)mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(buffer == MAP_FAILED){
        perror("Error en 2º mmap\n");
        munmap(mapa,sb.st_size);
        exit(1);
    }

    //fichero de salida
    fich2 = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fich2 == -1) {
        perror("Error en open fichero salida");
        munmap(mapa,sb.st_size);
        munmap(buffer, tam);
        exit(1);
    }

    //Se ajustar tamaño antes de la proyección
    if (ftruncate(fich2, tam) == -1) {
        perror("Error ftruncate");
        close(fich2);
        exit(1);
    }

    char *mapa_salida= (char*)mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_SHARED, fich2, 0);
    if (mapa_salida == MAP_FAILED) {
        perror("Error en mmap fichero salida\n");
        close(fich2);
        exit(1);
    }

    signal(SIGUSR1, manejador); //Se configura al señal

    
    pid_t hijo= fork();
    long mitad= sb.st_size/2;
    if(hijo==-1){
        perror("Error en el fork");
        munmap(mapa,sb.st_size);
        munmap(buffer, tam);
        close(fich2);
        exit(1);
    }

    if(hijo==0){
        //Proceso HIJO encargado de los asteriscos
        while(!señal_recibida){
            pause();
        }
        señal_recibida = 0; //Se resetea

        //Procesa la 1º mitad
        long j = 0; //Índice para el buffer de salida
        for (int i = 0; i < mitad; i++) {
            if (isdigit((unsigned char)mapa[i])) {
                int n = mapa[i] - '0';
                for(int k=0; k<n; k++){
                    buffer[j+k] = '*'; //Escribe asteriscos
                } 
                j += n;
            } else {
                j++; //Salta la letra (ya la escribió el padre)
            }
        }

        while(!señal_recibida){
            pause();
        }

        //Procesar la 2º mitad
        j = 0;
        for (int i = 0; i < sb.st_size; i++) {
            int avance = 1;
            if (isdigit((unsigned char)mapa[i])) {
                avance = mapa[i] - '0';
                if (i >= mitad) { //Solo actuamos en la 2ª mitad
                    for(int k=0; k<avance; k++){
                        buffer[j+k] = '*';
                    }
                }
            }
            j += avance;
        }

        exit(33);
    }else{
        //Proceso PADRE encargado de pasar a mayúsculas
        //Se procesa la 1º mitad
        long j = 0;
        for (int i = 0; i < mitad; i++) {
            if (isdigit((unsigned char)mapa[i])) {
                j += mapa[i] - '0'; // Deja el hueco 
            } else {
                buffer[j] = toupper((unsigned char)mapa[i]); // Escribe letra
                j++;
            }
        }

        kill(hijo,SIGUSR1); //Se avisa de que ya finalizó la primera mitad al hijo
        
        //Se procesa la 2º mitad
        for (int i = mitad; i < sb.st_size; i++) {
             if (isdigit((unsigned char)mapa[i])) {
                j += (mapa[i] - '0'); // Deja el hueco
            } else {
                buffer[j] = toupper((unsigned char)mapa[i]);
                j++;
            }
        }
        
        kill(hijo,SIGUSR1); //Avisa de que ya finalizó la segunda mitad
    }

    wait(NULL); //Se espera a que el hijo termine

    //Se copia el buffer a proyección
    memcpy(mapa_salida, buffer, tam);

    int asteriscos = 0;
    for(long k=0; k<tam; k++){
        if(mapa_salida[k] == '*'){
            asteriscos++;
        }
    }

    char resumen[50];
    sprintf(resumen, "\nTotal asteriscos: %d\n", asteriscos);
    
    lseek(fich2, 0, SEEK_END); //Ir al final del archivo
    write(fich2, resumen, strlen(resumen));

    //Limpieza
    munmap(mapa, sb.st_size);
    munmap(buffer, tam);
    munmap(mapa_salida, tam);
    close(fich2);
        
    printf("Proceso completado. Asteriscos: %d\n", asteriscos);
    exit(0);
}