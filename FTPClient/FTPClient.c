#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>


int main(int argc,char *argv[])
{
    struct sockaddr_in server;
    struct stat obj;
    int sock;
    int choice;
    char buf[100], command[5], filename[20], *f;
    int k, size, status;
    int filehandle;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1)
    {
        printf("Creacion del socket fallida");
        exit(1);
    }
    server.sin_family = AF_INET;
    server.sin_port = atoi(argv[1]);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    k = connect(sock,(struct sockaddr*)&server, sizeof(server));
    if(k == -1)
    {
        printf("Error de conexion\n");
        perror("connect");
        exit(1);
    }
    int i = 1;
    while(1)
    {
        printf("Escoja un comando:\n1- get\n2- put\n3- pwd\n4- ls\n5- cd\n6- quit\n");
        scanf("%d", &choice);
        switch(choice)
        {
            case 1:
                printf("Introduzca el nombre del archivo: ");
                scanf("%s", filename);
                strcpy(buf, "get ");
                strcat(buf, filename);
                send(sock, buf, 100, 0);
                recv(sock, &size, sizeof(int), 0);
                if(!size)
                {
                    printf("No se encontro el archivo\n\n");
                    break;
                }
                f = malloc(size);
                recv(sock, f, size, 0);
                while(1)
                {
                    filehandle = open(filename, O_CREAT | O_EXCL | O_WRONLY, 0666);
                    if(filehandle == -1)
                    {
                        sprintf(filename + strlen(filename), "%d", i);//needed only if same directory is used for both server and client
                    }
                    else break;
                }
                write(filehandle, f, size, 0);
                close(filehandle);
                strcpy(buf, "cat ");
                strcat(buf, filename);
                system(buf);
                break;
            case 2:
                printf("Introduzca nombre del archivo: ");
                scanf("%s", filename);
                filehandle = open(filename, O_RDONLY);
                if(filehandle == -1)
                {
                    printf("No existe el archivo\n\n");
                    break;
                }
                strcpy(buf, "put ");
                strcat(buf, filename);
                send(sock, buf, 100, 0);
                stat(filename, &obj);
                size = obj.st_size;
                send(sock, &size, sizeof(int), 0);
                sendfile(sock, filehandle, NULL, size);
                recv(sock, &status, sizeof(int), 0);
                if(status)
                    printf("Archivo guardado con exito\n");
                else
                    printf("Error al guardar el archivo\n");
                break;
            case 3:
                strcpy(buf, "pwd");
                send(sock, buf, 100, 0);
                recv(sock, buf, 100, 0);
                printf("La direccion del directorio remoto es: %s\n", buf);
                break;
            case 4:
                strcpy(buf, "ls");
                send(sock, buf, 100, 0);
                recv(sock, &size, sizeof(int), 0);
                f = malloc(size);
                recv(sock, f, size, 0);
                filehandle = creat("temp.txt", O_WRONLY);
                write(filehandle, f, size, 0);
                close(filehandle);
                system("cat temp.txt");
                break;
            case 5:
                strcpy(buf, "cd ");
                printf("Introduzca la direccion del directorio: ");
                scanf("%s", buf + 3);
                send(sock, buf, 100, 0);
                recv(sock, &status, sizeof(int), 0);
                if(status)
                    printf("Directorio cambiado con exito\n");
                else
                    printf("Error al tratar de cambiar directorio\n");
                break;
            case 6:
                strcpy(buf, "quit");
                send(sock, buf, 100, 0);
                recv(sock, &status, 100, 0);
                if(status)
                {
                    printf("Servidor desconectado\nSaliendo...\n");
                    exit(0);
                }
                printf("Error al cerrar la conexion con el servidor\n");
        }
    }
}