#define F_CPU 14745600UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include "lcd.h"

#define BIT_SET(a,b) ((a) |=  (1<<(b))) // 해당 비트 1로 SET.
#define BIT_CLEAR(a,b) ((a) &=  ~(1<<(b))) // 해당 비트 0으로 CLEAR.
#define BIT_TOGGLE(a,b) ((a) ^=  (1<<(b))) // 해당 비트 반전 시킨다.
#define BIT_CHECK(a,b) (!!((a) &  (1<<(b))))

#define BACK 0b01010101 // 역방향 회전
#define GO 0b10101010 // 정방향 회전
#define STOP 0b11111111 // 정지(브레이크)

#define REC_BUFF_MAX_LENGTH 100

#define ADC_P0 0   // ADC ch0
#define ADC_P1 1   // ADC ch1
#define ADC_P2 2   // ADC ch2
#define ADC_P3 3   // ADC ch3
#define ADC_P4 4   // ADC ch4
#define ADC_P5 5   // ADC ch5
#define ADC_P6 6   // ADC ch6
#define ADC_P7 7   // ADC ch7

#define ADC_AVCC_TYPE 0x40
#define ADC_VREF_TYPE 0x00
#define ADC_RES_TYPE  0x80
#define ADC_2_56_TYPE 0xc0

// 초음파 센서 활용
#define Inches      0x50
#define Centimeters 0x51
#define microSec   0x52

#define CommandReg      0
#define Unused         1
#define RangeHighByte   2
#define RangeLowByte   3

volatile unsigned int mode = 0; // 기본값 정지상태
volatile unsigned char joi_rx = 'i'; // 기본값

volatile unsigned int FB, RL = 0; // 전, 후진 정보 - FB        좌, 우회전 정보 - RL
volatile unsigned int F_obstacle, B_obstacle, L_obstacle, R_obstacle = 0;  // 해당 방향 장애물
volatile unsigned int S_flag = 0;

unsigned char RecBuff[REC_BUFF_MAX_LENGTH];
unsigned char RecBuffindex = 0;
unsigned char RecFlg;
unsigned char RecBuff_estLength = REC_BUFF_MAX_LENGTH;

volatile unsigned int F_CNT = 0;
volatile unsigned int B_CNT = 0;
volatile unsigned int R_CNT = 0;
volatile unsigned int L_CNT = 0;

// 부저 출력을 위한 세팅.
void buzzer_set(){
	TCCR0 |= 0x03;     //0b00000011     CS02~00 = 011이므로 프리스케일러 32분주
	TIMSK |= 0x01;      //0b00000001     TOIE0 '1'로 세팅하여 오버플로우 인터럽트 발생
	sei();      //전역 인터럽트 활성화
	TCNT0 = 17;
}
volatile unsigned int buz_flag = 0; // 부저 출력을 결정하기 위한 buz_flag
ISR(TIMER0_OVF_vect){
	TCNT0=17;     //인터럽트 발생시마다 TCNT0 초기화
	
	if(buz_flag == 1){ // 부저 출력
		BIT_TOGGLE(PORTG, 4);
	}
	else{ // 부저 출력 정지.
		BIT_CLEAR(PORTG, 4);
	}
}

// 타이머
void init_timer(void)
{
	TIMSK |= (1<<TOIE2) | (1<<OCIE2); // 오버플로우 인터럽트 선언
	TCCR2 |= (1<<CS21);   // 분주비 8
	OCR2 = 100;
	
	sei(); // global 인터럽트 선언
}

ISR(TIMER2_OVF_vect)
{
	if(S_flag == 1) // 장애물이 있을 때
	{
		BIT_SET(PORTE , 6);  // 적색 LED ON
		BIT_CLEAR(PORTE, 7); // 녹색 LED OFF
		if(FB == 1 && F_obstacle == 1) {PORTC &= 0xe1;}       // 1110 0001  - e1
		else if(FB == 2 && B_obstacle == 1) {PORTC &= 0xe1;}
		else if(FB == 0) {PORTC &= 0xe1;}
		else {
			BIT_SET(PORTC, 1);  // 앞쪽 왼쪽 바퀴  (1)
			BIT_SET(PORTC, 3);  // 뒤쪽 왼쪽 바퀴  (3)
			
			BIT_SET(PORTC, 2);  // 앞쪽 오른쪽 바퀴 (2)
			BIT_SET(PORTC, 4);  // 뒤쪽 오른쪽 바퀴 (4)
		}
	}
	else if(S_flag == 0){ // 장애물이 없을 때
		BIT_SET(PORTE , 7);  // 녹색 LED ON
		BIT_CLEAR(PORTE, 6); // 적색 LED OFF
		if(FB != 0){
			BIT_SET(PORTC, 1);  // 앞쪽 왼쪽 바퀴  (1)
			BIT_SET(PORTC, 3);  // 뒤쪽 왼쪽 바퀴  (3)
			
			BIT_SET(PORTC, 2);  // 앞쪽 오른쪽 바퀴 (2)
			BIT_SET(PORTC, 4);  // 뒤쪽 오른쪽 바퀴 (4)
		}
		else {PORTC &= 0xe1;}
	}
}

ISR(TIMER2_COMP_vect)
{
	// 우회전 오른쪽 바퀴가 천천히 돌아야 한다.
	if( (RL == 1) && (R_obstacle == 0)){
		BIT_CLEAR(PORTC, 2);  // 앞쪽 오른쪽 바퀴 (2)
		BIT_CLEAR(PORTC, 4);  // 뒤쪽 오른쪽 바퀴 (4)
	}
	// 좌회전 왼쪽 바퀴가 천천히 돌아야 한다.
	else if( (RL == 2) && (L_obstacle == 0)){
		BIT_CLEAR(PORTC, 1);  // 앞쪽 왼쪽 바퀴 (1)
		BIT_CLEAR(PORTC, 3);  // 뒤쪽 왼쪽 바퀴 (3)
	}
	
}

void putch_USART0(char data) {
	while( !(UCSR0A & (1<<UDRE0)));
	UDR0 = data;
}
void sendBuff_USART0(char *str,int length){  // 문자열 출력 루틴
	while (length--){
		putch_USART0(*str);  // ‘\0’이 나올 때까지 출력한다.
	str++; }
}
void Init_USART0_IntCon(void){
	// ① RXCIE1=1(수신 인터럽트 허가), RXEN0=1(수신 허가), TXEN0 = 1(송신 허가)
	UCSR0B = (1<<RXCIE0)| (1<<RXEN0) | (1 <<TXEN0);
	UBRR0H = 0x00; // ② 57600bps 보오레이트 설정
	UBRR0L = 0x5f; // ② 57600bps 보오레이트 설정
}
ISR(USART0_RX_vect){   // 인터럽트 루틴에서의 데이터 수신
	char buffer = UDR0;
	if(buffer == 'A') {
		RecBuffindex = 0;
		RecBuff[0] = buffer;
	}
	else RecBuff[RecBuffindex] = buffer;

	RecBuffindex++;
	if(RecBuffindex == 4) {
		RecBuffindex = 0;
		RecFlg = 1; // 패킷 수신 완료 flag
	}
}

int parsingPacket(char *recBuff){ // 패킷을 파싱하는 함수
	// 패킷검사 1. 만약 시작 데이터가 정해진 데이터(0xFF)가 아닌 경우 종료.
	if(recBuff[0] != 'A') return -1;
	
	switch(recBuff[1])
	{
		case 'F':  // 데이터의 종류가 전진인 경우
		FB = 1;
		PORTB = GO; // 전진
		break;
		
		case 'B':  // 데이터의 종류가 후진인 경우
		FB = 2;
		PORTB = BACK; // 후진
		break;
		
		case 'S':  // 데이터의 종류가 STOP인 경우
		FB = 0;
		PORTB = STOP; // 정지
		break;
	}
	
	switch(recBuff[2])
	{
		case 'R':  // 데이터의 종류가 우회전인 경우
		RL = 1;
		break;
		
		case 'L':  // 데이터의 종류가 좌회전인 경우
		RL = 2;
		break;
		
		case 'N':  // 데이터의 종류가 STOP인 경우
		RL = 0;
		break;
	}
	
	switch(recBuff[3])
	{
		case 'N': // 전, 후, 좌, 우 센서 값을 LCD에 출력하라는 명령어를 수신 받은 경우.
		mode = 0;
		break;
		
		case 'E': // EEPROM 저장 정보를 LCD에 출력하라는 명령어를 수신 받은 경우.
		mode = 1;
	}
	return 0;
}

//EEPROM
void EEPROM_Write(unsigned int EE_Addr, unsigned char EE_Data)
{
	while(EECR & (1<<EEWE));
	EEAR = EE_Addr;
	EEDR = EE_Data; // EEDR <- EE_Data
	cli(); // Global Interrupt Disable
	EECR |= (1 << EEMWE); // SET EEMWE
	EECR |= (1 << EEWE); // SET EEWE
	sei(); // Global Interrupt Enable
}
unsigned char EEPROM_Read(unsigned int EE_addr)
{
	while(EECR & (1<<EEWE));
	EEAR = EE_addr;
	EECR |= (1<<EERE); // SET EERE
	return EEDR; // return read value
}

/************************************************************************/
/*                          ADC                                          */
/************************************************************************/
void AD_init(void){
	//ADMUX = 0xc1;   // 0b0100 0000, 내부 기준 전압 5v 사용, select adc input 0
	//ADCSRA = 0xa0;   // 0b 1010 0000, ADC허가, 프리러닝 허가
	ADCSRA = 0x00;
	ADMUX  = 0x00;
	ACSR   = 0x80;
	ADCSRA = 0xc3;
}

unsigned int ADC_Read(short Data)
{
	unsigned int ADC_Value;
	
	switch(Data)
	{
		case ADC_P0: ADMUX |= 0XC0; break;   // ADC ch0
		case ADC_P1: ADMUX |= 0XC1; break;   // ADC ch1
		case ADC_P2: ADMUX |= 0XC2; break;   // ADC ch2
		case ADC_P3: ADMUX |= 0XC3; break;   // ADC ch3
		case ADC_P4: ADMUX |= 0XC4; break;   // ADC ch4
		case ADC_P5: ADMUX |= 0XC5; break;   // ADC ch5
		case ADC_P6: ADMUX |= 0XC6; break;   // ADC ch6
		case ADC_P7: ADMUX |= 0XC7; break;   // ADC ch7
		default: break;
	}
	
	ADCSRA |= 0X40;      // ADC 변환 시작(Start)
	ADC_Value = ADCW;   // ADCW에서 10비트 ADC 데이터 얻음
	
	return ADC_Value;
}

unsigned int Read_Adc_Data(unsigned char adc_input)
{
	unsigned int adc = 0;	// ADC_Data 변수 = 0
	
	ADCSRA = 0xc3;
	
	ADMUX = adc_input | ADC_AVCC_TYPE;
	ADCSRA |= 0x40; // AD 변환 시작
	
	while ((ADCSRA & 0x10) != 0x10);  // AD 변환 종료 대기
	
	adc = ADCL + (ADCH << 8); // ADC 변환 값 읽기. 하위 비트 먼저 읽고 상위 비트를 읽는다.
	
	ADCSRA = 0x00;
	return adc; // ADC 변환값 반환
}


// id 주소가 다른 초음파 센서 2개를 사용하기

void TWI_Init(){
	TWBR = 10;
	TWSR = 0;
	TWCR = 0;
}

unsigned char TWI_Read(unsigned char addr, unsigned char regAddr)
{
	unsigned char Data=0;
	
	TWCR = ((1<<TWINT)|(1<<TWEN)|(1<<TWSTA));   //Start조건 전송
	while(!(TWCR&(1<<TWINT)));
	
	TWDR = addr&(~0x01);                  //쓰기 위한 주소 전송
	TWCR = ((1<<TWINT)|(1<<TWEN));
	while(!(TWCR&(1<<TWINT)));
	
	TWDR = regAddr;                        //Reg주소 전송
	TWCR = ((1<<TWINT)|(1<<TWEN));
	while(!(TWCR&(1<<TWINT)));
	
	TWCR = ((1<<TWINT)|(1<<TWEN)|(1<<TWSTA));   //Restart 전송
	while(!(TWCR&(1<<TWINT)));
	
	TWDR = addr|0x01;                     //읽기 위한 주소 전송
	TWCR = ((1<<TWINT)|(1<<TWEN));
	while(!(TWCR&(1<<TWINT)));
	
	TWCR = ((1<<TWINT)|(1<<TWEN));
	while(!(TWCR&(1<<TWINT)));
	Data = TWDR;                        //Data읽기
	
	TWCR = ((1<<TWINT)|(1<<TWEN)|(1<<TWSTO));
	
	return Data;
}

void TWI_Write(unsigned char addr, unsigned char Data[],int NumberOfData)
{
	int i=0;
	
	TWCR = ((1<<TWINT)|(1<<TWEN)|(1<<TWSTA));
	while(!(TWCR&(1<<TWINT)));
	
	TWDR = addr&(~0x01);
	TWCR = ((1<<TWINT)|(1<<TWEN));
	while(!(TWCR&(1<<TWINT)));
	
	for(i=0;i<NumberOfData;i++){
		TWDR = Data[i];
		TWCR = ((1<<TWINT)|(1<<TWEN));
		while(!(TWCR&(1<<TWINT)));
	}
	
	TWCR = ((1<<TWINT)|(1<<TWEN)|(1<<TWSTO));
}

void Start_SRF02_Conv(unsigned char mode, char id)
{
	// 처음 전원 연결했을때, 초음파센서 LED가 감빡 거리는 횟수가 주소를 표현해주는 것이다.
	// 2번 깜박거려서 0XE2로 1번 깜박거리면 0XE0로 한다.
	unsigned char ConvMode[2] = {0x00,};
	ConvMode[1] = mode;
	TWI_Write(id,ConvMode,2);
}

unsigned int Get_SRF02_Range(char id)
{
	unsigned int range;
	unsigned char High,Low;

	High = TWI_Read(id, RangeHighByte);
	Low = TWI_Read(id, RangeLowByte);

	range = (High<<8)+Low;
	return range; // 센서 값 반환
}


void main(void)
{
	char len1[7]={0,};
	char len2[7]={0,};
	
	unsigned int range1;  // 초음파 센서 1
	unsigned int range2;  // 초음파 센서 2
	
	unsigned char id1=0xe0;      // 앞쪽 초음파 센서 주소
	unsigned char id2=0xe2;      // 뒤쪽 초음파 센서 주소
	
	float  ADC_data1 = 0; // 적외선 거리 감지 센서 1
	float  ADC_data2 = 0; // 적왼선 거리 감지 센서 2
	
	float voltage1 = 0;  // ADC로 읽어온 전압
	float voltage2 = 0;  // ADC로 읽어온 전압
	
	unsigned int  L1 = 0; // 오른쪽 적외선 거리 감지 센서 거리
	unsigned int  L2 = 0; // 왼쪽 적외선 거리 감지 센서 거리
	
	char str1[50] = {0};
	char str2[50] = {0};
	
	DDRB = 0xFF;  // DC엔코더 모터 사용 핀 출력 설정
	PORTB = 0x00;
	
	DDRC = 0xff;  // DC엔코더 모터 PWM 제어를 위해 출력핀 설정. PWM 제어는 PC1~4번을 사용한다.
	PORTC = 0x00;
	
	DDRD = 0x00;  // 초음파 센서 SRF02 사용을 위한 입력 설정.
	DDRE = 0xc0; // (1100 0000 C0)  3색 LED R,G 핀 2개만 사용 PE6~7번 사용.
	
	AD_init();
	TWI_Init();
	_delay_ms(10);
	
	LCD_Init(); // LCD 초기화
	LCD_Clear();
	LCD_Pos(0,0);
	Init_USART0_IntCon();
	init_timer();
	
	buzzer_set();
	
	unsigned int eep_sum = 0; // EEPROM에 저장된 총 CNT 합
	
	unsigned int swCnt = 0;
	char str_F[20]; char str_B[20]; char str_R[20]; char str_L[20]; // EEPROM 저장 정보 LCD에 출력을 위한 임시 문자열 선언.
	
	unsigned char lRecBuff[100];        // 수신패킷 임시저장용 배열
	unsigned char lRecBuffLength = 0;   // 수신패킷 길이 저장
	int res = 0;
	
	while(1){
		
		
		ADC_data1 = Read_Adc_Data(ADC_P5);   // ADC0 data read, ADCW 값, R  우
		ADC_data2 = Read_Adc_Data(ADC_P3);   // ADC0 data read, ADCW 값, L  좌
		
		voltage1 = ADC_data1 / 1024 * 5;
		L1 = (1/voltage1 - 0.09) * 37.7 * 10;         //우
		sprintf(str1,"%3dmm",L1); // 우측 적외선 거리 감지 센서 값 str1에 저장
		
		voltage2 = ADC_data2 / 1024 * 5;
		L2 = (1/voltage2 - 0.09) * 37.7 * 10;         //좌
		sprintf(str2,"%3dmm",L2); // 좌측 적외선 거리 감지 센서 값 str1에 저장
		
		Start_SRF02_Conv(Centimeters, id1);  //Centimeter 단위로 변환 시작
		_delay_ms(70);               //최소 대기시간
		range1 = Get_SRF02_Range(id1);      //변환값 가져옴  후
		sprintf(len1,"%3dcm", range1); //  후방 초음파 센서 값 len2에 저장
		
		Start_SRF02_Conv(Centimeters, id2);  //Centimeter 단위로 변환 시작
		_delay_ms(70);               //최소 대기시간
		range2 = Get_SRF02_Range(id2);      //변환값 가져옴
		sprintf(len2,"%3dcm", range2); //  전방 초음파 센서 값 len2에 저장
		
		if(RecFlg == 1) // 패킷 수신완료 플래그가 설정된 경우
		{
			// 데이터의 연속 수신을 고려하여, 임시변수에 수신된 데이터 저장
			memcpy(lRecBuff, RecBuff, RecBuff_estLength);
			lRecBuffLength = RecBuff_estLength;
			
			// 임시저장 후 또다시 패킷 수신을 위한 버퍼 및 관련변수 초기화
			RecFlg = 0;
			memset(RecBuff, 0, REC_BUFF_MAX_LENGTH);
			RecBuff_estLength = REC_BUFF_MAX_LENGTH;
			
			// 수신된 패킷을 파싱하는 함수
			res = parsingPacket(lRecBuff);
			
			if (range2 <= 30) {            //FRONT
				S_flag |= 1; // 다른 장애물 감지 센서들의 결과 값을 고려하여 OR 연산으로 연산한다.
				F_obstacle = 1; // 전방 장애물 감지시
				F_CNT += 1; // 전방 장애물 감지시 EEPROM에 해당 F_CNT 값 증가
				EEPROM_Write(0x10, F_CNT);	// EEPROM에 해당 F_CNT 값 작성
				buz_flag = 1; // 부저음 출력을 위한 buz_flag = 1
			}
			else {S_flag |= 0; F_obstacle = 0;} // 다른 장애물 감지 센서들의 결과 값을 고려하여 OR 연산으로 연산한다. F_obstacle = 0 초기화.
			
			if (range1 <= 30){             //BACK
				S_flag |= 1; // 다른 장애물 감지 센서들의 결과 값을 고려하여 OR 연산으로 연산한다.
				B_obstacle = 1; // 후방 장애물 감지시
				B_CNT += 1; // 후방 장애물 감지시 EEPROM에 해당 B_CNT 값 증가
				EEPROM_Write(0x20, B_CNT); // EEPROM에 해당 B_CNT 값 작성
				buz_flag = 1; // 부저음 출력을 위한 buz_flag = 1
			}
			else {S_flag |= 0; B_obstacle = 0;}   // 다른 장애물 감지 센서들의 결과 값을 고려하여 OR 연산으로 연산한다. B_obstacle = 0 초기화.
			
			if (L2 <= 150) {            //LEFT
				S_flag |= 1; // 다른 장애물 감지 센서들의 결과 값을 고려하여 OR 연산으로 연산한다.
				L_obstacle = 1; // 좌측 장애물 감지시
				L_CNT += 1; // 좌측 장애물 감지시 EEPROM에 해당 L_CNT 값 증가
				EEPROM_Write(0x40, L_CNT); // EEPROM에 해당 L_CNT 값 작성
				buz_flag = 1; // 부저음 출력을 위한 buz_flag = 1
			}
			else {S_flag |= 0; L_obstacle = 0;} // 다른 장애물 감지 센서들의 결과 값을 고려하여 OR 연산으로 연산한다. L_obstacle = 0 = 0 초기화.
			
			if (L1 <= 150) {            //RIGHT
				S_flag |= 1; // 다른 장애물 감지 센서들의 결과 값을 고려하여 OR 연산으로 연산한다.
				R_obstacle = 1;  // 우측 장애물 감지시
				R_CNT += 1; // 우측 장애물 감지시 EEPROM에 해당 R_CNT 값 증가
				EEPROM_Write(0x30, R_CNT); // EEPROM에 해당 R_CNT 값 작성
				buz_flag = 1; // 부저음 출력을 위한 buz_flag = 1
			}
			else { S_flag |= 0; R_obstacle = 0;} // 다른 장애물 감지 센서들의 결과 값을 고려하여 OR 연산으로 연산한다. R_obstacle = 0 초기화.
			
			// 모든 방향 (전, 후, 좌, 우) 장애물 없을 시 에만 S_flag = 0 초기화. 부저 울리지 않게 buz_flag = 0 초기화.
			if((F_obstacle | B_obstacle | L_obstacle | R_obstacle) == 0){
				S_flag = 0;  // 감지된 장애물 없음. S_flag = 0
				buz_flag = 0;  // 부저음 종료 buz_flag = 0
			}
			
			if(!(PIND & (1<<7))){
				// PD7 번 스위치를 눌렀다 뗴면 EEPROM에 저장된 모든 카운트 초기화
				while(!(PIND & (1<<7)));
				F_CNT = 0;
				B_CNT = 0;
				R_CNT = 0;
				L_CNT = 0;
				
				EEPROM_Write(0x10, F_CNT);
				EEPROM_Write(0x20, B_CNT);
				EEPROM_Write(0x30, R_CNT);
				EEPROM_Write(0x40, L_CNT);
			}
			// 평상시
			else{
				// EEPROM에 저장된 값을 읽어온다.
				F_CNT = EEPROM_Read(0x10);
				B_CNT = EEPROM_Read(0x20);
				R_CNT = EEPROM_Read(0x30);
				L_CNT = EEPROM_Read(0x40);
			}
			
			// 전, 후, 좌, 우 센서값 LCD 출력인 모드  : 수신 패킷 recBuff[3] == 'N' 인 경우
			if(mode == 0){
				LCD_Pos(0,0);
				
				LCD_Str("L1:");
				LCD_Str(str1); // 우측 적외선 거리 감지 센서 값 출력.
				
				LCD_Str("L2:");
				LCD_Str(str2); // 좌측 적외선 거리 감지 센서 값 출력.
				
				LCD_Pos(1,0);
				LCD_Str("r1:");
				LCD_Str(len1); // 후방 초음파 센서 값 출력.
				
				LCD_Str("r2:");
				LCD_Str(len2); // 전방 초음파 센서 값 출력.
				
				_delay_ms(100);
			}
			// EEPRMO 정보 출력을 위한 모드 : 수신 패킷 recBuff[3] == 'E' 인 경우
			else if(mode == 1){
				// 운전자의 운전 패턴을 표시하기 위해서 EEPROM에 저장된 모든 카운트 값을 더 하여 총 합 eep_sum을 계산하고
				// 각각의 CNT 값을 총 합 eep_sum 으로 나누고 100을 곱하여 백분율로 나타낸다.
				// 백분율 비율에 따라 운전자의 운전 취약 부분을 운전자에게 알려준다.
				eep_sum = F_CNT + B_CNT + R_CNT + L_CNT;
				if(eep_sum == 0){eep_sum = 1;} // PD7번 스위치를 눌러서 초기화된 경우 0 값을 나눗셈 분모로 쓰면 에러가 발생하기에 임의로 1값을 부여한다.

				// 운전자가 전방 장애물에 접근한 비율 계산 및 출력
				sprintf(str_F, "b_F:%2d %%", (F_CNT*100)/eep_sum);
				LCD_Pos(0,0);
				LCD_Str(str_F);
				
				// 운전자가 후방 장애물에 접근한 비율 계산 및 출력
				sprintf(str_B, "b_B:%2d %%", (B_CNT*100)/eep_sum);
				LCD_Pos(0,7);
				LCD_Str(str_B);
				
				// 운전자가 우측 장애물에 접근한 비율 계산 및 출력
				sprintf(str_R, "b_R:%2d %%", (R_CNT*100)/eep_sum);
				LCD_Pos(1,0);
				LCD_Str(str_R);
				
				// 운전자가 좌측 장애물에 접근한 비율 계산 및 출력
				sprintf(str_L, "b_L:%2d %%", (L_CNT*100)/eep_sum);
				LCD_Pos(1,7);
				LCD_Str(str_L);
				
				_delay_ms(100);
			}
		}
	}
}