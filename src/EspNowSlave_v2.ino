// Libreie per espnow e wifi
// Ispirato da https://www.fernandok.com/2018/03/esp32-com-protocolo-esp-now.html
// Laudx funzionante al 21/03/2021

#if defined(ESP32)
  #include <esp_now.h>
  #include <WiFi.h>
#elif defined (ESP8266)
    #include <espnow.h>  
    #include <ESP8266WiFi.h>
    #error (ESP8266 non ancora completato)
  #else
    #error (Ambiente ESP richiesto)
#endif

#include <locale.h>
#include <time.h>
//Definizione Buffer Trasmissione
#define MAXGPIO 2
#define MAXBUFF 200
typedef struct struct_GPIO {
  unsigned char PinNum;
  unsigned char PinVal;
} struct_GPIO;

typedef struct struct_TESTA {
  unsigned int CodeRequest;
    time_t DataTime;
} struct_TESTA;

typedef struct struct_message {
  struct_TESTA BufHeader;
  union u_Buffer {
    char Buffer[MAXBUFF-(sizeof(struct_TESTA))];
      struct_GPIO gpios[MAXGPIO];
  } DataBuffer;
} struct_message;


// Create a struct_message called myData
struct_message myData;

// Canale utilizzato per la connessione (al momento se uso diverso da 1 da errore)
#define CHANNEL 1
#define LEDA 13
#define LEDB 5

char LogBuffer[100];

//Prototipi funzioni
char *CreaTestataLog(char *, int);
void  GetNTPDataTime(void);
void  printLocalTime(void);
void  ScriviLog(char *);





// Pin che leggeremo (digitalRead) e invieremo agli Slaves 
// È importante che il codice sorgente di Slaves abbia lo stesso array con gli stessi gpios 
// nello stesso ordine 
uint8_t gpios[] = {LEDA, LEDB};



// Nel setup calcoleremo il numero di pin e inseriremo questa variabile, 
// quindi non abbiamo bisogno di cambiare qui ogni volta che cambiamo il numero di pin, 
// tutto viene calcolato nel setup 
int gpioCount;











void setup() {
  uint8_t MioMacAddress[6];
  // imposto velocità a 115200 della seriale, ricordarsi di settare la stessa velocità nel terminale che useremo per visualizzare i messaggi
  Serial.begin(115200);
  //Acquisisco Data e ora da server NTP
  GetNTPDataTime();  

  //attivo i LED e li metto spenti (Non serve piu)
  pinMode(LEDA, OUTPUT);
  pinMode(LEDB, OUTPUT);
  
  // Calcolo della dimensione dell'array di gpios che verrà letto con digitalRead 
  // sizeof (gpios) restituisce il numero di byte puntati dall'array gpios 
  // Sappiamo che tutti gli elementi dell'array sono del tipo uint8_t 
  // sizeof (uint8_t) restituisce il numero di byte che ha il tipo uint8_t 
  // Quindi per sapere quanti elementi ha l'array 
  // dividiamo il numero totale di byte dell'array e quanti 
  // byte ha ogni elemento 
  gpioCount = sizeof(gpios)/sizeof(uint8_t);

  //Mettiamo l'ESP in modalità station 
  WiFi.mode(WIFI_STA);
    delay(30000);
  // Mostriamo sul monitor seriale l'indirizzo Mac di questo ESP quando è in modalità stazione
  // Se si vuole che il Master invii messaggi specifici agli ESP, modificare la 
  // lista di slave (nel codice sorgente del Master) in modo che qui siano stampati solo gli indirizzi Mac
  //printf ( " L'indirizzo MAC di questo dispositivo è:% x:% x:% x:% x:% x:% x " , mac [ 0 ], mac [ 1 ], mac [ 2 ], mac [ 3 ], mac [ 4 ], mac [ 5 ]);
  // Leggo il MAC Address della mia scheda
  WiFi.macAddress(MioMacAddress);
  //Serial.println(WiFi.macAddress());
  sprintf(LogBuffer,"Mac Address Stazione Slave: %02X:%02X:%02X:%02X:%02X:%02X ",MioMacAddress[0],MioMacAddress[1],MioMacAddress[2],MioMacAddress[3],MioMacAddress[4],MioMacAddress[5]); 
  ScriviLog(LogBuffer);

  //Chiama la funzione che inizializza ESPNow
  InitESPNow();

  // Registrare la richiamata che ci informerà sullo stato della spedizione 
  // La funzione che verrà eseguita è OnDataSent ed è dichiarata di successivamente 
  esp_now_register_recv_cb(OnDataRecv);

  //Per ogni PIN definito
  for(int i=0; i<gpioCount; i++){
    //spegniamo i LED
    pinMode(gpios[i], OUTPUT);
    digitalWrite(gpios[i], LOW);
  }

}

void InitESPNow() {
  //Controllo risultato inizializzazione
  if (esp_now_init() == ESP_OK) {
//    Serial.println("ESPNow Init corretta");
//    sprintf(LogBuffer,"%s%s","Mac Address in Station: ",WiFi.macAddress()); 
    ScriviLog("ESPNow Init Corretta");

  }
  //Se errore
  else {
    ScriviLog("ESPNow Init Fallita");
    ESP.restart();
  }
}

// Funzione che funge da callback per avvisarci
// quando qualcosa è arrivato dal Master
// Laudix Modificare per leggere nuovo Buffer
// ispirarsi a Z:\Progetti_VSC\ESP32\ESPNowSlave\src\main.cpp
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  char macStr[18];
  time_t t = time(NULL); // Leggo data e ora di sistema
  Serial.printf("time_t locale -> %d remoto -> %d\n",t ,myData.BufHeader.DataTime); 
  // Copio il buffer ricevuto nella struttura dei mie dati
  memcpy(&myData, data, sizeof(myData));
  // Laudix Gestire  l'aggiornamento della data e ora
  if (myData.BufHeader.DataTime != t) {
    sprintf(LogBuffer,"Devo aggiornare Data e Ora ricevuto da master ->%d Locale ->%d","Ricevuto da:",myData.BufHeader.DataTime,t);
    ScriviLog(LogBuffer);
  }
  //Copiamo l'indirizzo Mac di origine in una stringa
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  //Mostriamo l'indirizzo Mac del Master da cui abbiamo ricevuto il messassio
//  Serial.print("Received from: "); 
//  Serial.println(macStr);
//  Serial.println("");
  sprintf(LogBuffer,"%s %s","Ricevuto da:",macStr); 
  ScriviLog(LogBuffer);


  //Per ogni PIN gestito
  for(int i=0; i<gpioCount; i++){
    //Imposto lo stato dei PIN ricevuto dal Master nei corrispondenti PIN dello Slave
    pinMode(myData.DataBuffer.gpios[i].PinNum, OUTPUT);
    digitalWrite(myData.DataBuffer.gpios[i].PinNum, myData.DataBuffer.gpios[i].PinVal);
    sprintf(LogBuffer,"Stato LED %02d --> %d", myData.DataBuffer.gpios[i].PinNum, myData.DataBuffer.gpios[i].PinVal);
    ScriviLog(LogBuffer);

  }
}

// Non è necessario eseguire alcuna operazione in "loop"
// Ogni volta che arriva qualcosa dal Maestro
// la funzione OnDataRecv viene eseguita automaticamente
// in quanto callback specifica aggiunta attraverso la funzione
// esp_now_register_recv_cb
void loop() {
}

char * CreaTestataLog(char *Buffer, int Dim) {
  time_t t = time (NULL);
  struct tm *tp = localtime (&t);
  setlocale (LC_ALL, "it_IT.UTF-8");
  strftime (Buffer,Dim, "%Y.%m.%d - %H:%M:%S - ", tp);
  return Buffer;
  // Laudix non dovrebbero essere necessarie altre modifiche
}

void ScriviLog(char *Buffer) {
  char Testa[30];
  CreaTestataLog(Testa,100);
  Serial.printf("%s%s\n",Testa,Buffer);
  return;
}

void GetNTPDataTime(){
  const char* ssid     = "D-Link-28B27F";
  const char* password = "Sara92Davide94";
  const char* ntpServer = "pool.ntp.org";
  const long  gmtOffset_sec = 0;
  const int   daylightOffset_sec = 3600;
  char LogBuffer[100];
  String Buffer;
  // Connect to Wi-Fi
  sprintf(LogBuffer,"Connessione alla rete in corso -> %s", ssid);
  ScriviLog(LogBuffer);
  //Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  ScriviLog("WiFi Connesso.");
  
  // Inizializza connessione all'NTP server e Sincronizza Data e Ora
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    ScriviLog("Failed to obtain time");
    return;
  }
//    sprintf(LogBuffer,"Stato LED %02d --> %d", gpios[i], data[i]);
//    ScriviLog(LogBuffer);


  
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");

  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
}
