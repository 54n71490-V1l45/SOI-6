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
#include <sys/types.h> 
#include <sys/wait.h>
#include <signal.h>

//Variable global para sincronización simple
volatile int señal_recibida = 0;

//Manejador de señal SIGUSR1
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
    if (fstat(fich1, &sb) == -1) { //Se averigua su longitud con fstat
        perror("Error en la función fstat()");
        close(fich1);
        exit(1);
    }

    //Proyección de memoria
    mapa= (char*)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fich1, 0);
    close(fich1); //Ya no necesitamos el descriptor
    if(mapa == MAP_FAILED){
        perror("Error en mmap\n");
        exit(1);
    }

    //Se calcula el tamaño final del buffer: cada dígito 'n' contribuye 'n' asteriscos, otros caracteres contribuyen 1
    long tam = 0;
    for (int i = 0; i < sb.st_size; i++) {
        if (isdigit((unsigned char)mapa[i])) {
            tam += mapa[i] - '0'; //número de asteriscos
        } else {
            tam += 1; //un carácter normal
        }
    }


    //Se crea un buffer compartido en memoria anónima para que padre e hijo puedan accederlo
    char *buffer= (char*)mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); //MAP_ANONYMOUS permite que no haya que crear otro archivo y mapearlo
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

    signal(SIGUSR1, manejador); //Se configura la señal

    //Se crea un proceso hijo, el Padre: maneja letras mayúsculas y el Hijo: maneja asteriscos
    //Sincronización con señales SIGUSR1.
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
        //Se espera señal del padre para procesar la primera mitad (asteriscos donde había dígitos).
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

        kill(getppid(), SIGUSR1); //Avisa al proceso padre para que continue
        while(!señal_recibida){ //Espera hasta que el proceso padre mande la señal que indica que ya finalizó con la segunda parte
            pause();
        }

        //Se procesa la segunda mitad
        for (int i = mitad; i < sb.st_size; i++) {
            if (isdigit((unsigned char)mapa[i])) {
                int n = mapa[i] - '0';
                for(int k=0; k<n; k++){
                    buffer[j+k] = '*';
                } 
                j += n;
            } else {
                j++; 
            }
        }

        exit(33);
    }else{
        //Proceso PADRE encargado de pasar a mayúsculas
        //Se procesa la primera mitad
        long j = 0;
        for (int i = 0; i < mitad; i++) {
            if (isdigit((unsigned char)mapa[i])) {
                j += mapa[i] - '0'; //Deja el hueco 
            } else {
                buffer[j] = toupper((unsigned char)mapa[i]); //Escribe letra
                j++;
            }
        }

        kill(hijo,SIGUSR1); //Se avisa de que ya finalizó la primera mitad al hijo
        //El padre se bloquea aquí hasta que el hijo le haga mande la señal para que pueda continuar
        while(!señal_recibida){
            pause();
        }

        //Se procesa la segunda mitad
        for (int i = mitad; i < sb.st_size; i++) {
             if (isdigit((unsigned char)mapa[i])) {
                j += (mapa[i] - '0'); //Deja el hueco
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
    if(write(fich2, resumen, strlen(resumen))==-1){ //Falle o no se tiene que hacer la limpieza el exit así que solo se imprime un mensaje informativo
        perror("Error escribiendo resumen");
    }

    //Limpieza
    munmap(mapa, sb.st_size);
    munmap(buffer, tam);
    munmap(mapa_salida, tam);
    close(fich2);
        
    printf("Proceso completado. Asteriscos: %d\n", asteriscos);
    exit(0);
}