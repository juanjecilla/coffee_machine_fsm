#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <wiringPi.h>
#include <stdio.h>
#include "fsm.h"

//IO FSM Coffee
#define GPIO_BUTTON	2
#define GPIO_LED	3
#define GPIO_CUP	4
#define GPIO_COFFEE	5
#define GPIO_MILK	6

//IO FSM Money
#define GPIO_COIN   7
#define GPIO_COIN_OUT 8

//MACROS
#define COIN 30
#define COFFEE_PRICE 50

//Temporizadores
#define CUP_TIME	250
#define COFFEE_TIME	3000
#define MILK_TIME	3000
//#define TOTAL_TIME 6500 //With a little secure margin

// -----------------------------------------------------------------------------------
// HOW TO USE!!!!
//
// PARA SIMULAR EL COMPORTAMIENTO DE LA RASPBERRY PI HEMOS SIMULADO LAS FUNCIONES 
// DE LA LIBRERIA Y SIMULAREMOS LAS ENTRADAS DEL SISTEMA CON VALORES INTRODUCIDOS 
// POR TECLADO, '1' PARA PULSAR EL BOTON DEL CAFE, '1' CUANDO SE INTRODUCE UNA MONEDA
// Y '1' CUANDO SE INICIA UN TIMER
//
// -----------------------------------------------------------------------------------


// FSMs
enum cofm_state {
  COFM_WAITING,
  COFM_CUP,
  COFM_COFFEE,
  COFM_MILK,
};

enum moneyfsm_state {
  MONEY_WAITING,
  MONEY_READY,
};

// ENTRADAS
static int button = 0;
static int cash = 0;
static int timer = 0;

// VARIABLES
static int coin = 0;
static int fin = 0;


static void button_isr (void){ 

  button = 1; 
}

static void coin_isr(void){

  coin = 1;
}

static void timer_isr (union sigval arg){ 

  timer = 1; 
}

static void timer_start (int ms){
  timer_t timerid;
  struct itimerspec value;
  struct sigevent se;
  se.sigev_notify = SIGEV_THREAD;
  se.sigev_value.sival_ptr = &timerid;
  se.sigev_notify_function = timer_isr;
  se.sigev_notify_attributes = NULL;
  value.it_value.tv_sec = ms / 1000;
  value.it_value.tv_nsec = (ms % 1000) * 1000000;
  value.it_interval.tv_sec = 0;
  value.it_interval.tv_nsec = 0;
  timer_create (CLOCK_REALTIME, &se, &timerid);
  timer_settime (timerid, 0, &value, NULL);
}

static int button_pressed (fsm_t* this){

  int ret = 0;

  if(button == 1){

    if(cash > COFFEE_PRICE){
      ret = button;
      button = 0;
      cash -= COFFEE_PRICE;
    }

    else{
      printf("Introduzca más dinero, hay sólo %d\n",cash);
    }
  }

  return ret;
}

static int coin_insert(fsm_t* this){
  
  int ret = 0;

  if(coin == 1){

    cash += COIN;
    coin = 0;

    if (cash>COFFEE_PRICE){
      ret = 1;
    }
    else{
      printf("Introduzca más dinero, hay sólo %d\n",cash);
    }
  }
  return ret;
}

static int timer_finished (fsm_t* this){
  int ret = timer;
  timer = 0;
  printf("Timer finalizado\n");
  return ret;
}

static void cup (fsm_t* this){
  digitalWrite (GPIO_LED, LOW);
  digitalWrite (GPIO_CUP, HIGH);
  timer_start (CUP_TIME);
}

static void coffee (fsm_t* this){
  digitalWrite (GPIO_CUP, LOW);
  digitalWrite (GPIO_COFFEE, HIGH);
  timer_start (COFFEE_TIME);
}

static void milk (fsm_t* this){
  digitalWrite (GPIO_COFFEE, LOW);
  digitalWrite (GPIO_MILK, HIGH);
  timer_start (MILK_TIME);
}

static void finish (fsm_t* this){
  digitalWrite (GPIO_MILK, LOW);
  digitalWrite (GPIO_LED, HIGH);
  fin = 1;
}

static int coffeFinish(void){
  int ret = 0;
  ret = fin;
  fin = 0;

  return ret;
}

// static void moreMoney(void){
//   printf("Introduzca mas dinero\n");
//   digitalWrite(GPIO_COIN_OUT, LOW);
// }

static void moneyReady(void){
  printf("Dinero listo\n");
  digitalWrite(GPIO_COIN_OUT, LOW);

  // Esperamos a que se pulse el boton del cafe
}

static void returnMoney(void){
  int cashBack = cash;
  cash = 0;
  printf("Su cambio es %d\n", cashBack);
  digitalWrite(GPIO_COIN_OUT, HIGH);
}

// static int isEnough(void){
//   int enough = 0;
//   if(cash > COFFEE_PRICE){
//     enough = 1;
//   }
//   return enough;
// }

// Utility functions, should be elsewhere

// res = a - b
void timeval_sub (struct timeval *res, struct timeval *a, struct timeval *b){
  res->tv_sec = a->tv_sec - b->tv_sec;
  res->tv_usec = a->tv_usec - b->tv_usec;
  if (res->tv_usec < 0) {
    --res->tv_sec;
    res->tv_usec += 1000000;
  }
}

// res = a + b
void timeval_add (struct timeval *res, struct timeval *a, struct timeval *b){
  res->tv_sec = a->tv_sec + b->tv_sec
    + a->tv_usec / 1000000 + b->tv_usec / 1000000; 
  res->tv_usec = a->tv_usec % 1000000 + b->tv_usec % 1000000;
}

// wait until next_activation (absolute time)
void delay_until (struct timeval* next_activation){
  struct timeval now, timeout;
  gettimeofday (&now, NULL);
  timeval_sub (&timeout, next_activation, &now);
  select (0, NULL, NULL, NULL, &timeout);
}

// Transitions
// Explicit FSM description
static fsm_trans_t cofm[] = {
  { COFM_WAITING, button_pressed, COFM_CUP,     cup    },
  { COFM_CUP,     timer_finished, COFM_COFFEE,  coffee },
  { COFM_COFFEE,  timer_finished, COFM_MILK,    milk   },
  { COFM_MILK,    timer_finished, COFM_WAITING, finish },
  {-1, NULL, -1, NULL },
};

static fsm_trans_t moneyfsm[] = {
  { MONEY_WAITING, coin_insert, MONEY_READY,   moneyReady   },
  { MONEY_READY,   coffeFinish, MONEY_WAITING, returnMoney  },
  { MONEY_READY,   coin_insert, MONEY_READY,   moneyReady   },
  {-1, NULL, -1, NULL },
};

int main (){

  struct timeval clk_period = { 0, 250 * 1000 };
  struct timeval next_activation;
  fsm_t* cofm_fsm = fsm_new (cofm);
  fsm_t* monfm_fsm = fsm_new (moneyfsm);

  wiringPiSetup();

  pinMode (GPIO_BUTTON, INPUT);
  pinMode (GPIO_COIN,INPUT);

  wiringPiISR (GPIO_BUTTON, INT_EDGE_FALLING, button_isr);
  wiringPiISR (GPIO_COIN, INT_EDGE_FALLING, coin_isr);

  pinMode (GPIO_CUP, OUTPUT);
  pinMode (GPIO_COFFEE, OUTPUT);
  pinMode (GPIO_MILK, OUTPUT);
  pinMode (GPIO_LED, OUTPUT);

  pinMode (GPIO_COIN_OUT, OUTPUT);


  digitalWrite (GPIO_LED, HIGH);
  digitalWrite (GPIO_COIN_OUT, LOW);
  
  gettimeofday (&next_activation, NULL);
  while (1){

    printf("Introduzca las entradas: (Boton, Moneda, Timer)\n");

    if(scanf("%d %d %d", &button, &coin, &timer)){

      fsm_fire (cofm_fsm);
      fsm_fire (monfm_fsm);
      timeval_add (&next_activation, &next_activation, &clk_period);
      delay_until (&next_activation);
    }
  }
}
