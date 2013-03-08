#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>

#define SEM_LOGIN 12345

int ownMSG, server_id;
char name[MAX_NAME_SIZE];
int sem_login_id;

static struct sembuf buf;

SemOperation(int semid, int semnum, int x){
  buf.sem_num = semnum;
  buf.sem_op = x;
  buf.sem_flg = 0;
  if (semop(semid, &buf, 1) == -1){
    perror("Semaphore operation");
    exit(1);
  }
}

int SemLoginInit(){
  int id = semget(SEM_LOGIN, 1, 0600 | IPC_CREAT | IPC_EXCL);
  if(id == -1)
    id = semget(SEM_LOGIN, 1, 0600 | IPC_CREAT);
  else
    semctl(id, 0, SETVAL, 1);
  return id;
}

int OwnMSGInit(){
  return msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
}
  
SERVER_LIST_RESPONSE  GetServerList(){
  int i;
  SERVER_LIST_REQUEST request;
  request.type = 1;
  request.client_msgid = ownMSG;
  int id = msgget(SERVER_LIST_MSG_KEY, 0600 | IPC_CREAT);
  msgsnd(id, &request, sizeof(ownMSG), 0);
  SERVER_LIST_RESPONSE response;
  msgrcv(ownMSG, &response, sizeof(response) - sizeof(long), 1, 0);
  //for(i=0; i<response.active_servers; i++)
    //printf("%d\n",response.servers[i]);
  return response;
}

int Login(){
  printf("Login...\n");
  CLIENT_REQUEST request;
  request.type = 5;
  request.client_msgid = ownMSG;
  char name[MAX_NAME_SIZE];
  printf("your login:\n");
  fgets(name, MAX_NAME_SIZE, stdin);
  strcpy(request.client_name, name);
  SemOperation(sem_login_id, 0, -1);
  msgsnd(server_id, &request, sizeof(request) - sizeof(long), 0);
  STATUS_RESPONSE response;
  msgrcv(ownMSG, &response, sizeof(response) - sizeof(long), STATUS, 0);
  SemOperation(sem_login_id, 0, 1);
  return response.status;
}

Logout(){
  CLIENT_REQUEST req;
  req.type = LOGOUT;
  req.client_msgid = ownMSG;
  strcpy(req.client_name, name);
  msgsnd(server_id, &req, sizeof(req) - sizeof(long), IPC_NOWAIT);
}

Heartbeat(){
  STATUS_RESPONSE response;
  msgrcv(ownMSG, &response, sizeof(response) - sizeof(long), HEARTBEAT, 0);
  CLIENT_REQUEST request;
  request.type = HEARTBEAT;
  request.client_msgid = ownMSG;
  strcpy(request.client_name, name);
  msgsnd(server_id, &request, sizeof(request) - sizeof(long), 0);
}

int ChangeRoom(){
  CHANGE_ROOM_REQUEST req;
  req.type = CHANGE_ROOM;
  req.client_msgid = ownMSG;
  strcpy(req.client_name, name);
  printf("new room name:\n");
  char room[MAX_NAME_SIZE];
  fgets(room, MAX_NAME_SIZE, stdin);
  strcpy(req.room_name, room);
  msgsnd(server_id, &req, sizeof(req) - sizeof(long), 0);
  STATUS_RESPONSE res;
  msgrcv(ownMSG, &res, sizeof(res) - sizeof(long), CHANGE_ROOM, 0);
  return res.status;
}

ROOM_LIST_RESPONSE* GetRoomList(){
  CLIENT_REQUEST req;
  req.type = ROOM_LIST;
  req.client_msgid = ownMSG;
  strcpy(req.client_name, name);
  msgsnd(server_id, &req, sizeof(req) - sizeof(long),  0);
  ROOM_LIST_RESPONSE res;
  msgrcv(ownMSG, &res, sizeof(res) - sizeof(long), ROOM_LIST, 0);
  
  return &res;
}
CLIENT_LIST_RESPONSE* GetClientList(){
  CLIENT_REQUEST req;
  req.type = ROOM_CLIENT_LIST;
  req.client_msgid = ownMSG;
  strcpy(req.client_name, name);
  msgsnd(server_id, &req, sizeof(req) - sizeof(long), 0);
  CLIENT_LIST_RESPONSE res;
  msgrcv(ownMSG, &res, sizeof(res) - sizeof(long), ROOM_CLIENT_LIST, 0);
  return &res;
}

CLIENT_LIST_RESPONSE* GetGlobalClientList(){
  CLIENT_REQUEST req;
  req.type = GLOBAL_CLIENT_LIST;
  req.client_msgid = ownMSG;
  strcpy(req.client_name, name);
  msgsnd(server_id, &req, sizeof(req) - sizeof(long), 0);
  CLIENT_LIST_RESPONSE res;
  msgrcv(ownMSG, &res, sizeof(res) - sizeof(long), GLOBAL_CLIENT_LIST, 0);
  return &res;
}

PrivateText(){
  TEXT_MESSAGE word;
  word.type = PRIVATE;
  word.from_id = ownMSG;
  strcpy(word.from_name, name);
  char receiver[MAX_NAME_SIZE];
  printf("write receiver's name:\n");
  gets(receiver);
  strcpy(word.to, receiver);
  char your_text[MAX_MSG_SIZE];
  printf("write your private message:\n");
  fgets(your_text, MAX_MSG_SIZE, NULL);
  strcpy(word.text, your_text);
  msgsnd(server_id, &word, sizeof(word), 0);
}

PublicText(){
  TEXT_MESSAGE word;
  word.type = PUBLIC;
  word.from_id = ownMSG;
  strcpy(word.from_name, name);
  char receiver[MAX_NAME_SIZE];
  printf("write receiver's name:\n");
  gets(receiver);
  strcpy(word.to, receiver);
  char your_text[MAX_MSG_SIZE];
  printf("write your public message:\n");
  fgets(your_text, MAX_MSG_SIZE, NULL);
  strcpy(word.text, your_text);
  msgsnd(server_id, &word, sizeof(word), 0);
}

ReceiveText(){
  TEXT_MESSAGE word;
  msgrcv(ownMSG, &word, sizeof(word) - sizeof(long), PUBLIC | PRIVATE, 0);
  printf("%s", word.from_name);
  if(word.type == PUBLIC)
    printf("[public]");
  else
    printf("[private]");
  printf("  %d\n", word.time);
  printf("%s\n", word.text); 
}		



  
int main(){
  
  int i;
  ownMSG = OwnMSGInit();
  SERVER_LIST_RESPONSE res = GetServerList();
  
  server_id = res.servers[0];
  sem_login_id = SemLoginInit();
  
  int login_res, change_res;
  while((login_res = Login()) != 201)
    if(login_res == 503){
      printf("server is too busy now...\n");
      sleep(5);
    }
    else if(login_res == 400)
	printf("your name is uncorrect\n");
      else if(login_res == 409)
	  printf("this name already exists\n");
  printf("Login successful !\n");/*
  printf("If you first time use this program it's reccomended to try /help\n");
  int pid[2];
  if(fork() == 0){
    pid[0] = getpid();
    while(true)
      Heartbeat();
  }
  if(fork() == 0){
    pid[1] = getpid
    while(true)
      ReceiveText();
  }
  char line[MAX_MSG_SIZE];
  while(true){
    fgets(line,MAX_MSG_SIZE, NULL);
    if(strcmp(line, "/help") == 0)
      printf("/users../gusurs../priv../logout../rooms../change..\n");
    else if(strcmp(line, "/users") == 0){
	CLIENT_LIST_RESPONSE* res = GetClientList();
	for(i=0; i<res->active_clients; i++)
	  printf("%s\n", res->names[i]);
      }
      else if(strcmp(line, "/gusers") == 0){
	  CLIENT_LIST_RESPONSE* res = GetGlobalClientList();
	  for(i=0; i<res->active_clients; i++)
	    printf("%s\n", res->names[i]);
	}
	else if(strcmp(line, "/priv") == 0)
	    PrivateText();
	  else if(strcmp(line, "/logout") == 0){
	      logout();
	      kill(pid[0], 9); kill(pid[1], 9);
	    }
	    else if(strcmp(line, "/rooms") == 0){
		ROOM_LIST_RESPONSE* res GetRoomList();
		for(i=0; i<res->active_rooms; i++)
		  printf("%s\n", res->rooms[i]);
	      }
	      else if((strcmp(line, "/change") == 0){
		  while(ChangeRoom() != 202)
		    printf("invalid room name\n");
		  printf("room change successful"\n);
		}
		else
		  PublicText();
  }
  */
  return 0;
}
