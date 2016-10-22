#ifndef ESTATISTICAS
#define ESTATISTICAS

#include "defines.h"




typedef struct{
    //1=dynamic;2=static;3=rejected
	long m_type;

	char nomeScript[21];
	int indThread;
    time_t recebido;
    time_t terminado;

} mqueue;

typedef mqueue* estatistica;

time_t horaArranque;

int numReqsEstaticos;

int numReqsDinamicos;

int numReqsRecusados;

//create new statistic from a given request
estatistica criaEstatistica(request *req){
	estatistica res;
	res=(estatistica)malloc(sizeof(mqueue));
    res->m_type=req->tipo;
	res->indThread=req->indThread;
	res->recebido=req->t_inicio;
	res->terminado=req->t_fim;

	sprintf(res->nomeScript,"%s",req->nomeScript);

	return res;
}

//return a string with time obtained from time(NULL) with format HH:MM:SS
char *converteHora(time_t tempo){
	char *res;
	res=(char*)malloc(9*sizeof(char));
	res[0]='\0';

	int horas,minutos, segundos;
	horas = (tempo/3600);
	minutos = (tempo -(3600*horas))/60;
	segundos = (tempo -(3600*horas)-(minutos*60));
	horas=horas%24;
	sprintf(res,"%d:%d:%d", horas, minutos, segundos);
	return res;
}

//print info relating to server use when a SIGHUP signal is received.
void imprimeStats(int sig){
    char *horaAtual=converteHora(time(NULL));
    char *arr=converteHora(horaArranque);

    printf("server statistics:\n");
    printf("startup time:%s\n",arr);
    printf("current time:%s\n",horaAtual);
    printf("static requests so far:%d\n",numReqsEstaticos);
    printf("dynamic requests so far:%d\n",numReqsDinamicos);

}

//send a stats log via the mqueue.return -1 on error.
int enviaEstatistica(int idMQ,estatistica m){

	if(msgsnd(idMQ,m,sizeof(mqueue)-sizeof(long),0)==-1){
        perror("error sending statistic");
		return -1;
	}
	return 0;
}

//create logs.txt as an empty file.
void criaFichEsts(){
    FILE *f;

    f=fopen("logs.txt","w");

    if(f){
        fclose(f);
    }
    else{
        printf("error creating logs file\n");
        exit(1);
    }
}

//add a statistic to logs.txt
void guardaEstatistica(estatistica est){
    FILE *f;
    f=fopen("logs.txt","a");
    char *horaInicio,*horaFim;


    if(f){
        horaInicio=converteHora(est->recebido);
        horaFim=converteHora(est->terminado);
        fprintf(f,"type:%ld;name:%s;index thr:%d;h_reception:%s;h_end:%s\n",est->m_type,est->nomeScript,est->indThread,horaInicio,horaFim);
        free(horaFim);
        free(horaInicio);
        fclose(f);
    }
    else{
        printf("error opening logs.txt\n");
    }
}


//receive a stat log through the mqueue
int recebeEstatistica(int idMQ){
    estatistica buf=(estatistica)malloc(sizeof(mqueue));

	if(msgrcv(idMQ,buf,sizeof(mqueue)-sizeof(long),0,0)==-1){
            //if error is not due to the reception of a SIGHUP signal, we print it.
            if(errno!=EINTR)
                perror("error receiving stats");
            return -1;
	}
    printf("received by stats process:%s tipo:%ld\n",buf->nomeScript,buf->m_type);

	switch(buf->m_type){
        case(1):
            numReqsDinamicos++;
            break;
        case(2):
            numReqsEstaticos++;
            break;
        case(3):
            numReqsRecusados++;
            break;
    }
	guardaEstatistica(buf);
	free(buf);
	return 0;
}


//ends stats process when we receive a SIGINT
void catch_ctrlcProcEsts(int sig)
{
    printf("terminating stats process...\n");

    exit(0);
}


void verificadorEstatisticas(int id,sem_t *init){
    numReqsDinamicos=0;
    numReqsEstaticos=0;
    numReqsRecusados=0;

    signal(SIGINT,catch_ctrlcProcEsts);

    horaArranque=time(NULL);
    criaFichEsts();

    printf("stat process PID:%d\n",getpid());

    signal(SIGHUP,imprimeStats);

    sem_post(init);

    printf("stats process now running...\n");
    while(1){
        recebeEstatistica(id);
    }

}


#endif
