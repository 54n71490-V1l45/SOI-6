#include <stdio.h>      //Para printf, perror
#include <stdlib.h>
#include <string.h>
#include <ctype.h>      //touper
#include <fcntl.h>      //Para open
#include <unistd.h>     //Para close, read
#include <sys/stat.h>   //Para fstat
#include <sys/mman.h>   //Para mmap, munmap

void convertir_letras(char *buff){ //para convertir a mayusculas
    int k=0;
    for (int i = 0; buff[i]; i++){
        if(isalpha((unsigned char)buff[i])){
            buff[i] = toupper((unsigned char) buff[i]);
        }
        else if(isdigit((unsigned char)buff[i])){
            int n= buff[i] - '0';
            for (int j=0;j<n;j++){
                buff[i]=' ';
            }
        }
        else{
            buff[k++]= buff[i];
        }
    }
    buff[k]='\0';
}
void convertir_num(char*buff){
    int k=0;
    for (int i = 0; buff[i]; i++){
        if(isdigit((unsigned char)buff[i])){
                int n= buff[i] - '0';
                for (int j=0;j<n;j++){
                    buff[i]='*';
                }
            }
        else{
            buff[k++]= buff[i];
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: ./programa <archivo entrada> <archivo salida>\n");
        return 1;
    }

    int fich1, fich2;
    struct stat sb;
    char *mapa, *buffer;
    int i;
    char c;

    fich1= open(argv[1], O_RDONLY);
    if(fich1==-1){
        perror("Error en open");
        return 1;
    }
    if (fstat(fich1, &sb) == -1) { //averigua su longitud con fstat
        perror("Error en fstat");
        close(fich1);
        return 1;
    }
    //proyeccion de memoria
    mapa= mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fich1, 0);

    //buffer
    int tamaño = 0;
    for (int i = 0; mapa[i]; i++) {
        if (isdigit((unsigned char)mapa[i])) {
            tamaño += mapa[i] - '0'; // número de asteriscos
        } else {
            tamaño += 1; // un carácter normal
        }
    }
    char *buffer= malloc(tamaño+1);

    int hijo= fork();

    if(hijo==0){
        convertir_num(buffer);
    }
    else{
        convertir_letras(buffer);
        waitpid(hijo);

        mremap(mapa, sb.st_size, strlen(buffer), 0);
        if (mapa == MAP_FAILED) {
            perror("Error en mremap\n");
            free(buffer);
            close(fich1);
            exit(1);
        }

    }

    

}