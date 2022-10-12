/*******************************  LIBRERÍAS Módulo De Recolección De Datos  *******************************/
#include <Adafruit_Fingerprint.h> //Librería lector huella
#include <SoftwareSerial.h>       //Librería para acceder a la comunicación serial en pines digitales
#include <LiquidCrystal_I2C.h>    //Librería para la pantalla LCD
#include <SD.h>                   //Librería para usar las funcionalidades de SD
#include "RTClib.h"               //Librería para el reloj externo rtc
#include "ESP.h"                  //Librería para acceder a funciones del ESP
#include <Adafruit_MLX90614.h>    //Librería sensor de temperatura
#include <WiFiManager.h>          //Librería para administrar el Wifi y cambio de conexión
#include <Firebase_ESP_Client.h>  //Librería para la conexion a Firebase
#include <addons/TokenHelper.h>   //Proporciona información del proceso de generación del token de Firebase 
#include <ESP.h>   //Proporciona información del proceso de generación del token de Firebase 
#include <ESP32Ping.h>

/**********************************  DEFINICIONES Y VARIABLES GLOBALES  *********************************/

/********************** Definicion de constantes **********************/

/*  Define el API Key */
#define API_KEY ""
/* Define el project ID */
#define FIREBASE_PROJECT_ID ""
/* Define el email y contraseña del usuario existente */
#define USER_EMAIL ""
#define USER_PASSWORD ""

/* Definición de constantes para pulsadores, sensor y cantidad de usuarios */
#define NUM_MAX_REGISTRO 20
const int PULL1 = 35;         //Cambio de pantalla
const int PULL2 = 34;         //Opciones
const int PULL3 = 33;         //aceptar generacion de registro
const int PULL4 = 32;         //cancelar generacion de registro
const int NUM_USUARIOS = 30;  //Cantidad de usuarios en el sistema
const int SENSOR_PROX = 25;    //Pin de sensor de proximidad
const char* remote_host = "www.google.com"; //host remoto
const int SUBIR_FIREBASE = 120000;


/********************** Definicion de estructuras **********************/

/* Estructura Usuario */
struct Usuario{
  String id;
  String nombreUsuario;
  int idHuella;
  String horasDeTrabajo;
  String horaIn;
};
/* Estructura Fecha */
struct Fecha{
  DateTime hoy;
  DateTime timeToSet;
  char fecha[12];
  char hora[10];
};
/* Estructura Registro */
struct Registro{
  String path;
  String id;
  String idUsuario;
  String asistencia;
  String temperatura;
  String usuario;
  String horario;
  int numRegistro;
  String hora;
  String horasTrabajadas;
};
/* Estructura Registro Diario */
struct RegistroDiario{
  int numeroRegistro;
  String horaRegistro;
  String horasTrabajadas;
};

/***************** Definicion de Variables Globales *****************/

//////////// Variables para autenticación Firebase ///////////
/* Define objeto para autenticación */
FirebaseAuth auth;
/* Define objeto para configurar el método de autenticación */
FirebaseConfig config;


//////////////////////// Módulo RTC ////////////////////////
/* Definición del objeto rtc (Real Time Clock) */
RTC_DS3231 rtc;
/* Define array días de la semana */
String diasDeLaSemana[7] = { "Domingo", "Lunes  ", "Martes", "Miercoles", "Jueves   ", "Viernes", "Sabado" };
/* Define array nombre de los meses */
int reset[4] = { 60000, 110000 , 143000 ,190000};

//////////////////////////// LCD ////////////////////////////
/* Objeto LCD con numero de filas y columnas */
LiquidCrystal_I2C lcd(0x27,20,4);
/* variable para controlar el numero de página en lcd */
int numPagLCD = 0;

//////////////////////// FingerPrint ////////////////////////
/* pines de comunicación */
SoftwareSerial mySerial(16, 17); //tx = 16, rx = 17//
/* Objeto de lector de huella */
Adafruit_Fingerprint finger =  Adafruit_Fingerprint(&mySerial);

//////////////////////////// SD /////////////////////////////
/* Objeto definido para acceder a los archivos de SD */
File miArchivo;

///////////////////// SENSOR TEMPERATURA /////////////////////
/* Objeto para acceder al sensor de temperatura */
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

///////////////// Variables para funcionalidad /////////////////
/* Variable para el control de acceso a internet por Wifi */
WiFiManager wm;
/* Valor de tolerancia de entrada */
int toleranciaIn;
/* Cantidad de huellas registradas en el sistema */
int numHuellasRegistradas;
/* Hora del servicio (solicitada) */
String timeServer;
/* Hora para generar faltas HHmmss */
int horaRegistroFaltas = 160000;
/* Identificador de usuario para eliminar o actualizar huella */
String idUsuario;
/* Identificador de huella para ser eliminada o actualizada */
uint8_t idFP;
/* Para controlar el tiempo a partir de una ejecución determinada, usado para sensor y botones */
unsigned long dataMillis = 0;
/* Indica el tiempo que transcurre tras el reinicio, usado para conexion wifi */
unsigned long dataMillisReset = 0;
/* Indica cuando se debe reiniciar por software, reiniciar en tiempo dataMillisReet */
bool debeReiniciar = false;
/* Indica el tiempo que transcurre tras el reinicio, falla subida de datos firebase */
unsigned long dataTryFirebase = 0;
/* Registros pendientes */
bool registrosPendientes = false;
/* reiniciar por firebase */
unsigned long dataResetFirebase = 0;
/* Indica cuando se debe reiniciar por software */
bool debeReiniciarFirebase = false;
/* Permite identificar si hay que generar registros diarios */
bool generarRegistroDiario = true;
/* Permite actualizar el módulo RTC  */
bool puedeActualizarFechaHora = false;

/* Array tipo de asistencia */
const String tipoDeAsistencia [4] = {"PRESENTE", "ATRASO", "FALTA", "DIA EXTRA"};
/* Array para datos de Usuario */
Usuario usuarios[NUM_USUARIOS];
/* Tiempo de reinicio en milisegundos */
unsigned long horaReinicio = 1800000;
/* Intentar conectar a firebase */


/**********************************  FUNCIÓN SETUP()  *********************************/
void setup() {
  delay(1000);
  ////////// comunicacion serial //////////
  /* inicio de la comunicacion serial */
  Serial.begin(9600);
  delay(1000);
  mySerial.begin(57600);
  delay(1000);
  while (!Serial);
  /*Inicio de comunicación serial de sensor de huella digital*/
  delay(1000);
  finger.begin(57600);
  delay(1000);
  
  /////////////// Definición de Entradas ///////////////
  pinMode(PULL1, INPUT);
  pinMode(PULL2, INPUT);
  pinMode(PULL3, INPUT);
  pinMode(PULL4, INPUT);
  pinMode(SENSOR_PROX, INPUT);
  

  ////INICIO DE COMPONENTES PRINCIPALES DEL MÓDULO DE RECOLECCIÓN DE DATOS////

  /****** PANTALLA ******/
  /*Inicio del módulo de pantalla.*/
  Serial.println("Iniciando Pantalla LCD");
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0); lcd.print(" INICIANDO  SISTEMA ");
  lcd.setCursor(0,1); lcd.print("  DE  CONTROL  DE   ");
  lcd.setCursor(0,2); lcd.print("     ASISTENCIA     ");
  
  /****** HUELLA DIGITAL ******/
  /*Inicio del sensor de huella digital. Verifica su funcionamiento*/
  Serial.print("Iniciando Sensor de Huella Digital: ");
  if (!finger.verifyPassword()){
    lcd.clear();
    lcd.setCursor(0,1); lcd.print("Error en sensor");
    lcd.setCursor(0,2); lcd.print("huella digital");
    Serial.println("Inicio Fallido!");
    delay(20000);
    ESP.restart();
  }
  Serial.println("OK!");
  
  /****** SENSOR DE TEMPERATURA ******/
  /*Inicio del sensor. Se verifica su funcionamiento*/
  Serial.print("Iniciando Sensor de temperatura: ");
  if (!mlx.begin()) {
    lcd.clear();
    lcd.setCursor(0,1); lcd.print("Error en sensor de");
    lcd.setCursor(0,2); lcd.print("temperatura");
    Serial.println("Inicio Fallido!");
    delay(20000);
    ESP.restart();
  };
  Serial.println("OK!");

  /****** MÓDULO DE TARJETA SD ******/
  /*Inicio del módulo. Se verifica su funcionamiento*/
  Serial.print("Iniciando Módulo SD: ");
  if (!SD.begin()) {
    lcd.clear();
    lcd.setCursor(0,1); lcd.print("Error en el modulo");
    lcd.setCursor(0,2); lcd.print("tarjeta SD");
    Serial.println("Inicio Fallido!");
    delay(20000);
    ESP.restart();
  }
  Serial.println("OK!");

  /****** MÓDULO RTC RELOJ ******/
  /*Inicio del módulo. Se verifica su funcionamiento*/
  Serial.print("Iniciando Módulo RTC: ");
  if (!rtc.begin()) {
    lcd.clear();
    lcd.setCursor(0,1); lcd.print("Error en modulo");
    lcd.setCursor(0,2); lcd.print("reloj integrado");
    Serial.println("Inicio Fallido!");
    delay(20000);
    ESP.restart();
   }
  Serial.println("OK!");
  delay(15000);
  lcd.setCursor(0,3); lcd.print("***Cargando Datos***");

  /////////////////INICIO DE CONEXION A INTERNET/////////////////
  /* Conexion a internet por WifiManager */
  WiFi.mode(WIFI_STA);
  wm.setConfigPortalBlocking(false);
  if(wm.autoConnect("AutoConnectAP")){
    Serial.print("Módulo conectado a la red con la IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Configurando el portal");
  }
  ///////////////// AUTENTICACIÓN EN FIREBASE /////////////////

  /* Conexión a Firebase */
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  /* asignación del api key (requerido) */
  config.api_key = API_KEY;
  /* asignación de credenciales para autenticación */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  /* Asignación de la función callback para el proceso de generación de tokens */
    /*uso de libreria addons/TokenHelper.h*/
  config.token_status_callback = tokenStatusCallback; 
  /* Proceso de autenticación */
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(false);
  
  /* En el caso de que se cuente con conexion a internet,
      se suben los registros faltantes */
  if(Firebase.ready() && Ping.ping(remote_host,2)){
    cargarRegistrosFaltantesFirebase();
  } else {
    registrosPendientes= true;
    dataTryFirebase = millis();
  }

  ////////////////// FUNCIONES ADICIONALES //////////////////  
  /* Leer preferencias */
  leerPreferencias();
  /* Actualizar hora del Módulo RTC */
  setModuloRTC();
  /* Verificar los archivos diarios */
  verificarArchivosDiarios();
  /* obtener informacion desde el archivo usuarios.txt */
  getDataUsuarios();
  /* limpiar pantalla lcd */
  lcd.clear();
  finger.getTemplateCount();
  Serial.print("Numeros de usuarios detectados: ");

  Serial.println(finger.templateCount);
}

/**********************************  FUNCIÓN LOOP()  *********************************/
void loop() {
  wm.process();
  getIDHuella();
  if(numPagLCD == 0){
    imprimirFechaLCD();  
  } else {
    imprimirConexion();    
  }
  if(digitalRead(PULL1) == HIGH) {
    if(numPagLCD == 0 ) {
      lcd.clear();
      numPagLCD = 1;
    } else {
      lcd.clear();
      numPagLCD = 0;
    }
    delay(500);
  }
  if (digitalRead(PULL2) == HIGH) {
    leerOpciones();
  }
  gestionRegistrosPendientes();
  gestionReset();
  gestionFaltas();
}

void gestionFaltas(){
  DateTime hoy = rtc.now();
  char hora[10];
  sprintf(hora,"%02d%02d%02d",hoy.hour(),hoy.minute(),hoy.second());
  int horaActual = atoi(hora);
  if((horaActual >= horaRegistroFaltas && generarRegistroDiario) &&  hoy.dayOfTheWeek() != 0 && hoy.dayOfTheWeek() != 6){
    Serial.println("Generar Registros Faltantes");
    generarRegistrosFaltantes();
    generarRegistroDiario = false;
    Serial.println("Se generaron registros faltantes");
  }
  if(horaActual < horaRegistroFaltas && !generarRegistroDiario){
    Serial.println("Esperando para generar archivos faltantes");
    Serial.println(horaActual);
    Serial.println(horaRegistroFaltas);
    generarRegistroDiario = true;
  }
}

void gestionRegistrosPendientes(){
  if(registrosPendientes){
    if(Firebase.ready() && Ping.ping(remote_host,1) && ((millis() - dataTryFirebase)> SUBIR_FIREBASE)){
      Serial.println("Subiendo datos..............");
      if(cargarRegistrosFaltantesFirebase()){
        dataTryFirebase = 0;
        dataResetFirebase = 0;
        registrosPendientes = false;
        debeReiniciar = false;
        debeReiniciarFirebase = false;
      } else {
        dataTryFirebase = millis();
        debeReiniciar = true;
        debeReiniciarFirebase = true;
        registrosPendientes = true;
      }
    }
  }
}
void gestionReset(){
  DateTime hoy = rtc.now();
  char hora[10];
  sprintf(hora,"%02d%02d%02d",hoy.hour(),hoy.minute(),hoy.second());
  int horaActual = atoi(hora);
  if((horaActual >= reset[0] && horaActual <= reset[0]+15) 
  || (horaActual >= reset[1] && horaActual <= reset[1]+15) 
  || (horaActual >= reset[2] && horaActual <= reset[2]+15)
  || (horaActual >= reset[3] && horaActual <= reset[3]+15)) {
    Serial.println("Reinicio periódico");
    ESP.restart();
  }
  bool p = Ping.ping(remote_host,1);
  if(!Firebase.ready() || WiFi.status() != 3 || !p){
    if(!debeReiniciar){
      dataMillisReset = millis();
    }
    debeReiniciar = true;
  } else {
    debeReiniciar = false;
  }
  if(debeReiniciar && (millis() - dataMillisReset)> horaReinicio){
    Serial.println("Reinicio por wifi");
    ESP.restart();
  }
  if(debeReiniciarFirebase && (millis() - dataResetFirebase)> horaReinicio){
    Serial.println("Reinicio por firebase");
    ESP.restart();
  }
}

/***************  ***************/
void setModuloRTC(){
  if(puedeActualizarFechaHora){
    String fecha = getSubCadena(timeServer,'T',0);
    String hora = getSubCadena(timeServer,'T',1);
    hora = getSubCadena(hora, '.' ,0);
    int yearS = getSubCadena(fecha,'-',0).toInt();
    int mesS = getSubCadena(fecha,'-',1).toInt();
    int diaS = getSubCadena(fecha,'-',2).toInt();
    int horaS = getSubCadena(hora,':',0).toInt();
    int minutoS = getSubCadena(hora,':',1).toInt();
    int segS = getSubCadena(hora, ':', 2).toInt();
    DateTime timeToSet = DateTime (yearS,mesS,diaS,horaS,minutoS,segS) - TimeSpan (0,5,0,0);
    char formato[] = "YYMMDD-hh:mm:ss";
    Serial.println(timeToSet.toString(formato));
    rtc.adjust(timeToSet);
  }
}

/***************  ***************/
bool actualizarDocumentoFirebase (FirebaseJson contenido, String pathDocumento, String mask) {
  FirebaseData fb;
  if (Firebase.Firestore.patchDocument(&fb, FIREBASE_PROJECT_ID, "" , pathDocumento.c_str(), contenido.raw(), mask.c_str())){
    Serial.printf("Documento Actualizado\n");
    return true;
  } else {
    Serial.println(fb.errorReason());
    return false;
  }
}

/***************  ***************/
bool crearDocumentoFirebase (FirebaseJson contenido, String pathDocumento) {
  FirebaseData fb;
  if (Firebase.Firestore.createDocument(&fb, FIREBASE_PROJECT_ID, "" , pathDocumento.c_str(), contenido.raw())){
    Serial.printf("Documento Creado \n");
    return true;
  } else {
    String error = fb.errorReason();
    Serial.println(error);
    return false;
  }
}

/***************  ***************/
void guardarHuellaFIrebase(){
  String documentPath = "usuarios/"+idUsuario;
  FirebaseJson content;
  content.set("fields/idHuella/integerValue", idFP); 
  Serial.print("Update a document... ");
  Serial.println(content.toString(Serial, true));
  actualizarDocumentoFirebase(content, documentPath, "idHuella");
}

/***************  ***************/
void getIDHuella() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return;
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  {
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Error en leer huella");
    delay(3000);
    lcd.clear();
    numPagLCD = 0;
    return;
  }
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  {
    Serial.println("El usuario no existe");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ERROR en leer huella");
    lcd.setCursor(0,2);
    lcd.print("EL USUARIO NO EXISTE");
    delay(3000);
    lcd.clear();
    numPagLCD = 0;
    return;
  }
  numPagLCD = 0;
  generarDatosRegistro(finger.fingerID,rtc.now(),"",false);
}

/***************  ***************/
String generarDatosRegistro (int idHuella, DateTime fecha, String idUsuario, bool esFalta){
  if(idHuella != -1){
    Serial.print("ID encontrado #"); Serial.println(idHuella);
  } else {
    Serial.print("ID encontrado #"); Serial.println(idUsuario);
  }
  String path;
  Usuario user;
  String horaRegistro;
  if(idHuella != -1 && !esFalta && idUsuario != "") {
    user = getUsuario(idHuella,"");
  } else {
    user = getUsuario(idHuella,idUsuario);
  }
  if(user.id != ""){
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("NOMBRE DE USUARIO:");
    lcd.setCursor(0,1); lcd.print(user.nombreUsuario.substring(0,19));
    verificarArchivosDiarios();
    RegistroDiario registroDiario = getRegistroDiario(user.id);
    Serial.print("Numero de registro: "); Serial.println(registroDiario.numeroRegistro);
    Registro registro = getRegistro(registroDiario.numeroRegistro, user,getFecha(fecha),esFalta);
    registro.horasTrabajadas = calculoHorasTrabajadas(registroDiario,fecha);
    lcd.setCursor(0,2); lcd.print("Creando Registro    ");
    lcd.setCursor(0,3); lcd.print("                    ");
    String minutos = String(fecha.minute());
    String horas = String(fecha.hour());
    String seg = String(fecha.second());
    if(registroDiario.numeroRegistro == 0){
      lcd.setCursor(0,3); lcd.print("Estado: "); lcd.print(registro.asistencia);
    }
    if(fecha.minute() < 10){
      minutos = "0" + String(fecha.minute());
    }
    if(fecha.hour() < 10){
      horas = "0" + String(fecha.hour());
    }
    if(fecha.second() < 10){
      seg = "0" + String(fecha.second());
    }
    horaRegistro = horas + ":" + minutos + ":" + seg;
    delay(2000);
    if(!esFalta){
      if(registroDiario.numeroRegistro > NUM_MAX_REGISTRO){
        lcd.clear();
        lcd.setCursor(0,0); lcd.print("Ya se han generado");
        lcd.setCursor(0,1); lcd.print("los registros de ");
        lcd.setCursor(0,2); lcd.print("entrada y salida");
        Serial.println("Ya se ha generado un nuevo registro");
        delay(4000);
        lcd.clear();
        return "";
      }
      lcd.clear();
      if(registroDiario.numeroRegistro % 2 == 0){
        lcd.setCursor(0,0); lcd.print("REGISTRO ENTRADA(");lcd.print(registroDiario.numeroRegistro+1);lcd.print(")");
      } else {
        lcd.setCursor(0,0); lcd.print("REGISTRO SALIDA(");lcd.print(registroDiario.numeroRegistro+1);lcd.print(")");
      }
      lcd.setCursor(0,1); lcd.print("Hora: "); lcd.print(horaRegistro);
      lcd.setCursor(0,2); lcd.print("Total Horas:"); lcd.print(registro.horasTrabajadas);
      lcd.setCursor(0,3); lcd.print("Guardar Registro ?");
      dataMillis = 0;
      dataMillis = millis();
      do{
        if(digitalRead(PULL3) == HIGH) {
          break;
        }
        if(digitalRead(PULL4) == HIGH){
          lcd.clear();
          lcd.setCursor(0,1); lcd.print("Cancelando Registro");
          delay(4000);
          return "";
        }
      } while(millis() - dataMillis < 10000);
      lcd.clear();
      lcd.setCursor(0,1); lcd.print("Creando Registro");
    } else {
      lcd.clear();
      lcd.setCursor(0,1); lcd.print("Generando Falta");
    }

    if (Firebase.ready() && Ping.ping(remote_host,2) && !registrosPendientes){
      Serial.println("--------SUBIR A FIREBASE -------------");
      path = guardarRegistroFirebase(registro);
      if(path == ""){
        path = guardarRegistroSinInternet(registro);
        dataTryFirebase = millis();
        registrosPendientes = true;
      }
    } else {
      Serial.println("--------  GUARDAR EN SD  -------------");
      path = guardarRegistroSinInternet(registro);
      if(dataResetFirebase == 0){
        dataResetFirebase = millis();  
      }
      if(dataTryFirebase == 0){
        dataTryFirebase = millis();  
      }
      registrosPendientes = true;
      debeReiniciarFirebase = true;
    }
    if(path != ""){
      String pathRegistroDiario = path + "_" + horaRegistro + "_" + registro.horasTrabajadas;
      if(registro.asistencia == "FALTA"){
          pathRegistroDiario = path + "_" + horaRegistro + "_FALTA";
      }
      guardaRegistroDiario(pathRegistroDiario);
      lcd.clear();
      lcd.setCursor(0,1); lcd.print("El registro se ha ");
      lcd.setCursor(0,2); lcd.print("guardado con exito");
      delay(3000); lcd.clear();
      return path; 
    } else {
      debeReiniciar = true;
      dataMillisReset = millis();
      horaReinicio = 120000;
      lcd.clear();
      lcd.setCursor(0,1); lcd.print("Error de generacion");
      lcd.setCursor(0,2); lcd.print("en el registro");
      delay(3000); lcd.clear();
      return ""; 
    }
  } else {
    Serial.println("No se ha encontrado usuario en la base de datos");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ERROR en leer huella");
    lcd.setCursor(0,2);
    lcd.print("EL USUARIO NO EXISTE");
    delay(3000); lcd.clear();
    return "";
  }  
}

/***************  ***************/
String calculoHorasTrabajadas(RegistroDiario registroDiario, DateTime fecha){
  
  if(registroDiario.numeroRegistro == 0){
    return "00:00:00";
  }
  if(registroDiario.numeroRegistro % 2 == 0){
    return registroDiario.horasTrabajadas;
  }
  int horaTrabajada = 0;
  int minTrabajada = 0;
  int segTrabajada = 0;
  int horaEntrada = 0;
  int minEntrada = 0;
  int segEntrada = 0;
  int horaAcumulada = 0;
  int minAcumulada = 0;
  int segAcumulada = 0;
  int horaTotal = 0; 
  int minTotal = 0;
  int segTotal = 0;
  
  horaEntrada = getSubCadena(registroDiario.horaRegistro,':',0).toInt();
  minEntrada = getSubCadena(registroDiario.horaRegistro,':',1).toInt();
  segEntrada = getSubCadena(registroDiario.horaRegistro,':',2).toInt();
  if(registroDiario.horasTrabajadas != "00:00:00"){
    horaTrabajada = getSubCadena(registroDiario.horasTrabajadas,':',0).toInt();
    minTrabajada = getSubCadena(registroDiario.horasTrabajadas,':',1).toInt();
    segTrabajada = getSubCadena(registroDiario.horasTrabajadas,':',2).toInt(); 
  }
  horaAcumulada = fecha.hour() - horaEntrada;
  minAcumulada = fecha.minute() - minEntrada;
  segAcumulada = fecha.second() - segEntrada;
  Serial.println(horaEntrada);
  Serial.println(minEntrada);
  Serial.println(segEntrada);
  Serial.println(horaTrabajada);
  Serial.println(minTrabajada);
  Serial.println(segTrabajada);
  Serial.println(horaAcumulada);
  Serial.println(minAcumulada);
  Serial.println(segAcumulada);
  if(segAcumulada < 0){
    minAcumulada--;
    segAcumulada =  60 + segAcumulada;
  }
  if(minAcumulada < 0){
    horaAcumulada--;
    minAcumulada =  60 + minAcumulada;
  }
  Serial.println(horaAcumulada);
  Serial.println(minAcumulada);
  Serial.println(segAcumulada);
  horaTotal = horaTrabajada + horaAcumulada; 
  minTotal = minTrabajada + minAcumulada;
  segTotal = segTrabajada + segAcumulada;
  if(segTotal >= 60){
    minTotal++;
    segTotal =  segTotal - 60;
  }
  if(minTotal >= 60){
    horaTotal++;
    minTotal =  minTotal - 60;
  }
  String hora = "0"+ String(horaTotal);
  String minutos = "0"+ String(minTotal);
  String segundos = "0"+ String(segTotal);
  
  return hora.substring(hora.length()-2,hora.length()) + ":" + minutos.substring(minutos.length()-2,minutos.length()) + ":" + segundos.substring(segundos.length()-2,segundos.length()); ;
}

/***************  ***************/
void verificarArchivosDiarios(){
  miArchivo = SD.open("/registrosDiarios.txt");
  bool borrarArchivos = true;
  String bufferRegistros;
  String auxLectura;
  Fecha fecha;
  fecha = getFecha(rtc.now());
  Serial.print("Fecha: ");
  Serial.println(fecha.fecha);
  
  if(miArchivo){
    while (miArchivo.available()){
      auxLectura = miArchivo.readStringUntil('\n'); 
      auxLectura = auxLectura.substring(0,auxLectura.length()-1);
      if(auxLectura.indexOf(fecha.fecha)>0){
        borrarArchivos = false;
      }
    }
    miArchivo.close();
    
    if(borrarArchivos){
      Serial.println("Borrando registros diarios...");
      miArchivo = SD.open("/registrosDiarios.txt",FILE_WRITE);
      if(miArchivo){
        miArchivo.print("");
        miArchivo.close();  
      }
    } else {
      Serial.println("No debe borrar registros diarios...");
    }
  } else {
    Serial.println("NO SE PUEDEN LEER LOS REGISTROS DIARIOS.");
  }
}

/***************  ***************/
void guardaRegistroDiario(String pathDocumento){
  if(pathDocumento != ""){
    miArchivo = SD.open("/registrosDiarios.txt", FILE_APPEND);
    if(miArchivo){
      miArchivo.println(" "+pathDocumento);
      Serial.println("Registro guardado: " + pathDocumento);
      miArchivo.close();    
    }  
  }
}

/***************  ***************/
Registro getRegistro(int numeroRegistro, Usuario user, Fecha fechaHoy, bool esFalta){
  Registro registro;
  registro.id = String(fechaHoy.fecha)+user.id;
  registro.path = "registros/" + registro.id;
  registro.idUsuario = user.id;
  registro.numRegistro = numeroRegistro;
  registro.usuario = user.nombreUsuario;
  registro.horario = user.horasDeTrabajo;
  registro.hora = fechaHoy.timeToSet.timestamp(DateTime::TIMESTAMP_FULL)+"Z";
  if(numeroRegistro % 2 == 0){
    String temperatura;
    if(!esFalta){
      temperatura = leerTemperatura();
    } else {
      temperatura = "-";
    }
    String asistenciaG = generarEstadoAsistencia(numeroRegistro, fechaHoy, user.horaIn, esFalta);
    registro.asistencia = asistenciaG;
    registro.temperatura = temperatura;
  }
  return registro;
}

/***************  ***************/
RegistroDiario getRegistroDiario(String idUsuario){
  RegistroDiario registroDiario;
  miArchivo = SD.open("/registrosDiarios.txt");
  String buffer;
  registroDiario.numeroRegistro = 0;
  String datoRecuperado;
  if (miArchivo) {
    while (miArchivo.available()) {
      buffer = miArchivo.readStringUntil('\n');
      if(buffer.indexOf(idUsuario)>0){
        datoRecuperado = getSubCadena(buffer,'_',1);
        registroDiario.horaRegistro = datoRecuperado;
        datoRecuperado = getSubCadena(buffer,'_',2);
        registroDiario.horasTrabajadas = datoRecuperado.substring(0,datoRecuperado.length()-1);
        registroDiario.numeroRegistro++;
      }
    }
    miArchivo.close(); 
  } else {
    Serial.println("Error en leer el numero de registros diario..");
    registroDiario.horaRegistro = "";
    registroDiario.horasTrabajadas = "";
    registroDiario.numeroRegistro = -1;
  }
  if(registroDiario.horasTrabajadas == "FALTA"){
    registroDiario.numeroRegistro = NUM_MAX_REGISTRO + 1;
  }
  return registroDiario;
}

/***************  ***************/
String guardarRegistroFirebase(Registro registro){
  String pathDocumento = registro.path;
  Serial.println("Documento: " + pathDocumento);
  if (Firebase.ready() && Ping.ping(remote_host,2)){
    FirebaseJson content;
    if(registro.numRegistro == 0){
      //id del registro
      content.set("fields/id/stringValue", registro.id);
      //idUsuario
      content.set("fields/idUsuario/stringValue", registro.idUsuario);
      //Usuario
      content.set("fields/usuario/stringValue", registro.usuario);
      //temperatura
      content.set("fields/temperatura/arrayValue/values/[0]/stringValue", registro.temperatura);
      //Asistencia
      content.set("fields/asistencia/arrayValue/values/[0]/stringValue", registro.asistencia);
      //horas trabajadas
      content.set("fields/horasTrabajadas/stringValue", registro.horasTrabajadas);
      //horas trabajadas
      content.set("fields/horario/stringValue", registro.horario);
      //hora
      String arraySet = "fields/hora/arrayValue/values/[0]/timestampValue";
      content.set(arraySet, registro.hora);
      Serial.println("Creando documento... ");
      Serial.println(content.toString(Serial, true));
      if(registro.numRegistro == -1){
        if(actualizarDocumentoFirebase(content, pathDocumento, "")){
          return pathDocumento;
        }  
      } else {
        if(crearDocumentoFirebase(content, pathDocumento)){
          return pathDocumento;
        }  
      }
    } else {
      if(registro.numRegistro % 2 == 0){
        //hora de registro
        content.set("values/[0]/timestampValue", registro.hora);
        if(agregarArrayFirebase(content, pathDocumento,"hora")){ 
          content.clear();
          delay(500);
          //horas trabajadas
          content.set("values/[0]/stringValue", registro.temperatura);
          if(agregarArrayFirebase(content,pathDocumento,"temperatura")){
            return pathDocumento;
          }
        }
      } else {
        Serial.println("Actualizando documento... ");
        //hora de registro
        content.set("values/[0]/timestampValue", registro.hora);
        if(agregarArrayFirebase(content, pathDocumento,"hora")){ 
          content.clear();
          delay(500);
          //horas trabajadas
          content.set("fields/horasTrabajadas/stringValue", registro.horasTrabajadas);
          if(actualizarDocumentoFirebase(content,pathDocumento,"horasTrabajadas")){
            return pathDocumento;
          }
        }  
      }
    }
  }
  return "";
}

/***************  ***************/
String guardarRegistroSinInternet(Registro registro){
  miArchivo = SD.open("/registrosSinSubir.txt",FILE_APPEND);
  String buffer;
  String registrosS;
  if(miArchivo){
    String documentPath = registro.path;
    registrosS = " " + String(registro.numRegistro) +"_" + registro.id.substring(0,8) + "_" + registro.idUsuario + "_" + registro.hora;
    if(registro.numRegistro == 0){
      registrosS = registrosS + "_" + registro.horario + "_" +registro.horasTrabajadas + "_" + registro.usuario+ "_" + registro.temperatura + "_" + registro.asistencia;
    } else if(registro.numRegistro %2 == 0){
      registrosS = registrosS + "_" + registro.temperatura;
    } else {
      registrosS = registrosS + "_" + registro.horasTrabajadas;
    }
    Serial.print(" numRegistro:");
    Serial.println(registro.numRegistro);
    Serial.println(" id: "+registro.id);
    Serial.println(" idUsuario: " + registro.idUsuario);
    Serial.println(" hora: " + registro.hora);
    Serial.println(" Horas Trabajadas:" + registro.horasTrabajadas);
    if(registro.numRegistro == 0){
      Serial.println(" temperatura:" + registro.temperatura);
      Serial.println(" asistencia:" + registro.asistencia);
    }
    Serial.println(" usuario:"+registro.usuario);
    miArchivo.println(registrosS);
    miArchivo.close();
    return documentPath;
  } else {
    Serial.println("Error en abrir el archivo registrosSinSubir.txt");
    return "";
  }
}

/***************  ***************/
bool cargarRegistrosFaltantesFirebase(){
  miArchivo = SD.open("/registrosSinSubir.txt");
  String buffer;
  Registro registro;
  String datoRecuperado;
  if (miArchivo) {
    while (miArchivo.available()) {
      buffer = miArchivo.readStringUntil('\n');
      registro.numRegistro = getSubCadena(buffer,'_',0).toInt();
      String fecha = getSubCadena(buffer,'_',1);
      registro.idUsuario = getSubCadena(buffer,'_',2);
      registro.id = fecha + registro.idUsuario;
      registro.hora = getSubCadena(buffer,'_',3);
      if(registro.numRegistro == 0){
        registro.horario = getSubCadena(buffer,'_',4);
        registro.horasTrabajadas = getSubCadena(buffer,'_',5);
        registro.usuario = getSubCadena(buffer,'_',6);
        registro.temperatura = getSubCadena(buffer,'_',7);
        registro.asistencia = getSubCadena(buffer,'_',8);
        registro.asistencia = registro.asistencia.substring(0,registro.asistencia.length()-1);
      } else if(registro.numRegistro %2 == 0){
        registro.temperatura = getSubCadena(buffer,'_',4);
        registro.temperatura = registro.temperatura.substring(0,registro.temperatura.length()-1);
      } else {
        registro.horasTrabajadas = getSubCadena(buffer,'_',4);
        registro.horasTrabajadas = registro.horasTrabajadas.substring(0,registro.horasTrabajadas.length()-1);
      }
      registro.path = "registros/" + registro.id;
      Serial.println(registro.path);
      Serial.println(registro.hora);
      Serial.println(registro.id);
      Serial.println(registro.asistencia);
      Serial.println(registro.temperatura);
      Serial.println(registro.usuario);
      if(guardarRegistroFirebase(registro) == ""){
        registro.numRegistro = -1;
        delay(500);
        if(guardarRegistroFirebase(registro) == ""){
          registrosPendientes = true;
          dataTryFirebase = millis();
          return false;
        }
      }
      registro. path = "";
      registro.id = "";
      registro.idUsuario = "";
      registro.asistencia = "";
      registro.temperatura = "";
      registro.usuario = "";
      registro.hora = "";
      registro.numRegistro = 0;
    }
    Serial.println("Se han cargado los datos correctamente");
    registrosPendientes= false;
    debeReiniciarFirebase = false;
    miArchivo.close( );
    miArchivo = SD.open("/registrosSinSubir.txt",FILE_WRITE);
    if(miArchivo){
      miArchivo.print("");
      miArchivo.close();  
      Serial.println("Vaciado de /registrosSinSubir: correcto");
      return true;
    } else {
      Serial.println("Vaciado de /registrosSinSubir: Incorrecto");
      return false;
    }
  } else {
    Serial.println("Error en cargar los datos de registros pendientes");
    return false;
  }
}

/***************  ***************/
String generarEstadoAsistencia(int numRegistro, Fecha hoy, String horario , bool esFalta){
  int hora = getSubCadena(horario,':',0).toInt();
  int minutos = getSubCadena(horario,':',1).toInt();
  int horaUsuario = hora*10000;
  int horaRegistrada = atoi(hoy.hora);
  if(hoy.hoy.dayOfTheWeek() == 0 || hoy.hoy.dayOfTheWeek() == 6){
    return tipoDeAsistencia[3];
  }
  if(esFalta) return tipoDeAsistencia[2];
  horaUsuario += (minutos + toleranciaIn)*100;
  Serial.println(horaUsuario);
  Serial.println(horaRegistrada);
  if(horaRegistrada <= horaUsuario || horario == "00:00"){
    return tipoDeAsistencia[0];
  } else {
    return tipoDeAsistencia[1];
  }
  return "";
}

/***************  ***************/
void leerPreferencias(){
  if (Firebase.ready() && Ping.ping(remote_host,2)){
    FirebaseData fbdo;
    Serial.print("Commit a document (set server value, update document)... ");

    //La matriz dinámica del objeto de escritura fb_esp_firestore_document_write_t
    std::vector<struct fb_esp_firestore_document_write_t> writes;

    //Un objeto de escritura que se escribirá en el documento.
    struct fb_esp_firestore_document_write_t transform_write;

    //Configure el tipo de operación de escritura del objeto de escritura.
    transform_write.type = fb_esp_firestore_document_write_type_transform;

    //Establecer la ruta del documento del documento para escribir (transformar)
    transform_write.document_transform.transform_document_path = "preferencias/sistema";

    //Establecer una transformación de un campo del documento.
    struct fb_esp_firestore_document_write_field_transforms_t field_transforms;

    //Establezca la ruta del campo para escribir.
    field_transforms.fieldPath = "timeServer";

    //Establezca el tipo de transformación.
    field_transforms.transform_type = fb_esp_firestore_transform_type_set_to_server_value;

    //Establezca el contenido de la transformación, valor del servidor para este caso.
     //Ver https://firebase.google.com/docs/firestore/reference/rest/v1/Write#servervalue
    field_transforms.transform_content = "REQUEST_TIME"; //set timestamp to "test_collection/test_document/server_time"

    //Agregue un objeto de transformación de campo a un objeto de escritura.
    transform_write.document_transform.field_transforms.push_back(field_transforms);

    //Agregue un objeto de escritura a una matriz de escritura.
    writes.push_back(transform_write);
    if (Firebase.Firestore.commitDocument(&fbdo, FIREBASE_PROJECT_ID, "", writes, ""))
        Serial.printf("OK!\n%s\n\n", fbdo.payload().c_str());
    else
        Serial.println(fbdo.errorReason());
        
    String collectionId = "preferencias/sistema";
    String mask = "numeroDeHuellasRegistradas,toleranciaIn,timeServer,horaRegistroFaltas";
    
    Serial.print("Listando los documentos en la colección: preferencias");
    
    if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", collectionId.c_str(), 100 , "", "" , mask.c_str(), false)){
      puedeActualizarFechaHora = true;
      Serial.printf("OK!\n%s\n\n", fbdo.payload().c_str());
      // Se abre el archivo. Solo se puede abrir un archivo a la vez,
      miArchivo = SD.open("/preferencias.txt", FILE_WRITE);
      // si el archivo se abrió bien, se escribirá en él
      if (miArchivo) {
        Serial.print("Escribiendo en usuarios.txt...");
        miArchivo.println(fbdo.payload().c_str());
        // Cerrar el archivo
        miArchivo.close();
        Serial.println("done.");
      } else {
        // si el archivo no se abre, imprime un error
        Serial.println("Error al abrir preferencias.txt");
      }
    } else {
      Serial.println(fbdo.errorReason()); 
    }
  }
  miArchivo = SD.open("/preferencias.txt");
  String auxLectura;
  String buffer;
  if (miArchivo) {
    // leer todo el contenido del archivo.
    while (miArchivo.available()) {
      buffer = miArchivo.readStringUntil('\n');

      if(buffer.indexOf("numeroDeHuellasRegistradas")>0){
        buffer = miArchivo.readStringUntil('\n');
        auxLectura = getSubCadena(buffer,'"',3);  
        numHuellasRegistradas = auxLectura.substring(0,auxLectura.length()-1).toInt();
        buffer = miArchivo.readStringUntil('\n');
      }
      if(buffer.indexOf("toleranciaIn")>0){
        buffer = miArchivo.readStringUntil('\n');
        auxLectura = getSubCadena(buffer,'"',3);
        toleranciaIn = auxLectura.substring(0,auxLectura.length()-1).toInt();
        buffer = miArchivo.readStringUntil('\n');
      }
      if(buffer.indexOf("timeServer")>0){
        buffer = miArchivo.readStringUntil('\n');
        auxLectura = getSubCadena(buffer,'"',3);
        timeServer =  auxLectura.substring(0,auxLectura.length()-1); 
        buffer = miArchivo.readStringUntil('\n');
      }
      if(buffer.indexOf("horaRegistroFaltas")>0){
        buffer = miArchivo.readStringUntil('\n');
        auxLectura = getSubCadena(buffer,'"',3);
        horaRegistroFaltas =  auxLectura.substring(0,auxLectura.length()-1).toInt(); 
        buffer = miArchivo.readStringUntil('\n');
      }
    }
    // cierra el archivo
    miArchivo.close();
  } else {
  // si el archivo no se abre, imprime un error:
  Serial.println("Error al abrir preferencias.txt");
  }
}

/***************  ***************/
void leerOpciones(){
  if (Firebase.ready() && Ping.ping(remote_host,2)){
    FirebaseData fbdo;
    FirebaseJson content;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Leyendo BaseDeDatos");
    String collectionId = "preferencias/sistema";
    String buffer;
    String mensaje;
    Serial.println("Listando los documentos de la coleeción... ");
    
    if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", collectionId.c_str(), 100 , "", "" , "puedeCrear" , false)){
      buffer = fbdo.payload().c_str();
      Serial.println(buffer);
      if(buffer.indexOf("puedeCrear")>0 && buffer.indexOf("true")>0){
        Serial.println("puedeCrear");
        lcd.setCursor(0,2);
        lcd.print("Crear Huella");
        delay(1000);
        obtenerUsuariosDesdeFirebase();
        getDataUsuarios();
        if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", collectionId.c_str(), 100 , "", "" , "numeroDeHuellasRegistradas" , false)){
          Serial.print("Numero de Huellas: ");
          buffer = fbdo.payload().c_str();
          Serial.println(getSubCadena(buffer,'"',11).toInt());
          if(crearPlantillaHuella(getUsuario(-1,""),getSubCadena(buffer,'"',11).toInt())){
            lcd.clear();
            lcd.setCursor(0,1); lcd.print("Actualizando datos");
            idFP++;
            content.set("fields/numeroDeHuellasRegistradas/integerValue", idFP); 
            content.set("fields/puedeCrear/booleanValue", false);
            actualizarDocumentoFirebase(content, collectionId, "numeroDeHuellasRegistradas,puedeCrear");  
            obtenerUsuariosDesdeFirebase();
            getDataUsuarios();
          }
        }
        lcd.clear();
        return;
      }
    }
    if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", collectionId.c_str(), 100 , "", "" , "puedeActualizar" , false)){
      buffer = fbdo.payload().c_str();
      Serial.println(buffer);
      if(buffer.indexOf("puedeActualizar")>0 && buffer.indexOf("true")>0){
        Serial.println("puedeActualizar");
        lcd.setCursor(0,2);
        lcd.print("Actualizar Huella");
        delay(1000);
        if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", collectionId.c_str(), 100 , "", "" , "idHuellaAccion" , false)){
          Serial.print("Numero de Huellas: ");
          buffer = fbdo.payload().c_str();
          int idHuellaAccion = getSubCadena(buffer,'"',11).toInt();
          Serial.println(getSubCadena(buffer,'"',11).toInt());
          if(eliminarHuellaDigital(idHuellaAccion)){
            if(crearPlantillaHuella(getUsuario(idHuellaAccion,""),idHuellaAccion)){
              lcd.clear();
              lcd.setCursor(0,1); lcd.print("Actualizando datos");
              
              String documentPath = "preferencias/sistema";
              content.set("fields/idHuellaAccion/integerValue", 0);
              content.set("fields/puedeActualizar/booleanValue", false);
              actualizarDocumentoFirebase (content, collectionId, "idHuellaAccion,puedeActualizar");
            }
          }
        }
        lcd.clear();
        return;
      }
    }
    if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", collectionId.c_str(), 20 , "", "" , "puedeEliminar" , false)){
      buffer = fbdo.payload().c_str();
      Serial.println(buffer);
      if(buffer.indexOf("puedeEliminar")>0 && buffer.indexOf("true")>0){
        Serial.println("puedeEliminar");
        lcd.setCursor(0,2);
        lcd.print("Eliminar Huella");
        delay(1000);
        if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", collectionId.c_str(), 20 , "", "" , "idHuellaAccion" , false)){
          Serial.print("Numero de Huellas: ");
          buffer = fbdo.payload().c_str();
          int idHuellaAccion = getSubCadena(buffer,'"',11).toInt();
          Serial.println(getSubCadena(buffer,'"',11).toInt());
          if(eliminarHuellaDigital(idHuellaAccion)){
            lcd.clear();
            lcd.setCursor(0,1); lcd.print("Actualizando datos");
            String documentPath = "preferencias/sistema";
            content.set("fields/idHuellaAccion/integerValue", 0); 
            content.set("fields/puedeEliminar/booleanValue", false); 
            actualizarDocumentoFirebase (content, collectionId, "idHuellaAccion,puedeEliminar");  
          }          
        }
        lcd.clear();
      return;
      }
    }
    if (Firebase.Firestore.listDocuments(&fbdo, FIREBASE_PROJECT_ID, "", collectionId.c_str(), 20 , "", "" , "actualizarUsuarios" , false)){
      buffer = fbdo.payload().c_str();
      Serial.println(buffer);
      if(buffer.indexOf("actualizarUsuarios")>0 && buffer.indexOf("true")>0){
        Serial.println("actualizarUsuarios");
        lcd.setCursor(0,2);
        lcd.print("Actualizar Usuarios");
        if(obtenerUsuariosDesdeFirebase()){
          lcd.clear();
          lcd.setCursor(0,1); lcd.print("Actualizando");
          lcd.setCursor(0,2); lcd.print(" datos");
          getDataUsuarios();
          content.set("fields/actualizarUsuarios/booleanValue", false); 
          actualizarDocumentoFirebase (content, collectionId, "actualizarUsuarios");
        }
        lcd.clear();
        return;
      }
    }
  } else {
    Serial.println("Sin conexión a Internet");
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Sin conexión a ");
    lcd.setCursor(0,1); lcd.print("Internet");
    delay(3000);lcd.clear();
  }
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("No hay opciones a ");
  lcd.setCursor(0,1); lcd.print("ejecutar");
  delay(3000);lcd.clear();
  return;  
}

/***************  ***************/
String leerTemperatura(){
    String temperatura;
    double temp;
    bool breakTemp = false;
    Serial.println("Tomando temperatura");
    lcd.setCursor(0,2); lcd.print("Favor acercarse al");
    lcd.setCursor(0,3); lcd.print("sensor");
    dataMillis = 0;
    dataMillis = millis();
    while(digitalRead(SENSOR_PROX) == HIGH){
      if(millis() - dataMillis > 3000){
        breakTemp = true;
        break;
      }
    }
    if(breakTemp){
      return "-";
    }
    lcd.setCursor(0,2); lcd.print("Tomando temperatura.");
    lcd.setCursor(0,3); lcd.print("                    ");
    delay(1000);
    temp = double(mlx.readObjectTempC());
    lcd.setCursor(0,2); lcd.print("Temperatura:  "); lcd.print(temp);
    Serial.print("Temperatura:");
    Serial.println(temp);
    delay(3000);
    return String(temp);
}

/***************  ***************/
bool eliminarHuellaDigital(int idHuellaEliminar) {
  int p = -1;
  if(Firebase.ready() && Ping.ping(remote_host,2)){
    p = finger.deleteModel(idHuellaEliminar); 
    if (p == FINGERPRINT_OK) {
      Serial.println("Huella eliminada!");
      lcd.clear();
      lcd.setCursor(0,1); lcd.print("Huella digital #"); lcd.print(idHuellaEliminar);
      lcd.setCursor(0,2); lcd.print(" eliminada..");
      delay(3000);lcd.clear();
      return true;
    }
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("ERROR: ");lcd.print(p);
    lcd.setCursor(0,1); lcd.print("Al eliminar Huella");
    lcd.setCursor(0,2); lcd.print("Vuelva a intentar..");
    delay(3000);lcd.clear();
    Serial.print("Error en Sensor Huella Digital: 0x"); Serial.println(p, HEX);
    return false;
  }
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("No hay conexion");
  lcd.setCursor(0,1); lcd.print("a internet");
  lcd.setCursor(0,2); lcd.print("Vuelva a intentar..");
  delay(3000);lcd.clear();
  return false;
}

/***************  ***************/
bool obtenerUsuariosDesdeFirebase(){
  FirebaseData fbdoUser;
  fbdoUser.setResponseSize(20480);
  if (Firebase.ready() && Ping.ping(remote_host,2)) {
    String documentPath = "usuarios";
    String mask = "name,uid,horaIn,idHuella,horasDeTrabajo";
    Serial.println("Obteniendo documentos (Usuarios) de Firebase... ");
    if (Firebase.Firestore.getDocument(&fbdoUser, FIREBASE_PROJECT_ID, "", documentPath.c_str(), mask.c_str())){
      Serial.println("Documentos Leidos");
      miArchivo = SD.open("/usuarios.txt", FILE_WRITE);
      if (miArchivo) {
        Serial.print("Escribiendo en usuarios.txt: ");
        miArchivo.println(fbdoUser.payload().c_str());
        // close the file:
        miArchivo.close();
        Serial.println("OK");
        return true;
      }
      Serial.println("Error al abrir el archivo usuarios.txt");
    } else {
      Serial.println(fbdoUser.errorReason());
    }
  }
  return false;
}

/***************  ***************/
void generarRegistrosFaltantes(){
  String bufferRegistros;
  String buffer;
  String auxLectura;
  Fecha fechaHoy;
  fechaHoy = getFecha(rtc.now());
  miArchivo = SD.open("/registrosDiarios.txt");
  if(Firebase.ready() && Ping.ping(remote_host,2)){
    if (miArchivo) {
      while (miArchivo.available()){
        auxLectura = miArchivo.readStringUntil('\n'); 
        bufferRegistros += auxLectura + "\n";
      }
      for(int i = 0; i<NUM_USUARIOS; i++){
        if(usuarios[i].id != ""){
          if(bufferRegistros.indexOf(fechaHoy.fecha+usuarios[i].id)<=0){
            if(usuarios[i].horaIn != "00:00" && usuarios[i].horasDeTrabajo != "00:00"){
              Serial.println("Generar falta para: " + usuarios[i].nombreUsuario);
              String path = generarDatosRegistro(-1, rtc.now(), usuarios[i].id, true);
              if(path != ""){
                guardaRegistroDiario(path);  
              }
            }
          }  
        } else {
          break;
        }
      }
      generarRegistroDiario = false;
      miArchivo.close();
    } else {
      Serial.println("Error al abrir el usuarios.txt");
      registrosPendientes = true;
      dataTryFirebase = millis();
    }
  }
}

/***************  ***************/
void imprimirFechaLCD(){
  DateTime hoy = rtc.now();
  lcd.setCursor(0,0); lcd.print(diasDeLaSemana[hoy.dayOfTheWeek()]);
  char fecha[12];
  sprintf(fecha,"%02d/%02d/%04d",hoy.day(),hoy.month(),hoy.year());
  char hora[10];
  sprintf(hora,"%02d:%02d:%02d",hoy.hour(),hoy.minute(),hoy.second());
  lcd.setCursor(0,1); lcd.print(fecha); lcd.print("  "); lcd.print(hora);
  lcd.setCursor(0,2); lcd.print("Ingresar Huella...");
  sprintf(hora,"%02d%02d%02d",hoy.hour(),hoy.minute(),hoy.second());
  if(debeReiniciar){
    lcd.setCursor(0,3); lcd.print("SinConexion Internet");
  } else {
    lcd.setCursor(0,3); lcd.print("                    ");
  }
}

/***************  ***************/
void imprimirConexion() {
  lcd.setCursor(0, 0);  
  if(WiFi.status() == 3){
    lcd.print("WiFi conectado");
  } else {
    lcd.print("WIFI desconectado");
  }
  lcd.setCursor(0, 1);
  lcd.print("IP: ");
  lcd.print(WiFi.localIP());
  if(debeReiniciar){
    lcd.setCursor(0, 2); lcd.print("Se reiniciara en:");
    lcd.setCursor(0, 3);lcd.print((horaReinicio - (millis() - dataMillisReset))/60000);lcd.print(" minutos");
  }
}

/***************  ***************/
Fecha getFecha(DateTime hoy){
  Fecha fechaHoy;
  fechaHoy.hoy = hoy;
  sprintf(fechaHoy.fecha,"%04d%02d%02d",fechaHoy.hoy.year(),fechaHoy.hoy.month(),fechaHoy.hoy.day());
  sprintf(fechaHoy.hora,"%02d%02d%02d",fechaHoy.hoy.hour(),fechaHoy.hoy.minute(),fechaHoy.hoy.second());
  fechaHoy.timeToSet = fechaHoy.hoy + TimeSpan(0,5,0,0);
  return fechaHoy;
}

/***************  ***************/
void getDataUsuarios(){
  Usuario auxUser;
  int indexUsuario = 0;
  String buffer;
  String auxLectura;
  boolean tieneHuella = false;
  auxUser.id = "";
  auxUser.nombreUsuario = "";
  auxUser.idHuella = -2;
  auxUser.horaIn = "";
  for(int i = 0 ; i< NUM_USUARIOS ; i++ ){
    usuarios[i].id = "";
    usuarios[i].nombreUsuario = "";
    usuarios[i].idHuella = -2;
    usuarios[i].horaIn = "";
    usuarios[i].horasDeTrabajo = "";
  }
  miArchivo = SD.open("/usuarios.txt");
  if (miArchivo) {
    //Lee el archivo hasta que no contenga ningun caracter más
    while (miArchivo.available()) {
      buffer = miArchivo.readStringUntil('\n');
      if(buffer.indexOf("/documents/usuarios/")>0){
        auxUser.id = getSubCadena(getSubCadena(buffer, '/', 6),'"',0);
        buffer = miArchivo.readStringUntil('\n');
      }
      if(buffer.indexOf("\"name\":")>0){
        buffer = miArchivo.readStringUntil('\n');
        auxLectura = getSubCadena(buffer,'"',3);
        auxUser.nombreUsuario = auxLectura.substring(0,auxLectura.length()-1); 
        buffer = miArchivo.readStringUntil('\n');
      }
      if(buffer.indexOf("\"idHuella\":")>0){
        tieneHuella = true;
        buffer = miArchivo.readStringUntil('\n');
        auxLectura = getSubCadena(buffer,'"',3);  
        auxUser.idHuella = auxLectura.substring(0,auxLectura.length()-1).toInt();
        buffer = miArchivo.readStringUntil('\n');
      }
      if(buffer.indexOf("\"horasDeTrabajo\":")>0){
        buffer = miArchivo.readStringUntil('\n');
        auxLectura = getSubCadena(buffer,'"',3);
        auxUser.horasDeTrabajo = auxLectura.substring(0,auxLectura.length()-1);  
        buffer = miArchivo.readStringUntil('\n');
              }
      if(buffer.indexOf("\"horaIn\":")>0){
        buffer = miArchivo.readStringUntil('\n');
        auxLectura = getSubCadena(buffer,'"',3);
        auxUser.horaIn = auxLectura.substring(0,auxLectura.length()-1);  
        buffer = miArchivo.readStringUntil('\n');
      }
      if(buffer.indexOf("\"createTime\":")>0){
        usuarios[indexUsuario] = auxUser;
        indexUsuario++;
        auxUser.id = "";
        auxUser.nombreUsuario = "";
        auxUser.idHuella = -2;
        auxUser.horasDeTrabajo = "";
        auxUser.horaIn = "";
      }
    }
    Serial.println("Listar Usuarios: ");
    for(int i = 0 ; i< NUM_USUARIOS ; i++ ){
      if(usuarios[i].id != ""){
        Serial.println(usuarios[i].id);
        Serial.println(usuarios[i].nombreUsuario);
        Serial.println(usuarios[i].idHuella);
        Serial.println(usuarios[i].horaIn); 
      } else{
        break;
      }
    }
    miArchivo.close();
  } else {
    Serial.println("Error al abrir el usuarios.txt");
  }
}

/***************  ***************/
Usuario getUsuario(int idHuellaUsuario, String idUsuario){
  Usuario user;
  String buffer;
  String auxLectura;
  bool encontroUsuario = false;
  bool tienehuella = false;
  for(int i = 0 ; i< NUM_USUARIOS ; i++ ){
    if(usuarios[i].id != ""){
      if(usuarios[i].id == idUsuario && idHuellaUsuario == -1) {
        Serial.print("Usuario encontrado: ");
        Serial.println(usuarios[i].nombreUsuario);  
        return usuarios[i];
      }
      if(usuarios[i].idHuella == idHuellaUsuario && idUsuario == ""){
        Serial.print("Usuario encontrado: ");
        Serial.println(usuarios[i].nombreUsuario);  
        return usuarios[i];
      }
      if(usuarios[i].idHuella == -2){
        Serial.print("Usuario encontrado: ");
        Serial.println(usuarios[i].nombreUsuario);  
        return usuarios[i];
      }
    }else{
      break;
    }
  }
  Serial.println("NO SE ENCONTRÓ EL USUARIO");
  user.id = "";
  user.nombreUsuario = "";
  user.idHuella = -2;
  user.horasDeTrabajo = "";
  user.horaIn = "";
  return user;
}

/***************  ***************/
bool crearPlantillaHuella(Usuario user,int idHuellaUsuario){
  if(user.id != ""){
    do{
      finger.getTemplateCount();
      idFP = idHuellaUsuario;
      idUsuario = user.id;
      Serial.println("Creando una Huella.......");
      Serial.print("Enrrollando ID #");
      Serial.println(idFP);
      lcd.clear();
      lcd.setCursor(0,0); lcd.print("Creando huella: "); lcd.print(idFP);
      lcd.setCursor(0,1); lcd.print("Usuario: ");
      lcd.setCursor(0,2); lcd.print(user.nombreUsuario.substring(0,20));
      lcd.setCursor(0,3); lcd.print("Ingresar Huella Dig.");
      delay(3000);
    } while(!solicitarHuella());
    return true;
  }
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("No se puede crear:"); lcd.print(idFP);
  lcd.setCursor(0,2); lcd.print("No se encuentra el");
  lcd.setCursor(0,3); lcd.print("usuario a crear");
  delay(4000); lcd.clear();
  return false;
}

/***************  ***************/
bool agregarArrayFirebase(FirebaseJson content, String path, String mask){
  FirebaseData fbdo;
  Serial.print("Commit a document (append array)... ");
  
  //La matriz dinámica del objeto de escritura fb_esp_firestore_document_write_t.
  std::vector<struct fb_esp_firestore_document_write_t> writes;
  
  //Un objeto de escritura que se escribirá en el documento.
  struct fb_esp_firestore_document_write_t transform_write;
  transform_write.type = fb_esp_firestore_document_write_type_transform;
  
  // Establecer la ruta del documento del documento para escribir (transformar)
  transform_write.document_transform.transform_document_path = path.c_str();
  
  //Establecer una transformación de un campo del documento.
  struct fb_esp_firestore_document_write_field_transforms_t field_transforms;
  
  //Establece la ruta del campo para escribir.
  field_transforms.fieldPath = mask.c_str();
  
  //Establecer el tipo de transformación.
  field_transforms.transform_type = fb_esp_firestore_transform_type_append_missing_elements;
    
  //Establecer el contenido de la transformación.
  field_transforms.transform_content = content.raw();
  
  //Agregue un objeto de transformación de campo a un objeto de escritura.
  transform_write.document_transform.field_transforms.push_back(field_transforms);
  
  //Agregue un objeto de escritura a una matriz de escritura.
  writes.push_back(transform_write);
  
  if (Firebase.Firestore.commitDocument(&fbdo, FIREBASE_PROJECT_ID, "" , writes, "")){
    Serial.printf("Array Actualizado \n");
    return true;
  } else {
    Serial.println(fbdo.errorReason());    
    return false;
  }
}

/***************  ***************/
bool solicitarHuella() {
  int p = -1;
  Serial.print("Esperando por ua huella valida para enrollar #");
  Serial.println(idFP);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      lcd.clear();
      lcd.setCursor(0,0); lcd.print("Huella 1 Guardada");
      Serial.println("Imagen capturada");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    default:
      lcd.clear();
      lcd.setCursor(0,0); lcd.print("ERROR");
      lcd.setCursor(0,1); lcd.print("Al guardar la Huella");
      lcd.setCursor(0,2); lcd.print("Vuelva a intentar..");
      delay(2000); lcd.clear();
      Serial.println("Error desconocido");
      return false;
      break;
    }
  }

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Imagen convertida");
      break;
    case FINGERPRINT_IMAGEMESS:
      lcd.clear();
      lcd.setCursor(0,0); lcd.print("ERROR: ");lcd.print(p);
      lcd.setCursor(0,1); lcd.print("Al guardar la Huella");
      lcd.setCursor(0,2); lcd.print("Vuelva a intentar..");
      delay(2000); lcd.clear();
      Serial.println("Imagen desordenada");
      return false;
    default:
      lcd.clear();
      lcd.setCursor(0,0); lcd.print("ERROR: ");lcd.print(p);
      lcd.setCursor(0,1); lcd.print("Al guardar la Huella");
      lcd.setCursor(0,2); lcd.print("Vuelva a intentar..");
      delay(2000); lcd.clear();
      Serial.println("Error desconocido");
      return false;
  }

  Serial.println("Listo para tomar 2da huella");
  delay(2000);
  lcd.setCursor(0,1); lcd.print("Retirar Huella");
  p = 0;
  delay(2000);
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  p = -1;
  
  lcd.setCursor(0,1); lcd.print("Ingresar Huella Dig.");
  Serial.println("Colocar huella");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      lcd.setCursor(0,2); lcd.print("Huella 2 Guardada");
      Serial.println("Imagen capturada");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    default:
      lcd.clear();
      lcd.setCursor(0,0); lcd.print("ERROR: ");lcd.print(p);
      lcd.setCursor(0,1); lcd.print("Al guardar la Huella");
      lcd.setCursor(0,2); lcd.print("Vuelva a intentar..");
      delay(2000); lcd.clear();
      Serial.println("Error desconocido");
      break;
    }
  }
  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Imagen convertida");
      break;
    case FINGERPRINT_IMAGEMESS:
      lcd.clear();
      lcd.setCursor(0,0); lcd.print("ERROR: ");lcd.print(p);
      lcd.setCursor(0,1); lcd.print("Al guardar la Huella");
      lcd.setCursor(0,2); lcd.print("Vuelva a intentar..");
      delay(2000); lcd.clear();
      Serial.println("Imagen demasiado desordenada");
      return false;
    default:
      lcd.clear();
      lcd.setCursor(0,0); lcd.print("ERROR: ");lcd.print(p);
      lcd.setCursor(0,1); lcd.print("Al guardar la Huella");
      lcd.setCursor(0,2); lcd.print("Vuelva a intentar..");
      delay(2000); lcd.clear();
      Serial.println("Error desconocido");
      return false;
  }
  
  Serial.print("Creando modelo de#");  Serial.println(idFP);
  delay(2000);
  
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    lcd.setCursor(0,3); lcd.print("Huellas Coinciden");
    delay(2000);
    Serial.println("Impresiones coincidentes");
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Huellas NO Coinciden");
    lcd.setCursor(0,1); lcd.print("Vuelva a intentar");
    delay(2000);lcd.clear();
    Serial.println("Las huellas no coiciden");
    return false;
  } else {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("ERROR: ");lcd.print(p);
    lcd.setCursor(0,1); lcd.print("Al guardar la Huella");
    lcd.setCursor(0,2); lcd.print("Vuelva a intentar..");
    delay(2000); lcd.clear();
    Serial.println("Error desconocido");
    return false;
  }
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Guardando ID de huella");
  
  if (Firebase.ready() && Ping.ping(remote_host,2)){
    lcd.clear();
    lcd.setCursor(0,1); lcd.print("Huella Almacenada");
    Serial.println("Huella digital guardada!");
    Serial.print("ID "); Serial.println(idFP);
    p = finger.storeModel(idFP);
    if (p == FINGERPRINT_OK) {
      lcd.clear();
      lcd.setCursor(0,1); lcd.print("Huella Almacenada");
      delay(2000); lcd.clear();
      Serial.println("Huella digital guardada en módulo!");
    } else {
      Serial.println("Huella no almacenada");
      lcd.clear();
      lcd.setCursor(0,0); lcd.print("ERROR: ");lcd.print(p);
      lcd.setCursor(0,1); lcd.print("Al guardar la Huella");
      lcd.setCursor(0,2); lcd.print("Vuelva a intentar..");
      delay(2000); lcd.clear();
      return false;
    }
    guardarHuellaFIrebase();
    Serial.println("Huella digital En firebase!");
  } else {
    Serial.println("Huella no almacenada");
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("ERROR: ");lcd.print(p);
    lcd.setCursor(0,1); lcd.print("Huella NO almacenada");
    lcd.setCursor(0,2); lcd.print("Vuelva a intentar...");
    delay(2000); lcd.clear();
  }
  lcd.setCursor(0,1); lcd.print("Hecho!");
  return true;
}

/***************  ***************/
String getSubCadena(String cadena, char separador, int index){
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = cadena.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(cadena.charAt(i)==separador || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? cadena.substring(strIndex[0], strIndex[1]) : "";
}
