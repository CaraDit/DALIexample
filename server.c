/*
 * server.c
 *
 * Description: This program initializes a server and outputs
 * a port number. When a player or spectator input the port
 * number, they are able to gonnect to my team's nugget game.
 * This module takes in user-inputted keys, handles the input
 * accordingly, and sends messages to the users that allow them
 * to play the game. The game ends and the server shuts down 
 * when all of the gold has been collected.
 *
 * Usage: ./server mapFile seed (seed is optional)
 *
 * Input: 2 arguments to stdin (see above)
 *
 * Output:
 *   - messages (map strings, gold information, game summary, etc.)
 *     send to players and spectator
 *     a list of URLs) is outputted to stdout
 *
 * Team CASH
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include "log.h"
#include "file.h"
#include "message.h"
#include "map.h"
#include "player.h"

/**************** global variables ****************/
#define MaxBytes 65507     // max number of bytes in a message
#define MaxNameLength 50   // max number of chars in playerName
#define MaxPlayers 26      // maximum number of players
#define GoldTotal 250      // amount of gold in the game
#define GoldMinNumPiles 10 // minimum number of gold piles
#define GoldMaxNumPiles 30 // maximum number of gold piles

/**************** prototypes ****************/
int validateArgs(int argc, char *mapFileInput, char *seedInput, FILE *fp);
void goldInit(gameInfo_t *gameInfo);
void gridInit(gameInfo_t *gridInfo);
static bool handleMessage(void *arg, const addr_t from, const char *message);
int newMove(gameInfo_t *gameInfo, addr_t clientAddr, char C);
int isNum(char *input);
void connectSpectator(gameInfo_t *gameInfo, addr_t clientAddr);
void connectNewPlayer(gameInfo_t *gameInfo, const char *message, addr_t clientAddr);
bool addNewPlayer(gameInfo_t *gameInfo, const char *playerName, addr_t clientAddr);
void sendMap(map_t *map, gameInfo_t *gameInfo);
void sendGoldInfo(addr_t clientAddr, goldBag_t *gb, gameInfo_t *gameInfo, int p);
void sendSummary(gameInfo_t *gameInfo, int numPlayers);
void deleteGameInfo(gameInfo_t *gameInfo);
void getColRow(char *gridRaw, int *col, int *row);
int numDigits(int num);

/**************** main ****************/
int main(int argc, char *argv[])
{
  // LOCAL VARIABLES
  FILE *fp = NULL;
  gameInfo_t *gameInfo = malloc(sizeof(gameInfo_t));  // structure to hold information about game
  map_t *mapRaw = malloc(sizeof(map_t));  // structure to hold original map string
  map_t *map = malloc(sizeof(map_t));
  char *gridRaw;
  char *grid;
  int numPlayers = 0;
  int result = 0;
  player_t *players[MaxPlayers];  // array of player_t pointers, initialized as NULL
  player_t *spectator = malloc(sizeof(player_t));
  for (int i=0; i<MaxPlayers; i++) {
    players[i] = NULL;
  }
  
  // VALIDATE ARGUMENTS
  fp = fopen(argv[1], "r");
  result = validateArgs(argc, argv[1], argv[2], fp);
  if (result>0) {
    free(spectator);
    free(mapRaw);
    free(map);
    free(gameInfo);
    if (result==1) {
      return 1;  //wrong number of arguments   
    } else if (result==2) {
      return 2;  //invalid map file  
    } else if (result==3) {
      fclose(fp);
      return 3;  //invalid seed 
    }
  }
  else {
    
    // INITIALIZE GAME INFO STRUCTURE
    gameInfo->players = players;  // add players to structure
    gameInfo->numPlayers = numPlayers;
    gameInfo->mapRaw = mapRaw;
    gameInfo->map = map;
    gridRaw = freadfilep(fp);  // create 2 copies of map string
    grid = malloc(strlen(gridRaw)+1);
    strcpy(grid, gridRaw);
    gameInfo->mapRaw->grids = gridRaw;  // add map strings to structure
    gameInfo->map->grids = grid;    
    gameInfo->spectator = spectator;  // add spectator to structure
    gameInfo->spectator->connected=false;
    
    // INITIALIZE GRID (rows and columns)
    gridInit(gameInfo);

    // INITIALIZE GOLD BAGS
    goldInit(gameInfo);

    // INITIALIZE SERVER
    int port = message_init(stderr);  //inialize module and get port number
    printf("message_init: ready at port '%d'\n",port);
    bool ok = message_loop(gameInfo, 0, NULL, NULL, handleMessage);  //wait for and handle client input
                                                                     //stops looping when bool is true
    // SHUT DOWN SERVER AND FREE MEMORY
    message_done();
    log_done();
    deleteGameInfo(gameInfo);
    fclose(fp);
    return ok? 0 : 1;  //status code depends on result of message_loop
  }
  return 0;
}


/* *************** validateArgs *************** */
/* Parces the command line arguments and determines
 * whether or not it is a valid program call.
 *
 * Caller provides:
 *   - number of arguments (int)
 *   - each argument (string)
 *   - pointer to file
 * We return:
 *   - 1 if the wrong number of arguments was provided
 *   - 2 if the map file was invalid
 *   - 3 if the seed provided was invalid
 *   - 0 if no errors occurred
 */
int validateArgs(int argc, char *mapFileInput, char *seedInput, FILE *fp)
{
  int seed;
  if ((argc!=2) && (argc!=3)) {
    printf("usage: ./server mapFile [seed]\n");
    fprintf(stderr, "usage: ./server mapFile [seed]\n");
    return 1;  // wrong number of arguments
  } else {
    if (fp == NULL) {
      printf("%s is not a readable file\n", mapFileInput);
      fprintf(stderr, "%s is not a readable file\n", mapFileInput);
      return 2;  // invalid map file
    } else {
      if (argc==3) {  // seed was provided
        if (isNum(seedInput)) {  // check if seed is a number
          seed = atoi(seedInput);
          srandom(seed);  // call random() whenever a random number is needed
        } else {
          printf("%s is not a valid seed\n", seedInput);
          fprintf(stderr, "%s is not a valid seed\n", seedInput);
          return 3;  // invalid seed
        }
      } else {  // seed was not provided
        srandom(time(NULL));
      }
    }
  }
  return 0;
}


/* *************** goldInit ***************** */
/* Initializes the gold bags and places them on the map.
 *
 * Caller provides:
 *   - the structure of game information
 * We ensure:
 *   - a random number of gold bags is created
 *   - pointers to each gold bag is stored in an array
 *   - a random number of gold nuggets is placed in each
 *     gold bag (for a total of 250 nuggets)
 */
void goldInit(gameInfo_t *gameInfo)
{
  int GoldNumPiles = randNumInRange(GoldMinNumPiles, GoldMaxNumPiles);  // find number of gold bags
  gameInfo->GoldNumPiles = GoldNumPiles;
  goldBag_t **goldBags = (goldBag_t **)malloc(sizeof(goldBag_t *)*GoldNumPiles);  // store bags in array
  gameInfo->goldBags = goldBags;
  gameInfo->totalGold = GoldTotal;
  randomizeNuggets(gameInfo->map, goldBags, GoldTotal, GoldNumPiles);  // randomize number of nuggets in each bag
}


/* **************** gridInit **************** */
/* Initializes the grid information in the game
 * info structure.
 *
 * Caller provides:
 *   - the structure of game information
 * We ensure:
 *   - the number of rows and columns in the map
 *     is stored in the game information structure
 */
void gridInit(gameInfo_t *gameInfo)
{
  int nC=0;
  int nR=0;
  getColRow(gameInfo->mapRaw->grids, &nC, &nR);
  gameInfo->map->nR = nR;
  gameInfo->map->nC = nC;
  gameInfo->mapRaw->nR =nR;
  gameInfo->mapRaw->nC = nC;
}


/* *************** handleMessage *************** */
/* Receives message from user and handles it accordingly.
 * Valid messages include:
 *   - SPECTATE: indicates spectator connecting
 *   - PLAY: indicates player connecting
 *   - KEY: indicates player moving or quitting
 * Messages are sent back to the user to indicate:
 *   - OK: gives letter of player
 *   - NO...: indicates an error
 *   - GRID: number of rows and columns in grid
 *   - DISPLAY: map string
 *   - GOLD n p r: current gold bag information
 *   - GAMEOVER: sends summary of game after game is over
 *
 * Caller provides:
 *   - structure of game information
 *   - address of client
 *   - message (string)
 * We return:
 *   - true if the server should quit (game is over)
 *   - false if the server should continue to receive
 *     messages
 */
static bool handleMessage(void *arg, const addr_t from, const char *message)
{
  addr_t clientAddr;
  gameInfo_t *gameInfo = (gameInfo_t *)arg;

  if (arg==NULL) {
    fprintf(stderr, "handleMessage called with arg=NULL\n");
    return true;
  }

  // sender becomes our correspondent
  clientAddr = from;

  fprintf(stderr, "[%s@%05d]: %s\n", 
         inet_ntoa(from.sin_addr), //IP address of the sender
         ntohs(from.sin_port),     //port number of the sender
         message);                 //message from the sender

  // check if maximum players have already been reached
  if (gameInfo->numPlayers > MaxPlayers) {
    message_send(clientAddr, "NO Maximum players reached");
    if (strcmp(message, "KEY Q")==0) {
      message_send(clientAddr, "QUIT");
    }
    return false;
  } else {
  

    // SPECTATOR CONNECTS
    if (strcmp(message, "SPECTATE")==0) {
      connectSpectator(gameInfo, clientAddr);
      return false;
    }
    
    // PLAYER CONNECTS
    else if (message[0]=='P' && message[1]=='L' && message[2]=='A' && message[3]=='Y') {
      connectNewPlayer(gameInfo, message, clientAddr);
      return false;
    }
    
    // QUIT
    else if (strcmp(message, "KEY Q")==0) {
      // disconnect spectator
      if (gameInfo->spectator->connected) {
        if (message_eqAddr(gameInfo->spectator->clientAddr, clientAddr)) {
          message_send(clientAddr, "QUIT");
          fprintf(stderr, "[%s@%05d]: spectator quit\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port));
          gameInfo->spectator->connected=false;
        }
      }
      // disconnect player
      else if ((findPlayer(gameInfo, clientAddr) != NULL)) {
        playerQuit(gameInfo, clientAddr);  //remove player from board
        sendMap(gameInfo->map, gameInfo);  //send updated map to all players
      }
      return false;
    }
    
    // PLAYER MAKES A MOVE
    else if (message[0]=='K' && message[1]=='E' && message[2]=='Y') {
      int result = newMove(gameInfo, clientAddr, message[4]);
      // valid move to a gold bag   
      if (result == 2) {
        goldBag_t *gb = findGoldBag(GoldMaxNumPiles, gameInfo->x, gameInfo->y, gameInfo->goldBags); // pointer to goldbag structure you landed on
        gameInfo->totalGold -= gb->numNugs;  // subtracts gold from total
        player_t *ptr = findPlayer(gameInfo, clientAddr);
        sendGoldInfo(clientAddr, gb, gameInfo, ptr->numNugs);  // send gold info to all players
      }
      // valid move
      if (result>0) {
        updateVisibility(gameInfo->map, gameInfo->players, MaxPlayers, gameInfo->mapRaw);  //update visibility for all players
        sendMap(gameInfo->map, gameInfo); //send map to all players
        // end game if all gold has been collected
        if (gameInfo->totalGold==0) {
          sendSummary(gameInfo, gameInfo->numPlayers);
          return true;
        }
      }
      return false;
    }
    return false;
  }
  return false;
}


/**************** newMove  ****************/
/* Determines a player's new coordinates based
 * on the key that they pressed to make a move.
 * Possible keys are:
 *    - Q: quit 
 *    - h: move left
 *    - j: move down
 *    - k: move up
 *    - l: move right
 *    - y: move diagonally up and left
 *    - u: move diagonally up and right
 *    - b: move diagonally down and left
 *    - n: move diagonally down and right
 * (capital letters translate into the corresponding
 * move until the player can not move any further in
 * that direction)
 *
 * Caller provides:
 *    - structure of game information
 *    - address of current player
 *    - key letter entered by user
 * We guarantee:
 *    - the player quits 
 *    OR
 *    - the player's location is updated accordingly
 *    - If the move takes the player to a gold bag, it is picked up.
 *    - If the move makes a player intersect with another player, their locations are swapped.
 * We return:
 *    - 0 if the player is making an invalid move
 *    - 1 if the player made a valid move
 *    - 2 if the player landed on a gold bag
 *    - 3 if the player swapped places with another
 *      player
 */
int newMove(gameInfo_t *gameInfo, addr_t clientAddr, char C)
{
  //find current coordinates of player
  player_t *ptr = findPlayer(gameInfo, clientAddr);
  int x = ptr->x;
  int y = ptr->y;
  int result=1;
  // if user entered a capital letter
  if (C=='H' || C=='J' || C=='K' || C=='L' || C=='Y' || C=='U' || C=='B' || C=='N') {
     while (result!=0) {
       result = newMove(gameInfo, clientAddr, C+32); // convert to corresponding lower case and move until invalid
       if (result==-1) {  // invalid space
         result=0;
         return 0;
       } else if (result==2) {  // gold bag
         goldBag_t *gb = findGoldBag(GoldMaxNumPiles, gameInfo->x, gameInfo->y, gameInfo->goldBags); // pointer to goldbag structure you landed on
         gameInfo->totalGold -= gb->numNugs;  // subtracts gold from total
         player_t *ptr = findPlayer(gameInfo, clientAddr);
         sendGoldInfo(clientAddr, gb, gameInfo, ptr->numNugs);  // send gold info to all players
       }
       updateVisibility(gameInfo->map, gameInfo->players, MaxPlayers, gameInfo->mapRaw);  //update visibility for all players
       sendMap(gameInfo->map, gameInfo);  // send map to all players         
    }
     return 0;
  }
  // calculate new x,y coordinates based on the key entered
  switch(C) {
    case 'h':
      x-=1;
      break;
    case 'j':
      y+=1;
      break;
    case 'k':
      y-=1;
      break;
    case 'l':
      x+=1;
      break;
    case 'y':
      x-=1;
      y-=1;
      break;
    case 'u':
      x+=1;
      y-=1;
      break;
    case 'b':
      x-=1;
      y+=1;
      break;
    case 'n':
      x+=1;
      y+=1;
      break;
    default:  // invalid key
      message_send(clientAddr, "NO Invalid key");
      fprintf(stderr, "[%s@%05d]: NO Invalid key\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
      result=0;
      break;
  }
  if (result==0) {
    return 0;
  }
  // update game info structure
  gameInfo->x = x;
  gameInfo->y = y;
  gameInfo->ID = ptr->L;
  result = movePlayer(gameInfo);  // updates location/info players in map if valid
  return result;
}


/* *************** isnum *************** */
/* Determines whether or not a number is
 * an integer.
 *
 * Caller provides:
 *   - a string
 * We return:
 *   - 1 if the string is an integer
 *   - 0 if the string is not an integer
 */
int isNum(char *input) {
  int i=0;
  while (input[i]!='\0') {
    if (!isdigit(input[i])) {
      return 0;
    } else {
      i++;
    }
  }
  return 1;
}

/* ************* connectSpectator ************ */
/* Connects a spectator to the current game.
 *
 * Caller provides:
 *   - structure of game information
 *   - address of current client
 * We guarantee:
 *   - if an existing spectator is already connected,
 *     if spectator is already connected, kick them out
 *     and replace them with the new spectator 
 */
void connectSpectator(gameInfo_t *gameInfo, addr_t clientAddr)
{
  char *gridMessage;
  // if no spectator is currently connected
  if (!(gameInfo->spectator->connected)) {
    // save information in game info structure
    gameInfo->spectator->clientAddr = clientAddr;
    fprintf(stderr, "[%s@%05d]: new spectator\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
  } else {
    // if spectator is already connected, kick them out and replace them with the new spectator
    message_send(gameInfo->spectator->clientAddr, "QUIT");
    fprintf(stderr, "[%s@%05d]: new spectator\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
    gameInfo->spectator->clientAddr = clientAddr;
  }
  gameInfo->spectator->connected = true;
  // send grid dimensions to spectator
  gridMessage = malloc(numDigits(gameInfo->map->nR)+(numDigits(gameInfo->map->nC)+1)+7);
  sprintf(gridMessage, "GRID %d %d", gameInfo->map->nR, (gameInfo->map->nC)+1);
  fprintf(stderr, "[%s@%05d]: %s\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), gridMessage);
  message_send(clientAddr, gridMessage);
  free(gridMessage);
  // send map and gold info to spectator
  sendMap(gameInfo->map, gameInfo);
  sendGoldInfo(clientAddr, NULL, gameInfo, 0);
}


/* ************* connectNewPlayer ************ */
/* Connects a new player to the current game.
 *
 * Caller provides:
 *   - structure of game information
 *   - message (string)
 *   - address of current client
 * We guarantee:
 *   - addNewPlayer is called to add player to the array
 *     of players in the game
 *   - if the max number of players has already been
 *     reached, then addNewPlayer returns false, and
 *     additional players are not allowed to connect
 */
void connectNewPlayer(gameInfo_t *gameInfo, const char *message, addr_t clientAddr)
{
  if (addNewPlayer(gameInfo, message, clientAddr)) {  // add player to array
    randomizeOnePlayerLoc(gameInfo, clientAddr);  // add player to board with random location
    updateVisibility(gameInfo->map, gameInfo->players, MaxPlayers, gameInfo->mapRaw);  // update visibility for all players
    sendMap(gameInfo->map, gameInfo);  //send updated map and gold info to all players
    sendGoldInfo(clientAddr, NULL, gameInfo, 0);
    fprintf(stderr, "[%s@%05d]: new player\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
  } else {
    message_send(clientAddr, "NO Max players reached\n");
    fprintf(stderr, "[%s@%05d]: NO Max players reached\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
  }
}


/**************** addNewPlayer ****************/
/* Adds a new player to the player array and sends
 * the player's letter ID and the grid information to the
 * new player.
 *
 * Caller provides:
 *   - pointer to gameInfo structure
 *   - PLAY message from client
 *   - address of current client
 * We guarantee:
 *   - playerConnect is called to add the client
 *     to the game
 * We return:
 *   - true if the player was added to the game
 *   - false if the max number of players has already
 *     been reached
 */
bool addNewPlayer(gameInfo_t *gameInfo, const char *message, addr_t clientAddr) {
  char *playerName;
  char *nameMessage;
  char *gridMessage;
  gameInfo->numPlayers++;  // increase number of players 
  playerName = malloc(strlen(message+5)+1);
  strcpy(playerName, message+5);
  player_t *newplayer = playerConnect(gameInfo, playerName, clientAddr);  // initialize player structure and add to array
  if (newplayer == NULL) {
    free(playerName);
    return false;  // max number of players was reached
  } else {
    // send letter of player
    nameMessage = malloc(5*sizeof(char));
    sprintf(nameMessage, "OK %c", newplayer->L);
    message_send(clientAddr, nameMessage);
    fprintf(stderr, "[%s@%05d]: %s\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), nameMessage);
    // send grid dimensions to new player
    gridMessage = malloc(numDigits(gameInfo->map->nR)+(numDigits(gameInfo->map->nC)+1)+7);
    sprintf(gridMessage, "GRID %d %d", gameInfo->map->nR, (gameInfo->map->nC)+1);
    fprintf(stderr, "[%s@%05d]: %s\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), gridMessage);
    message_send(clientAddr, gridMessage);
    free(nameMessage);
    free(gridMessage);
    free(playerName);
    return true;
  }
  return true;
}


/* ********************* sendMap ********************** */
/* Sends a map to all connected players in a game
 *
 * Caller provides:
 *   - the gameInfo structure pointer
 *   - the current map of the game
 * We guarantee:
 *   - only the part of the map that is visible to each player
 *     is sent to those players. Invisible spots in the map are
 *     represented as spaces in the map string
 *   - the spectator is allowed to see the entire board
 *   - memory for the message string is allocated and freed
 */
void sendMap(map_t *map, gameInfo_t *gameInfo)
{
  if (map->grids!=NULL) {
    char *mapMessage = malloc(strlen(map->grids)+11);
    int j;
    // send visible map to all connected players
    for (j=0; j<gameInfo->numPlayers; j++) {
      if (gameInfo->players[j]->connected == true) {
        sprintf(mapMessage, "DISPLAY\n%s", gameInfo->players[j]->map->grids);
        message_send(gameInfo->players[j]->clientAddr, mapMessage);
      }
    }
    // send whole map to spectator
    if (gameInfo->spectator->connected) {
      sprintf(mapMessage, "DISPLAY\n%s", gameInfo->map->grids);
      message_send(gameInfo->spectator->clientAddr, mapMessage); 
    }
    free(mapMessage);
  }
}


/* ********************* sendGoldInfo ********************** */
/* Sends updated gold info to all players after a player picks
 * up a gold bag.
 *
 * Caller provides:
 *    - address of client
 *    - pointer to goldbag that was picked up by current client
 *    - number of nuggets currently in that clients purse
 *    - structure of game information
 * We guarantee:
 *    - the updated n, p, and r is send to the corresponding
 *      players
 *    - memory for the message string is allocated and freed 
 */
void sendGoldInfo(addr_t clientAddr, goldBag_t *gb, gameInfo_t *gameInfo, int p)
{
  int n;  // number of nuggets picked up by player
  int r;  // number of gold nuggets remainin
  char *goldMessage = malloc(17*sizeof(char));  // string with gold info to be sent to client
  if (gb!=NULL) {
    n = gb->numNugs;  // if nuggets were picked up
  } else {
    n=0;  // if no nuggets were picked up
  }
  r = gameInfo->totalGold;
  for (int i=0; i<gameInfo->numPlayers; i++) {
    // send new n, p, r to player that picked up gold 
    if (message_eqAddr(gameInfo->players[i]->clientAddr, clientAddr)) {
     sprintf(goldMessage, "GOLD %d %d %d", n, p, r);
     fprintf(stderr, "[%s@%05d]: %s\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), goldMessage);
     message_send(clientAddr, goldMessage);
    }
    // send new r to everyone else
    else if (gameInfo->players[i]->connected == true) {
      sprintf(goldMessage, "GOLD %d %d %d", 0, gameInfo->players[i]->numNugs, r);
      fprintf(stderr, "[%s@%05d]: %s\n", inet_ntoa(gameInfo->players[i]->clientAddr.sin_addr), ntohs(gameInfo->players[i]->clientAddr.sin_port), goldMessage);
      message_send(gameInfo->players[i]->clientAddr, goldMessage);      
    }
  }
  // send new r to spectator
  if (gameInfo->spectator->connected == true) {
    sprintf(goldMessage, "GOLD %d %d %d", 0, 0, r);
    message_send(gameInfo->spectator->clientAddr, goldMessage);
    fprintf(stderr, "[%s@%05d]: %s\n", inet_ntoa(gameInfo->spectator->clientAddr.sin_addr), ntohs(gameInfo->spectator->clientAddr.sin_port), goldMessage);
  }
  free(goldMessage);
}


/* ********************* sendSummary ********************** */
/* Sends end game summary to all players after all of the gold has
 * been collected.
 *
 * Caller provides:
 *   - structure of game information
 *   - current number of players that have connected since the
 *     beginning of the game
 * We guarantee:
 *   - results are ranked by number of gold nuggets collected
 *   - the summary message is sent to all connected players
 *     and spectator
 *   - even players that have disconnected since the start of
 *     the game are represented in the summary
 *   - memory for the message string is allocated and freed
 */
void sendSummary(gameInfo_t *gameInfo, int numPlayers)
{
  // rank results in players array by number of nuggets
  player_t **players = gameInfo->players;
  player_t *swap = NULL;
  for (int i=0; i<numPlayers; i++) {
    for (int j=0; j<numPlayers-1; j++) {
      if (players[j]->numNugs < players[j+1]->numNugs) {
        swap = players[j];
        players[j] = players[j+1];
        players[j+1] = swap;
      }
    }
  }
  // construct summary message string
  int length=0;
  for (int i=0; i<numPlayers; i++) {
    length += strlen(players[i]->realname);
    length += numDigits(players[i]->numNugs);
  }
  char *summaryMessage = malloc(10+length+(8*numPlayers));
  sprintf(summaryMessage, "GAMEOVER\n");
  for (int i=0; i<numPlayers; i++) {
    char *temp = malloc(strlen(players[i]->realname)+numDigits(players[i]->numNugs)+8);
    sprintf(temp, "%d. %c %s %d\n", i+1, players[i]->L, players[i]->realname, players[i]->numNugs);
    strcat(summaryMessage, temp);
    free(temp);
  }
  // send summary to all players
  for (int j=0; j<numPlayers; j++) {
    if (players[j]->connected == true) {
      message_send(players[j]->clientAddr, summaryMessage);
    }
  }
  // send summary to spectator if there is one
  if (gameInfo->spectator->connected) {
    message_send(gameInfo->spectator->clientAddr, summaryMessage);
  }
  free(summaryMessage);
}


/* ********************* deleteGameInfo ********************** */
/* Frees the allocated strings and structures in the gameInfo structure
 *
 * Caller provides: 
 *   - pointer to the structure of game information
 */
void deleteGameInfo(gameInfo_t *gameInfo)
{
  int i=0;
  // free the players array
  while (gameInfo->players[i] != NULL) {
    free(gameInfo->players[i]->map->grids);
    free(gameInfo->players[i]->map);
    free(gameInfo->players[i]->past);
    free(gameInfo->players[i]->realname);
    free(gameInfo->players[i]);
    i++;
  }
  // free the gold bags array
  for (int j=0; j<gameInfo->GoldNumPiles; j++) {
    free(gameInfo->goldBags[j]);
  }
  free(gameInfo->goldBags);
  free(gameInfo->mapRaw->grids);
  free(gameInfo->mapRaw);
  free(gameInfo->map->grids);
  free(gameInfo->map);
  free(gameInfo->spectator);
  free(gameInfo);
}


/* ********************* getColRow ********************** */
/* Calculates the number of columns and rows in a map string
 *
 * Caller provies:
 *   - string of valid map
 *   - pointers to row and column integers
 * We guarantee:
 *   - row and col are updated to contain the number of 
 *     row and columns in the grid
 */
void getColRow(char *gridRaw, int *col, int *row)
{
  int done = 0;
  int i=0;
  // scan until end of string
  while (gridRaw[i] != '\0') {
    // scan until end of first line (1 row)
    while (gridRaw[i] != '\n') {
      if (!done) {
        (*col)++;  // increase col for each character in row 1
      }
      i++;
    }
    done=1;
    (*row)++;  // increase row for each /n reached
    i++;
  }
}



/**************** numDigits ****************/
/* Calculates the number of digits in a number
 *
 * Caller provides:
 *   - integer
 * We return:
 *   - number of digits in the integer
 * Note:
 *   - this is used to calculate the amound of space
 *     needed to malloc a string that will contain
 *     an integer     
 */
int numDigits(int num)
{
  int dig=1;
  while(num/10 > 0) {
    dig++;  // increase digit count by 1 for each time you can divide by 10
    num=num/10;
  }
  return (dig);
}
