#include <stdio.h>      //Para printf, perror
#include <fcntl.h>      //Para open
#include <unistd.h>     //Para close, read
#include <sys/stat.h>   //Para fstat
#include <sys/mman.h>   //Para mmap, munmap

int main(int argc, char *argv[]) {
    int fd;
    struct stat sb;
    char *mapa;
    char c;
    int i;

    //El nombre del archivo se pasa como argumento
    if (argc < 2) {
        printf("Uso: ./programa <archivo>\n");
        return 1;
    }

    //Se abre el archivo y obtiene el tamaño 
    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Error en open");
        return 1;
    }

    if (fstat(fd, &sb) == -1) { //averigua su longitud con fstat
        perror("Error en fstat");
        close(fd);
        return 1;
    }

    //Bucle 1: leer con read
    printf("--- Lectura con read ---\n");
    for (i = 0; i < sb.st_size; i++) {
        read(fd, &c, 1);
        printf("%c", c);
    }
    printf("\n");

    //Proyectar en memoria
    //NULL = el OS elige la dirección
    //sb.st_size = longitud del archivo
    //PROT_READ = solo lectura
    //MAP_PRIVATE = memoria privada
    //0 = desplazamiento inicial
    mapa = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (mapa == MAP_FAILED) { //chequeo de error
        perror("Error en mmap");
        close(fd);
        exit(1);
    }

    //Se cierra el archivo e imprime proyección 
    close(fd); //Ya no necesitamos el descriptor

    printf("--- Lectura con mmap ---\n");
    for (i = 0; i < sb.st_size; i++) {
        //Accedemos como si fuera un array
        printf("%c", mapa[i]);
    }
    printf("\n");

    //Cerrar proyección
    if (munmap(mapa, sb.st_size) == -1) {
        perror("Error en munmap");
        exit(1);
    }

    exit(0);
}