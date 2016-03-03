
#include <EEPROM.h>  
#include <SPI.h>      
#include <MFRC522.h>   

#define COMMON_ANODE

#ifdef COMMON_ANODE
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

#define redLed 7
#define greenLed 6
#define blueLed 5
#define relay 4
#define button 3 // buton pini

boolean match = false; // kart start false
boolean programMode = false; // programlama modu false

int successRead; //başarılı okuma var ise değeri

byte storedCard[4];   // EEPROM ID yazma
byte readCard[4];           // ID modülden okuma
byte masterCard[4]; // EEPROM master

/*
 * MOSI: Pin 11 / ICSP-4
 * MISO: Pin 12 / ICSP-1
 * SCK : Pin 13 / ICSP-3
 * SS : Pin 10 (Configurable)
 * RST : Pin 9 (Configurable)
 */

#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);  // mfrc start 

///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
  //Arduino Pin ayarları
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH); // kapı kilitli
  digitalWrite(redLed, LED_OFF); // kırmızı led kapalı
  digitalWrite(greenLed, LED_OFF); // yeşil led kapalı
  digitalWrite(blueLed, LED_OFF); // mavi led kapalı
  
  //Protokol Ayarları
  Serial.begin(9600);  // Seri haberleşmeyi aç
  SPI.begin();           // MFRC522 için SPI'ı aç
  mfrc522.PCD_Init();    // MFRC522 start
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max); //Okuma mesafesini maksimum yap

  //EEPROM'u Start butonu ile başlat 
  pinMode(button, INPUT_PULLUP);  // EEPROM silme enable pininin pull up'ı
  if (digitalRead(button) == LOW) {     // buton grounda bağlı
    digitalWrite(redLed, LED_ON);   // silme start led
    Serial.println("!!! Tünel'e girildi master kartı okut !!!");
    Serial.println("5 sn sonra iptal");
    Serial.println("Bu kayıt Geri alınamaz");
    delay(5000);    // Kullanıcıya işlem iptali için zaman
    if (digitalRead(button) == LOW) {  // Eğer butona basıldıysa EEPROM'u sıfırla
      Serial.println("!!! EEPROM sıfırlanıyor !!!");
      for (int x=0; x<1024; x=x+1){ //EEPROM adreslerini çevir
        if (EEPROM.read(x) == 0){ //Eğer adres 0'da ise 
          // temizleme işlemi başarılı, Bir sonraki adrese git
        } 
        else{
          EEPROM.write(x, 0); // 0 yazma, 3.3mS sürüyor
        }
      }
      Serial.println("!!! Silindi !!!");
      digitalWrite(redLed, LED_OFF); // Silme başarılı
      delay(200);
      digitalWrite(redLed, LED_ON);
      delay(200);
      digitalWrite(redLed, LED_OFF);
      delay(200);
      digitalWrite(redLed, LED_ON);
      delay(200);
      digitalWrite(redLed, LED_OFF);
    }
    else {
      Serial.println("!!! Silme İptal !!!");
      digitalWrite(redLed, LED_OFF);
    }
  }
 
  if (EEPROM.read(1) != 1) {  // EEPROM adres 1 de tanımlı MASTERCARD var mı
    Serial.println("Tanımlı Master Card Yok");
    Serial.println("Kart Tanımla");
    do {
      successRead = getID(); //oku
      digitalWrite(blueLed, LED_ON); // tanimlama gerceklestiginde yak
      delay(200);
      digitalWrite(blueLed, LED_OFF);
      delay(200);
    }
    while (!successRead); //Okuma başarılı değilse
    for ( int j = 0; j < 4; j++ ) { // 4 kez dödür
      EEPROM.write( 2 +j, readCard[j] ); // EEPROM ADERS 3TEN YAZMAYA BAŞLA
    }
    EEPROM.write(1,1); //Tanimlanan karti EEPROM a yaz
    Serial.println("Master Card Tanımli");
  }
  Serial.println("##### RFID Kapi kontrol sistemi v0.0.1 #####"); //
  Serial.println("Master Card UID");
  for ( int i = 0; i < 4; i++ ) {     // EEPROM Master Card UID oku
    masterCard[i] = EEPROM.read(2+i); // MasterCardi yaz
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println("Okuma Bekleniyor :)");
  cycleLeds();    // sıralı led yanıyor
}


///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop () {
  do {
    successRead = getID(); // okuma başarılı ise 1
    if (programMode) {
      cycleLeds(); // program modu göster
    }
    else {
      normalModeOn(); // Normal mod, mavi led
    }
  }
  while (!successRead); // başarılı okuma yoksa data da yok
  if (programMode) {
    if ( isMaster(readCard) ) {  //MasterCard okunursa program moduna çık
      Serial.println("Bu Master Card"); 
      Serial.println("Programdan çıkma modu");
      Serial.println("-----------------------------");
      programMode = false;
      return;
    }
    else {  
      if ( findID(readCard) ) { //taranan kart biliniyorsa sil
        Serial.println("Kart siliniyor");
        deleteID(readCard);
        Serial.println("-----------------------------");
      }
      else {                    // bilinmiyorsa ekle
        Serial.println("kart ekleniyor");
        writeID(readCard);
        Serial.println("-----------------------------");
      }
    }
  }
  else {
    if ( isMaster(readCard) ) {  // Taranan kartın kimliği eşleşirse MasterCard'ın kimlik programı moduna gir
      programMode = true;
      Serial.println("Master Program modu");
      int count = EEPROM.read(0); // EEPROM ilk bayt oku
      Serial.print("Icine kayitli ");    // EEPROM içindeki kimlik numarasını saklar
      Serial.print(count);
      Serial.print("EEPROM'a kayitli");
      Serial.println("");
      Serial.println("Eklemek veya kaldırmak için bir karti tarayin");
      Serial.println("-----------------------------");
    }
    else {
      if ( findID(readCard) ) {        // EEPROM'da oku
        Serial.println("Gecebilirsin");
        openDoor(1500);                // kapı acılıyor 1.5sn
      }
      else {        
        Serial.println("Gecemezsin");
        failed(); 
      }
    }
  }
}

///////////////////////////////////////// Oku ///////////////////////////////////
int getID() {
  // Okuma init
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //Yeni bir PICC RFID varsa devam
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) { //kart varsa seriye yapıştır
    return 0;
  }
  // 4 byte kart okuma
  // 7 byte destekleme
  Serial.println("Kart Okunuyor:");
  for (int i = 0; i < 4; i++) {  // 
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // okumayı durdur
  return 1;
}

///////////////////////////////////////// LED (Program Mode) ///////////////////////////////////
void cycleLeds() {
  digitalWrite(redLed, LED_OFF); // 
  digitalWrite(greenLed, LED_ON); // 
  digitalWrite(blueLed, LED_OFF); // 
  delay(200);
  digitalWrite(redLed, LED_OFF); // 
  digitalWrite(greenLed, LED_OFF); // 
  digitalWrite(blueLed, LED_ON); // 
  delay(200);
  digitalWrite(redLed, LED_ON); // 
  digitalWrite(greenLed, LED_OFF); //
  digitalWrite(blueLed, LED_OFF); //
  delay(200);
}

//////////////////////////////////////// Normal Mod LED  ///////////////////////////////////
void normalModeOn () {
  digitalWrite(blueLed, LED_ON); //
  digitalWrite(redLed, LED_OFF); //
  digitalWrite(greenLed, LED_OFF); //
  digitalWrite(relay, HIGH); // 
}

////////////////////////////////////////EEPROM oku //////////////////////////////
void readID( int number ) {
  int start = (number * 4 ) + 2; // başlama pozisyonu
  for ( int i = 0; i < 4; i++ ) { // 4 kez 4 baytlık data ÇEVİR
    storedCard[i] = EEPROM.read(start+i); // EEEPROM'dan diziye
  }
}

///////////////////////////////////////// ID'yi EEPROM'a yapıştır   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) { // EEPROM'a yazmadan önce bu kart EEPROM'da var mı?
    int num = EEPROM.read(0); // kaçıncı kart
    int start = ( num * 4 ) + 6; // bir sonraki yazma başlangıcı
    num++; // bir arttır
    EEPROM.write( 0, num ); // yeni sayıyı  EEPROM(0)'a yaz
    for ( int j = 0; j < 4; j++ ) { // 4 dön
      EEPROM.write( start+j, a[j] ); // EEPROM'adzi değerini yaz
    }
    successWrite();
  }
  else {
    failedWrite();
  }
}

///////////////////////////////////////// KARTI EEPROM'DAN SİL   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) { // Kart EEPROM'da var mı?
    failedWrite(); // yoksa
  }
  else {
    int num = EEPROM.read(0); // Kart sayısını al
    int slot; // kart bölmesi
    int start;// = ( num * 4 ) + 6; // kart hangi bölmede
    int looping; // döngü kaç kez tekrarlandı
    int j;
    int count = EEPROM.read(0); // EEMROM'a kaç kart kayıtlı
    slot = findIDSLOT( a ); // kartı silmek için hangi bölmede
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--; // kart sayısnı 1 azalt
    EEPROM.write( 0, num ); // yeni değeri yaz
    for ( j = 0; j < looping; j++ ) { // döngü kadar tekrarla
      EEPROM.write( start+j, EEPROM.read(start+4+j)); // önceki 4 konuma dizi değeri döndür düzenle
    }
    for ( int k = 0; k < 4; k++ ) { //değişen döngü
      EEPROM.write( start+j+k, 0);
    }
    successDelete();
  }
}

///////////////////////////////////////// Byte Kontrol   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != NULL ) // 
    match = true; //
  for ( int k = 0; k < 4; k++ ) { // 
    if ( a[k] != b[k] ) //
      match = false;
  }
  if ( match ) { //
    return true; // 
  }
  else  {
    return false; // 
  }
}

///////////////////////////////////////// Slot Arama   ///////////////////////////////////
int findIDSLOT( byte find[] ) {
  int count = EEPROM.read(0); // EEPROM 1 değeri oku
  for ( int i = 1; i <= count; i++ ) { // EEPROM girişi için değer kadar döndür
    readID(i); // ID'yi EEPROM'dan oku, Depolanmış kart saklanır[4]
    if( checkTwo( find, storedCard ) ) { // depolanan Kart EEPROM okuma olmadığını görmek için kontrol
      // geçmiş kimlik kartlarını bulmak
      return i; // kart slot yuva sayısı(sırası)
      break; // 
    }
  }
}

///////////////////////////////////////// EEPROM'dan ID bulma   ///////////////////////////////////
boolean findID( byte find[] ) {
  int count = EEPROM.read(0); // İlk baytı oku
  for ( int i = 1; i <= count; i++ ) {  // bayt kadar ilerle
    readID(i); // EEPROM kimliğini oku ve sakla storedCard[4]
    if( checkTwo( find, storedCard ) ) {  //depolana kart eeprom
      return true;
      break; // buldun durdur
    }
    else {  // değilse, return false   
    }
  }
  return false;
}

///////////////////////////////////////// EEPROM'a yazma Başarılı    ///////////////////////////////////
// 
void successWrite() {
  digitalWrite(blueLed, LED_OFF); // 
  digitalWrite(redLed, LED_OFF); // 
  digitalWrite(greenLed, LED_OFF); // 
  delay(200);
  digitalWrite(greenLed, LED_ON); //
  delay(200);
  digitalWrite(greenLed, LED_OFF); //
  delay(200);
  digitalWrite(greenLed, LED_ON); // 
  delay(200);
  digitalWrite(greenLed, LED_OFF); //
  delay(200);
  digitalWrite(greenLed, LED_ON); // 
  delay(200);
  Serial.println("Başarıyla EEPROM'a ID kayıt edildi");
}

///////////////////////////////////////// EEPROM Başarısız Yazma  ///////////////////////////////////
//
void failedWrite() {
  digitalWrite(blueLed, LED_OFF); // 
  digitalWrite(redLed, LED_OFF); // 
  digitalWrite(greenLed, LED_OFF); // 
  delay(200);
  digitalWrite(redLed, LED_ON); // 
  delay(200);
  digitalWrite(redLed, LED_OFF); // 
  delay(200);
  digitalWrite(redLed, LED_ON); //
  delay(200);
  digitalWrite(redLed, LED_OFF); // 
  delay(200);
  digitalWrite(redLed, LED_ON); //
  delay(200);
  Serial.println("Başarısız! ID hatalı veya EEPROM da yanlış şeyler var");
}

///////////////////////////////////////// EEPROM'dan başarılı kaldırma  ///////////////////////////////////
// 
void successDelete() {
  digitalWrite(blueLed, LED_OFF); //
  digitalWrite(redLed, LED_OFF); // 
  digitalWrite(greenLed, LED_OFF); //
  delay(200);
  digitalWrite(blueLed, LED_ON); // 
  delay(200);
  digitalWrite(blueLed, LED_OFF); // 
  delay(200);
  digitalWrite(blueLed, LED_ON); //
  delay(200);
  digitalWrite(blueLed, LED_OFF); // 
  delay(200);
  digitalWrite(blueLed, LED_ON); // 
  delay(200);
  Serial.println("Başarı ile kaldırıldı");
}

//////////////////////  readCard kontrol masterCard ise   ///////////////////////////////////
// geçirilen ID yöneticisi programlama kartı olup olmadığını kontrol et
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}

///////////////////////////////////////// Kapı Açılış   ///////////////////////////////////
void openDoor( int setDelay ) {
  digitalWrite(blueLed, LED_OFF); // 
  digitalWrite(redLed, LED_OFF); // 
  digitalWrite(greenLed, LED_ON); //
  digitalWrite(relay, LOW); // Kapı Açılıyor
  delay(setDelay); // verilen saniye kadar açık tut
  digitalWrite(relay, HIGH); // 
  delay(2000); //

///////////////////////////////////////// Başarısız Erişim  ///////////////////////////////////
void failed() {
  digitalWrite(greenLed, LED_OFF); // 
  digitalWrite(blueLed, LED_OFF); // 
  digitalWrite(redLed, LED_ON); //
  delay(1200);
}
