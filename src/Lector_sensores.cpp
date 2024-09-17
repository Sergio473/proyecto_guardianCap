#include <dummy.h>
#include <Wire.h>
#include <Firebase.h>
#include <FirebaseESP32.h>
#include <WiFi.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050.h>
#include <PubSubClient.h>
#include <I2Cdev.h>

//iniciar mosquitto de esta forma para que funcione -> net start mosquitto o net stop mosquitto
// Configuración de los pines
#define TRIG_PIN 5
#define ECHO_PIN 18
#define DHT_PIN 15
#define BUZZER_PIN 4
#define LED_RED_PIN 23         // Nuevo pin para el LED RGB (Rojo)
#define LED_GREEN_PIN 25       // Nuevo pin para el LED RGB (Verde)
#define LED_BLUE_PIN 26        // Nuevo pin para el LED RGB (Azul)
#define VIBRATION_MOTOR_PIN 27 // Nuevo pin para el motor de vibración
#define LDR_PIN 34  // Pin analógico al que está conectada la LDR
#define BUZZER_TEMPERATURA_PIN 35 
#define NUM_LECTURAS 5

// Pines por defecto display i2c
// SDA (Data): GPIO21
// SCL (Clock): GPIO22
// Definir pines I2C
#define I2C_SDA 21
#define I2C_SCL 22
// Crear instancia de la pantalla OLED
Adafruit_SSD1306 display(128, 64, &Wire);

#define DHTTYPE DHT11

DHT dht(DHT_PIN, DHTTYPE);

// Configuración de WiFi en mi telefono
//const char *ssid = "HONOR X7a";
//const char *password = "11111111";

// Configuración de WiFi en mi pc
const char *ssid = "DESKTOP-MN22LRK 7268";
const char *password = "12345678";
const char* mqtt_server = "192.168.137.1";

int currentMenuOption = 0;  // Variable para rastrear la opción actual del menú
const int totalMenuOptions = 3;  // Número total de opciones en el menú
bool inMenu = true; //Indica si estas en el menú o no

// Crear instancia del MPU6050
//MPU6050 mpu;

// Configuración de Firebase
#define FIREBASE_HOST "https://flutter-app-b7fc4-default-rtdb.firebaseio.com/"
#define FIREBASE_API_KEY "AIzaSyBIkSZof01P0GikgPJUNXCoPIqYIW0MTY4"

// Credenciales de usuario de Firebase
#define USER_EMAIL "losdesmadrososdelaesquina@gmail.com"
#define USER_PASSWORD "kodigo36"

FirebaseData firebaseData;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;

// Variables para gráficas
float historialTemp[10];
float historialHumedad[10];
float historialDistancia[10];
int indice = 0;
float lecturasDistancia[NUM_LECTURAS];  // Almacena las últimas lecturas de distancia
int indiceDistancia = 0;
float sumaDistancia = 0;

float sumaTemperatura = 0;  // Suma de lecturas de temperatura
int countTemperatura = 0;  // Conteo de lecturas de temperatura


WiFiClient espClient;
PubSubClient client(espClient);

// Declaraciones de funciones
float medirDistancia();
void activarBuzzer(int duracion);
void cambiarColorLED(int r, int g, int b);
void actualizarHistorial(float temp, float humedad, float distancia);
void mostrarGraficas();
void mostrarMenu();

void setup_wifi();
void callback(char *topic, byte *payload, unsigned int length);

void setup()
{
  // Inicializar el bus I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUZZER_TEMPERATURA_PIN, OUTPUT);
  pinMode(DHT_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  dht.begin();

  // Iniciar OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("No se encontró la pantalla OLED"));
    while (true)
      ;
  }

      // Inicializar el array de lecturas de distancia en 0
  for (int i = 0; i < NUM_LECTURAS; i++) {
    lecturasDistancia[i] = 0;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("OLED Iniciado correctamente");
  mostrarMenu();
 
  // Configuración de Firebase
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.api_key = FIREBASE_API_KEY;

  // Autenticación del usuario
  firebaseAuth.user.email = USER_EMAIL;
  firebaseAuth.user.password = USER_PASSWORD;

  // Inicializar Firebase
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);

  if (Firebase.ready())
  {
    Serial.println("Firebase inicializado y autenticado");
  }
  else
  {
    Serial.println("Error al inicializar Firebase");
    Serial.println(firebaseData.errorReason());
  }
  // Inicializar pantalla OLED con la dirección y bus I2C
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("Error al inicializar la pantalla OLED"));
    for (;;)
      ;
  }
}
void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Conectado a WiFi");
}

void callback(char *topic, byte *payload, unsigned int length)
{
  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.print("Mensaje recibido en tópico: ");
  Serial.println(topic);
  Serial.print("Mensaje: ");
  Serial.println(msg);

  if (msg == "subir") {
    currentMenuOption--;
    if (currentMenuOption < 0) {
      currentMenuOption = totalMenuOptions - 1;  // Vuelve al final del menú
    }
    mostrarMenu();
  } else if (msg == "bajar") {
    currentMenuOption++;
    if (currentMenuOption >= totalMenuOptions) {
      currentMenuOption = 0;  // Vuelve al inicio del menú
    }
    mostrarMenu();
  } else if (msg == "seleccionar") {
    // Lógica para seleccionar la opción actual del menú
    inMenu = false;  // Salir del menú
    mostrarGraficas();
    Serial.println("Opción seleccionada: " + String(currentMenuOption));
  } else if (msg == "retroceder") {
    // Lógica para retroceder en el menú
    inMenu = true;  // Regresar al menú desde las gráficas
    mostrarMenu();
    Serial.println("Retrocediendo...");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Conectado");
            // Suscribirse al tópico
      client.subscribe("menu/control");
      Serial.println("Suscrito a tópico: menu/control");

      // Publicar mensaje para confirmar conexión
      client.publish("menu/control", "ESP32 conectada correctamente al servidor MQTT");

    } else {
      Serial.print("fallo, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}
// Función para actualizar el array de lecturas de distancia
void actualizarLecturasDistancia(float nuevaLectura) {
  sumaDistancia -= lecturasDistancia[indiceDistancia];  // Restar la lectura más antigua
  lecturasDistancia[indiceDistancia] = nuevaLectura;  // Reemplazarla con la nueva lectura
  sumaDistancia += nuevaLectura;  // Sumar la nueva lectura
  indiceDistancia = (indiceDistancia + 1) % NUM_LECTURAS;  // Avanzar el índice circularmente
}

// Función para calcular el promedio de las lecturas de distancia
float calcularPromedioDistancia() {
  return sumaDistancia / NUM_LECTURAS;
}

// Función para activar el buzzer de distancia
void activarBuzzerDistancia(int duracion) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duracion);
  digitalWrite(BUZZER_PIN, LOW);
  delay(duracion);
}

// Función para activar el buzzer de temperatura
void activarBuzzerTemperatura(int duracion) {
  digitalWrite(DHT_PIN, HIGH);
  delay(duracion);
  digitalWrite(DHT_PIN, LOW);
  delay(duracion);
}

// Sumar las lecturas de temperatura y activar buzzer si se supera un umbral
void checkSumaTemperatura(float temp) {
  sumaTemperatura += temp;  // Añadir la temperatura actual a la suma
  countTemperatura++;  // Incrementar el conteo de lecturas

  if (countTemperatura == 5) {  // Activar cuando haya 5 lecturas
    if (sumaTemperatura > 100) {  // Ajusta el umbral según tus necesidades
      activarBuzzerTemperatura(1000);  // Activar el buzzer por 1 segundo
      Serial.println("Suma de temperaturas alta, activando buzzer");
    }
    // Reiniciar suma y conteo para la próxima serie de lecturas
    sumaTemperatura = 0;
    countTemperatura = 0;
  }
}


void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Medir distancia
  float distancia = medirDistancia();
  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.println(" cm");

  // Medir temperatura y humedad
  float temp = dht.readTemperature();
  float humedad = dht.readHumidity();

  Serial.print("Temperatura: ");
  Serial.print(temp);
  Serial.println(" C");

  Serial.print("Humedad: ");
  Serial.print(humedad);
  Serial.println(" %");

  // Actualizar historial de datos para las gráficas
  actualizarHistorial(temp, humedad, distancia);

  int ldrValue = analogRead(LDR_PIN);  // Leer valor de la LDR (0-4095 en ESP32)
  
  // Convertir el valor LDR a un rango de brillo adecuado (0-255)
  int brightness = map(ldrValue, 0, 4095, 0, 255);
  
  // Ajustar el brillo de la pantalla OLED (si la librería soporta dimming)
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(brightness);

    // ---- Para el buzzer de distancia (basado en promedio - AVG) ----
  actualizarLecturasDistancia(distancia);
  float promedioDistancia = calcularPromedioDistancia();

  if (promedioDistancia < 50) {  // Si el promedio de las últimas 5 lecturas es menor que 50 cm
    activarBuzzerDistancia(3000); // Activar el buzzer por 3 segundos
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("Estas a una buena distancia");
  }
  Serial.println("_______________________");

  if (isnan(temp) || isnan(humedad))
  {
    Serial.println("Error leyendo el sensor DHT");
  }
  else
  {
    checkSumaTemperatura(temp);  // Usar la función SUM para la temperatura
  }
    // Cambiar color del LED RGB según la humedad
    if (humedad > 80) // Umbral alto de humedad
    {
      cambiarColorLED(0, 0, 255); // Azul: Alta humedad (posible lluvia)
      Serial.println("Alta humedad detectada, cambiando LED a azul");
    }
    else if (humedad > 60) // Humedad media
    {
      cambiarColorLED(0, 255, 0); // Verde: Humedad moderada
      Serial.println("Humedad moderada detectada, cambiando LED a verde");
    }
    else
    {
      cambiarColorLED(255, 0, 0); // Rojo: Baja humedad
      Serial.println("Baja humedad detectada, cambiando LED a rojo");
    }
  Serial.println("_______________________");
  // Actualizar Firebase
  if (Firebase.ready())
  {
    // Actualizar distancia
    if (Firebase.setFloat(firebaseData, "/sensorData/distancia", distancia))
    {
      Serial.println("Distancia actualizada en Firebase");
    }
    else
    {
      Serial.print("Error al actualizar distancia: ");
      Serial.println(firebaseData.errorReason());
    }

    // Actualizar temperatura
    if (Firebase.setFloat(firebaseData, "/sensorData/temperatura", temp))
    {
      Serial.println("Temperatura actualizada en Firebase");
    }
    else
    {
      Serial.print("Error al actualizar temperatura: ");
      Serial.println(firebaseData.errorReason());
    }

    // Actualizar humedad
    if (Firebase.setFloat(firebaseData, "/sensorData/humedad", humedad))
    {
      Serial.println("Humedad actualizada en Firebase");
    }
    else
    {
      Serial.print("Error al actualizar humedad: ");
      Serial.println(firebaseData.errorReason());
    }
  }

  delay(5000); // Pausa antes de la siguiente iteración
}

float medirDistancia()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  float distancia = (duracion / 2.0) / 29.1;
  return distancia;
}

/*
void activarBuzzer(int duracion)
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duracion);
  digitalWrite(BUZZER_PIN, LOW);
  delay(duracion);
}
*/

void cambiarColorLED(int r, int g, int b)
{
  analogWrite(LED_RED_PIN, r);
  analogWrite(LED_GREEN_PIN, g);
  analogWrite(LED_BLUE_PIN, b);
}

void actualizarHistorial(float temp, float humedad, float distancia)
{
  historialTemp[indice] = temp;
  historialHumedad[indice] = humedad;
  historialDistancia[indice] = distancia;
  indice = (indice + 1) % 10;
}

void mostrarMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  if (currentMenuOption == 0) {
    display.setCursor(0, 0);
    display.println("> Sensor 1");
    display.setCursor(0, 10);
    display.println("  Sensor 2");
    display.setCursor(0, 20);
    display.println("  Sensor 3");
  } else if (currentMenuOption == 1) {
    display.setCursor(0, 0);
    display.println("  Sensor 1");
    display.setCursor(0, 10);
    display.println("> Sensor 2");
    display.setCursor(0, 20);
    display.println("  Sensor 3");
  } else if (currentMenuOption == 2) {
    display.setCursor(0, 0);
    display.println("  Sensor 1");
    display.setCursor(0, 10);
    display.println("  Sensor 2");
    display.setCursor(0, 20);
    display.println("> Sensor 3");
  }

  display.display();
}


void mostrarGraficas() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE); 

  // Mostrar gráfica en función de la opción seleccionada
  if (currentMenuOption == 0) {
    // Graficar temperatura
    display.setCursor(0, 0);
    display.print("Temp:");
    for (int i = 0; i < 9; i++) {
      display.drawLine(i * 12, 63 - historialTemp[i], (i + 1) * 12, 63 - historialTemp[i + 1], WHITE);
    }
  } else if (currentMenuOption == 1) {
    // Graficar humedad
    display.setCursor(0, 20);
    display.print("Hum:");
    for (int i = 0; i < 9; i++) {
      display.drawLine(i * 12, 43 - historialHumedad[i], (i + 1) * 12, 43 - historialHumedad[i + 1], WHITE);
    }
  } else if (currentMenuOption == 2) {
    // Graficar distancia
    display.setCursor(0, 40);
    display.print("Dist:");
    for (int i = 0; i < 9; i++) {
      display.drawLine(i * 12, 23 - historialDistancia[i], (i + 1) * 12, 23 - historialDistancia[i + 1], WHITE);
    }
  }

  display.display();  // Actualizar el OLED con la gráfica seleccionada
}