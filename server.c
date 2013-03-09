#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>

int ownMSG, servers_id, servers_list_id, sem_rep_id, sem_log_id, shared_id;
static struct sembuf buf;

InitMSGs(){
  ownMSG = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
  servers_id = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
  servers_list_id = msgget(SERVER_LIST_MSG_KEY, 0600 | IPC_CREAT);
  printf("%d \n", ownMSG);
}

SemOperation(int semid, int semnum, int x){
  buf.sem_num = semnum;
  buf.sem_op = x;
  buf.sem_flg = 0;
  if (semop(semid, &buf, 1) == -1){
    perror("Semaphore operation");
    exit(1);
  }
}

SortServers(REPO *addr){
  int i, j, value, value_server, value_clients;
 
    for(i = 1; i < (*addr).active_servers; i++)
    {
        value = (*addr).servers[i].client_msgid;
	value_server = (*addr).servers[i].server_msgid;
	value_clients = (*addr).servers[i].clients;
        for (j = i - 1; j >= 0 && (*addr).servers[j].client_msgid > value; j--)
        {
            (*addr).servers[j + 1].client_msgid = (*addr).servers[j].client_msgid;
	    (*addr).servers[j + 1].server_msgid = (*addr).servers[j].server_msgid;
	    (*addr).servers[j + 1].clients = (*addr).servers[j].clients;
        }
        (*addr).servers[j + 1].client_msgid = value;
	(*addr).servers[j + 1].server_msgid = value_server;
	(*addr).servers[j + 1].clients = value_clients;
	
    }
}

SortClients(REPO *addr){
  int i, j, server_id;
  char value[MAX_NAME_SIZE];
  char room[MAX_NAME_SIZE];
 
    for(i = 1; i < (*addr).active_clients; i++)
    {
        strcpy(value, (*addr).clients[i].name);
	strcpy(room, (*addr).clients[i].room);
	server_id = (*addr).clients[i].server_id;
        for (j = i - 1; j >= 0 && ((strcmp((*addr).clients[j].name, value)) > 0); j--)
        {
            strcpy((*addr).clients[j + 1].name, (*addr).clients[j].name);
	    strcpy((*addr).clients[j + 1].room, (*addr).clients[j].room);
	    (*addr).clients[j+1].server_id = (*addr).clients[j].server_id;
        }
        strcpy((*addr).clients[j + 1].name, value);
	strcpy((*addr).clients[j + 1].room, room);
	(*addr).clients[j + 1].server_id = server_id;
	
    }
}

Register(){
  REPO *addr;
  int x;
  int id = semget(SEM_REPO, 1, 0666 | IPC_CREAT | IPC_EXCL);
  
  //printf("%d\n", id);
  if(id == -1){ 
    sem_rep_id = semget(SEM_REPO, 1, 0666 );
    semctl(sem_rep_id, 0, GETVAL, x);
    SemOperation(sem_rep_id, 0, -1);
    shared_id = shmget(ID_REPO, sizeof(REPO), 0666);
    sem_log_id = semget(SEM_LOG, 1, 0666);
    addr = (REPO*)shmat(shared_id, NULL, 0);
    (*addr).active_servers = (*addr).active_servers + 1; 
    (*addr).servers[(*addr).active_servers - 1].client_msgid = ownMSG; 
    (*addr).servers[(*addr).active_servers - 1].server_msgid = servers_id; 
    (*addr).servers[(*addr).active_servers - 1].clients = 0; 
    SortServers(addr); printf("x1\n");
    shmdt(addr);  printf("x1\n");
    SemOperation(sem_rep_id, 0, 1); 
    
  }
  else{ printf("x2\n");
    sem_rep_id = id;
    semctl(sem_rep_id, 0, SETVAL, 0);
    x = semctl(sem_rep_id, 0, GETVAL, x);
    shared_id = shmget(ID_REPO, sizeof(REPO), 0666 | IPC_CREAT);
    addr = (REPO*)shmat(shared_id, NULL, 0);
    (*addr).active_clients = 0;
    (*addr).active_rooms = 0;
    (*addr).active_servers = 1;
    (*addr).servers[0].client_msgid = ownMSG;
    (*addr).servers[0].server_msgid = servers_id;
    (*addr).servers[0].clients = 0;
    sem_log_id = semget(SEM_LOG, 1, 0666 | IPC_CREAT);
    semctl(sem_log_id, 0, SETVAL, 1);
    shmdt(addr);
    SemOperation(sem_rep_id, 0, 1);
    x = semctl(sem_rep_id, 0, GETVAL, x);
  }
  //semctl(sem_rep_id, 0, IPC_RMID, 0); printf("x\n");
  
}

Unregister(){
  int i = 0,j;
  SemOperation(sem_rep_id, 0, -1);
  REPO* addr = (REPO*)shmat(shared_id, NULL, 0);
  while((*addr).servers[i].client_msgid != ownMSG)
    i++;
  while((*addr).servers[i].clients > 0)
    UnloginClient(ownMSG);
  msgctl((*addr).servers[i].client_msgid, IPC_RMID, NULL);
  msgctl((*addr).servers[i].server_msgid, IPC_RMID, NULL);
  for(j=i;j<(*addr).active_servers;j++){
    (*addr).servers[j].client_msgid = (*addr).servers[j+1].client_msgid;
    (*addr).servers[j].server_msgid = (*addr).servers[j+1].server_msgid;
    (*addr).servers[j].clients = (*addr).servers[j+1].clients;
  }
  --(*addr).active_servers;
  if((*addr).active_servers == 0){
    shmdt(addr);
    shmctl(shared_id, IPC_RMID, NULL);
    semctl(sem_log_id, 0, IPC_RMID, 0);
    semctl(sem_rep_id, 0, IPC_RMID, 0);
  }
  else{
    shmdt(addr);
    SemOperation(sem_rep_id, 0, 1);
  }
}

CleanREPO(){
}

SendServerList(){
  int i;
  SERVER_LIST_REQUEST req;
  SERVER_LIST_RESPONSE res;
  msgrcv(servers_list_id, &req, sizeof(req) - sizeof(long), SERVER_LIST, 0);
  SemOperation(sem_rep_id, 0, -1);
  REPO* addr = (REPO*)shmat(shared_id, NULL, 0);
  res.type = SERVER_LIST;
  res.active_servers = (*addr).active_servers;
  for(i=0; i<(*addr).active_servers; i++){
    res.servers[i] = (*addr).servers[i].client_msgid;
    printf("%d\n", (*addr).servers[i].client_msgid);
  }
  shmdt(addr);
  SemOperation(sem_rep_id, 0, 1);
  msgsnd(req.client_msgid, &res, sizeof(res) - sizeof(long), 0);
}

LoginClient(){
  int i, j=0, size;
  CLIENT_REQUEST req;
  STATUS_RESPONSE res;
  msgrcv(ownMSG, &req, sizeof(req) - sizeof(long), LOGIN, 0);
  res.type = STATUS;
  res.status = 0;
  SemOperation(sem_rep_id, 0, -1); 
  REPO* addr = (REPO*)shmat(shared_id, NULL, 0);
  while((*addr).servers[j].client_msgid != ownMSG)
    j++;
  if((*addr).servers[j].clients == 17)
    res.status = 503;
  else if(res.status == 0){ 
      res.status = 400;
      for(i=0; i<MAX_NAME_SIZE; i++)
	if(req.client_name[i] == '\0'){
	  res.status = 0;
	  size = i; 
	  break;
	}
      for(i=0; i<size; i++)
	if(isprint(req.client_name[i]) ==0){
	  res.status = 400;
	}
    }
  if(res.status == 0){ 
    for(i=0;i<(*addr).active_clients; i++)
      if(strcmp((*addr).clients[i].name, req.client_name) == 0)
	res.status = 409;
      }
  if(res.status == 0){ 
    strcpy((*addr).clients[(*addr).active_clients].name, req.client_name);
    (*addr).clients[(*addr).active_clients].server_id = ownMSG;
    ++(*addr).active_clients;
    SortClients(addr);
    res.status = 201;
  } 
  msgsnd(req.client_msgid, &res, sizeof(res) - sizeof(long), 0); 
  shmdt(addr);
  SemOperation(sem_rep_id, 0, 1);
}

UnloginClient(int client_msgid){ //client_msgid == kolejka servera do komunikacji z klientami
  int i = 0,j;
  REPO* addr = (REPO*)shmat(shared_id, NULL, 0);
  while((*addr).clients[i].server_id != client_msgid)
    i++;
  for(j=i;j<(*addr).active_clients;j++){
    (*addr).clients[j].server_id = (*addr).clients[j+1].server_id;
    strcpy((*addr).clients[j].name, (*addr).clients[j+1].name);
    strcpy((*addr).clients[j].room, (*addr).clients[j+1].room);
  }
  --(*addr).active_clients;
  shmdt(addr);
}

SendRoomsList(){
  int i;
  CLIENT_REQUEST req;
  ROOM_LIST_RESPONSE res;
  msgrcv(ownMSG, &req, sizeof(req) - sizeof(long), ROOM_LIST, 0);
  SemOperation(sem_rep_id, 0, -1);
  REPO* addr = (REPO*)shmat(shared_id, NULL, 0);
  res.type = ROOM_LIST;
  res.active_rooms = (*addr).active_rooms;
  for(i=0;i<(*addr).active_rooms;i++){
    strcpy(res.rooms[i].name, (*addr).rooms[i].name);
    res.rooms[i].clients = (*addr).rooms[i].clients;
  }
  msgsnd(req.client_msgid, &res, sizeof(res) - sizeof(long), 0);
  shmdt(addr);
  SemOperation(sem_rep_id, 0, 1);
}

SendUsersList(){
  int i;
  CLIENT_REQUEST req;
  CLIENT_LIST_RESPONSE res;
  msgrcv(ownMSG, &req, sizeof(req) - sizeof(long), GLOBAL_CLIENT_LIST, 0); printf("V\n");
  SemOperation(sem_rep_id, 0, -1); printf("V\n");
  REPO* addr = (REPO*)shmat(shared_id, NULL, 0);;
  res.type = GLOBAL_CLIENT_LIST;
  res.active_clients = (*addr).active_clients; 
  for(i=0;i<(*addr).active_clients;i++)
    strcpy(res.names[i], (*addr).clients[i].name);
  msgsnd(req.client_msgid, &res, sizeof(res) - sizeof(long), 0); printf("V\n");
  shmdt(addr);
  SemOperation(sem_rep_id, 0, 1);
}

SendUsersOnRoomList(){
  int i,j=0,k=0;
  CLIENT_REQUEST req;
  CLIENT_LIST_RESPONSE res;
  msgrcv(ownMSG, &req, sizeof(req) - sizeof(long), ROOM_CLIENT_LIST, 0);
  SemOperation(sem_rep_id, 0, -1);
  REPO* addr = (REPO*)shmat(shared_id, NULL, 0);
  res.type = ROOM_CLIENT_LIST;
  res.active_clients = 0;
  char room[MAX_NAME_SIZE];
  while( strcmp( (*addr).clients[j].name, req.client_name) == 0) j++;
  strcpy(room, (*addr).clients[j].room);
  for(i=0; i<(*addr).active_clients;i++)
    if( strcmp( (*addr).clients[i].room, room) ){
      strcpy(res.names[k], (*addr).clients[i].name);
      k++;
      ++res.active_clients;
    }
  msgsnd(req.client_msgid, &res, sizeof(res) - sizeof(long), 0);
  shmdt(addr);
  SemOperation(sem_rep_id, 0, 1);
}

ChangeUserRoom(){
 int i, size, room_i; 
 CHANGE_ROOM_REQUEST req;
 STATUS_RESPONSE res; 
 msgrcv(ownMSG, &req, sizeof(req) - sizeof(long), CHANGE_ROOM, 0);
 SemOperation(sem_rep_id, 0, -1);
 REPO* addr = (REPO*)shmat(shared_id, NULL, 0);
 res.type = CHANGE_ROOM;
 res.status = 400;
 for(i=0; i<MAX_NAME_SIZE; i++)
  if(req.room_name[i] == '\0'){
    res.status = 0;
    size = i; 
    break;
  }
  printf("%d\n", res.status);
 for(i=0; i<size; i++)
  if(isprint(req.room_name[i]) ==0){
    res.status = 400;
  }printf("%d\n", res.status);
  if(res.status == 400)
    msgsnd(req.client_msgid, &res, sizeof(res) - sizeof(long), 0);
  else{
    for(i=0; i<(*addr).active_rooms; i++)
      if(strcmp(req.room_name, (*addr).rooms[i].name) ){
	res.status = 202;
	room_i = i;
	break;
      }
    if(res.status == 202){
      ++(*addr).rooms[room_i].clients;
      for(i=0; i<(*addr).active_clients; i++)
	if(strcmp(req.client_name, (*addr).clients[i].name) == 0)
	  strcpy( (*addr).clients[i].room, req.room_name);
    }
    else{
      strcpy( (*addr).rooms[ (*addr).active_rooms].name, req.room_name);
      (*addr).rooms[ (*addr).active_rooms].clients = 1;
      (*addr).active_rooms = 1;
    }
    res.status = 202;
    msgsnd(req.client_msgid, &res, sizeof(res) - sizeof(long), 0);
  }
  shmdt(addr);
  SemOperation(sem_rep_id, 0, 1);
}

int main(){
  int y=0;
  InitMSGs();
  Register();
  printf("shared %d\n", shared_id);
  if(fork() == 0)
    while(1)
      SendServerList();
  if(fork() == 0)
    while(1)
      LoginClient();
  if(fork() == 0)
    while(1)
      SendUsersList();
  if(fork() == 0)
    while(1)
      SendRoomsList();
  if(fork() == 0)
    while(1)
      SendUsersOnRoomList();
  if(fork() == 0)
    while(1)
      ChangeUserRoom();
  while(y == 0){
    printf("uregister?\n");
    scanf("%d", &y);
  }
  Unregister();
  
  //wait(NULL);
  
  return 0;
  
}
  
    
    