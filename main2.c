/*
Autores: Adriana Sánchez-Bravo Cuesta y Santiago Vilas Pampín
*/

#include <stdio.h>      //Para printf, perror
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>      //Para open
#include <unistd.h>     //Para close, read
#include <sys/stat.h>   //Para fstat
#include <sys/mman.h>   //Para mmap, munmap

//si en mmap pones MAP_PUBLIC se modifican los dos, si pones MAP_PRIVATE solo se modifica la proyección

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Uso: ./programa <archivo>\n");
        return 1;
    }

    int fd;
    struct stat sb;
    char *mapa;
    int i;
    char c;
    char texto[50]= "Este archivo ha sido modificado\n";

    fd= open(argv[1], O_RDWR);

    //archivo
    if (fstat(fd, &sb) == -1) { //averigua su longitud con fstat
        perror("Error en fstat");
        close(fd);
        return 1;
    }

    //mapa
    mapa = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

    if (mapa == MAP_FAILED) { //chequeo de error
        perror("Error en mmap");
        close(fd);
        exit(1);
    }

    
    //modificacion en la proyeccion

    memcpy(mapa, texto, strlen(texto));

    //lecturas

    printf("--- Lectura con read ---\n");
    for (i = 0; i < sb.st_size; i++) {
        read(fd, &c, 1);
        printf("%c", c);
    }
    printf("\n");

    printf("--- Lectura con mmap ---\n");
    for (i = 0; i < sb.st_size; i++) {
        //Accedemos como si fuera un array
        printf("%c", mapa[i]);
    }
    printf("\n");


    //Se cierra el archivo e imprime proyección 
    close(fd); //Ya no necesitamos el descriptor

    //Cerrar proyección
    if (munmap(mapa, sb.st_size) == -1) {
        perror("Error en munmap");
        exit(1);
    }

    exit(0);
}

/* 4
#include <sys/mman.h>
int msync(void addr[.length], size_t length, int flags);
*/