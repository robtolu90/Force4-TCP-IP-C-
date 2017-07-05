//-------------------------------------------------------------------------------------//
//-------------------Progetto Reti Informatiche 2015-----------------------------------//
//-------------------------------------------------------------------------------------//

//--------------------------------------LATO-------------------------------------------//
//-------------------------------------SERVER------------------------------------------//
//-------------------------------------------------------------------------------------//

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
#define SA struct sockaddr

#define INI_REQ		1	//rischiesta di inizializzazione di nome e porta		
#define INI_OK		2	//inizializzazzione andata a buon fine			
#define INI_REF		3	//iniz. fallita ripeti iniz. di nome e porta		
#define WHO_REQ 	4	//richiesta dei client connessi				
#define WHO_RESP	5	//risposta contenente gli utenti connessi		
#define CONN_REQ 	6	//richiesta di connessione a un'altro utente		
#define CONN_OK		7	//accettazione di connessione a un'altro client 	
#define CONN_REF 	8	//rifiuto di connessione a un'altro client		
#define INS		9	//insert column						
#define SM		10	//shiw mappa 						
#define DISCONN_REQ 	11	//rischiesta di disconnessione da un client		
#define ENEMY_DISC 	12	//disconnessione nemico 
#define ERR		13
#define CONN_OCC 	14	//rifiuto di connessione a un'altro client		
#define END_GAME 	15	//fine partita


#define HELP 1
#define CONN 2 
#define DISCONN 3
#define COMB 4
#define QUIT 5
#define WHO 6
#define SI 7
#define NO 8

#define SA struct sockaddr

//STATO DEL CLIENT
typedef enum{ LIBERO, OCCUPATO} state;
//SIMBOLO 
typedef enum{ CROCE, CERCHIO, VUOTO} simbol;

//descrittore serv socket e client socket
int servsk, clientsk;

int fdmax;
int sfidato = 0;
fd_set master;

//struttura messaggio di inizializzazione


struct init_msg{
    short port;
    char name[15];
    
    }; 
    
    
//struttura messaggio generale
struct header{
  unsigned char dim;
  unsigned char opcode;
}; 
    
struct messaggio{
	struct header head;
	char body[253];
};

char buffer_tcp[253];



//messaggio dati avversario
struct conn_msg{
	uint32_t addr;
	short port;
	char name[15];
};

//messaggio udp che comunica la mossa all'avversario
struct mossa{
	char my_move;
};
    

char my_name[15];
short my_port;
state my_state;
simbol my_simbol;
int my_turn; //1 true 0 false

//dati dell'avversario
struct sockaddr_in enemyaddr;
char enemyname[15];

//Matrice che rappresenta il gioco
simbol matrice[42];


//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------//
//---------------------FUNZIONI DI CONTROLLO----------------------------------//
//----------------------------------------------------------------------------//

void sendMsgToServ(unsigned char type, void *body, int size);
void sendMsgToServ_wb(unsigned char type);

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

//verifico correttezza porta
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

//verifico correttezza IP
int controlloIp(const char *indirizzo){
   int ret;
   uint32_t address; 
   ret = inet_pton(AF_INET, indirizzo, &address);
   if (ret < 1)
      return -1;
   return address;
}

//Verifico correttezza nome
int controllo_nome(const char* name){
   int res, len;
   if (name == NULL) return -1;
   res = regularExp("([^[:alnum:]]+)", name);
   len = strlen(name);
   return (!res && (len > 2) && (len<11)) ;
}

//------------------------------END-------------------------------------------//

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------//
//-------------------------------FUNZIONI_DI_UTILITA'-------------------------//
//----------------------------------------------------------------------------//

void showMenu(){
   printf("\nSono disponibili i seguenti comandi:\n");
   printf("* !help --> mostra l'elenco dei comandi disponibili \n");
   printf("* !who --> mostra l'elenco dei client connessi al server \n");
   printf("* !connect nome_client --> avvia una partita con l'utente nome_client \n");
   printf("* !disconnect --> disconnette il client dall'attuale partita intrapresa con un'altro client \n");
   printf("* !insert [a-g] --> permette al client di fare una mossa posizionando un gettone nella colonna indicata \n");
   printf("* !quit --> disconnette il client dal server \n");
}



//Inizializzazione struttura dati 
void init_game(){
  int i;
  sfidato = 0;
  for(i=0; i<42; i++) matrice[i] = VUOTO;
}

//Richiedo Nome
void name_quest(char* name){
	int res;
	do{
	   printf("\nInserisci nome (lunghezza min 3, max 10)\n");
	   scanf("%s", name);
	   name = strtok(name,"\n");
	   if((res = controllo_nome(name))<=0)
   	   	printf("Nome troppo lungo e/o caratteri illegali\n");
   	}while(res <= 0);
}

//Richiedo Porta
int port_quest(){
   char buf[10];
   int res;
   do{
      printf("\nInserisci porta\n");    
      scanf("%s", buf);
      if((res = controlloPorta(buf))<=0)
	printf("Porta invalida\n");
   }while(res <= 0);
   return res;
}
//-----------------------------------END-------------------------------------------//

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------//
//--------------------------FUNZIONI IN GAME---------------------------------------//
//---------------------------------------------------------------------------------//

int InsertInMat(int lettera, simbol simbolo){
  int indice, i;
    indice = lettera - 97;
  
  //Controllo se posso inserire
  if(matrice[indice] != VUOTO) return -1;
  
  for(i=1;i<6;i++){
    if(matrice[indice+i*7] != VUOTO) break;
  }
  matrice[indice+(i-1)*7] = simbolo;
 return 1;
}

int checkDX(int i){
  if(i%7>3) return 0;
  if(matrice[i] == matrice[i+1] &&
     matrice[i+1] == matrice[i+2] &&
     matrice[i+2] == matrice[i+3] &&
     matrice[i+3] != VUOTO)
	return 1;
  else return 0;
}

int checkUP(int i){
  if(i<20) return 0;
  if(matrice[i] == matrice[i-7] &&
     matrice[i-7] == matrice[i-14] &&
     matrice[i-14] == matrice[i-21] &&
     matrice[i-21] != VUOTO)
	 return  1;
  else return 0;
}

int checkDiag(int i){
  if(i%7>3 || i<20) return 0;
  if(matrice[i] == matrice[i-6] &&
     matrice[i-6] == matrice[i-12] &&
     matrice[i-12] == matrice[i-18] &&
     matrice[i-18] != VUOTO)
	return 1;
  else return 0;
}

int checkAntiDiag(int i){
 if(i%7<3 || i<20) return 0;
  if(matrice[i] == matrice[i-8] &&
     matrice[i-8] == matrice[i-16] &&
     matrice[i-16] == matrice[i-24] &&
     matrice[i-24] != VUOTO)
	return 1;
  else return 0;
}

int CheckWin(){
  int i;
  for(i=0; i<42; i++){
    if(checkDX(i) > 0 ||
	checkUP(i) > 0 ||
	 checkDiag(i) > 0 ||
	  checkAntiDiag(i) > 0) return 1;
  }
  return 0; //non hai vinto
}

int CheckPair(){
  int i;
  for(i = 0; i<7; i++)
      if(matrice[i]==VUOTO) return -1; //no parita
  return 1; //parita
}




//-----------------------------------END-------------------------------------------//

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------//
//--------------------------PROCESSO MESSAGGI--------------------------------------//
//---------------------------------------------------------------------------------//

void initServerConn();

int verify_yesORno(const char* choice){
   int res1,res2;
   if (choice == NULL) return -1;
   res1 = regularExp("^(yes|si|y|s)$", choice);
   res2 = regularExp("^(no|n)$", choice);
   if((res1 == res2))
	return -1;
   return res1;
}

void fine_partita(){
  init_game();
  my_state = LIBERO;
  sendMsgToServ_wb(END_GAME);
}


int demandYoN(){
   int res; char choice[10];
   do{
	printf("\ndigita \"si\" o \"no\"\n");
	scanf("%s",choice);
	if((res = verify_yesORno(choice))< 0)
		printf("Errore nella risposta...\n");
   } while(res < 0);
   return res;
}
//copio informazioni nemico
void copia_nemico(struct conn_msg* infoavv){
   memset(enemyname, 0, sizeof(enemyname));
   strcpy(enemyname, infoavv->name);
   enemyaddr.sin_family = AF_INET;
   enemyaddr.sin_port = infoavv->port;
   enemyaddr.sin_addr.s_addr = infoavv->addr;
}

//stampa mappa
void visualizza_mappa(){
  int i, j;
  for(i=0; i<6; i++){ //righe
    switch(i){
      case 0: printf("\n6 |"); break;
      case 1: printf("5 |"); break;
      case 2: printf("4 |"); break;
      case 3: printf("3 |"); break;
      case 4: printf("2 |"); break;
      case 5: printf("1 |"); break;
      
    }
    for(j=0; j<7; j++){
      switch(matrice[j+(i*7)]){
	case CROCE: printf("| X |"); break;
	case CERCHIO: printf("| O |"); break;
	case VUOTO: printf ("|   |"); break;
      }
      if(j==6) printf("\n");
    }
  }
  for(i=0;i<7;i++){
    switch(i){
      case 0: printf("   | a |"); break;
      case 1: printf("| b |"); break;                        
      case 2: printf("| c |"); break;
      case 3: printf("| d |"); break;
      case 4: printf("| e |"); break;
      case 5: printf("| f |"); break;
      case 6: printf("| g |\n"); break;
    }
  }
}

void insExec(int lettera, simbol simbolo){
  struct mossa miamossa;
  int nbytes;
  if(lettera < 97 || lettera > 103){
    printf("Hai inserito una lettera errata\n"); exit(1);
  }
  if(InsertInMat(lettera, simbolo) < 0){
    printf("Non puoi inserire in questa colonna\n"); exit(1);
  }
  //send UDP
  miamossa.my_move = lettera;
   if ((nbytes = sendto(clientsk, &miamossa, sizeof(struct mossa), 0, (SA *) &enemyaddr, sizeof(enemyaddr))) <= 0){
   	printf("Errore invio UDP\n"); 
   	exit(1);
   }
  
  //CheckWin
  if(CheckWin() > 0){
    printf("HAI VINTO!\n");
    visualizza_mappa();
    fine_partita();}
  else if(CheckPair() > 0){ 
    printf("La Partita è finita in parità!\n");
    visualizza_mappa();
    fine_partita();
  }
  else{ printf("Mossa inviata, aspetta la risposta!\n"); }
}




void processClientReq(struct mossa msgFromClient){
  simbol simbolo; 
   simbolo = (my_simbol == CERCHIO) ? CROCE : CERCHIO;
   char lettera = msgFromClient.my_move;
   InsertInMat(lettera, simbolo);
   my_turn = 1;
   
   if(CheckWin() > 0){
    printf("HAI PERSO!\n");
    visualizza_mappa();
    my_state = LIBERO;
    init_game();
  }
   else if(CheckPair() > 0){ 
    printf("La Partita è finita in parità!\n");
    visualizza_mappa();
    my_state = LIBERO;
    init_game();
  }
   else {visualizza_mappa(); printf("Ora tocca a te, fai la tua mossa\n");}
}


void processServerReq(struct header my_h, char * body){
   struct conn_msg *msg;
   switch(my_h.opcode){
	case(CONN_REQ):	//SFIDATO 
			msg = (struct conn_msg*) body;
		        my_state = OCCUPATO;
			printf("\n%s ti ha sfidato. Vuoi accettare ?\n digita 'si' oppure 'no'\n ", msg->name);
                        sfidato = 1;
                        copia_nemico((struct conn_msg *) body);
                        break;
                        
	
	case(CONN_REF):	printf("\nLa Sfida è stata rifiutata!\n");
			my_state = LIBERO;
			break; 
			
	case(CONN_OCC):	printf("\nL'avversario è occupato in un'altra partita!\n");
			putchar('>');
			my_state = LIBERO;
			break; 
			
	case(CONN_OK):	copia_nemico((struct conn_msg *) body);
			my_state = OCCUPATO;
			my_turn = 1; //è il mio turno
			printf("\n%s ha accettato la tua rischiesta, fai la tua mossa!\n", enemyname);
			my_simbol = CROCE;
			break; 

	case(INI_OK):	//inizializzazzione andata a buon fine
			printf("\nInizializzazione completata con successo\n");
			showMenu();
			putchar('>');
			break;

	case(INI_REF):	printf("\nInizializzazzione rifiutata: %s\n", body);
			initServerConn();
			break;  

	case(WHO_RESP): //risposta contenente gli utenti connessi
			printf("\nI client connessi sono:\n%s\n", body);
			break;

	case(ERR):	printf("\nErrore: L'avversario richiesto è inesistente\n");
			my_state = LIBERO;
			putchar('>');
			break; 
			
	case(ENEMY_DISC): //nemico disconnesso
			 my_state = LIBERO;
			 printf("HAI VINTO, il tuo avversario si e' ritirato\n");
			 sfidato = 0;
			 init_game();
			 break;
   }
}

int controllo_lettera(const char* arg){
  int res, len;
  if(arg == NULL) return -1;
  if(strcmp(arg,"") == 0) return -1;
  res = regularExp("/^[a-g]$/", arg);
  len = strlen(arg);
  return (!res && (len == 1)) ;
  
}


int processChoice(const char* cmd, const char* arg){
	if(cmd == NULL) return -1;
	if (strcmp("!help",cmd) == 0 && sfidato == 0)
		return HELP;
	if (strcmp("!connect", cmd)==0 && my_state == LIBERO && sfidato == 0)
		if (controllo_nome(arg) == 1) 
			return CONN;
	if (strcmp("!who", cmd) == 0 && my_state == LIBERO && sfidato == 0)
		return WHO;
	if (strcmp("!disconnect", cmd) == 0 && my_state == OCCUPATO && sfidato == 0)
		return DISCONN;
	if (strcmp("!insert", cmd) == 0 && my_state == OCCUPATO && sfidato == 0)
	        if(controllo_lettera(arg) == 1)
		  return INS;
	if (strcmp("!show_map", cmd) == 0 && my_state == OCCUPATO && sfidato == 0)
			return SM;
	if (strcmp("!quit", cmd) == 0 && sfidato == 0)
		return QUIT;
        if (strcmp("si", cmd) == 0 && sfidato == 1 && my_state == OCCUPATO)
                        return SI;
        if (strcmp("no", cmd) == 0 && sfidato == 1 && my_state == OCCUPATO)
                        return NO;
	return -1;
}

void executeChoice(int choice, const char* arg){
   int arg_int = 0;
   switch (choice){
	case WHO:	
			sendMsgToServ_wb(WHO_REQ);
			break;

	case HELP:	showMenu();
			break; 

	case SM:	printf("Ecco la mappa:\n");
			visualizza_mappa();
			break;
			
	case INS:	//Inserimento nuovo Gettone
			arg_int = (int) *arg;
			if(my_turn){
			    insExec(arg_int,my_simbol);    
			}
			else { printf("Non è il tuo turno, aspetta l'avversario..\n"); return;}
			my_turn = 0;
			break;

	case QUIT:	close(clientsk);
			close(servsk);
			exit(0); 

	case CONN:	
				if(strcmp(my_name, arg)==0) printf("\nNon puoi sfidare te stesso\n");
				else {
					my_state = OCCUPATO;
   					sendMsgToServ(CONN_REQ, (void *)arg, strlen(arg) + 1);
					printf("\nRischiesta di sfida inviata a %s\nAttendi la risposta dell'avversario\n", arg);
					
				}
			break; 

	case DISCONN:	if (my_state == LIBERO)
				printf("\nNon stai giocando nessuna partita\n");
   			else {
				sendMsgToServ_wb(DISCONN_REQ);
				printf("\nTi sei disconnesso dalla partita\n");}
			my_state = LIBERO;
			init_game();
			break; 
                        
        case SI:      
                        my_turn = 0;
                        my_simbol=CERCHIO;
                        sendMsgToServ_wb(CONN_OK);
                        printf("\nNon è il tuo turno attendi l'avversario\n");
                        sfidato = 0;
                        break;
        case NO:
                        sendMsgToServ_wb(CONN_REF);
                        my_state = LIBERO;
                        printf("\nHai rifiutato la sfida\n");
                        sfidato = 0;
                        break;
            
   }
}

void processInputReq(char *input){
   char *cmd;
   char *arg; 
   int choice;
   cmd = arg = NULL;
   
   cmd = strtok(input," \n");
   arg = strtok(NULL, " \n");
   if ((choice = processChoice(cmd, arg)) < 0)
	printf("\nComando non conosciuto e/o parametri errati\n"); 
   else
	executeChoice(choice, arg);
   memset(&input, 0, sizeof(input));
} 

//------------------------------------END-------------------------------------//

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------//
//--------------------COMUNICAZIONE CON SERVER--------------------------------//
//----------------------------------------------------------------------------//

//MESSAGGIO CON BODY
void sendMsgToServ(unsigned char type, void *body, int size){
   int nbytes;
   struct messaggio my_msg;
   memset(&my_msg, 0, sizeof(struct messaggio));
   my_msg.head.dim = (unsigned char) size; 
   my_msg.head.opcode = type;
   memcpy(my_msg.body, body, size);
   if((nbytes = send(servsk, &my_msg, my_msg.head.dim +  2 , 0)) <= 0) {
   	printf("error on send to client message of type: %d\n", type); 
   	return;
   }
}

//MESSAGGIO DI TIPO SEGNALE
void sendMsgToServ_wb(unsigned char type){
   int size = 0;
   int nbytes;
   struct messaggio my_msg;
   memset(&my_msg, 0, sizeof(struct messaggio));

   my_msg.head.dim = (unsigned char)size; 
   my_msg.head.opcode = type;
   if((nbytes = send(servsk, &my_msg, 2 , 0)) <= 0) {
   	printf("error on send to client message of type: %d\n", type); 
   	return;
   }
}

//messaggio di inizializzazione
void sendIniMsg(const char* name, const short port){
   struct init_msg ini;
   memset( &ini, 0, sizeof(ini));
   strcpy(ini.name, name);
   ini.port = htons(port);
   sendMsgToServ(INI_REQ, &ini, 2 + strlen(ini.name) + 1);
}
//------------------------------END-------------------------------------------//

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------//
//---------------------FUNZIONI_DI_INIZIALIZZAZIONE---------------------------//
//----------------------------------------------------------------------------//

void initServerConn(){
   memset(my_name, 0, sizeof(my_name));

   name_quest(my_name);
   my_port = port_quest(); 
   sendIniMsg(my_name, my_port);
}

void init(char* s_addr, char* s_port){
   struct sockaddr_in serveraddr, clientaddr;
   memset(&serveraddr, 0, sizeof(serveraddr));
   memset(&clientaddr, 0, sizeof(clientaddr));
	//connessione col server
   if((servsk = socket(AF_INET, SOCK_STREAM, 0)) < 0){
	perror("Error on creating server socket");
	exit(1);
   }
   serveraddr.sin_family = AF_INET;
   inet_pton(AF_INET, s_addr, &serveraddr.sin_addr.s_addr);
   serveraddr.sin_port = htons((short)atoi(s_port));
   if (connect(servsk, (SA *) &serveraddr, sizeof(serveraddr)) < 0){
	perror("Error on connecting server socket");
	exit(1);
   }
   FD_SET(servsk,&master);

	//inizializzazione di nome e porta 
   initServerConn();

	//apertuta del socket UDP
   if ((clientsk = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
	perror("Error on creating UDP socket");
	exit(1);
   }
   clientaddr.sin_family = AF_INET;
   clientaddr.sin_port = htons(my_port);
   clientaddr.sin_addr.s_addr = htonl(INADDR_ANY);
   if (bind(clientsk, (SA *) &clientaddr, sizeof(clientaddr)) < 0){
	perror("Error on binding UDP socket");
	exit(1);
   }
   FD_SET(clientsk, &master);
   fdmax = (clientsk > servsk)?clientsk:servsk;
}
//------------------------------END-------------------------------------------//

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------//
//---------------------------------MAIN---------------------------------------//
//----------------------------------------------------------------------------//




int main (int argc, char* argv[]) {
   int nbytes, i;
   char input[20];
   fd_set read_fds;
   struct sockaddr_in whoaddr;
   struct messaggio msgFromServ;
   socklen_t wholen;
   struct timeval tmout;
   struct mossa msgFromClient;
     struct header my_h;


   my_state = OCCUPATO;
   FD_ZERO(&master);
   FD_SET(0,&master);
   memset(&whoaddr , 0, sizeof(whoaddr));
   memset(&msgFromClient , 0, sizeof(msgFromClient));
   memset(&msgFromServ , 0, sizeof(msgFromServ));
   memset(&input, 0, sizeof(input));
   tmout.tv_sec = 60;
   tmout.tv_usec= 0;
   
   init_game();
   
   if (argc < 3){ printf("Troppi pochi argomenti\n"); exit(1);} 
   if (controlloIp(argv[1]) < 0){ printf("Indirizzo non corretto\n"); exit(1);}
   if (controlloPorta(argv[2]) < 0){ printf("Porta non corretta\n"); exit(1);}
   
   init(argv[1], argv[2]);
   
   my_state = LIBERO;
   while(getchar()!='\n');
   for(;;){
	read_fds = master;
	fflush(stdout);
	select(fdmax+1, &read_fds, NULL, NULL, &tmout);
	if((tmout.tv_sec==0)&&(tmout.tv_usec==0)){
	    if (my_state == OCCUPATO){
	    	my_state = LIBERO;
		sendMsgToServ_wb(DISCONN_REQ); //sistemare qui
	    	printf("\nSono trascorsi 60 secondi di inattività: Disconnessione dalla partita\n");
		init_game();
	    }
	}
	for(i = 0; i <= fdmax && (tmout.tv_sec!=0 || tmout.tv_usec!=0); i++){
	    if (FD_ISSET(i,&read_fds)){
		if (i == 0){ //da tastiera
			nbytes = read(0, input, 20);
			if ( nbytes < 0){
				perror("Error on reading from Keyboard");
				return 0;}
			if ( nbytes >= 20) {
				printf("Comando non conosciuto e/o parametri errati\n");
				while(getchar()!='\n');
				putchar('>');
				continue;
			}
			processInputReq(input);
		}
		else {
		    if(i == servsk){ //messaggio da server
		        if((nbytes = recv(servsk,&my_h, sizeof(my_h),0)) <= 0){
			  perror("Error receive from server sock");
			    return 0;
			}
			
			if(my_h.dim == 0) { processServerReq(my_h, NULL); continue; }  
			if ((nbytes = recv(servsk,&buffer_tcp, my_h.dim, 0)) <= 0){
			    perror("Error receive from server sock");
			    return 0;}
			processServerReq(my_h, buffer_tcp);
		    }
		    else{ //messaggio p2p da client
			wholen = sizeof(whoaddr);
			recvfrom(clientsk, &msgFromClient, sizeof(struct mossa),0 , (SA *)&whoaddr, &wholen);
			if ((whoaddr.sin_addr.s_addr == enemyaddr.sin_addr.s_addr) && (my_state == OCCUPATO))
			    processClientReq(msgFromClient);
		    }
		}
		putchar((my_state==OCCUPATO)?'#':'>');
	    }
	}
	tmout.tv_sec = 60;
   }
}

    
    
