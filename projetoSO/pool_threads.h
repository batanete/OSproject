
#ifndef POOLTHREADS
#define POOLTHREADS

//estrutura usada como argumento das threads neste projeto
typedef struct{
	//indice da thread na pool
	int indice;
	//array de ints que indica que threads estao livres ou ocupadas
	//NOTA:cada thread apenas pode alterar o seu proprio indice!
	int* ocupadas;
	//pipe usado para enviar requests a thread
	int fd[2];
}infothread_str;

typedef infothread_str* infothread;

//funcoes para criar e destruir estas estruturas
infothread criaArgsThread(int ind,int* ocupadas){
	infothread res;
	res=(infothread)malloc(sizeof(infothread_str));
	res->indice=ind;
	res->ocupadas=ocupadas;
	return res;
}

void destroiArgsThread(infothread it){
	free(it);
}

//esta estrutura ira ser usada para guardar dinamicamente as threads em execucao, a capacidade atual da pool e quais threads estao ocupadas naquele momento.
typedef struct{
	//array de threads
	pthread_t *threads;
	//tamanho da pool
	int capacidade;
	//as posicoes deste array que estiverem a 1 representam threads ocupadas com um request.as que estao a 0 representam as livres.
	int *ocupadas;

}pool_threads_str;


typedef pool_threads_str* pool_threads;

//cria uma pool de threads com a capacidade pretendida e o array de pipes dado
pool_threads criaPool(int capacidade){
	int i;
	pool_threads res;
	res=(pool_threads)malloc(sizeof(pool_threads_str));
	res->capacidade=capacidade;
	res->threads=(pthread_t*)malloc(capacidade*sizeof(pthread_t));
	res->ocupadas=(int*)malloc(capacidade*sizeof(int));


	for(i=0;i<capacidade;i++)
		res->ocupadas[i]=0;
	printf("pool de threads com capacidade %d criada.\n",capacidade);
	return res;
}

//inicia as threads da pool com a funcao dada e o array de args dado(se quisermos inicializa las sem argumentos,este parametro pode ser NULL)
//no caso do projeto estes argumentos irao ser pipes usados para "enviar" requests a cada thread,bem como informacoes sobre o seu indice e
//um ponteiro para o array ocupadas da estrutura
void iniciaPool(pool_threads pt,void*((f_ptr)(void*)),void *args[]){
	int i;

	if(args==NULL){
		for(i=0;i<pt->capacidade;i++)
			pthread_create(&(pt->threads[i]),NULL,f_ptr,NULL);
		return;
	}
	for(i=0;i<pt->capacidade;i++)
		pthread_create(&(pt->threads[i]),NULL,f_ptr,args[i]);
}

//liberta a memoria ocupada pela pool
void destroiPool(pool_threads pt){
	free(pt->threads);
	free(pt->ocupadas);
	free(pt);
}

//devolve o indice de uma thread que esteja livre(ou -1 se nenhuma estiver)
int procuraThreadLivre(pool_threads pt){
	int i;
	for(i=0;i<pt->capacidade;i++){
		if((pt->ocupadas[i])==0)
			return i;
	}
	return -1;
}


//aguarda que a thread com o indice dado termine a execucao
void esperaPorThread(pool_threads pt,int ind){
	pthread_join(pt->threads[ind],NULL);
}

//espera que todas as threads da pool acabem a execucao.deve ser usada antes de destruir a pool
void esperaPorPool(pool_threads pt){
	int i;
	for(i=0;i<pt->capacidade;i++)
        	pthread_join(pt->threads[i],NULL);
	
}

//diz se todas as threads da pool estao ocupadas
int poolCheia(pool_threads pt){
    int cap=pt->capacidade;
    int i;
    for(i=0;i<cap;i++){
        if((pt->ocupadas[i])==0)
            return 0;
    }
    return 1;
}


#endif
