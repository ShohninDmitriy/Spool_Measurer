/* Beta test code pour projet PIC IMT11 SpoolMeasurer */
/* Programme de test du materiel*/

#include <TimerOne.h>
#include <LiquidCrystal_I2C.h>

//******* Déclaration des Pin de l'arduino **********

	// PIN pour le Driver/Moteur A4988
	#define PIN_STEP  9
	#define PIN_DIR   8
	#define PIN_ENABLE_DRIVER 10

	LiquidCrystal_I2C lcd(0x27, 16, 2);// définit le type d'écran lcd avec connexion I2C sur les PIN SLA SCL

	//Pin du Joystick
	const int PIN_X = A0; // analog pin connected to X output
	const int PIN_Y = A1; // analog pin connected to Y output
	const int PIN_CLICK = 3;// Digital pin connected to Y output
	//Pin Fourche Optique
	const byte PIN_FOURCHE = 2; //Digital Pin connected to optical switch

//******* Déclaration des Variables de statu *************
	//Variable des bouton joystick
	bool X_PLUS = 0;
	bool X_MOIN = 0;
	bool Y_PLUS = 0;
	bool Y_MOIN = 0;
	bool CLICK = 0;

	// Variable de statu
	bool MOTOR_RUN = 0;
	bool MOTOR_ENABLE = 0;

	// Variable de de fenetre IHM
	bool window_Menu = 0;
	bool window_Manual = 0;
	bool window_Auto = 0;


//******* Variable pour le fonctinnement moteur **********
	int actual_speed;
	int actual_direction;
	int target_speed;
	int ticks;
	int tick_count;
	// Constante de direction du moteur
	#define FORWARD   HIGH
	#define BACKWARD  LOW

//*********** Parametre de vitesse du moteur ***************

	const int speed_ticks[] = {-1, 400,375,350,325,300,275,250,225,200,175,150,125,110,100,90,85,80,75,70,65,60,55,50};
	size_t max_speed = (sizeof(speed_ticks) / sizeof(speed_ticks[0])) -1 ; // nombre de variable dans le tableau



//******* Variable pour la fonction de mesure **********

	volatile unsigned int  counter_steps=0;// Compteur du nombre de steps de la roue codeuse
	volatile float measurement = 0; //mesure a afficher sur l'IHM
	//                           IIIII
	//                           IIIII
	//     Variable pour         IIIII
	//      la calibration     IIIIIIIII
	//                          IIIIIII
	//                           IIII
	//                            II
	const float perimeter_gear = 33.9; //périmetre de la roue denté de mesure en mm
	const int encoder_hole = 2;//nombre de fenetre sur la roue codeuse




/*
 * =============================================================================================
 * ========================================= Setup =============================================
 * =============================================================================================
*/
void setup() {
	
	digitalWrite(PIN_ENABLE_DRIVER, HIGH);// Désactivation du moteur avant le setup

	// initial values
  	actual_speed = 0;
  	target_speed = max_speed/2;
  	actual_direction = BACKWARD;
  	tick_count = 0;
  	ticks = -1;
	
	// Init du Timer1, interrupt toute les 0.1ms pour lancer un step moteur
	Timer1.initialize(20);
	Timer1.attachInterrupt(timerMotor);
	
	// Init de l'interrupt de compteur de la fourche optique en fonction du soulevement du signal
	attachInterrupt(digitalPinToInterrupt(PIN_FOURCHE), measure_filament, RISING);

	//Initialisation du LCD I2C
	lcd.init();
	lcd.backlight();

	//Initialisation des PIN
  	
	// pins DRIVER
  	pinMode(PIN_ENABLE_DRIVER, OUTPUT);
	digitalWrite(PIN_ENABLE_DRIVER, HIGH);// Désactivation du moteur
  	pinMode(PIN_STEP, OUTPUT);
  	pinMode(PIN_DIR, OUTPUT);
	digitalWrite(PIN_DIR, actual_direction);

  	// Pin Fourche ??????????????????????????????????????????????????????
  	pinMode(PIN_FOURCHE, INPUT_PULLUP);

}
/*
 * =============================================================================================
 * ======================================== Main Loop ==========================================
 * =============================================================================================
*/
void loop() {

	updateLCD();		// Mise a jours du LCD
	read_joystick();	// Lectue Joystick

	if (MOTOR_RUN){
	increase_speed();
	}
	else {
	decrease_speed();
	}


  /*// check which button was pressed
  switch(button) {
    
    case btnUP:
      increase_speed();
      break;
    case btnDOWN:
      decrease_speed();
      break;
    case btnLEFT:
      change_direction(BACKWARD);
      break;
    case btnRIGHT:
      change_direction(FORWARD);
      break;
    case btnSELECT:
      emergency_stop();
      break;
  }*/
  
  
  
}

/********************************* Fonction Diminution de la vitesse Moteur *********************
 * 
 * - Augmentation de la vitesse moteur en fonction des valeurs du tableau speed_ticks
 * 
************************************************************************************************/
void increase_speed() {
  
  if(actual_speed < target_speed) {
    actual_speed += 1;
    tick_count = 0;
    ticks = speed_ticks[actual_speed];
  }
  else if (actual_speed > target_speed){
    actual_speed -= 1;
    tick_count = 0;
    ticks = speed_ticks[actual_speed];
  }
  

}

/********************************* Fonction Diminution de la vitesse Moteur *********************
 * 
 * - Diminution de la vitesse moteur en fonction des valeurs du tableau speed_ticks
 * 
************************************************************************************************/
void decrease_speed() {
  
  if(actual_speed > 0) {
    actual_speed -= 1;
    tick_count = 0;
    ticks = speed_ticks[actual_speed];
  }
  
}


/********************************* Fonction arret d'urgence Moteur ******************************
 * 
 * - Non utilisé
 * 
************************************************************************************************/
void emergency_stop() {
  actual_speed = 0;
  tick_count = 0;
  ticks = speed_ticks[actual_speed / 5];
}

/********************************* Update LCD **************************************************
 *
 * - Fonction qui met a jours l'IHM en fonction de la fenetre dans lequels on est 
 * 
************************************************************************************************/
void updateLCD() {
  
	lcd.setCursor(0,0);
	lcd.print("Speed: ");
	lcd.print(actual_speed);
	lcd.print("RPM ");

	// Calcule de la mesure réell de filament.
	measurement = ((counter_steps * perimeter_gear)/encoder_hole)/1000;

	lcd.setCursor(0,1);
	lcd.print(counter_steps);
	lcd.print("   ");
} 

/********************************* Fonction TimerMotor *****************************************
 *
 * - Fonction lancé toute les 20ms avec au Timer1
 * - Fait tourner le moteur un tour en fonction de la vitesse
 * 
************************************************************************************************/
void timerMotor() {
	if(actual_speed == 0) return;

	digitalWrite(PIN_ENABLE_DRIVER, LOW);// Activation du moteur
	MOTOR_ENABLE = 1;//Flag motor en marche

	tick_count++;

	if(tick_count == ticks) {  
		// 1 step du moteur
		digitalWrite(PIN_STEP, HIGH);
		digitalWrite(PIN_STEP, LOW);

		tick_count = 0;
	}
}


/********************************* Fonction de compteur de tour par interupt *******************
 *
 *  - Ajoute 1 à la variable compteur_hole
 * - la roue codeuse comporte 2 Hole --> + 2 au compteur_hole = 1 tours
 * 
************************************************************************************************/
void measure_filament() {
	if (digitalRead(PIN_FOURCHE)){
		counter_steps += 1;
	}
}


/********************************* Fonction de Lecture du Joystick *****************************
 *
 *  - Lit le joystick en X et Y sur les pin Analog
 * - Mets à jours l'IHM en fonction des fenetre dans lequel ce trouve l'utilisateur
 * - Mets à jours les Flags Y_PLUS et Y_MOINS
 * 
************************************************************************************************/

void read_joystick() {

	int XValue = analogRead(PIN_X);     // Read the analog value from The X-axis from the joystick
	int YValue = analogRead(PIN_Y);     // Read the analog value from The Y-axis from the joystick


  	if (!Y_PLUS & !Y_MOIN){
		if (YValue < 10){ // joystick Y - -> reduce speed
			Y_MOIN = 1;
			Y_PLUS = 0;
		}
		else if (YValue > 800 ){ // joystick Y +  -> rise speed
			Y_MOIN = 0;
			Y_PLUS = 1;
		}
	}
	else if (YValue < 800 && YValue > 50 && actual_speed > 0 ){        // Y en home position      
		Y_MOIN = 0;
		Y_PLUS = 0;
	} 


    if (XValue < 400 && XValue > 10){ // joystick X - -> reduce motor speed
		if (MOTOR_RUN && target_speed > 5 ){
			target_speed = target_speed - 1;
      	}
    }
    else if (XValue < 10){ // joystick X - -> Stop motor
        if (MOTOR_RUN){
          	MOTOR_RUN = 0;
        }
        else if ( actual_speed <= 0){
			Y_MOIN = 0;
			Y_PLUS = 0;
			MOTOR_ENABLE = 0;
			digitalWrite(PIN_ENABLE_DRIVER, HIGH);
        }
        
      }
	      
   else if (XValue > 800){ // joystick X + -> Start motor or Speed
      	if (!MOTOR_RUN){
			target_speed = max_speed/2;
			MOTOR_RUN = 1;
		}

		if (MOTOR_RUN){
			if (target_speed < max_speed){
				target_speed += 1;
			}
      	}
        
    }
}
