//----------------------------------------------------------------------//
//----Progetto Reti Informatiche 2015-----------------------------------//
//----------------------------------------------------------------------//


//----------------------SERVER------------------------------------------//


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <regex.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <regex.h>


#define BUFLEN 256 //Lunghezza buffer nome
#define MAXPENDING 10


#define INI_REQ		1	//rischiesta di inizializzazione di nome e porta		
#define INI_OK		2	//inizializzazzione andata a buon fine			
#define INI_REF		3	//iniz. fallita ripeti iniz. di nome e porta		
#define WHO_REQ 	4	//richiesta dei client connessi				
#define WHO_RESP	5	//risposta contenente gli utenti connessi		
#define CONN_REQ 	6	//richiesta di connessione a un'altro utente		
#define CONN_OK		7	//accettazione di connessione a un'altro client 	
#define CONN_REF 	8	//rifiuto di connessione a un'altro client		
#define CONN_OCC 	14	//rifiuto di connessione a un'altro client		
#define INS		9	//insert column						
#define SM		10	//shiw mappa 						
#define DISCONN_REQ 	11	//rischiesta di disconnessione da un client		
#define ENEMY_DISC 	12	//disconnessione nemico 
#define ERR		13
#define END_GAME 	15	//fine partita


#define HELP 1
#define CONN 2 
#define DISCONN 3
#define COMB 4
#define QUIT 5
#define WHO 6


typedef enum{ LIBERO, OCCUPATO} state;

char bufferIN_TCP[BUFLEN]; //buffer in ingresso tcp
char bufferOUT_TCP[BUFLEN]; //buffer in uscita tcp

//parametri del server
struct sockaddr_in sock_cfg;

//Struttura messaggio fra C/S
struct header{
  unsigned char dim;
  char opcode;
}; 
    
struct messaggio{
	struct header head;
	char body[253];
};

char buffer_tcp[253];


struct init_msg{
    short port;
    char name[15]; 
    }; 

struct conn_msg{
	uint32_t addr;	//indirizzo ip dello sfidante
	short port;	//porta di ascolto dello sfidante
	char name[15];	//nome dello sfidante
};


//elementi lista client connessi
struct cl{
    char name[BUFLEN];          //nome del client
    struct sockaddr_in cli_cfg; //parametri client
    short port;                 //UDP del client
    int status;                  //impegnato o meno in una partita
    int socket;                  //socket associata
    struct cl* enemy;            //puntatore all'avversario
    struct cl* next;             //puntatore a elemento successivo
    };
    
struct cl* mylist;

//select
    fd_set master, read_fds;     
    int fdsmax;

//---------------------------SEND RECEIVE-------------------------------------//

//MESSAGGIO CON BODY
void sendMsg(unsigned char type,  void *body, int size, const struct cl* cli){
   int nbytes;
   struct messaggio my_msg;
   memset(&my_msg, 0, sizeof(struct messaggio));
   my_msg.head.dim = (unsigned char)size;
   my_msg.head.opcode = type;
   memcpy(my_msg.body, body, size);
   if( (nbytes = send(cli->socket, &my_msg, (size_t)my_msg.head.dim + 2, 0) <= 0) ) {
   	printf("error on send to client message of type: %d\n", type); 
   	return;
   }
}

//MESSAGGIO DI TIPO SEGNALE
void sendMsg_wb(unsigned char type, const struct cl* cli){
   int nbytes;
   int size = 0;
   struct messaggio my_msg;
   memset(&my_msg, 0, sizeof(struct messaggio));
   my_msg.head.dim = (unsigned char)size; 
   my_msg.head.opcode = type;
   if( (nbytes = send(cli->socket, &my_msg, 2, 0) <= 0) ) {
   	printf("error on send to client message of type: %d\n", type); 
   	return;
   }
}

//---------------------------------------------------------------------------//



//---------------------FUNZIONI DI CONTROLLO----------------------------------//
int regularExp(const char* regexpr, const char* str){
   regex_t preg;
   if (regcomp(&preg, regexpr, REG_EXTENDED|REG_NOSUB)){
	printf("Error on compile regex\n");
	return -1;
   }
   if (regexec(&preg, str, 0, 0, 0) == 0)
	return 1;
   return 0;
}

int controlloPorta(const char *porta){
   int port, res, len;
   if (porta == NULL) return -1;
   res = regularExp("([^[:digit:]]+)", porta);
   len = strlen(porta);
   port = atoi(porta);
   if ((port < 0) || (port > 0xFFFF))
	return -1;
   if(res || (len>5))
	return 0;
   return port;
}

int controlloIp(const char *indirizzo){
   int ret;
   uint32_t address; 
   ret = inet_pton(AF_INET, indirizzo, &address);
   if (ret < 1)
      return -1;
   return address;
}
//------------------------------END-------------------------------------------//



//---------------FUNZIONI DI UTILITA' PER LE LISTE----------------------------//

/* Inserisce in testa a "clients" un nuovo client */
void ins_client(int sock, const struct sockaddr_in* client){ 
   struct cl* newclient;
   newclient = (struct cl *) malloc(sizeof(struct cl));
   memset(newclient, 0, sizeof(newclient));
   newclient->socket = sock;
   newclient->cli_cfg.sin_addr.s_addr = client->sin_addr.s_addr;	//addr salvato in formato network;
   newclient->status = OCCUPATO;		//OCCUPATO fino a quando non inizializza nome e porta
   newclient->next = mylist;
   mylist = newclient;
}

/* Trova Client dato il suo socket */
struct cl* find_client_by_name(const char *name){
   struct cl *s;
   for(s = mylist; s!=NULL && (strcmp(name,s->name)!=0); s = s->next);
   return s;
}

/* Trova Client dato il suo nome */
struct cl* find_client_by_sock(int sock){
   struct cl *s;
   for(s = mylist; s!=NULL && s->socket!=sock; s=s->next);
   return s;
}

void rm_client(int sock){
   struct cl *r, **p;
   for(p = &mylist; (*p)!=NULL && (*p)->socket!=sock; p = &((*p)->next));
   if ((*p)==NULL) return;
   r = *p;
   *p = (*p)->next;
   free(r);
}
//------------------------------END-------------------------------------------//

//---------------------------ESECUZIONE COMANDI DA CLIENT-----------------------//

void sendConnMsg(unsigned char type, const struct cl *sender, const struct cl *receiver){
	struct conn_msg msg;
	memset(&msg, 0, sizeof(msg));
	strcpy(msg.name, sender->name);
	msg.addr = sender->cli_cfg.sin_addr.s_addr;	//era salvato in formato network, niente conversione
	msg.port = sender->port;	//era salvato in formato network, niente conversione
	sendMsg(type, &msg, 6 + strlen(msg.name) + 1, receiver); 
}


void WhoExec(const struct cl *cli){
	struct cl* p;
	char msg[512];
	memset(&msg, 0, sizeof(msg));
	msg[0] = '\0';
	for(p = mylist; p!= NULL; p = p->next){
		if(cli->socket == p->socket) continue;
		strcat(msg, p->name);
		if(p->status == OCCUPATO) strcat(msg,"\t[OCCUPATO]");
		strcat(msg, "\n");
	}
	sendMsg(WHO_RESP, msg, strlen(msg) + 1, cli);
}

void ConnExec(struct cl* cli, struct cl* avv){
	cli->status = OCCUPATO;
	if (avv!=NULL){
		printf("%s ha sfidato %s\n", cli->name, avv->name);
		if(avv->status == LIBERO){ //se avversario è libero
		        avv->status = OCCUPATO;
			cli->enemy = avv;
			avv->enemy= cli;
			sendConnMsg(CONN_REQ, cli, avv);
		}
		else{
			printf("%s è occupato in un'altra partita\n", avv->name);
			cli->status = LIBERO;
			sendMsg_wb(CONN_OCC, cli);
		}
	}
	else{
		printf("%s ha richiesto un'avversario inesistente\n", cli->name);
		cli->status = LIBERO;
		sendMsg_wb(ERR, cli);
	}
}


//----------------------------------------------------------------------------//



//-----------------------GESTIONE MESSAGGI CLIENT-----------------------------//
void IniExec(struct cl* cli, const struct init_msg* ini){
	if (!find_client_by_name(ini->name)){
		strcpy(cli->name, ini->name);
		cli->port = ini->port;	//viene salvato in formato network
		cli->status = LIBERO;
		sendMsg_wb(INI_OK,cli);
		printf("Il client su socket %d ha nome = %s e porta UDP = %d\tIniT_OK\n",cli->socket, ini->name, ntohs(ini->port));
	}
	else{
		sendMsg_wb(INI_REF, cli);
		printf("Il client sul socket %d ha scelto un nome già occupato = %s\tIniT_RefuseD\n", cli->socket, ini->name);
	}
}




void processClientMsg(struct header my_h, char* body, int sock){
   struct cl *cli, *avv;
   struct init_msg *ini;
   cli = find_client_by_sock(sock);
   
   switch(my_h.opcode){
	
	case INI_REQ: 	
			ini = (struct init_msg *) body;
			IniExec(cli, ini); 	
                        break;
        

   	case WHO_REQ :	
			WhoExec(cli);
			break;
	
	case CONN_REQ : //richiedi sfida
			avv = find_client_by_name(body);
			ConnExec(cli, avv);
			break;

	case CONN_OK:	//accetta sfida
			avv = cli->enemy;
			cli->status = OCCUPATO;
			avv->status = OCCUPATO;
			sendConnMsg(CONN_OK, cli, avv);
			printf("%s ha accettato la sfida di %s\n", cli->name, avv->name);
			printf("%s è occupato\n%s è occupato\n", cli->name, avv->name);
			break; 

	case CONN_REF: 	
			avv = cli->enemy;
			cli->status = LIBERO;
			avv->status = LIBERO;
			sendMsg_wb(CONN_REF, avv);
			printf("%s ha rifiutato la sfida di %s\n", cli->name, avv->name);
			break; 
			
	case DISCONN_REQ:
			if(cli->status == OCCUPATO){
			  cli->status = LIBERO;
			  if(cli->enemy->status == OCCUPATO){
			      cli->enemy->status = LIBERO;
			      sendMsg_wb(ENEMY_DISC, cli->enemy);
			      printf("%s si è disconnesso dalla partita\n", cli->enemy->name);
			  }
			}
			printf("%s si è disconnesso dalla partita\n", cli->name);
			printf("%s è libero\n", cli->name);
			cli->enemy = NULL;
			break;
			
	case END_GAME:  cli->status = LIBERO;
			cli->enemy->status = LIBERO;
			printf("La partita fra %s e %s è finita\n", cli->name, cli->enemy->name);
			printf("%s e %s sono liberi\n", cli->name, cli->enemy->name);
			cli->enemy = NULL;
			break;
                        
   }
}


//----------------------------------------------------------------------------//
//---------------------------------MAIN---------------------------------------//
//----------------------------------------------------------------------------//

int main(int argc, char* argv[]){
    int ret, i, listener, addrlen, conn_sk, nbytes;
    fd_set master, read_fds;
    struct header my_h;
    struct sockaddr_in clientaddr;
    struct cl* client;


    //controllo paramentri
    if(argc!=3 || controlloIp(argv[1])==-1 || controlloPorta(argv[2])==-1){
        printf("Errore nei parametri");
        exit(-1);
        }
        
        
    //configurazione del socket di ascolto
    memset(&sock_cfg,0,sizeof(sock_cfg));        //azzero paramentri del server
    memset(&clientaddr, 0, sizeof(clientaddr));

    sock_cfg.sin_family=AF_INET;
    sock_cfg.sin_port=htons((short)controlloPorta(argv[2])); //network ordered
    inet_pton(AF_INET,argv[1],(void*)&sock_cfg.sin_addr.s_addr); //configurazione indirizzo IP
    printf("SERVER STARTED\n");
    //Inizializzazione Socket
    listener=socket(AF_INET,SOCK_STREAM,0);
    if(listener==-1){
            printf("socket(); failed!\n");
            exit(-1);
            }
    ret=bind(listener, (struct sockaddr*)&sock_cfg, sizeof(sock_cfg));  
    if(ret==-1){
            printf("bind(); failed!\n");
            exit(-1);
            }     
    ret=listen(listener, MAXPENDING);
    if(ret==-1){
            printf("listen(); failed!\n");
            exit(-1);
            }  
    
    //Set_INI
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(listener, &master);
    fdsmax=listener;
    
    printf("Server is ON\n");
    for(;;){
        read_fds = master;
        if(select(fdsmax+1,&read_fds,NULL,NULL,NULL)==-1){
            printf("select() failed");
            exit(-1);
            }
        //un socket è pronto
        for(i = 0; i<= fdsmax; i++){
            if(FD_ISSET(i,&read_fds)){
                if(i==listener){ //NUOVA CONNESSIONE (E' IL SOCKET DI ASCOLTO)
                    addrlen = sizeof(clientaddr);
                    if ((conn_sk = accept(listener, (struct sockaddr *) &clientaddr, (socklen_t *) &addrlen)) < 0){
                            perror("Accept() failed\n");
                            exit(1);
                            }
                    else{ //se ho una nuova connessione
                            ins_client(conn_sk, &clientaddr); //inerisco nella lista del server il nuovo client
                            printf("Un client si è connesso socket = %d\n", conn_sk);
                            FD_SET(conn_sk, &master);
                            if (conn_sk > fdsmax)
                                    fdsmax = conn_sk;
				}
                    }
                
                else { //BLOCCO DI GESTIONE COMANDI CLIENT
			  if(( nbytes = recv(i,&my_h, sizeof(my_h),0)) <= 0){
				    if (nbytes == 0)
						      printf("Un client si è disconnesso socket = %d\n",i);
					      else
						      printf("Errore nella connessione %d, la connessione sara' chiusa \n",i);
					      client = find_client_by_sock(i);
				              if(client->enemy != NULL){
						    if(client->enemy->status == OCCUPATO)
							{sendMsg_wb(ENEMY_DISC, client->enemy);
							  client->enemy->status = LIBERO;
							  printf("La partita fra %s e %s è finita\n", client->name, client->enemy->name);
							  printf("%s si è disconnesso\n", client->name);
							}
					      }
					      rm_client(i);
					      FD_CLR(i, &master);
					      close(i);
					      continue;
			  }
			  if(my_h.dim == 0)  processClientMsg(my_h, NULL ,i);
			  else{
				if ((nbytes = recv(i, &buffer_tcp, my_h.dim, 0)) <= 0){
					printf("Errore nella connessione %d, la connessione sara' chiusa \n",i);
					rm_client(i);
					FD_CLR(i, &master);
					close(i);
				      }
				      else{
					      processClientMsg(my_h, buffer_tcp ,i);
				      }
			  }
			} //blocco comando client 
        }// if isset
    }//for scorro
}//for infinito

}
