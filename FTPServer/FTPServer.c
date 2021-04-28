#include<pthread.h>
#include<stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include<sys/stat.h>
#include<sys/sendfile.h>
#include<fcntl.h>
#include <arpa/inet.h>

#define maxlen 100000

int N,PORT;
char path[maxlen];
char ROOT[maxlen];
int listening, clientSocket, len;
struct sockaddr_in servaddr, cli;
pthread_t thread[maxlen];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct node{
    struct node* next;
    int *socket;
};

struct node* top = NULL;
struct node* base = NULL;

void push(int *socket){
    struct node *new_node = malloc(sizeof(struct node));
    new_node->socket = socket;
    new_node->next = NULL;
    if(base == NULL) top = new_node;
    else base->next = new_node;
    base = new_node;
}

int* pop(){

    if(top == NULL){
        return NULL;
    }
    int *result = top->socket;
    struct node *aux = top;
    top = top->next;
    if(top == NULL) base = NULL;
    free(aux);
    return result;


}
void error(char *s){
    printf("ERROR %s",s);
}

void* clientConnection(){

    while(1){
        pthread_mutex_lock(&lock);
        int *client = pop();
        pthread_mutex_unlock(&lock);

        if(client != NULL){
            //aceptamos la conexion

            char buffer[30000] = {0};
            char *args[3];
            int i, size, c;
            int filehandle;
            struct stat obj;
            char filename[20];

            long long valread = read( *((int*)client) , buffer, 30000);
            //tomamos el primer argumento

            args[0] = strtok(buffer," \t\n");

            //si es una peticion ls
            if(!strcmp(args[0], "ls"))
            {
                system("ls >temps.txt");
                i = 0;
                stat("temps.txt",&obj);
                size = obj.st_size;
                send(*((int*)client), &size, sizeof(int),0);
                filehandle = open("temps.txt", O_RDONLY);
                sendfile(*((int*)client),filehandle,NULL,size);
            }
            else if(!strcmp(args[0],"get"))
            {
                sscanf(buffer, "%s%s", filename, filename);
                stat(filename, &obj);
                filehandle = open(filename, O_RDONLY);
                size = obj.st_size;
                if(filehandle == -1)
                    size = 0;
                send(*((int*)client), &size, sizeof(int), 0);
                if(size)
                    sendfile(*((int*)client), filehandle, NULL, size);

            }
            else if(!strcmp(args[0], "put"))
            {
                int c = 0, len;
                char *f;
                sscanf(buffer+strlen(args[0]), "%s", filename);
                recv(*((int*)client), &size, sizeof(int), 0);
                i = 1;
                while(1)
                {
                    filehandle = open(filename, O_CREAT | O_EXCL | O_WRONLY, 0666);
                    if(filehandle == -1)
                    {
                        sprintf(filename + strlen(filename), "%d", i);
                    }
                    else
                        break;
                }
                f = malloc(size);
                recv(*((int*)client), f, size, 0);
                c = write(filehandle, f, size);
                close(filehandle);
                send(*((int*)client), &c, sizeof(int), 0);
            }
            else if(!strcmp(args[0], "pwd"))
            {
                system("pwd>temp.txt");
                i = 0;
                FILE*f = fopen("temp.txt","r");
                while(!feof(f))
                    buffer[i++] = fgetc(f);
                buffer[i-1] = '\0';
                fclose(f);
                send(*((int*)client), buffer, 100, 0);
            }
            else if(!strcmp(args[0], "cd"))
            {
                if(chdir(buffer+3) == 0)
                    c = 1;
                else
                    c = 0;
                send(*((int*)client), &c, sizeof(int), 0);
            }


            else if(!strcmp(args[0], "bye") || !strcmp(args[0], "quit"))
            {
                printf("FTP server quitting..\n");
                i = 1;
                send(*((int*)client), &i, sizeof(int), 0);
                exit(0);
            }
            close(*((int*)client));
        }

    }

}


int start_listening(){
    //empiezo a crear el sevidor

    //pre creo los hlilos
    for(int i = 0; i < N; i++){
        pthread_create(&thread[i],NULL,clientConnection, NULL);
    }


    //creo el socket de escucha
    listening = socket(AF_INET, SOCK_STREAM, 0);
    if (listening == -1){
        error("Creación del socket fallida...\n");
        return 1;
    }
    else
        printf("Socket creado correctamente..\n");

    //establezco la direccion y los puertos
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = PORT;

    // Enlace de socket recién creado a la IP y verificación // < 0
    if((bind(listening, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0){
        printf("Error de enlace...\n");
        exit(0);
    }

    // El sever está listo para escuchar y verificar
    if ((listen(listening, 5)) != 0) {
        printf("Escucha fallida...\n");
        exit(0);
    }
    printf("Escuchando..\n");

}

void waitConnection(){

    while(1){//loop infinito esperando las conexiones
        struct sockaddr_in addr;
        socklen_t addr_len;
        addr_len = sizeof(addr);

        printf("Esperando conexion\n");
        clientSocket = accept(listening, (struct sockaddr*)&addr, &addr_len);
        if (clientSocket < 0){
            printf("Aceptación fallida...\n");
            exit(0);
        }

        printf("Cliente aceptado e insertandolo en la cola\n");
        int *client_socket = malloc(sizeof(int));
        *client_socket = clientSocket; // le damos al cliente el socket que recibimos

        pthread_mutex_lock(&lock);//le doy prioridad a este proceso
        push(client_socket);
        pthread_mutex_unlock(&lock);
    }
}

int main(int argc, char *argv[]){


    //agarro los parametros del programa
    if(argc<7){
        error("Hacen falta parametros\n");
    }
    else if(argc>7){
        error("Tiene muchos parametros\n");
    }
    else{

        if(strcmp(argv[1],"-n")){
            error("Se necesita el parametro -n\n");
            return 1;
        }
        N = atoi(argv[2]);

        if(strcmp(argv[3],"-w")){
            error("Falta el parametro -w\n");
            return 1;
        }
        strcpy(ROOT,argv[4]);
        printf("Creando server con root en %s \n",ROOT);

        if(strcmp(argv[5],"-p")){
            error("Falta el parametro -p\n");
            return 1;
        }
        PORT = atoi(argv[6]);

        start_listening();
        waitConnection();


    }
}