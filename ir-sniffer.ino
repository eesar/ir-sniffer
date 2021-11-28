//--------------------------------------------------------------------------------------------------
// 29.08.2020 - Stephan Rau: initial version
//--------------------------------------------------------------------------------------------------
// Assumptions:
//  - long low phase is start sequence
//  - data is not more than 32 bits, else long variable for code is not enough
//  - a zero is repesented by a longer low than high phase
//  - a one is repesentend by a longer high than low phase
//--------------------------------------------------------------------------------------------------

const unsigned int min_init  = 2000; // min low phase of init phase in us
const unsigned int min_valid = 150;  // min duration time for valid pulse
const unsigned int max_valid = 1500; // max duration time for valid pulse
const byte RF_RCV_PIN        = 3;    // main rcv input
const byte RF_VAL_PIN        = 13;   // copy rcv value to on board LED

// serial strings ----------------------------------------------------------------------------------
const String ENABLE_CMD     = "on";  // enable sniffing
const String DISABLE_CMD    = "off"; // disable sniffing
const char   CMD_END        = '\n';  // character for command end

// global variables --------------------------------------------------------------------------------
static bool   sniff_on      = false;
static String serialInStr;           // read string from serial port


//--------------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); // higher speed needed else print is not finshed before next interrupt
  pinMode(RF_VAL_PIN, OUTPUT);
  pinMode(RF_RCV_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RF_RCV_PIN), count_time, CHANGE);
}


//--------------------------------------------------------------------------------------------------
void loop() {

  // indicate that rcv is receiving something
  digitalWrite(RF_VAL_PIN, digitalRead(RF_RCV_PIN));

  // check and read command from PC
  if ( Serial.available() > 0 ) {
    serialInStr = Serial.readStringUntil(CMD_END);

    if ( serialInStr == DISABLE_CMD ) {
      sniff_on = false;
    }

    if ( serialInStr == ENABLE_CMD ) {

      // print header line

      // start sequence
      Serial.print("Start Seq Low,");
      Serial.print("Start Seq High,");
      Serial.print("Start Seq Low,");

      // zero encoding
      Serial.print("Zeros High,");
      Serial.print("Zeros Low,");

      // one encoding
      Serial.print("Ones High,");
      Serial.print("Ones Low,");

      // end sequence
      Serial.print("End Seq High,");
      Serial.print("End Seq Low,");
      Serial.print("End Seq High,");
      Serial.print("End Seq Low,");

      // number of bits counted, should be 32 for IR RGB sensors
      Serial.print("Num of Bits,");

      // finally the read code value
      Serial.println("Code");

      sniff_on = true;
    }
  }
}


//--------------------------------------------------------------------------------------------------
void count_time() {

  // variables needed for calculation (static do they do not get deleted when function finished)
  static unsigned int currentTime = 0;
  static unsigned int lastTime = 0;
  static unsigned int duration = 0;
  static unsigned int lastDuration = 0;
  static byte lastState = 0;
  static byte state = 0;
  static byte count_data_bits = 0;
  static unsigned int start_0_low = 0;
  static unsigned int start_1_high = 0;  
  static unsigned int start_2_low = 0;
  static byte fsm = 0;
  
  static unsigned int end_0_high = 0;
  static unsigned int end_1_low = 0;  
  static unsigned int end_2_high = 0;
  static unsigned int end_3_low = 0;  
    
  static unsigned int zeros_high = 0;
  static unsigned int zeros_low = 0;
  static unsigned int ones_high = 0;
  static unsigned int ones_low = 0;
  static unsigned long code = 0;

  if ( sniff_on ) {

    currentTime = micros();
    duration = currentTime - lastTime;
    state = digitalRead(RF_RCV_PIN);

    // always check after short low pulse - this one should is always similar
    if ( state == 1 ) {

      // final end sequence part - print info
      if ( fsm == 3 ) {
        fsm = 4;
        end_2_high = lastDuration;
        end_3_low = duration;
      }

      // cout bits and search for end sequence
      if ( fsm == 2 ) {
        if ( duration > max_valid ) {
          // find end sequece, very long high, long low, medium hifh, short low
          fsm = 3;
          end_0_high = lastDuration;
          end_1_low = duration;
        } else {
          // find bit, long or short high, short low
          count_data_bits++;
          code <<= 1; // shift by one
        
          //    high phase  > low phase
          if ( lastDuration > duration  && (lastState == 0) ) {
            ones_high = lastDuration;
            ones_low = duration;
            code |= 1; // set current value to 1
          } else {
            zeros_high = lastDuration;
            zeros_low = duration;
            //code &= 0; // set current value to 0 (not needed is default)
          }
        }
      }

      // find start sequence, infinete high, long low, medium high, short low
      if ( fsm == 1 ) {
        start_1_high = lastDuration;
        start_2_low  = duration;
        fsm = 2;
        count_data_bits = 0;
        code = 0;
      } else {
        if ( (lastState == 0) && (duration > min_init) && (fsm == 0) ) {
          fsm = 1;
          start_0_low = duration;
        }
      }

      // final state print result
      if ( fsm == 4 ) {
        fsm = 0;

        // only print if more than 0 bits are counted to avoid printing false readings
        if ( count_data_bits > 0 ) {
          // start sequence
          Serial.print(start_0_low);      Serial.print(","); // Low
          Serial.print(start_1_high);     Serial.print(","); // High
          Serial.print(start_2_low);      Serial.print(","); // Low
          // zero encoding
          Serial.print(zeros_high);       Serial.print(","); // High
          Serial.print(zeros_low);        Serial.print(","); // Low
          // one encoding
          Serial.print(ones_high);        Serial.print(","); // High
          Serial.print(ones_low);         Serial.print(","); // Low
          // end sequence
          Serial.print(end_0_high);       Serial.print(","); // High
          Serial.print(end_1_low);        Serial.print(","); // Low        
          Serial.print(end_2_high);       Serial.print(","); // High
          Serial.print(end_3_low);        Serial.print(","); // Low         
          
          // number of bits counted, should be 32 for ir rcv
          Serial.print(count_data_bits);  Serial.print(",");
          // finaly the read code value
          Serial.println(code, HEX);
  
          // reset variables
          count_data_bits = 0;
          start_0_low = 0;
          start_1_high = 0;  
          start_2_low = 0;
          end_0_high = 0;
          end_1_low = 0;  
          end_2_high = 0;
          end_3_low = 0;  
          zeros_high = 0;
          zeros_low = 0;
          ones_high = 0;
          ones_low = 0;
          code = 0;
        }
      }

    }

    // save data for evaluation in next change phase
    lastTime = currentTime;
    lastState = state;
    lastDuration = duration;
    
  }

}
