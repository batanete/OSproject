#ifndef DEFINES
#define DEFINES

typedef struct{
    //1=dynamic;2=static;3=rejected
	int tipo;
        int sock;
	char nomeScript[21];
	//time obtained using time(NULL)
	time_t t_inicio;
	time_t t_fim;
	//index from thread that attended the request.
	int indThread;
}request;

#define TIPO request*

//create new request
request *criaRequest(char *nomeScript,int tipo,int sock){
    request *res;

    res=(request*)malloc(sizeof(request));

    res->t_inicio=time(NULL);
    res->t_fim=0;
    res->tipo=tipo;
    res->sock=sock;
    sprintf(res->nomeScript,"%s",nomeScript);

    return res;
}

//set's request to be attended by thread index ind
void atribuiThread(request *req,int ind){
    req->indThread=ind;
}

//returns 1 if request has already been processed/rejected
int reqEstaProcessado(request* req){
    return (req->t_fim>0);
}

//returns the oldest request from the two given.
//in case one is NULL returns the other one.if both are, it returns NULL(error)
request *reqMaisVelho(request* r1,request* r2){
    if((r1==NULL)&&(r2==NULL))
        return NULL;
    else if(r2==NULL)
        return r1;
    else if(r1==NULL)
        return r2;

    if((r1->t_inicio)<=(r2->t_inicio))
        return r1;
    else
        return r2;
}




void printTipo(TIPO t){
	if(t->tipo==1)
		printf("request type:estatico\n");
	else
		printf("request type:dinamico\n");

	printf("script requested:%s\n",t->nomeScript);

	printf("starting instant:%lld\n",(long long)t->t_inicio);
	printf("ending instant:%lld\n",(long long)t->t_fim);

}

void destroiTipo(TIPO t){
	free(t);
}

#endif

