#ifndef FILA
#define FILA



/*Esta implementacao de fila(estrutura FIFO) recorrendoa lista ligada serve para qualquer tipo de dados pretendido.Para tal deve
editar o header "defines" consoante o tipo de dados a tratar.*/

typedef struct fnode *Fila;


typedef struct fnode{
	TIPO info;
	Fila proximo;
	int capacidade;
	int n_elementos;
}FNode;


//cria uma nova fila.
Fila criaFila(int capacidade){
	Fila res;
	res=(Fila) malloc(sizeof(FNode));
	res->proximo=NULL;
	res->capacidade=capacidade;
	res->n_elementos=0;
	return res;
}



//diz se a fila esta vazia.
int filaVazia(Fila f){
	return ((f->n_elementos)==0);
}

//diz se a fila esta cheia.
int filaCheia(Fila f){
	return ((f->n_elementos)>=(f->capacidade));
}



//executa um push na fila,ou seja,insere um elemento.devolve 0 caso a fila esteja cheia e 1 em caso de sucesso.
int push(Fila f,TIPO el){
	Fila aux;
	aux=f;

	if(filaCheia(f))
		return 0;

	while(aux->proximo!=NULL)
		aux=aux->proximo;
	aux->proximo=(Fila)malloc(sizeof(FNode));
	aux->proximo->info=el;
	aux->proximo->proximo=NULL;

	f->n_elementos++;

	return 1;

}


//retira o primeiro elemento da fila do tipo indicado e devolve o(no caso do projeto de SO o "tipo" representa o tipo de request).
TIPO pop(Fila f){
	TIPO res;
	Fila aux;
	if(filaVazia(f))
		return NULL;
	aux=f->proximo;
	res=aux->info;
	f->n_elementos--;
	f->proximo=f->proximo->proximo;

	free(aux);

	return res;
}

//altera a capacidade maxima da fila.
void mudaCapacidade(Fila f,int n){
	f->capacidade=n;
}



//devolve o primeiro elemento da fila,sem o retirar de la(usado para ver as datas dos requests sem os retirar da fila)
TIPO popSemRetirar(Fila f){

    if(filaVazia(f))
        return NULL;
    else
        return f->proximo->info;
}

//liberta a memoria ocupada pela fila
void destroiFila(Fila f){
	while(!filaVazia(f)){
		destroiTipo(pop(f));
	}

	free(f);
}

//imprime a fila no ecran.usada para debug.
void imprimeFila(Fila f){
	Fila aux;
	int n;

	n=f->n_elementos;
	aux=f->proximo;

	if(n==0){
		printf("a fila esta vazia\n");
		return;
	}

	while(n>0){
		printTipo(aux->info);
		aux=aux->proximo;
		n--;
	}

}




#endif
