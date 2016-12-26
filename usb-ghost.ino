//////////////////////////////////////////////////////////////////////////////
// Sketch: usb_ghost
// แสดงตัวอย่างการจำลองบอร์ดไมโครคอนโทรลเลอร์ให้เป็นอุปกรณ์คีย์บอร์ด เมาส์ และเกมแพด
// ในเวลาเดียวกัน
//
// == การคอมไพล์ ==
// ต้องการไลบรารี pt และ usbdrv (มีมาให้กับแพคเกจ cpeku-practicum เรียบร้อยแล้ว)
//
// == การใช้งาน ==
// ทำแสงบริเวณตัวรับแสงให้มืดเพื่อสั่งให้บอร์ดเริ่มส่งคีย์และขยับเมาส์
//
// == หมายเหตุ ==
// อุปกรณ์ USB ที่จำลองขึ้นกำหนดให้มีหมายเลข VID:PID เป็น 16c0:0482 ซึ่งได้รับการจดทะเบียน
// ให้ใช้งานเป็น Keyboard/Mouse/Joystick combo โดย VOTI
//////////////////////////////////////////////////////////////////////////////
#include <pt.h>
#include <usbdrv.h>
#include "peri.h"
#include "keycodes.h"

#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_MOUSE    2
#define REPORT_ID_GAMEPAD  3

#define LIGHT_HIGH_THRES    700
#define LIGHT_LOW_THRES     300
int btn_green = PIN_PC1;
int btn_red = PIN_PC2;
int btn_yellow = PIN_PC5;
int btn_blue = PIN_PC3;
int btn_white = PIN_PC4;
int joystick_y = PIN_PC0;

// มาโครสำหรับจำลองการหน่วงเวลาใน protothread
#define PT_DELAY(pt,ms,tsVar) \
  tsVar = millis(); \
  PT_WAIT_UNTIL(pt, millis()-tsVar >= (ms));

/////////////////////////////////////////////////////////////////////
// USB report descriptor, สร้างขึ้นจาก HID Descriptor Tool ซึ่ง
// ดาวน์โหลดได้จาก
//    http://www.usb.org/developers/hidpage/dt2_4.zip
//
// หรือใช้ HIDEdit ซึ่งให้บริการฟรีผ่านเว็บที่ http://hidedit.org/
//
// *** ไม่แนะนำให้สร้างเองด้วยมือ ***
/////////////////////////////////////////////////////////////////////
PROGMEM const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = 
{
  ////////////////////////////////////
  // โครงสร้าง HID report สำหรับคียบอร์ด
  ////////////////////////////////////
  0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
  0x09, 0x06,                    // USAGE (Keyboard)
  0xa1, 0x01,                    // COLLECTION (Application)
  0x85, 0x01,                    //   REPORT_ID (1)
  0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
  0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
  0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
  0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,                    //   REPORT_SIZE (1)
  0x95, 0x08,                    //   REPORT_COUNT (8)
  0x81, 0x02,                    //   INPUT (Data,Var,Abs)
  0x95, 0x06,                    //   REPORT_COUNT (1)
  0x75, 0x08,                    //   REPORT_SIZE (8)
  0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
  0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
  0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
  0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
  0xc0,                          // END_COLLECTION

  //////////////////////////////////////
  // โครงสร้าง HID report สำหรับเมาส์ 3 ปุ่ม
  //////////////////////////////////////
  0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
  0x0b, 0x02, 0x00, 0x01, 0x00,  // USAGE (Generic Desktop:Mouse)
  0xa1, 0x01,                    // COLLECTION (Application)
  0x0b, 0x01, 0x00, 0x01, 0x00,  //   USAGE (Generic Desktop:Pointer)
  0xa1, 0x00,                    //   COLLECTION (Physical)
  0x85, 0x02,                    //     REPORT_ID (2)
  0x05, 0x09,                    //     USAGE_PAGE (Button)
  0x1b, 0x01, 0x00, 0x09, 0x00,  //     USAGE_MINIMUM (Button:Button 1)
  0x2b, 0x03, 0x00, 0x09, 0x00,  //     USAGE_MAXIMUM (Button:Button 3)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
  0x75, 0x01,                    //     REPORT_SIZE (1)
  0x95, 0x03,                    //     REPORT_COUNT (3)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x75, 0x05,                    //     REPORT_SIZE (5)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
  0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
  0x0b, 0x30, 0x00, 0x01, 0x00,  //     USAGE (Generic Desktop:X)
  0x0b, 0x31, 0x00, 0x01, 0x00,  //     USAGE (Generic Desktop:Y)
  0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
  0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
  0x75, 0x08,                    //     REPORT_SIZE (8)
  0x95, 0x02,                    //     REPORT_COUNT (2)
  0x81, 0x06,                    //     INPUT (Data,Var,Rel)
  0xc0,                          //     END_COLLECTION
  0xc0,                          // END_COLLECTION

  ////////////////////////////////////////////////////////////
  // โครงสร้าง HID report สำหรับเกมแพดแบบหนึ่งปุ่มกดและหนึ่งก้านแอนะล็อก
  ////////////////////////////////////////////////////////////
  0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
  0x09, 0x04,                    // USAGE (Joystick)
  0xa1, 0x01,                    // COLLECTION (Application)
  0x05, 0x01,                    //   USAGE_PAGE (Generic Desktop)
  0x09, 0x01,                    //   USAGE (Pointer)
  0xa1, 0x00,                    //   COLLECTION (Physical)
  0x85, 0x03,                    //     REPORT_ID (3)
  0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
  0x09, 0x32,                    //     USAGE (Z)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x26, 0xff, 0x03,              //     LOGICAL_MAXIMUM (1023)
  0x75, 0x0a,                    //     REPORT_SIZE (10)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x05, 0x09,                    //     USAGE_PAGE (Button)
  0x09, 0x01,                    //     USAGE (Button 1)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
  0x75, 0x01,                    //     REPORT_SIZE (1)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x75, 0x01,                    //     REPORT_SIZE (1)
  0x95, 0x05,                    //     REPORT_COUNT (5)
  0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
  0xc0,                          //   END_COLLECTION
  0xc0                           // END_COLLECTION
};

struct ReportKeyboard
{
  /* +----\------+-----+-----+-----+-----+-----+-----+-----+-----+
   * |Byte \ Bit |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
   * +------\----+-----+-----+-----+-----+-----+-----+-----+-----+
   * |  0        |               Report ID = 1                   |
   * +-----------+-----+-----+-----+-----+-----+-----+-----+-----+
   * |  1        |           Modifiers (shift,ctrl,etc)          |
   * +-----------+-----+-----+-----+-----+-----+-----+-----+-----+
   * |  2        |                 Key Code                      |
   * +-----------+-----+-----+-----+-----+-----+-----+-----+-----+
   */
  uint8_t  report_id;
  uint8_t  modifiers;
  uint8_t  key_code[6];
  
};

struct ReportMouse
{
  /* +----\------+-----+-----+-----+-----+-----+-----+-----+-----+
   * |Byte \ Bit |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
   * +------\----+-----+-----+-----+-----+-----+-----+-----+-----+
   * |  0        |               Report ID = 2                   |
   * +-----------+-----+-----+-----+-----+-----+-----+-----+-----+
   * |  1        |              Buttons' statuses                |
   * +-----------+-----+-----+-----+-----+-----+-----+-----+-----+
   * |  2        |                  Delta X                      |
   * +-----------+-----+-----+-----+-----+-----+-----+-----+-----+
   * |  3        |                  Delta Y                      |
   * +-----------+-----+-----+-----+-----+-----+-----+-----+-----+
   */
  uint8_t  report_id;
  uint8_t  buttons;
  int8_t   dx;
  int8_t   dy;
};

struct ReportGamepad
{
  /*
   * +----\------+-----+-----+-----+-----+-----+-----+-----+-----+
   * |Byte \ Bit |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
   * +------\----+-----+-----+-----+-----+-----+-----+-----+-----+
   * |  0        |               Report ID = 3                   |
   * +-----------+-----+-----+-----+-----+-----+-----+-----+-----+
   * |  1        |                 Light (L)                     |
   * +-----------+-----+-----+-----+-----+-----+-----+-----+-----+
   * |  2        |            UNUSED           | Btn | Light (H) |
   * +-----------+-----+-----+-----+-----+-----+-----+-----+-----+
   */
  uint8_t  report_id;
  uint16_t light:10;  // ค่าแสง 10 บิต (0-1023)
  uint8_t  button:1;  // ปุ่ม 1 บิต
  uint8_t  unused:5;  // ไม่ใช้ แต่ต้องเติมให้เต็มไบต์
};


ReportKeyboard reportKeyboard;
ReportMouse reportMouse;
ReportGamepad reportGamepad;

uint16_t light;  // Updated by Light-Task; to be shared among threads

// Protothread states
struct pt main_pt;
struct pt light_pt;
struct pt haunting_pt;
struct pt gamepad_pt;
struct pt shift_pt;
struct pt green_pt;
struct pt red_pt;
struct pt yellow_pt;
struct pt blue_pt;
struct pt down_pt;

////////////////////////////////////////////////////////////////
// Automatically called by usbpoll() when host makes a request
////////////////////////////////////////////////////////////////
usbMsgLen_t usbFunctionSetup(uchar data[8])
{
  return 0;  /* Return nothing to host for now */
}

//////////////////////////////////////////////////////////////
void sendKey(uint8_t index, uint8_t keycode, uint8_t modifiers)
{
  reportKeyboard.key_code[index] = keycode;
  reportKeyboard.modifiers = modifiers;
  usbSetInterrupt((uchar*)&reportKeyboard, sizeof(reportKeyboard));
}

//////////////////////////////////////////////////////////////
void sendMouse(int8_t dx, int8_t dy, uint8_t buttons)
{
  reportMouse.dx = dx;
  reportMouse.dy = dy;
  reportMouse.buttons = buttons;
  usbSetInterrupt((uchar*)&reportMouse, sizeof(reportMouse));
}

//////////////////////////////////////////////////////////////
/*PT_THREAD(light_task(struct pt *pt))
{
  PT_BEGIN(pt);

  for (;;)
  {
    light = get_light();

    // Set the LED corresponding to the current light level
    if (light > LIGHT_HIGH_THRES)
      set_led_value(0b100);
    else if (light > LIGHT_LOW_THRES)
      set_led_value(0b010);
    else
      set_led_value(0b001);

    PT_YIELD(pt);
  }

  PT_END(pt);
}*/

//////////////////////////////////////////////////////////////

///////////////////////////////////////////////////
PT_THREAD(shift_task(struct pt *pt))
{
  static uint32_t ts = 0;
  PT_BEGIN(pt);

  /*for (;;) {
  PT_WAIT_UNTIL(pt,digitalRead(btn_green)==LOW);
  sendKey(0,KEY_NONE,KEY_MOD_LEFT_SHIFT);                                                                      
  PT_WAIT_UNTIL(pt,digitalRead(btn_green)==HIGH);
  sendKey(0,KEY_NONE,0);                                                                      
  }*/
  
  int green = digitalRead(btn_green);
  Serial.print("white = ");
  Serial.println(green);
  if(green ==LOW)
  {
    sendKey(0,KEY_NONE,KEY_MOD_LEFT_SHIFT);                                                                            
  }else
  {
     sendKey(0,KEY_NONE,0);
  }
  
  PT_DELAY(pt,1,ts)
  PT_END(pt); 
}

//////////////////////////////////////////////////////////////
PT_THREAD(green_task(struct pt *pt))
{
   static uint32_t ts = 0;
  PT_BEGIN(pt);

  for (;;) {
  PT_WAIT_UNTIL(pt,digitalRead(btn_white)==LOW);
  sendKey(1,KEY_V,0);                                                                      
  PT_WAIT_UNTIL(pt,digitalRead(btn_white)==HIGH);
  sendKey(1,KEY_NONE,0);                                                                      
  }
  //bool isgreen = white; 
  //Serial.print("green = ");
  //Serial.println(white);
//  if(white ==LOW)
//  {
//  }
//  else
//  {
//    sendKey(0,KEY_NONE,0);
//  }
//  PT_DELAY(pt,1,ts)
  PT_END(pt);
}
//////////////////////////////////////////////////////////////
PT_THREAD(red_task(struct pt *pt))
{
   static uint32_t ts = 0;
  PT_BEGIN(pt);

  for (;;) {
  PT_WAIT_UNTIL(pt,digitalRead(btn_red)==LOW);
  sendKey(2,KEY_C,0);                                                                      
  PT_WAIT_UNTIL(pt,digitalRead(btn_red)==HIGH);
  sendKey(2,KEY_NONE,0);                                                                      
  }
  //bool isgreen = white; 
  //Serial.print("green = ");
  //Serial.println(white);
//  if(white ==LOW)
//  {
//  }
//  else
//  {
//    sendKey(0,KEY_NONE,0);
//  }
//  PT_DELAY(pt,1,ts)
  PT_END(pt);
}
//////////////////////////////////////////////////////////////
PT_THREAD(yellow_task(struct pt *pt))
{
   static uint32_t ts = 0;
  PT_BEGIN(pt);

  for (;;) {
  PT_WAIT_UNTIL(pt,digitalRead(btn_yellow)==LOW);
  sendKey(3,KEY_X,0);                                                                      
  PT_WAIT_UNTIL(pt,digitalRead(btn_yellow)==HIGH);
  sendKey(3,KEY_NONE,0);                                                                      
  }
  //bool isgreen = white; 
  //Serial.print("green = ");
  //Serial.println(white);
//  if(white ==LOW)
//  {
//  }
//  else
//  {
//    sendKey(0,KEY_NONE,0);
//  }
//  PT_DELAY(pt,1,ts)
  PT_END(pt);
}
//////////////////////////////////////////////////////////////
PT_THREAD(blue_task(struct pt *pt))
{
   static uint32_t ts = 0;
  PT_BEGIN(pt);

  for (;;) {
  PT_WAIT_UNTIL(pt,digitalRead(btn_blue)==LOW);
  sendKey(4,KEY_Z,0);                                                                      
  PT_WAIT_UNTIL(pt,digitalRead(btn_blue)==HIGH);
  sendKey(4,KEY_NONE,0);                                                                      
  }
  //bool isgreen = white; 
  //Serial.print("green = ");
  //Serial.println(white);
//  if(white ==LOW)
//  {
//  }
//  else
//  {
//    sendKey(0,KEY_NONE,0);
//  }
//  PT_DELAY(pt,1,ts)
  PT_END(pt);
}
//////////////////////////////////////////////////////////////
PT_THREAD(down_task(struct pt *pt))
{
   static uint32_t ts = 0;
  PT_BEGIN(pt);

  for (;;) {
  PT_WAIT_UNTIL(pt,analogRead(joystick_y)<=2);
  sendKey(5,KEY_UP_ARROW,0);                                                                      
  PT_WAIT_UNTIL(pt,analogRead(joystick_y)>2);
  sendKey(5,KEY_NONE,0);                                                                      
  }
  //bool isgreen = white; 
  //Serial.print("green = ");
  //Serial.println(white);
//  if(white ==LOW)
//  {
//  }
//  else
//  {
//    sendKey(0,KEY_NONE,0);
//  }
//  PT_DELAY(pt,1,ts)
  PT_END(pt);
}
//////////////////////////////////////////////////////////////
PT_THREAD(gamepad_task(struct pt *pt))
{
  PT_BEGIN(pt);
  reportGamepad.light = light;
//  reportGamepad.button = IS_SW_PRESSED();
  usbSetInterrupt((uchar*)&reportGamepad, sizeof(reportGamepad));
  PT_END(pt);
}

//////////////////////////////////////////////////////////////
PT_THREAD(main_task(struct pt *pt))
{
  PT_BEGIN(pt);

//  light_task(&light_pt);
  shift_task(&shift_pt);
  green_task(&green_pt);
  red_task(&red_pt);   
  yellow_task(&yellow_pt);
  blue_task(&blue_pt);
  down_task(&down_pt);
 /* PT_WAIT_UNTIL(pt,usbInterruptIsReady());
  gamepad_task(&gamepad_pt);
  PT_WAIT_UNTIL(pt,usbInterruptIsReady());
  haunting_task(&haunting_pt);
  */
  PT_END(pt);
}

//////////////////////////////////////////////////////////////
void setup()
{
  // Initialize USB subsystem
  usbInit();
  usbDeviceDisconnect();
  delay(300);
  usbDeviceConnect();
   Serial.begin(9600);
  
  init_peripheral();
  
 // pinMode(PIN_PC0,INPUT);
  pinMode(btn_green,INPUT);
  pinMode(btn_yellow,INPUT);
  pinMode(btn_blue,INPUT);
  pinMode(btn_white,INPUT);
  pinMode(btn_red,INPUT);                                                           
  pinMode(PIN_PD3,OUTPUT);
  
                                
  // Initialize USB reports
  reportKeyboard.report_id = REPORT_ID_KEYBOARD;
  reportKeyboard.modifiers = 0;
  reportKeyboard.key_code[0] = KEY_NONE;
  reportKeyboard.key_code[1] = KEY_NONE;

  reportMouse.report_id = REPORT_ID_MOUSE;
  reportMouse.dx = 0;
  reportMouse.dy = 0;
  reportMouse.buttons = 0;
                          
  reportGamepad.report_id = REPORT_ID_GAMEPAD;
  reportGamepad.light = 0;
  reportGamepad.button = 0;

  // Initialize tasks
  PT_INIT(&main_pt);
  PT_INIT(&light_pt);
  PT_INIT(&haunting_pt);
  PT_INIT(&gamepad_pt);
  PT_INIT(&shift_pt);       
  PT_INIT(&green_pt);                                 
}                                                                         
//////////////////////////////////////////////////////////////                                  
void loop()
{                                                                                                                                                      
  usbPoll();
  int y = analogRead(joystick_y);   
  int red = digitalRead(btn_red);
  int yellow = digitalRead(btn_yellow);
  int blue = digitalRead(btn_blue);
  int white = digitalRead(btn_white);
  
  //Serial.println(y);  
 /* 
  if(y <= 2) 
  {                                                                      
    sendKey(KEY_UP_ARROW,0);                                                               
  }
  else
  if(red ==LOW)
  {
    sendKey(KEY_C,0);                                                                             
  }else
  if(yellow ==LOW)
  {
    sendKey(KEY_X,0);                                                                             
  }
  else
  if(blue ==LOW)
  {
    sendKey(KEY_Z,0);                                                                           
  }else
  if(white ==LOW)
  {
    sendKey(KEY_V,0);                                                                             
  }
  else
  {
    sendKey(KEY_NONE,0);
  }
  */
  

  //shift_task(&shift_pt);      
  //green_task(&green_pt);                                                   
  main_task(&main_pt);
}
