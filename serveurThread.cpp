#include<stdio.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<pthread.h>

int mysocket,size,client_socket;      //Variable globale


//Création du serveur
int createServ(){
  struct sockaddr_in server;
  printf("Création du serveur\n");
  mysocket = socket(AF_INET , SOCK_STREAM , 0);    //Initialisation socket
  if (mysocket == -1) {
    printf("Error : socket creation\n");
    return -1;
  } else {printf("socket created\n");}

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(6666);
  if(bind(mysocket,(struct sockaddr *)&server , sizeof(server)) < 0) {  //bind de la socket
    printf("Error : bind failed\n");
    return -1;
  } else {printf("binded successfully\n");}

	listen(mysocket,3);   //ecoute de la socket
}

////////Thread de communication/////////////////////////////
//Quand un msg est en entrée, il est lu, modifié et renvoyer 
void *tComMsg(void *pt){
while (1){    
	char msg[1];
	struct sockaddr_in client;
  size = sizeof(struct sockaddr_in);
  client_socket = accept(mysocket, (struct sockaddr *)&client, (socklen_t*)&size);  //Msg en entrée
  if (client_socket < 0) {
    //printf("accept failed\n");
  } else {
	printf("accepted\n");
  recv(client_socket, msg, 1, 0);
  printf("Serveur received %c\n",msg[0]);
  //Envoie d'un message en retour
  sleep(3);   //delay d'attente pour réaliser la q2
  msg[0]=msg[0]+1;
  write(client_socket, msg, 1); //On renvoi 1 caractère
  }
}
}

int main(int argc , char *argv[]) {
//THREAD
pthread_t tC;

//Initialisaiton serveur & thread
createServ();
pthread_create(&tC, NULL, tComMsg, NULL);
pthread_join(tC, NULL);

return 0;

}

