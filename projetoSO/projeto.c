/*

 author: Joao Pedro Santos Batanete 
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>


#include "defines.h"
#include "pool_threads.h"
#include "filas.h"
#include "reqsDinamicos.h"
#include "VerificadorConfs.h"
#include "VerificadorEstatisticas.h"



// Produce debug information
#define DEBUG	  	1

// Header of HTTP reply to client
#define	SERVER_STRING 	"Server: simpleserver/0.1.0\r\n"
#define HEADER_1	"HTTP/1.0 200 OK\r\n"
#define HEADER_2	"Content-Type: text/html\r\n\r\n"

#define GET_EXPR	"GET /"
#define CGI_EXPR	"cgi-bin/"
#define SIZE_BUF	1024


int  fireup(int port);
void identify(int socket);
void get_request(int socket);
int  read_line(int socket, int n);
void send_header(request *req);
void send_page(request* req);
void execute_script(request* req);
void not_found(int socket);
void catch_ctrlcProcPrincipal(int);
void cannot_execute(int socket,int razaoDeNaoPoder);
void criaMP();
int tipoEscalonamento();
void criaFilhos();
void criaSemaforos();
void mapeiaMP();
void criaMQ();
void criaFilas();
void criaStrArgs();
void leConfigs();
void rebindSock();
void* trataPedido(void* args);

void destroiSemaforos();
void destroiMP();
void destroiMQ();
void destroiFilas();

void *escalonador(void *args);
void processaDin();
void processaEst();
void processaFifo();
void confsAlteradas(int sig);
void terminaProcessos(int ret);

//pids
pid_t pidconfs;
pid_t pidests;

//global vars
int defesa=0;
char buf[SIZE_BUF];
char req_buf[SIZE_BUF];
confs *configs;

int port,socket_conn,new_conn;



//threads
pool_threads poolThreads;
pthread_t escalonador_thr;
void **argsThreads;

//shared memory
confs *mpconfigs;
int shmidConfs;

//mem_queue
int idMQ;

//semaphores
sem_t *semMP;
sem_t *sem_filas;
sem_t *semMQ;
sem_t *semFilasVazias;
sem_t *seminit;
sem_t *sem_pool;


//queues
Fila filaDinamicos;
Fila filaEstaticos;

int main(int argc, char ** argv)
{
	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);
    configs=NULL;

    if(argc>=2){
        defesa=atoi(argv[1]);
    }

	criaMP();
	criaMQ();

	mapeiaMP();
	criaSemaforos();

    criaFilhos();

    //waits for the other two processes to initializ their structures
    sem_wait(seminit);

    leConfigs(0);

    poolThreads=criaPool(configs->n_threads);
    //
    criaStrArgs();

    criaFilas();
    iniciaPool(poolThreads,&trataPedido,argsThreads);

    signal(SIGINT,catch_ctrlcProcPrincipal);
    signal(SIGHUP,leConfigs);
    //port is obtained from shared memory
	port=configs->porto;

	// Configure listening port
	if ((socket_conn=fireup(port))==-1)
		terminaProcessos(1);
    printf("Listening for HTTP requests on port %d\n",port);
    pthread_create(&escalonador_thr,NULL,&escalonador,NULL);
    request *novo;

	// Serve requests
	while (1)
	{
		// Accept connection on socket
		if ( (new_conn = accept(socket_conn,(struct sockaddr *)&client_name,&client_name_len)) == -1 ) {
			printf("Error accepting connection\n");
			terminaProcessos(1);
		}
		// Identify new client
		identify(new_conn);

		// Process request
		get_request(new_conn);
        sem_wait(sem_filas);
        
		//pushes request into the correct queue
		if(!strncmp(req_buf,CGI_EXPR,strlen(CGI_EXPR))){
            if(filaCheia(filaDinamicos)){
                cannot_execute(new_conn,1);
            }
            else{
                novo=criaRequest(req_buf,1,new_conn);
                push(filaDinamicos,novo);
                sem_post(semFilasVazias);
            }
        }
		else{
            if(filaCheia(filaEstaticos)){
                cannot_execute(new_conn,1);
            }
            else{
                novo=criaRequest(req_buf,2,new_conn);
                push(filaEstaticos,novo);
                sem_post(semFilasVazias);
            }
        }
        sem_post(sem_filas);
	}
}

//rebinds socket if configs change
void rebindSock(){
    int port;
    port=configs->porto;
    close(socket_conn);
    if ((socket_conn=fireup(port))==-1)
        terminaProcessos(1);
    printf("Listening for HTTP requests on port %d\n",port);
}


//creates shared memory to use in configs
void criaMP(){
	shmidConfs=shmget(IPC_PRIVATE,1024,IPC_CREAT|0777);
	if(shmidConfs==-1){
		perror("erro ao criar a MP das confs");
		terminaProcessos(1);
	}
}

//creates arguments to pass for thread functions.
void criaStrArgs(){
    int i;
    int tam=configs->n_threads;
    argsThreads=(void**)malloc(tam*sizeof(void*));
    infothread aux;

    for(i=0;i<tam;i++){
        aux=criaArgsThread(i,poolThreads->ocupadas);
        pipe(aux->fd);
        argsThreads[i]=(void*)aux;
    }
}

//sends sigint to both subprocesses, so they can terminate.
//after that it ends program execution.
void terminaProcessos(int ret){
	
	kill(pidconfs,SIGINT);
	kill(pidests,SIGINT);
	while(wait(NULL)!=-1);
	exit(ret);
}

//sends a null pointer to threads, so they can close
void terminaThreads(){
    int i;
    infothread aux;
    int tam=configs->n_threads;
    request* r=NULL;
    printf("sending termination messages to thread pool\n");
    for(i=0;i<tam;i++){
        aux=(infothread)argsThreads[i];
        write(aux->fd[1],&r,sizeof(request*));
    }

}


//reads configs from the shared memory and rebinds server socket when they change
void leConfigs(int sig){
    int primeiraVez=0;
    if(configs==NULL){
        configs=(confs*)malloc(sizeof(confs));
        primeiraVez=1;
    }

    if(!primeiraVez){
	sem_close(sem_pool);
	terminaThreads();
        esperaPorPool(poolThreads);
        destroiPool(poolThreads);
        destroiFilas();
    }

    sem_wait(semMP);
    configs->alterada=1;
    configs->n_threads=mpconfigs->n_threads;
    configs->porto=mpconfigs->porto;
    configs->pol_escalonamento=mpconfigs->pol_escalonamento;
    configs->scripts_permitidos[0]='\0';
    sprintf(configs->scripts_permitidos,"%s",mpconfigs->scripts_permitidos);
    configs->alterada=0;
    sem_post(semMP);

    

    sem_unlink("SEMPOOL");
    sem_pool=sem_open("SEMPOOL",O_CREAT|O_EXCL,0700,configs->n_threads);

    if(!primeiraVez){
	
        criaFilas();
        poolThreads=criaPool(configs->n_threads);
        criaStrArgs();
        iniciaPool(poolThreads,&trataPedido,argsThreads);
        rebindSock(socket_conn);
        printf("configs applied.\n");
    }
}

//creates message queue
void criaMQ(){
    if ((idMQ=msgget(IPC_PRIVATE, 0777 | IPC_CREAT)) == -1){
		perror("Error creating message queue");
		exit(1);
	}
}


//maps shared memory block used for configs
void mapeiaMP(){
	mpconfigs=(confs*)shmat(shmidConfs,NULL,0);
	if(!mpconfigs){
		perror("error mapping shared memory for configs");
		exit(1);
	}
}

//initialize semaphores to use in program
void criaSemaforos(){
    sem_unlink("SEMMP");

    semMP=sem_open("SEMMP",O_CREAT|O_EXCL,0700,0);

    sem_unlink("SEMFILAS");

    sem_filas=sem_open("SEMFILAS",O_CREAT|O_EXCL,0700,1);

    sem_unlink("SEMMQ");


    semMQ=sem_open("SEMMQ",O_CREAT|O_EXCL,0700,1);

    sem_unlink("SEMVAZIAS");

    semFilasVazias=sem_open("SEMVAZIAS",O_CREAT|O_EXCL,0700,0);

    sem_unlink("SEMINIT");

    seminit=sem_open("SEMINIT",O_CREAT|O_EXCL,0700,2);

    //NOTE:the pool semaphore can only be initialized after we read the confs from the file
}

//creates the configs and stats processes
void criaFilhos(){
	pid_t pid;

	if((pid=fork())==0){
		verificadorConfs(mpconfigs,semMP,seminit);
		exit(0);
	}
	
	else if(pid==-1){
		perror("error creating configs process");
		exit(1);
	}
	pidconfs=pid;
	if((pid=fork())==0){
		verificadorEstatisticas(idMQ,seminit);
		exit(0);
	}
	
	else if(pid==-1){
		perror("error creating configs process");
		kill(pidconfs,SIGINT);
		exit(1);
	}
	pidests=pid;
	printf("subprocesses created\n");
}

//create the request queues with capacity equal to double that of the pool of threads
void criaFilas(){
    int capacidade;
    capacidade=2*(configs->n_threads);
    filaDinamicos=criaFila(capacidade);
    filaEstaticos=criaFila(capacidade);
    printf("request queues created\n");
}

//returns the current scheduling policy(FIFO,dynamic,static)
int tipoEscalonamento(){
    int res;
    res=configs->pol_escalonamento;
	return res;
}

//scheduling thread. works according to the current scheduling policy
void* escalonador(void *args) {
    int prioridade;

    if(defesa!=0){
        printf("scheduling thread starting in %d seconds.\n",defesa);
        sleep(defesa);
    }
    printf("scheduling thread online.\n");
    while(1){
        //este wait evita esperas ativas na thread
        sem_wait(semFilasVazias);
        sem_wait(sem_filas);
	prioridade=tipoEscalonamento();
        switch(prioridade){
            case 1:
                processaFifo();
                break;
            case 2:
                processaEst();
                break;
            case 3:
                processaDin();
                break;
            default:
                printf("error on scheduling function\n");
                exit(1);
        }
        sem_post(sem_filas);

    }

	pthread_exit(NULL);
}

//the pool's threads use this function to run.
void* trataPedido(void* args){
    infothread info=(infothread)args;
    request* buf=NULL;
    estatistica est;

    while(1){
        //if configs changed, we must change pool
                read(info->fd[0],&buf,sizeof(request*));
                //exit thread when we receive a NULL pointer
                if(buf==NULL){
	            printf("thread %d ashutting down...\n",info->indice);
                    pthread_exit(NULL);
                }
                printf("request %s being attented to by thread number %d\n",buf->nomeScript,info->indice);
                buf->indThread=info->indice;
                if(buf->tipo==1)
                    execute_script(buf);
                else if(buf->tipo==2){
                    send_page(buf);
                }
                close(buf->sock);
                buf->t_fim=time(NULL);
                est=criaEstatistica(buf);
                sem_wait(semMQ);
                enviaEstatistica(idMQ,est);
                sem_post(semMQ);
		printf("request %s terminated by thread number %d\n",buf->nomeScript,info->indice);
                free(est);
                free(buf);
                info->ocupadas[info->indice]=0;
                sem_post(sem_pool);
    }
}

//process requests, in FIFO order.
void processaFifo(){
    int threadLivre;
    infothread aux;
    request* maisVelho;
    //we wait until there is a free thread on the pool
    sem_wait(sem_pool);

    request* r1=popSemRetirar(filaDinamicos);
    request* r2=popSemRetirar(filaEstaticos);

    maisVelho=reqMaisVelho(r1,r2);

    //pop the request from it's queue
    if(maisVelho==r1)
        maisVelho=pop(filaDinamicos);
    else
        maisVelho=pop(filaEstaticos);


    threadLivre=procuraThreadLivre(poolThreads);
    poolThreads->ocupadas[threadLivre]=1;
    aux=(infothread)(argsThreads[threadLivre]);

    write(aux->fd[1],&maisVelho,sizeof(request*));
}

//process requests, giving priority to static ones.
void processaEst(){
    int threadLivre;
    infothread aux;
    request* req;

    sem_wait(sem_pool);
    //first of all, we check if the priority queue is empty. in that case, go to the other.
    if(!filaVazia(filaEstaticos))
        req=pop(filaEstaticos);
    else
        req=pop(filaDinamicos);

    
    threadLivre=procuraThreadLivre(poolThreads);
    poolThreads->ocupadas[threadLivre]=1;
    aux=(infothread)(argsThreads[threadLivre]);

    //the request address is sent to thread
    write(aux->fd[1],&req,sizeof(request*));
}

//process requests, giving priority to dynamic ones.
void processaDin(){
    int threadLivre;
    infothread aux;
    request* req;

    sem_wait(sem_pool);
    //first of all, we check if the priority queue is empty. in that case, go to the other.
    if(!filaVazia(filaDinamicos))
        req=pop(filaDinamicos);
    else
        req=pop(filaEstaticos);

    
    threadLivre=procuraThreadLivre(poolThreads);
    poolThreads->ocupadas[threadLivre]=1;
    aux=(infothread)(argsThreads[threadLivre]);

    //the request address is sent to thread
    write(aux->fd[1],&req,sizeof(request*));
}

// Processes request from client
void get_request(int socket)
{
	int i,j;
	int found_get;

	found_get=0;
	while ( read_line(socket,SIZE_BUF) > 0 ) {
		if(!strncmp(buf,GET_EXPR,strlen(GET_EXPR))) {
			// GET received, extract the requested page/script
			found_get=1;
			i=strlen(GET_EXPR);
			j=0;
			while( (buf[i]!=' ') && (buf[i]!='\0') )
				req_buf[j++]=buf[i++];
			req_buf[j]='\0';
		}
	}
	// Currently only supports GET
	if(!found_get) {
		printf("Request from client without a GET\n");
		//printf("buf:%s\n",buf);
		terminaProcessos(1);
	}
	printf("buf:%s\n",buf);
	printf("reqbuf:%s\n",req_buf);
	// If no particular page is requested then we consider htdocs/index.html
	if(!strlen(req_buf))
		sprintf(req_buf,"index.html");

	#if DEBUG
	printf("get_request: client requested the following page: %s\n",req_buf);
	#endif

	return;
}


// Send message header (before html page) to client
void send_header(request* req)
{
    char buffer[SIZE_BUF];

	#if DEBUG
	printf("send_header: sending HTTP header to client\n");
	#endif
	sprintf(buffer,HEADER_1);
	send(req->sock,buffer,strlen(HEADER_1),0);
	sprintf(buffer,SERVER_STRING);
	send(req->sock,buffer,strlen(SERVER_STRING),0);
	sprintf(buffer,HEADER_2);
	send(req->sock,buffer,strlen(HEADER_2),0);
	return;
}

//return 1 if requested script can be run
int validaRequestCGI(char *nomeScript){
    char delims1[2]=";";
    char delims2[2]=" ";
    char buf1[21];
    char buf2[51];

    strcpy(buf1,nomeScript);

    sem_wait(semMP);
    strcpy(buf2,mpconfigs->scripts_permitidos);
    sem_post(semMP);

    char *tok1=strtok(buf1,delims1);
	char* tok2=strtok(buf2,delims2);

    while(tok2!=NULL){
        if(strcmp(tok1,tok2)==0)
            return 1;

        tok2=strtok(NULL,delims2);
    }
    return 0;
}

// Execute script in /cgi-bin
void execute_script(request *req)
{
    char *s=(req->nomeScript)+strlen(CGI_EXPR);

    if(!validaRequestCGI(s)){
        req->tipo=3;
        cannot_execute(req->sock,0);
        return;
    }

	char *res=reqDinamico(s);
	if(res==NULL){
        res=(char*) malloc(51*sizeof(char));
        res[0]='\0';
		sprintf(res,"%s","the specified file doesn't exist\n");
	}

	send_header(req);
	if(send(req->sock,res,strlen(res),0)==-1){
        perror("error sending response to a dynamic request");
	}

	free(res);

	return;
}


// Send html page to client
void send_page(request *req)
{
	FILE * fp;

    char buf_tmp[51];


	// Searchs for page in directory htdocs
	sprintf(buf_tmp,"htdocs/%s",req->nomeScript);

	#if DEBUG
	printf("send_page: searching for %s\n",buf_tmp);
	#endif

	// Verifies if file exists
	if((fp=fopen(buf_tmp,"rt"))==NULL) {
		// Page not found, send error to client
		printf("send_page: page %s not found, alerting client\n",buf_tmp);
		not_found(req->sock);
	}
	else {
		// Page found, send to client

		// First send HTTP header back to client

		send_header(req);
		printf("send_page: sending page %s to client\n",buf_tmp);
		while(fgets(buf_tmp,51,fp))
			send(req->sock,buf_tmp,strlen(buf_tmp),0);
		// Close file
		fclose(fp);
	}
	return;
}


// Identifies client (address and port) from socket
void identify(int socket)
{
	char ipstr[INET6_ADDRSTRLEN];
	socklen_t len;
	struct sockaddr_in *s;
	int port;
	struct sockaddr_storage addr;

	len = sizeof addr;
	getpeername(socket, (struct sockaddr*)&addr, &len);

	// Assuming only IPv4
	s = (struct sockaddr_in *)&addr;
	port = ntohs(s->sin_port);
	inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

	printf("identify: received new request from %s port %d\n",ipstr,port);

	return;
}


// Reads a line (of at most 'n' bytes) from socket
int read_line(int socket,int n)
{
	int n_read;
	int not_eol;
	int ret;
	char new_char;

	n_read=0;
	not_eol=1;


	while (n_read<n && not_eol) {
		ret = read(socket,&new_char,sizeof(char));
		if (ret == -1) {
			printf("Error reading from socket (read_line)");
			return -1;
		}
		else if (ret == 0) {
			return 0;
		}
		else if (new_char=='\r') {
			not_eol = 0;
			// consumes next byte on buffer (LF)
			read(socket,&new_char,sizeof(char));
			continue;
		}
		else {
			buf[n_read]=new_char;
			n_read++;
		}
	}



	#if DEBUG
	printf("read_line: new line read from client socket: %s\n",buf);
	#endif

	return n_read;
}


// Creates, prepares and returns new socket
int fireup(int port)
{
	int new_sock;
	struct sockaddr_in name;


	int iSetOption = 1;

	// Creates socket
	if ((new_sock = socket(PF_INET, SOCK_STREAM, 0))==-1) {
		printf("Error creating socket\n");
		return -1;
	}

    //para fazer unbind quando a socket e fechada
	setsockopt(new_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&iSetOption,
        sizeof(iSetOption));

	// Binds new socket to listening port
 	name.sin_family = AF_INET;
 	name.sin_port = htons(port);
 	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(new_sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		printf("Error binding to socket\n");
		return -1;
	}

	// Starts listening on socket
 	if (listen(new_sock, 5) < 0) {
		printf("Error listening to socket\n");
		return -1;
	}

	return(new_sock);
}


// Sends a 404 not found status message to client (page not found)
void not_found(int socket)
{
 	sprintf(buf,"HTTP/1.0 404 NOT FOUND\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,SERVER_STRING);
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"Content-Type: text/html\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<HTML><TITLE>Not Found</TITLE>\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<BODY><P>Resource unavailable or nonexistent.\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"</BODY></HTML>\r\n");
	send(socket,buf, strlen(buf), 0);

	return;
}


// Send a 5000 internal server error (script not configured for execution or server is overloaded)
void cannot_execute(int socket,int reason)
{
    char buffer[SIZE_BUF];
	sprintf(buffer,"HTTP/1.0 500 Internal Server Error\r\n");
	send(socket,buffer, strlen(buffer), 0);
	sprintf(buffer,"Content-type: text/html\r\n");
	send(socket,buffer, strlen(buffer), 0);
	sprintf(buffer,"\r\n");
	send(socket,buffer, strlen(buffer), 0);
	if(!reason)
        sprintf(buffer,"<P>Error!Prohibited CGI execution.\r\n");
    else
        sprintf(buffer,"<P>Error! Server is currently overloaded.Try again later\r\n");
	send(socket,buffer, strlen(buffer), 0);
    close(socket);
	return;
}

//releases memory taken by the semaphores
void destroiSemaforos(){
    sem_close(semFilasVazias);
    sem_close(sem_filas);
    sem_close(semMP);
    sem_close(semMQ);
    sem_close(seminit);
    sem_close(sem_pool);
    printf("semaphores destroyed\n");
}

//release memory taken by both queues, after closing sockets still in use.
void destroiFilas(){
    request* auxReq;
    Fila auxFila;

    for(auxFila=filaDinamicos->proximo;auxFila!=NULL;auxFila=auxFila->proximo){
	auxReq=auxFila->info;
        close(auxReq->sock);
    }  

    for(auxFila=filaEstaticos->proximo;auxFila!=NULL;auxFila=auxFila->proximo){
	auxReq=auxFila->info;
        close(auxReq->sock);
    }   

    destroiFila(filaDinamicos);
    destroiFila(filaEstaticos);
    printf("queues destroyed\n");
}

//release memory taken by shared memory block
void destroiMP(){
    shmdt(mpconfigs);
    shmctl(shmidConfs, IPC_RMID, NULL);
    printf("shared memory destroyed\n");
}

//release memory taken by message queue
void destroiMQ(){
    msgctl(idMQ,IPC_RMID,NULL);
    printf("message queue destroyed\n");
}

//closes socket and releases memory taken by the program.
void catch_ctrlcProcPrincipal(int sig)
{
    	terminaThreads();
	esperaPorPool(poolThreads);
	//waits for all child processes to terminate
    	while(wait(NULL)!=-1);
	printf("subprocesses terminated.\n");
    	destroiMP();
    	destroiMQ();
    	destroiSemaforos();
    	destroiFilas();
    	close(socket_conn);
	printf("Server shutdown\n");
	exit(0);
}
