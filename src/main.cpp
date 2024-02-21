/*
  ESP-NOW Demo - Receive
  esp-now-demo-rcv.ino
  Reads data from Initiator

  DroneBot Workshop 2022
  https://dronebotworkshop.com
*/

// Include Libraries
#include <WiFi.h>
#include <esp_now.h>
#include <ezButton.h>

#define BUTTON_PIN 23
#define DEBOUNCE_TIME 50
#define INNIT_HEALTH 100
#define HIT_DAMAGE 10

// Peer info
esp_now_peer_info_t peerInfo;
// MAC Address of responder - edit as required
uint8_t screenAddress[] = {0x24, 0x6F, 0x28, 0xD1, 0x9A, 0xBC};
// Define a data structure
typedef struct struct_message {
  uint8_t swordNumber;
  char direction;
  uint8_t action;
} struct_message;

// Create a structured object
struct_message myData;

typedef struct struct_player {
  char direction;
  uint8_t action;
  int health;
} struct_player;

struct_player player1;
struct_player player2;

void resetPlayer(struct_player *player) {
  player->action = 0;
  player->direction = 'n';
  player->health = INNIT_HEALTH;
}

// game loop
typedef struct struct_game {
  uint8_t gameStage;    // 0: waiting, 1: playing, 2: wined
  uint8_t playerNumber; // 1: player1, 2: player2, 0: default
  uint8_t action;       // 1: block, 2: hit(lost blood), 0: default
  int player1health;
  int player2health;
} struct_game;

struct_game gameLoop;

void resetGame(struct_game *game) {
  game->gameStage = 0;
  game->playerNumber = 0;
  game->action = 0;
  game->player1health = INNIT_HEALTH;
  game->player2health = INNIT_HEALTH;
}

void nextStage(struct_game *game) {
  game->gameStage = (game->gameStage + 1) % 3;
  if (game->gameStage == 0) {
    resetGame(game);
    resetPlayer(&player1);
  }
  Serial.printf("Game stage change: %d\n", game->gameStage);
}

void sendGameLoop() {
  gameLoop.player1health = player1.health;
  gameLoop.player2health = player2.health;
  esp_err_t result =
      esp_now_send(screenAddress, (uint8_t *)&gameLoop, sizeof(gameLoop));
  if (result == ESP_OK) {
    Serial.println("Game loop sent successfully");
  } else {
    Serial.println("Sending error");
  }
  Serial.print("Player 1 health: ");
  Serial.println(gameLoop.player1health);
  Serial.print("Player 2 health: ");
  Serial.println(gameLoop.player2health);
  if (gameLoop.action != 0) {
    Serial.printf("Player number: %d is %s", gameLoop.playerNumber,
                  gameLoop.action == 1 ? "can block" : "being attacking");
  }
  Serial.println();
}

void endGame() {
  gameLoop.gameStage = 2;
  gameLoop.action = 0;
  gameLoop.playerNumber = player1.health == 0 ? 2 : 1;
  sendGameLoop();
}

struct Button {
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  bool pressed;
};

ezButton button(BUTTON_PIN);

// Callback function executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  // if not playing, ignore the data
  if (gameLoop.gameStage != 1)
    return;
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Data received from sword: ");
  Serial.println(myData.swordNumber);
  struct_player *sender_player = myData.swordNumber == 1 ? &player1 : &player2;
  struct_player *opponent_player =
      myData.swordNumber == 1 ? &player2 : &player1;
  // blocking or unblocking
  if (myData.action != 1) {
    sender_player->action = myData.action;
    sender_player->direction = myData.direction;
  }
  // attacking
  else {
    // hit
    if (opponent_player->action != 2 ||
        (opponent_player->action == 2 &&
         opponent_player->direction == myData.direction)) {
      opponent_player->health -= HIT_DAMAGE;
      Serial.println("Hit!");
      gameLoop.action = 2;
      if (opponent_player->health == 0) {
        endGame();
      }
    }
    // block
    else {
      Serial.println("Blocked!");
      gameLoop.action = 1;
    }
    gameLoop.playerNumber = myData.swordNumber == 1 ? 2 : 1;
    sendGameLoop();
  }
}

/* -------------------------------- main code ------------------------------- */
void setup() {
  // Set up Serial Monitor
  Serial.begin(9600);

  // Set ESP32 as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  // Register callback function
  esp_now_register_recv_cb(OnDataRecv);

  // Register peer
  memcpy(peerInfo.peer_addr, screenAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // initialize players
  resetPlayer(&player1);
  resetPlayer(&player2);

  // initialize button
  button.setDebounceTime(DEBOUNCE_TIME);

  // initialize game
  resetGame(&gameLoop);
}

void loop() {
  button.loop();
  if (button.isReleased()) {
    nextStage(&gameLoop);
    sendGameLoop();
  }
}