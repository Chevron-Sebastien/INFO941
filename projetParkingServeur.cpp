#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "grovepi.h"
#include "grove_rgb_lcd.h"

using namespace GrovePi;

// Variable
char entree[1]; //entree utilisateur pour simuler l'action de l'automobiliste
int N = 2;      //nbre de places de parking
int fin = 1;    //pour couper toutes les threads
int fileAttente = 0; //passe à 1 si le parking est plein mais que qq veut rentrer
LCD lcd;

//Variable serveur
int mysocket, size, client_socket;

//initialisation
pthread_mutex_t protectN;
pthread_cond_t condition;

//initialisation jetons semaphores
int jetonEntree = 0;
int jetonSortie = 0;
int jetonAffichage = 1; //permet d'afficher le nbre de places restantes à l'ouverture
                        //du parking
//Inititalisation composant
int pinE = 7;		//bouton entree
int pinS = 8;		//bouton sortie
int pinLE = 3;		//LED entree
int pinLS = 4;		//LED sortie
int stateE;		//valBouton entree
int stateS;		//valBouton sortie

//variable heure fermeture du parking (en secondes)
int heureFermeture = 61;

//Init semaphore
sem_t sem_entree;
sem_t sem_sortie;
sem_t sem_clavier;
sem_t sem_affichage;

//Init structure gestion du temps
time_t t;
struct tm tm;

//tâche Clavier
void *tClavier( void *pt){
    //la tâche tourne tant que on ne souhaite pas de fin = 'f'
    while (entree[0] != 'f'){
	//entree[0] = getchar(); 	    //Saisie clavier bloquante
	delay(50);
	stateE = digitalRead(pinE); 	//Lecture bouton entree
	stateS = digitalRead(pinS);
      if (stateE==1){ 			    //si entrée demandee
	delay(20); 			            //Empeche les bugs (saut de bouton)
	while(digitalRead(pinE) !=0){}	//Attente front descendant bouton
            printf("\n system : Entree demandee\n");
            t = time(NULL);
            tm = *localtime(&t);	            //MAJ de l'heure
            if (tm.tm_sec <heureFermeture ) {   //Test de l'heure
                sem_post(&sem_entree);          //on rend possible le thread entree
            }
            else {
                sem_post(&sem_affichage);       //Semaphore Affichage
            }
      }
      else if (stateS == 1){		            //lecture bouton sortie
	delay(20);
	while(digitalRead(pinS)!=0){}
            printf("\n system: Sortie demandee\n");
            pthread_cond_signal(&condition);    //signal de sortie
            sem_post(&sem_sortie); 	            //on rend possible le thread sortie
        }
      else if (entree[0] == '\n'){          /*on vide le ENTREE que le getchar attrape*/}
      else if (entree[0] == 'f'){fin = 0;}
      else { 
	//printf("\n system: Commande inconnue\n"); //Commente sinon la console est spam de msg
 	}
    }
    pthread_exit(0);
}

//tâche Entree
void *tEntreeVoiture(void *pt){
    while (fin){
        sem_wait(&sem_entree);		    //recup le jeton du semaphore
        printf("\n system:lock mut\n");
        pthread_mutex_lock(&protectN); //lock N
        if(N>0){
            printf("\n system: Parking pas plein\n");
        }
        else
        {
            printf("\n system: Parking Plein, attente de place\n");
            pthread_cond_wait(&condition, &protectN); //Moniteur dans le cas ou parking plein
            printf("\n system: place libere\n");
        }
        N=N-1; 				            //on decremente le nbre de places
        pthread_mutex_unlock(&protectN);//unlock N
        sem_post(&sem_affichage);	    //on rend possible le thread Affichage
        printf("\n system:unlock mut\n");
    }
    pthread_exit(0);
}

//tâche Sortie
void *tSortieVoiture(void *pt){
    while (fin){
        sem_wait(&sem_sortie);          //on attend un jeton
        printf("\n system: Sortie voiture\n");
        pthread_mutex_lock(&protectN);  //on lock N
        printf("\n system: Sortie voiture2\n");
        N=N+1;                          //une place de plus dans le parking
        pthread_mutex_unlock(&protectN); //on unlock N
        sem_post(&sem_affichage);       //on rend disponible le thread affichage
    }
    pthread_exit(0);
}

//tâche Affichage
void *tAffichage(void *pt){
    while (fin){
        sem_wait(&sem_affichage);
        //on attend un jeton affichage et on affiche
	char buf[50];
        if (tm.tm_sec >heureFermeture) {//Si h>heureFermeture
        printf("\n -----------------");
        printf("\n Affichage : Parking ferme ");//P ferme
        printf("\n Affichage : Place dispo : ");
        printf("%d", N);
        printf("\n");
	sprintf(buf,"Parking ferme\nPlaces dispo %d",N);//creation chaine de caractere pour l'affichage
	}
        else{				    //Sinon parking ouvert
        if (N != -1)			//Parking pas plein
        {
        printf("\n -----------------");
        printf("\n Affichage : N = ");
        printf("%d", N);
        printf("\n");
	sprintf(buf,"Park ouvert\nPlaces dispo %d",N);
        }
        else				//parking plein - attente place
        {
        printf("\n -----------------");
        printf("\n Affichage : Liberation en cours");
        printf("\n");
        }}
	lcd.setText(buf);		//Affichage écran
    }
    pthread_exit(0);
}

//Gere les LEDS représentant l'ouverture/fermeture du parking
void *tHeure(void *pt){
	while (fin){
		t = time(NULL);
		tm = *localtime(&t);
		if (tm.tm_sec > 30){
			digitalWrite(pinLE,HIGH);
			digitalWrite(pinLS,LOW);
			//lcd.setRGB(0,150,150);
		}
		else{
			digitalWrite(pinLE,LOW);
			digitalWrite(pinLS,HIGH);
//			lcd.setRGB(150,0,0);
		}
		sem_post(&sem_affichage);//MAJ de l'écran
		delay(500);
	}
}


/////reception msg - coté serveur/////
void *tComMsg(void *pt){
while (fin){
        char msg[1];
        struct sockaddr_in client;
    size = sizeof(struct sockaddr_in);
    client_socket = accept(mysocket, (struct sockaddr *)&client, (socklen_t*)&size);
    if (client_socket < 0) {
      //printf("accept failed\n");
    }
else{
    printf("Requete accepté\n");            //demande de place parking recu
    recv(client_socket, msg, 1, 0);
    printf("Serveur received %c\n",msg[0]);
    if (N>0){                               //Test sur le nombre de place dispo
	sem_post(&sem_entree);                  //Si place dispo, on execute thread entrée
	write(client_socket,"e",1);             //Et on renvoi un msg comme quoi la requete est accepté
	}else{                                  //Sinon, parking plein
    write(client_socket,"f", 1);            //et donc on renvoi msg requete refusé
}
}}
}


///Creation serveur
//Identique au serveur crée dans le code précédent
int createServ(){
    struct sockaddr_in server;
    printf("CreationSever\n");
    mysocket = socket(AF_INET , SOCK_STREAM , 0);
    if (mysocket == -1) {
        printf("Error : socket creation\n");
        return -1;
    } else {printf("socket created\n");}

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(6666);
    if(bind(mysocket,(struct sockaddr *)&server , sizeof(server)) < 0) {
      printf("Error : bind failed\n");
      return -1;
    } else {printf("binded successfully\n");}

    listen(mysocket,3);
}

int main(int argc, char *argv[]){
	//GROVEPI
	initGrovePi();
	lcd.connect();
	lcd.setText("Init");
	//Configure les entree/sortie des boutons/LED
	pinMode(pinE,INPUT);
	pinMode(pinS,INPUT);
	pinMode(pinLE,OUTPUT);
	pinMode(pinLS,OUTPUT);
	digitalWrite(pinLE,HIGH);
	lcd.setRGB(150,150,0);
    t = time(NULL);
    tm = *localtime(&t);
    printf("now: %d:%d:%d\n", tm.tm_hour, tm.tm_min, tm.tm_sec);

	//Appel méthode creation serv
	createServ();

    //declaration variable
    pthread_t tC, tE, tS, tA, tH, tM;
    
    //initialisation mutex & semaphores
    pthread_mutex_init(&protectN, NULL);
    pthread_cond_init(&condition, NULL);

    sem_init(&sem_entree, 0, jetonEntree);
    sem_init(&sem_sortie, 0, jetonSortie);
    sem_init(&sem_affichage, 0, jetonAffichage);

    //creation des threads pour les taches
    pthread_create(&tC, NULL, tClavier, NULL);
    pthread_create(&tE, NULL, tEntreeVoiture, NULL);
    pthread_create(&tS, NULL, tSortieVoiture, NULL);
    pthread_create(&tA, NULL, tAffichage, NULL);
    pthread_create(&tH, NULL, tHeure, NULL);
    pthread_create(&tM, NULL, tComMsg, NULL);

    //Join des threads
    pthread_join(tC, NULL);
    pthread_join(tE, NULL);
    pthread_join(tS, NULL);
    pthread_join(tA, NULL);
    pthread_join(tH, NULL);
    pthread_join(tM, NULL);

    //destroy &co
    sem_destroy(&sem_entree);
    sem_destroy(&sem_sortie);
    sem_destroy(&sem_affichage);


    printf("\n Fin programme \n");
    return 0;
}
