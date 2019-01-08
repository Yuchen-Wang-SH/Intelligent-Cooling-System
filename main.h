#ifndef MAINH
#define MAINH

#define Somenop();       _nop_();_nop_();_nop_();_nop_();_nop_();
#define Somenop10();	 Somenop();Somenop();
#define Somenop25();	 Somenop();Somenop();Somenop();Somenop();Somenop();
#define Somenop50();	 Somenop25();Somenop25();
#define CMD_RESET 0xA4
#define PIDA 12 
#define PIDB 10
#define DOWN 0x00
#define UP 0x01
#define BACK 0x02
#define ENTER 0x03
#define UPPER_LINE 0x00
#define LOWER_LINE 0x01
#define DOUBLE_LINE 0x02
#define TRUE 0x01
#define FALSE 0x00
#define U8 unsigned char
#define U16 unsigned short

#define FOSC    11059200        //振荡频率
#define BAUD    9600            //波特率
#define TC_VAL  (256-FOSC/16/12/BAUD)

sbit CS  = P1^4;
sbit CLK  = P1^5;
sbit DATA = P1^7;
sbit DS1820_DQ = P1^3;
// extern DS1820_DQ
sbit Motor  = P1^2;
sbit ECLK  = P1^1;
sbit EDTA  =  P1^0;
// extern sbit CS;
// extern sbit CLK;
// extern sbit DATA;
// extern sbit DS1820_DQ;
// extern sbit Motor;
// extern sbit ECLK;
// extern sbit EDTA;


// KeyValue is the address, and KeyNum is the index
unsigned char KeyNum, KeyValue;
unsigned char TempCount, RunCount, MotorCount, MotorNow,PACount;
unsigned char rNum, bFNwum, NowEPVal, LastVal;
int  NowEPVal1, LastVal1;
unsigned int  KeyCount1, KeyCount2;
unsigned char PAStep;
void Init_7279(void) ;
void send_byte (unsigned char) ;
void delay (unsigned char);
void write_7279 (unsigned char,unsigned char); 
void display (unsigned char buff[]);
void Key(void);
unsigned char receive_byte (void);
unsigned char ReadKey (void);
void display_string_in_row (char* string, unsigned char upper);
unsigned char change_menu_ptr(unsigned char current, unsigned char inc, unsigned char volume);

void show_PA_menu(void);
void show_current_run(void);
void show_current_temp_threshold(void);
// unsigned char changeNumConti(unsigned char i);
void change_num_conti(unsigned char *num);
void wait_until_release(void);

void show_temperature(unsigned char);
void display_temperature(unsigned char);

// sbit DS1820_DQ = P1^4;
void DS18B20_Init();
bit DS1820_Reset();
void DS1820_WriteData(U8 wData);
U8 DS1820_ReadData();

void show_motor_test(void);
void run_motor_with_PWM();

void con_with_temp(unsigned char);
void calc_current_PWM();

void read_run_options_from_C16();
void write_run_options_to_C16();
void read_temp_threshold_from_C16();
void write_temp_threshold_to_C16();

void show_current_PID_goal_temp();
unsigned char check_pwd();

void read_PID_goal_temp_from_C16();
void write_PID_goal_temp_to_C16();

void PIDInit();
void calc_PWM_PID();

void UART_ISR(void);
void InitUART(void);
void SendOneByte(U8 c);
void send_temp_to_computer();

#endif