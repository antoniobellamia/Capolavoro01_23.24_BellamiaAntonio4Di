#include <Servo.h>
#include <math.h>

#define ledZeroPin 11
#define ledOnePin 5
#define ledTwoPin 6
#define ldrInputPin A0
#define buzzerOutPin A5
#define servoAttachPin 3
#define buzzerVirtualGroundPin 16

/* SG-90 SERVO PINOUT
  ORANGE = PWM
  RED = 5V
  BROWN = GND
*/

const double MAX_SENS_VALUE = 50;//Valore massimo leggibile dal LDR
int MIN_SENS_VALUE = 49; //Soglia minima luminosità
const float sensDb[10] = {49, 47, 45, 43, 41, 39, 35, 27, 17, 9}; //Corrispondenze sensore ved doc
const int MAX_BITS=8; //Numero massimo di bit

bool bits[MAX_BITS]; //Byte ricevuto (globale)
double LDR;//Valore attuale di luminosità

Servo servo;//Dich servoMotore
bool servoAuto = true; //Automazione motore on/off
int servoDegrees = 0; //Valore utente motore

struct light {
  int pin; //Il pin al quale è connesso fisicamente il led
  short int status = -1; //Lo stato in cui si trova il led (acceso/spento/auto)
  int index; //L'indice del led all'interno del vettore lights
  short int autoStat = -2; //0= no auto, 1= auto off, 2= auto on
} lights[3];//, bckLights[3]; //3 LED + BACKUP DEL VALORE PRECEDENTE DEI LED


void setup() {
  /*pinMode(12, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(4, OUTPUT);

  digitalWrite(12, LOW);
  digitalWrite(7, LOW);
  digitalWrite(4, LOW);*/

  pinMode(ldrInputPin, INPUT);//Sens lum
  pinMode(buzzerOutPin, OUTPUT);//Buzzer

  pinMode(buzzerVirtualGroundPin, OUTPUT);//Punto di massa simulata per il servo
  digitalWrite(buzzerVirtualGroundPin, LOW);//Punto di massa simulata per il servo 


  pinMode(ledZeroPin, OUTPUT);//led 0
  pinMode(ledOnePin, OUTPUT);//led 1
  pinMode(ledTwoPin, OUTPUT);//led 2

  //Assegna led ai pin
  lights[0].pin = ledZeroPin; //Led 0
  lights[1].pin = ledOnePin; //Led 1
  lights[2].pin = ledTwoPin; //Led 2

  lights[0].index=0;  //Assegna indice (led 0)
  lights[1].index=1;  //Assegna indice (led 1)
  lights[2].index=2;  //Assegna indice (led 2)


  //Collegamento servo
  pinMode(servoAttachPin, OUTPUT); //Assegna il pin di uscita (info) al servo
  servo.attach(servoAttachPin); //Comunica al servo il pin di collegamento

  Serial.begin(9600); //Inizializza la connessione seriale
  noTone(buzzerOutPin); //Elimina eventuali suoni del buzzer all'avvio

}

void setBrightness(struct light a) {//Imposta la luminosità per il led definito dal parametro

  switch (a.status) {//Controlla lo stato del led

    case 0: digitalWrite(a.pin, LOW); a.autoStat=0; break;//SPENTO (0V)
    case 1: analogWrite(a.pin, MAX_SENS_VALUE); a.autoStat=0; break;//ACCESO (Custom max value)

    default:analogWrite(a.pin, LDR); //AUTO (Valore analogo alla luminosità in ingresso)
            if(LDR>=MIN_SENS_VALUE) a.autoStat = 2;
            else a.autoStat = 1;
    break;

  }

  sendLedsData(a.index); //Invia i dati del led
}

void setServoRotation(int d) {//Imposta il valore di rotazione del servo motore INFLUENZATO DA VALORE PRECEDENTE
  int p = servo.read(); //Legge il valore di rotazione corrente
  
  servo.write(d); //Ruota il servo al valore desiderato
  sendServoData();//Invia i dati del servo sul seriale
  if(p!=d) tone(buzzerOutPin, 700, 15);

}

void eseguiScrittura() { //Instrada il messaggio di scrittura verso le diverse funzioni
  int act = bits[6] * 2 + bits[5];//Converte i primi due bit del messaggio ricevuto per instradare il messaggio

  switch (act) {
    case 0: setLeds(); break; // cod =0 instrada verso i led
    case 1: setServo(); break; // cod =1 instrada verso il servo
    case 2: setMinSensValue(); break;// cod =2 instrada verso le impostazioni di lum minima
    case 3: setAuto(true); break;// cod =3 instrada verso le impostazioni di automazione (attiva)
  }

}

void setLeds() {//Imposta le caratteristiche di led lette
  int id = bits[4] * 2 + bits[3];//Identifica il led
  int sts = bits[2] * 2 + bits[1];//0→SPENTO, 1→ACCESO, 2→AUTO

  if(id<3)
    lights[id].status = sts;

}

void setServo() {//Imposta le caratteristiche del servo lette

  if (bits[4]){
    servoAuto = true;
  }else{
    servoAuto = false;
    servoDegrees = 0;

    if(bits[3]) servoDegrees+=8;
    if(bits[2]) servoDegrees+=4;
    if(bits[1]) servoDegrees+=2;
    if(bits[0]) servoDegrees+=1;

    /*for (int i = 3; i >=0; i--)
      servoDegrees += pow(2, i) *bits[i];*/

    servoDegrees *= 6;
  }

}

void setAuto(bool s) {//Imposta l'automazione automatica

  servoAuto = s; //rotaz servo automatica

  for (int i = 0; i < 3; i++){//ANALIZZA I LED
    int lts;

    if(s) lts = 2; //SE AUTOMATICO
    else if(LDR>=MIN_SENS_VALUE && lights[i].status!=0) lts=1; //SE GIA' ACCESO (Non spento, lum adeguata)
    else if(LDR<MIN_SENS_VALUE && lights[i].status!=1) lts=0; //SE GIA' SPENTO (Non acceso, lum scarsa)

    lights[i].status=lts;
     
  } 
    
}

void setMinSensValue() {//Imposta il minimo valore di luminosità
  int val = 0;

  if(bits[4]) val+=8;
  if(bits[3]) val+=4;
  if(bits[2]) val+=2;
  if(bits[1]) val+=1;

  /*for (int i = 4; i > 0; i--)//Conversione
    val += pow(2, i) *bits[i];*/

  if (val < 10) {
    MIN_SENS_VALUE = sensDb[val];

  }else if(val>10) setAuto(false);

  sendMinLumData();
}

void leggiSeriale() {//Legge dal seriale

  if (Serial.available() > 0) {

  byte readByte = Serial.read();  //legge dal seriale

    for (int i = 7; i >= 0; i--) {
      bits[i] = bitRead(readByte, i); //setta array boolean
    }

    if (bits[7]) eseguiScrittura();
    //Ignora i byte di lettura (ved documentazione)

  }

}

void sendData(bool array[]){//Invia i dati alla porta seriale
  byte data = 0;

  //Conversione array bool in byte
  for(int i=0; i<MAX_BITS; i++){
      if(array[i])
        data |= (1 << i);
  }

  Serial.write(data); //Invia il dato alla porta seriale.
}

void sendSensData(){//INVIA I DATI DI LUMINOSITA' ALLA GUI

  bool conv[5];//Array per la conversione del numero da dec a bin
  double dn = 31 * LDR / MAX_SENS_VALUE;  //Estrapola valore da inviare, Ved documentazione
  dn=31-dn;                               //Estrapola valore da inviare, Ved documentazione

  int in = round(dn);                     //Estrapola valore da inviare, Ved documentazione

   

  /*for(int i = 0, p=0; i<10; i++, p+=10){
    if(LDR>sensDb[i]){
        in=p;
        break;
    } 
  }*/

  for(int i=0; i<5; i++){//Conversione dec/bin
    conv[i]=in%2;
    in/=2;
  }
  
  bool array[MAX_BITS] = {0, 1, 0, conv[4], conv[3], conv[2], conv[1], conv[0]};//Prot 010 + valore luminosità

  sendData(array);
  

}

void sendMinLumData(){//Invia i dati di luminosità minima alla GUI
  bool conv[4];
  int dVal=0;

  for(int i=0; i<10; i++){
    if(MIN_SENS_VALUE == sensDb[i]){
      dVal=i;
      break;
    }
  }

  for(int i=0; i<4; i++){//Conversione dec/bin
    conv[i]=dVal%2;
    dVal/=2;
  }

  bool array[MAX_BITS]={0, 1, 1, conv[3], conv[2], conv[1], conv[0], 0};

  sendData(array);
}

void sendServoData(){//Invia i dati di rotazione del servo
  bool conv[4];
  delay(1000);
  int dVal= round( (double)servo.read() /6 );

  /*if(servo.read()==24) dVal=14;
  if(dVal>3) dVal++;*/


  for(int i=0; i<4; i++){//Conversione dec/bin
    conv[i]=dVal%2;
    dVal/=2;
  }

  bool array[MAX_BITS]={0, 0, 1, servoAuto, conv[3], conv[2], conv[1], conv[0]};

  sendData(array);
}

void sendLedsData(int ledIndex){//Invia i dati dei vari led INFLUENZATO DA BACKUP
    
    //if(lights[ledIndex].status != bckLights[ledIndex].status || lights[ledIndex].autoStat != bckLights[ledIndex].autoStat){

      bool id[2], rsts[2];
      int v=ledIndex;
      
      /*for(int j=0;  j<2; j++){//Conversione dec/bin ID
        id[j]=v%2;
        v/=2;
      }*/

      switch(ledIndex){
        case 0:
          id[0] = 0; id[1]=0; break;
        case 1:
          id[0] = 1; id[1]=0; break;
        case 2:  
          id[0] = 0; id[1]=1; break;

      }

      switch(lights[ledIndex].status){
        case 0: 
          rsts[1]=0; rsts[0]=0;
        break;

        case 1: 
          rsts[1]=0; rsts[0]=1;
        break;

        default:
          rsts[1]=1;
          if(LDR>=MIN_SENS_VALUE && lights[ledIndex].status!=0) rsts[0]=1;//AUTO ED ACCESO
          else if(LDR<MIN_SENS_VALUE && lights[ledIndex].status!=1) rsts[0]=0;//AUTO E SPENTO
        break;

      }

      bool array[MAX_BITS]={0, 0, 0, id[1], id[0], rsts[1], rsts[0], 1};

      //bckLights[ledIndex].status = lights[ledIndex].status;
      //bckLights[ledIndex].autoStat = lights[ledIndex].autoStat;

      sendData(array);
    //}
}

void loop() {
  
  LDR = (double)analogRead(ldrInputPin) / 13.0; //Legge val ponderato dal sensore

  //Serial.println((String)LDR);

  sendSensData(); //INVIA I DATI DI LUMINOSITA' ALLA GUI

  leggiSeriale();

  if (LDR < MIN_SENS_VALUE) LDR = 0; //Se sens troppo basso, pone a 0
  if (LDR > MAX_SENS_VALUE) LDR=MAX_SENS_VALUE;//Se sens troppo basso, pone uguale alla costante massima

  if (servoAuto) { //Se autom mot attiva
    if (LDR == 0) //se NOTTE
      setServoRotation(0);//motore 'chiuso'
    else //se GIORNO
      setServoRotation(90);//motore 'aperto'
  } else setServoRotation(servoDegrees);//se autom off, imp valore utente

  for (int i = 0; i < 3; i++)
    setBrightness(lights[i]);



}