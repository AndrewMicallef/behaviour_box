String version = "#behaviourbox151218";

//TODO: make a conditioning protocol
//TODO: rename singlestim operant
//TODO: find an obvious method for producing constant noise

/*
    Author: Naoya Takahashi
        modified by Andrew Micallef
      
    This program delivers sensory stimulation and opens water 
    valve when the animal touches the licking sensor within a 
    certain time window.

    Setup connections:
    ------------------
      
    ---------   -----------------  ------------  
    DIGITAL     output             variable
    ---------   -----------------  ------------
    pin 2       recording trigger  `recTrig`  
    pin 4       stimulus TTL       `whiskStim`
    pin 6       speaker for cues   `speakerPin`

    pin 8       syringe pump TTL   `waterValve`
    pin 9       vacuum tube valve  `vacValve`

    pin 13      lick report        `licking`

    ---------   -----------------  ------------
    ANALOG IN   input
    ---------   -----------------  ------------
    A0          piezo lick sensor  lickSens
    ---------   -----------------  ------------
    Table: connections to lick controller

      
    Start program:
    --------------

    Trial intervals will randomized between 'minITI' and 'maxITI'. 
    During a trial, the animal has to wait a stimulation without 
    licking (no-lick period, 'nolickPer').

    If it licks during no-lick period, the time of stimulation 
    will be postponed by giving a time out (randomized between 
    'minTimeOut' and 'maxTimeOut').

    When a stimulation is delivered (stimulus duration: 'stimDur'), 
    the animal need to respond (touch the sensor) within a certain 
    time window ('crPer') to get a water reward.

    Delayed licking after the time window will not be rewarded. 
    Opening duration of the water valve is defined by 'valveDur'.

    A TTL trigger for recording will be delivered 
    'baseLineTime' (1 s in default setting) before the stimulus.



//lines preceded by `#` are for debug purposes
 */


// IO port settings:
const byte recTrig = 2;    // digital pin 2 triggers ITC-18
const byte stimulusPin = 3;    // digital pin 4 control whisker stimulation
const byte speakerPin = 8;
const byte vacValve = 7;     // digital pin 9 controls vacuum
const byte statusLED = 13;
      
const byte waterPort[] = {10,11};    // digital pin 8 control water valve 
const byte lickRep[] = {13,13};      // led connected to digital pin 13
const byte lickSens[] = {A0,A1}; // the piezo is connected to analog pin 0


int lickThres = 450;

// timing parameters
unsigned long t_init;

int trial_delay = 500;
int t_noLickPer = 0;   // ms

int t_stimONSET[] = {5000,6000};
int stimDUR = 500;

int t_rewardSTART = 4500; // ms
int t_rewardEND = 10000;   // ms
int t_trialEND = 10000;   // ms


char mode = 'c';
char rewardCond = 'B'; // a value that is 'L' 'R', 'B' or 'N' to represent lick port to be used

// stimulus parameters
unsigned long ON[] = {1000, 1000};
unsigned long OFF[] = {5000, 5000};

// audio
int toneGood = 2000; //Hz
int toneBad = 500; //Hz
int toneDur = 100; 

// Global value to keep track of the total water consumed
int waterCount = 0; 
byte waterVol = 10; //uL per dispense

// Global lick on
bool lickOn[] = {false, false};
bool stimTrial = true; //sets if there is a stimulus this run
bool verbose = true;
bool break_wrongChoice = false; // stop the trial if the animal makes a mistake during reward period


int t_now(unsigned long t_init){
    // is less than 0 before the trial starts
    // is greater than 0 after the start of trial
    return (int) millis() - t_init;
}

bool senseLick(bool sensor){
    // check to see if the lick sensor has moved
    // set lickDetected
    boolean lickDetected = false;
    int sensVal = analogRead(lickSens[sensor]);

    if (sensVal >= lickThres){
        digitalWrite(lickRep[sensor], HIGH);
        lickDetected = true;
    } 
    else {
        digitalWrite(lickRep[sensor], LOW);
        lickDetected = false;
    }

    return lickDetected;
}

String getSerialInput(){

    String readString;
    
    while (Serial.available()) { 
        delay(3);  //delay to allow buffer to fill
        char c = Serial.read();  //gets one byte from serial buffer
        readString += c; //makes the string readString
    }
    
    return readString;
}

int getSepIndex(String input) {
    char c = 1;
    int i = 0;
   
    while (c != 0) {
        c = input[i];
        
        if (c == ':'){
            return i;
        }
        i ++;
    }
    return 0;
}

void flutter(byte stim_pin, int on, int off){
  
  digitalWrite(stim_pin, HIGH);
  digitalWrite(statusLED, HIGH);
  
  delay(on);
    
  digitalWrite(stim_pin, LOW);
  digitalWrite(statusLED, LOW);
  
  delay(off);
}


/*
-----------------
THE TRIAL STATES
----------------
*/

char ActiveDelay(int wait, 
    bool break_on_lick = false, 
    bool verbose = true) {

    int t = t_now(t_init);
    
    char response = '-';
    
    if (verbose) {Serial.print("#Enter `ActiveDelay`:\t"); Serial.println(t);}
    
    while (t < wait) {
        t = t_now(t_init);
        lickOn[0] = senseLick(0);
        lickOn[1] = senseLick(1);
        
        if (lickOn[0] or lickOn[1]){
            
            
            if (lickOn[0]) { 
                response = 'l';
                Serial.print("port[0]:\t"); Serial.println(t); 
            }
            if (lickOn[1]) { 
                response = 'r';
                Serial.print("port[1]:\t"); Serial.println(t); 
            }
            
            if (break_on_lick){
                if (verbose) { Serial.print("#Exit `ActiveDelay`:\t"); Serial.println(t); }
                return response;
            }
        }
    }
    
    if (verbose) {Serial.print("#Exit `ActiveDelay`:\t"); Serial.println(t);}
    return response;
}

//0 PRETRIAL

void preTrial(bool verbose = true) {   
    /* while the trial has not started 
       1. update the time
       2. check for licks
       4. trigger the recording by putting recTrig -> HIGH
    */
    int t = t_now(t_init);
    if (verbose) {Serial.print("#Enter `preTrial`:\t"); Serial.println(t);}
    
    while (t < 0){
        // 1. update time
        // 2. check for licks
        t = t_now(t_init);
        lickOn[0] = senseLick(0); 
        lickOn[1] = senseLick(1);
        
        digitalWrite(vacValve, HIGH);
        
        //TODO: define statusLED and plug in an LED to hardware
        // LED to flash each second before the trial
        if (t%1000 < 20){
            digitalWrite(statusLED, HIGH);
        } 
        else {digitalWrite(statusLED, LOW);}
        
        // 3. trigger the recording
        if (t > -10){
            digitalWrite(recTrig, HIGH);
        }
        
        // note, recTrig is switched off immediately after the
        // loop to avoid artefacts in the recording
    }
    
    digitalWrite(recTrig, LOW); 
    digitalWrite(vacValve, LOW);
    
    if (verbose) {Serial.print("#Exit `preTrial`:\t"); Serial.println(t);}
    
} 

int TrialStimulus(byte stimulusPin,
    int stimDUR,
    int ON = 5, // us time of ON pulse    ie FREQUENCY of flutter
    int OFF = 5, // us time of off pulse  ie FREQUENCY of flutter
    bool verbose = true) {
    
    int t_local = millis();
    int t = t_now(t_local);
    
    if (verbose) {
        // TODO make verbosity a scale instead of Boolean
        Serial.print("#Enter `TrialStimulus`:\t"); Serial.println(t_now(t_init));
        Serial.print("#\tstimDUR:\t"); Serial.println(stimDUR);
        Serial.print("#\tON:\t"); Serial.println(ON);
        Serial.print("#\tOFF:\t"); Serial.println(OFF);
    }
   
    while (t < stimDUR){
        /* Run the buzzer while:
           1. update the time
           2. check for licks
        */
        t = t_now(t_local);
        lickOn[0] = senseLick(0); 
        lickOn[1] = senseLick(1);
        
        flutter(stimulusPin, ON, OFF);

    } digitalWrite(stimulusPin, LOW); //this is a safety catch

    
    if (verbose) {Serial.print("#Exit `TrialStimulus`:\t"); Serial.println(t);}
    return 1;
}

char TrialReward(char mode, // -'c'onditioning (guaranteed reward) -'o'perant (reward on lick)
                int t_rewardEND,
                char rewardCond, // 'L'eft, 'R'ight, 'B'oth, 'N'either
                bool break_wrongChoice = false, // exits the function if the animal makes a bad decision
                byte waterVol = 10, // 10 ms gives ~ 5-8 uL
                bool verbose = true) {

    /* 
    returns a character: 
             'L' -- correct hit on left port
             'R' -- correct hit on right port
             'l' -- incorrect lick on left port
             'r' -- incorrect lick on right port
             '-' -- unknown; in conditioning this 
                    function exits before the animal has 
                    a chance to respond
             'M' -- No lick detected during reward period
    */
                
    int t = t_now(t_init);
    bool RewardTest;
    bool RewardPort;
    char response;
    
    if (verbose) {Serial.print("#Enter `TrialReward`:\t"); Serial.println(t);}
    
    while (t < t_rewardEND) {
        
        t = t_now(t_init);
        
        lickOn[0] = senseLick(0); 
        lickOn[1] = senseLick(1);
        
        // response reports if there was a lick in the reward period
        
        switch (rewardCond){
            
            case 'L':
                RewardTest = (lickOn[0]) or (mode == 'c');
                RewardPort = 0;
            break;
                
            case 'R':
                RewardTest = (lickOn[1]) or (mode == 'c');
                RewardPort = 1;
            break;
            
            case 'B':
                RewardTest = (lickOn[0] or lickOn[1]) or (mode == 'c');
                if (lickOn[0]){RewardPort = 0;}
                if (lickOn[1]){RewardPort = 1;}
            break;
            
            case 'N':
                if (verbose) { Serial.print("#Exit `TrialReward`:\t"); Serial.println(t);}
                return 0;
            break;
            
            default:
                Serial.print("ERROR: rewardCond not specified");
                Serial.println(" requires 'L'eft, 'R'ight, 'B'oth, 'N'either");
                return -1;
            break;
        }
        
        if (RewardTest) {
            
            digitalWrite(waterPort[RewardPort], HIGH);
            
            if (rewardCond == 'B') { digitalWrite(waterPort[!RewardPort], HIGH); }
            
            delay(waterVol);
            
            digitalWrite(waterPort[0], LOW);
            digitalWrite(waterPort[1], LOW);            
           
            if (mode != 'c'){ 
                if (lickOn[0]){ response = 'L'; } // hit left
                if (lickOn[1]){ response = 'R';} // hit right
                
                Serial.print("response:\t"); Serial.println(response);
                Serial.print("response_time:\t"); Serial.println(t);
            }
            else { response = '-'; } // unknown

            if (verbose) { Serial.print("#Exit `TrialReward`:\t"); Serial.println(t);}
            return response;
        }
        else if ((lickOn[0] or lickOn[1]) and break_wrongChoice){
            // declare the fail condition??
            
            if (lickOn[0]) {
                response = 'l'; //bad left 
                Serial.print("port[0]:\t"); Serial.println(t); }
            
            if (lickOn[1]) {
                response = 'r'; //bad right
                Serial.print("port[1]:\t"); Serial.println(t); }
            
            Serial.print("response:\t"); Serial.println(response);
            Serial.print("response_time:\t"); Serial.println(t);
            
            if (verbose) {Serial.print("#Exit `TrialReward`:\t"); Serial.println(t);}
            
            return response;
        }
        
        digitalWrite(waterPort[0], LOW);
        digitalWrite(waterPort[1], LOW);        //safety catch
    }
    
    response = 'M';
    
    Serial.print("response:\t"); Serial.println(response);
    Serial.print("response_time:\tNan");
    
    // miss 
    if (verbose) {Serial.print("#Exit `TrialReward`:\t"); Serial.println(t);}
    return response;
}


/* ------------------------------
     THE MAIN MAIN FUNCTION
--------------------------------- */ 

int runTrial ( int mode,
    int trial_delay,
    int t_noLickPer,   // ms
    int t_stimONSET[2], //time stim turns on
    int stimDUR, // duration of stimuli
    int t_rewardSTART, // ms
    int t_rewardEND,   // ms
    int t_trialEND,   // ms
    char rewardCond,
    byte waterVol,
    bool verbose,
    bool break_wrongChoice = false) { 
    
    // returns 0 if the stimulus was applied
    // returns 1 if a timeout is required
    // until next trial
    
    // local variables and initialisation of the trial
    /* t_init is initialised such that t_now
       returns 0 at the start of the trial, and 
       increases from there. */ 
    int t; 
    // local time
    t_init = millis() + trial_delay;
    t = t_now(t_init);

    char response = '-';
    
    /*trial_phase0
    while the trial has not started 
       1. update the time
       2. check for licks
       3. trigger the recording by putting recTrig -> HIGH
    */
    
    preTrial(verbose);
    t = t_now(t_init);
      
    ActiveDelay(t_noLickPer, false, verbose);
    t = t_now(t_init);
    
    ActiveDelay(t_stimONSET[0], false, verbose);
    t = t_now(t_init);
    
    if (ON[0]) {TrialStimulus(stimulusPin, stimDUR, ON[0], OFF[0], verbose);}
    else {if (verbose){Serial.println("#skipping stim0");}}
    t = t_now(t_init);
    
    ActiveDelay(t_stimONSET[1], false, verbose);
    t = t_now(t_init);
    
    if (ON[1]){ TrialStimulus(stimulusPin, stimDUR, ON[1], OFF[1], verbose);}
    else {if (verbose){Serial.println("#skipping stim1");}}
    t = t_now(t_init);
    
    
    // TODO include contingency to report on lick early without breaking?
    ActiveDelay(t_rewardSTART, false, verbose);
    t = t_now(t_init);
    
    /* this is a little complicated:
       1. check that there is a reward due, ie the condition is not `N`
       2. TrialReward returns 0 if break_wrongChoice is set.
          therefore if TrialReward is false, the incorrect sensor was tripped
          during the reward period. A bad tone is played; and the function
          returns, resseting the program
       3. Otherwise we wait until the trial period has ended
    */
    
    if (rewardCond != 'N') {
        
        response = TrialReward(mode, t_rewardEND, rewardCond, 
                                break_wrongChoice, waterVol, verbose); 
        if ( response == rewardCond ) {
            // that we are here means the animal got it right
            if (mode == 'o') { 
                tone(speakerPin, toneGood, 25); 
                //Serial.print(something informative);
            }
            
            ActiveDelay(t_trialEND, false, verbose);
            return 1;
        }
        else if (response == '-') {
            // The program can only get here if the animal got it wrong
            Serial.print(":\t"); Serial.println(t);
            return 0;
        }
    }
    else {
        response = ActiveDelay(t_trialEND, true, verbose);
        
        Serial.print("response:\t"); Serial.println(response);
        Serial.print("response_time:\t"); Serial.println(t);
        
        ActiveDelay(t_trialEND, false, verbose);
        
        return 0;
    }   
    
    return 0;
}

/* ------------------------------
     END MAIN MAIN FUNCTION
--------------------------------- */ 

/* ------------------------------
     END OF THE MAP... here be monsters
--------------------------------- */ 


int UpdateGlobals(String input) {
    /*
    This is a big ugly function which compares the
    input string to the names of variables that I have
    stored in memory; This is very much not the `C` 
    way to do things...
    */
    
    
    int sep = getSepIndex(input);
        
        
    if (sep) {
        
        String variable_name = input.substring(0,sep);
        String variable_value = input.substring(sep+1);
        
        Serial.print("#");Serial.print(variable_name);Serial.print("\t");Serial.println(variable_value);
        
        // input before seperator?
        if (variable_name == "lickThres" ) {
            lickThres = variable_value.toInt();
            Serial.print("lickThres:\t"); Serial.println(lickThres);
            return 1;
        }
        if (variable_name == "trial_delay" ) {
            trial_delay = variable_value.toInt();
            Serial.print("trial_delay:\t"); Serial.println(trial_delay);
            return 1;
        }
        if (variable_name == "t_noLickPer" ) {
            t_noLickPer = variable_value.toInt();
            Serial.print("t_noLickPer:\t"); Serial.println(t_noLickPer);
            return 1;
        }
        if (variable_name == "t_stimONSET[0]" ) {
            t_stimONSET[0] = variable_value.toInt();
            Serial.print("t_stimONSET[0]:\t"); Serial.println(t_stimONSET[0]);
            return 1;
        }
        if (variable_name == "t_stimONSET[1]" ) {
            t_stimONSET[1] = variable_value.toInt();
            Serial.print("t_stimONSET[1]:\t"); Serial.println(t_stimONSET[1]);                
            return 1;
        }
        if (variable_name == "stimDUR" ) {
            stimDUR = variable_value.toInt();
            Serial.print("stimDUR:\t"); Serial.println(stimDUR);                
            return 1;
        }
        if (variable_name == "t_rewardSTART" ) {
            t_rewardSTART = variable_value.toInt();
            Serial.print("t_rewardSTART:\t"); Serial.println(t_rewardSTART);                
            return 1;
        }
        if (variable_name == "t_rewardEND" ) {
            t_rewardEND = variable_value.toInt();
            Serial.print("t_rewardEND:\t"); Serial.println(t_rewardEND);                
            return 1;
        }
        if (variable_name == "t_trialEND" ) {
            t_trialEND = variable_value.toInt();
            Serial.print("t_trialEND:\t"); Serial.println(t_trialEND); 
            return 1;
        }
        if (variable_name == "waterVol" ) {
            waterVol = byte(variable_value.toInt());
            Serial.print("waterVol:\t"); Serial.println(waterVol);
            return 1;
        }

        if (variable_name == "ON[0]" ) {
            ON[0] = variable_value.toInt();
            Serial.print("ON[0]:\t"); Serial.println(ON[0]);
            return 1;
        }

        if (variable_name == "ON[1]" ) {
            ON[1] = variable_value.toInt();
            Serial.print("ON[1]:\t"); Serial.println(ON[1]);
            return 1;
        }
        
        if (variable_name == "OFF[0]" ) {
            OFF[0] = variable_value.toInt();
            Serial.print("OFF[0]:\t"); Serial.println(OFF[0]);                
            return 1;
        }
        if (variable_name == "OFF[1]" ) {
            OFF[1] = variable_value.toInt();
            Serial.print("OFF[1]:\t"); Serial.println(OFF[1]);  
            return 1;
        }

        if (variable_name == "mode" ) {
            mode = variable_value[0];
            Serial.print("mode:\t"); Serial.println(mode);                
            return 1;
        }
        if (variable_name == "rewardCond" ) {
            rewardCond = variable_value[0];
            Serial.print("rewardCond:\t"); Serial.println(rewardCond);    
            return 1;
        }
        if (variable_name == "verbose" ) {
            verbose = variable_value.toInt();
            Serial.print("verbose:\t"); Serial.println(verbose);                  
            return 1;
        }
        
        if (variable_name == "break_wrongChoice" ) {
            break_wrongChoice = variable_value.toInt();
            Serial.print("break_wrongChoice:\t"); Serial.println(break_wrongChoice);                  
            return 1;
        }
   }
       
   return 0;
}





void setup (){
    // Open serial communications and wait for port to open:
    Serial.begin(115200);
    // This requires RX and TX channels (pins 0 and 1)
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB port only
    }
    //Confirm connection and telegraph the code version
    Serial.println("#Arduino online");
    Serial.println("#behaviourbox");
    Serial.println(version);
    
    // convert lickthreshold to V
    Serial.print("#Lick Threshold:\t"); Serial.print((float(lickThres)/1024)*5); Serial.println(" V");
    
    randomSeed(analogRead(5));

    pinMode(recTrig, OUTPUT); // declare the recTrig as as OUTPUT
    pinMode(waterPort[0], OUTPUT); // declare the waterValve as as OUTPUT
    pinMode(vacValve, OUTPUT); // declare the vacValve as as OUTPUT
    pinMode(stimulusPin, OUTPUT); // declare the whiskStim as as OUTPUT
    pinMode(lickRep[0], OUTPUT); // declare the licking as as OUTPUT
    pinMode(speakerPin, OUTPUT);
    
    Serial.println("-- Status: Ready --");
}


void loop () {
    
    if (Serial.available()){
        
        String input = getSerialInput();
        
        
        if (input == "GO"){

            runTrial(mode, trial_delay, t_noLickPer, t_stimONSET,
                    stimDUR, t_rewardSTART, t_rewardEND, 
                    t_trialEND, rewardCond, waterVol, verbose);
                    
            Serial.println("-- Status: Ready --");
        }
        
        else { UpdateGlobals(input); }

    }

    if (senseLick(0) or senseLick(1)){
        tone(speakerPin, toneBad, 10);
        delay(100);
    }
}






