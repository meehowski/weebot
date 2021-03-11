//*****************************************************************************
//
// uart_echo.c - Example for reading data from and writing data to the UART in
//               an interrupt driven fashion.
//
//*****************************************************************************

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"

#include <stdlib.h>

#include "utils/uartstdio.h"
#include "shell_task.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "inttypes.h"
#include "stepper.h"
#include "platform.h"
#include "string.h"

#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <float.h>

//*****************************************************************************
//
// The stack size for the SHELL toggle task.
//
//*****************************************************************************
#define SHELLTASKSTACKSIZE        256         // Stack size in words

//*****************************************************************************
//
// The item size and queue size for the SHELL message queue.
//
//*****************************************************************************
#define SHELL_ITEM_SIZE           sizeof(unsigned char)
#define SHELL_QUEUE_SIZE          5

//*****************************************************************************
//
// Default SHELL cmd poll delay value. 
//
//*****************************************************************************
#define SHELL_POLL_DELAY        250

//*****************************************************************************
//
// The queue that holds messages sent to the SHELL task.
//
//*****************************************************************************
xQueueHandle g_pSHELLQueue;

extern xSemaphoreHandle g_pUARTSemaphore;


//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, unsigned long ulLine)
{
}
#endif

#define CMDBUFSIZE 256
static char g_cmd_buf[CMDBUFSIZE];
static int16_t g_cmd_ready = 0;

//*****************************************************************************
//
// The UART interrupt handler.
//
//*****************************************************************************
void
UARTIntHandler(void)
{
    unsigned long ulStatus;
    char print = 1;
    char chr = 0;

    //
    // Get the interrrupt status.
    //
    ulStatus = ROM_UARTIntStatus(UART0_BASE, true);

    //
    // Clear the asserted interrupts.
    //
    ROM_UARTIntClear(UART0_BASE, ulStatus);

    //
    // Loop while there are characters in the receive FIFO.
    //
    while(ROM_UARTCharsAvail(UART0_BASE))
    {
        //
        // Read the next character from the UART and write it back to the UART.
        //
        //ROM_UARTCharPutNonBlocking(UART0_BASE, ROM_UARTCharGetNonBlocking(UART0_BASE));
        if (g_cmd_ready < 0)
        {
          return;
        }

        chr = ROM_UARTCharGetNonBlocking(UART0_BASE);
        if (chr == '\r') // end-of-line
        {
          g_cmd_ready = -1;
          print = 0;
          ROM_UARTCharPutNonBlocking(UART0_BASE, '\r');
          ROM_UARTCharPutNonBlocking(UART0_BASE, '\n');
        }
        else if (chr >= ' ') // usable character
        {
          if (g_cmd_ready < CMDBUFSIZE -1)
          {
            g_cmd_buf[g_cmd_ready++] = chr;
            g_cmd_buf[g_cmd_ready] = 0;
          }
        }
        else if (chr == 8) // backspace
        {
          if (g_cmd_ready > 0)
          {
              g_cmd_ready--;
              g_cmd_buf[g_cmd_ready] = 0;
              // erase last char
              ROM_UARTCharPutNonBlocking(UART0_BASE, 8);
              ROM_UARTCharPutNonBlocking(UART0_BASE, ' ');
          }
          else
          {
              print = 0;
          }
        }
        else // crap, restart
        {
          g_cmd_buf[0] = 0;
          g_cmd_ready = -1;
          print = 0;
          ROM_UARTCharPutNonBlocking(UART0_BASE, '\r');
          ROM_UARTCharPutNonBlocking(UART0_BASE, '\n');
        }

        if (print == 1)
        {
            ROM_UARTCharPutNonBlocking(UART0_BASE, chr);
        }
     }
}

//*****************************************************************************
//
// Send a string to the UART.
//
//*****************************************************************************
void
UARTSend(const unsigned char *pucBuffer, unsigned long ulCount)
{
    //
    // Loop while there are more characters to send.
    //
    while(ulCount--)
    {
        //
        // Write the next character to the UART.
        //
        ROM_UARTCharPutNonBlocking(UART0_BASE, *pucBuffer++);
    }
}

static char
*next_str(char *str)
{
  while ((*str != '\0') && (*str != ' ') && (*str != ','))
  {
    str++;
  }

  if (*str == '\0')
  {
    return(NULL);
  }
  else
  {
    //UARTprintf("next_str=%s\n", str+1);
    return(str+1);
  }
}

float _strtof(const char *str, char **endptr) {
  float number;
  int exponent;
  int negative;
  unsigned char *p = (unsigned char *) str;
  float p10;
  int n;
  int num_digits;
  int num_decimals;

  // Skip leading whitespace
  while (isspace(*p)) p++;

  // Handle optional sign
  negative = 0;
  switch (*p) {
    case '-': negative = 1; // Fall through to increment position
    case '+': p++;
  }

  number = 0.;
  exponent = 0;
  num_digits = 0;
  num_decimals = 0;

  // Process string of digits
  while (isdigit(*p)) {
    number = number * 10. + (*p - '0');
    p++;
    num_digits++;
  }

  // Process decimal part
  if (*p == '.') {
    p++;

    while (isdigit(*p)) {
      number = number * 10. + (*p - '0');
      p++;
      num_digits++;
      num_decimals++;
    }

    exponent -= num_decimals;
  }

  if (num_digits == 0) {
    errno = ERANGE;
    return 0.0;
  }

  // Correct for sign
  if (negative) number = -number;

  // Process an exponent string
  if (*p == 'e' || *p == 'E') {
    // Handle optional sign
    negative = 0;
    switch (*++p) {
      case '-': negative = 1;   // Fall through to increment pos
      case '+': p++;
    }

    // Process string of digits
    n = 0;
    while (isdigit(*p)) {
      n = n * 10 + (*p - '0');
      p++;
    }

    if (negative) {
      exponent -= n;
    } else {
      exponent += n;
    }
  }

  if (exponent < FLT_MIN_EXP  || exponent > FLT_MAX_EXP) {
    errno = ERANGE;
    return HUGE_VAL;
  }

  // Scale the result
  p10 = 10.;
  n = exponent;
  if (n < 0) n = -n;
  while (n) {
    if (n & 1) {
      if (exponent < 0) {
        number /= p10;
      } else {
        number *= p10;
      }
    }
    n >>= 1;
    p10 *= p10;
  }

  if (number == HUGE_VAL) errno = ERANGE;
  if (endptr) *endptr = (char *)p;

  return number;
}

#define SHELL_CMD(CMD, USAGE, EXEC) \
  { \
    const char *c = CMD; \
    if (cmd == 0) \
    { \
      UARTprintf("  %s %s\n", c, USAGE); \
    } else if ((memcmp(cmd, c, sizeof(CMD) - 1) == 0) \
            && (cmd[sizeof(CMD) - 1] <= ' ') && (valid == 0)) \
    { \
      EXEC; valid = 1; \
    } \
  }

static int32_t i(int8_t idx)
{
  uint8_t n = 0;
  char *p = g_cmd_buf;

  while ((n <= idx) && (p != 0))
  {
    p = next_str(p);
    n++;
  }

  if (p == 0)
  {
    return(0);
  }

  return(strtol(p, 0, 10));
}

static float f(int8_t idx)
{
  uint8_t n = 0;
  char *p = g_cmd_buf;

  while ((n <= idx) && (p != 0))
  {
    p = next_str(p);
    n++;
  }

  if (p == 0)
  {
    return(0);
  }

  return(_strtof(p, 0));
}

static void
shell_cmd(char *cmd)
{
  uint8_t valid = 0;

  // command definitions and legend
  if (cmd == 0) // help cmd
  {
    UARTprintf(
        "Legend:\n"
        "  sid: stepper id\n"
        "  v: velocity (m/s), r: radius (m), w: angular velocity (rotation/sec)\n"
        "  a: acceleration (steps-per-sec^2 or meters-per-sec^2) d: distance (meters)\n"
        "  sps: steps-per-sec, st: number-of-steps\n"
        "\n"
        "Command List:\n");
  } 

  SHELL_CMD("help", "(list of commands)", shell_cmd(0));
  SHELL_CMD("h", "(list of commands)", shell_cmd(0));
  SHELL_CMD("?", "(list of commands)", shell_cmd(0));
  SHELL_CMD("cls", "(clear screen)", UARTprintf("\033[2J"));
  SHELL_CMD("reset", "(system reset)", SysCtlReset());
  SHELL_CMD("reboot", "(system reset)", SysCtlReset());

  SHELL_CMD("sg", "(stepper go) sid, sps, a, st", stepper_go(i(0), f(1), f(2), i(3)));
  SHELL_CMD("si", "(stepper idle) sid", stepper_idle(i(0)));
  SHELL_CMD("ss", "(stepper stop) sid, hard_stop_flag", stepper_stop(i(0), i(1)));
  SHELL_CMD("sst", "(stepper status) sid", stepper_status(i(0)));
  SHELL_CMD("ssc", "(stepper scan) sid, sps, a, st", stepper_scan(i(0), f(1), f(2), i(3)));

  SHELL_CMD("pg", "(platform go) v, a, w, d", platform_go(f(0), f(1), f(2), f(3)));
  SHELL_CMD("pi", "(platform idle)", platform_idle());
  SHELL_CMD("ps", "(platform stop) hard_stop_flag", platform_stop(i(0)));
  SHELL_CMD("pst", "(platform status)", platform_status());

  if ((valid == 0)
   && (cmd != 0)
   && (strlen(cmd) > 0))
  {
    UARTprintf("no such command '%s'\ntry 'help'\n", cmd);
  }
}

//*****************************************************************************
//
// This example demonstrates how to send a string of data to the UART.
//
//*****************************************************************************
static void
shellTask(void *pvParameters)
{
    portTickType ulWakeTime;

    //bzero(g_cmd_buf, CMDBUFSIZE);
    memset(g_cmd_buf, 0, CMDBUFSIZE);
    g_cmd_ready = 0;

    //
    // Enable the peripherals used by this example.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Enable processor interrupts.
    //
    //ROM_IntMasterEnable();

    //
    // Set GPIO A0 and A1 as UART pins.
    //
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Configure the UART for 115,200, 8-N-1 operation.
    //
    ROM_UARTConfigSetExpClk(UART0_BASE, ROM_SysCtlClockGet(), 115200,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));

    //
    // Enable the UART interrupt.
    //
    ROM_IntEnable(INT_UART0);
    ROM_UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);

    //
    // Prompt for text to be entered.
    //
    UARTprintf("shell# ");

    while(1)
    {
        if (g_cmd_ready == -1)
        {
          //UARTSend((unsigned char *)"CMD:", 4);
          //UARTSend((unsigned char *)g_cmd_buf, strlen(g_cmd_buf));
          shell_cmd(g_cmd_buf);
          g_cmd_ready = 0;
          UARTprintf("shell# ");
        }
        //
        // Wait for the required amount of time.
        //
        vTaskDelayUntil(&ulWakeTime, SHELL_POLL_DELAY / portTICK_RATE_MS);
    }
}

//*****************************************************************************
//
// Initializes the SHELL task.
//
//*****************************************************************************
unsigned long
shellTaskInit(void)
{
    //
    // Print the current loggling SHELL and frequency.
    //
    UARTprintf("Shell task init.\n");

    //
    // Create a queue for sending messages to the SHELL task.
    //
    g_pSHELLQueue = xQueueCreate(SHELL_QUEUE_SIZE, SHELL_ITEM_SIZE);

    //
    // Create the SHELL task.
    //
    if(xTaskCreate(shellTask, (signed portCHAR *)"SHELL", SHELLTASKSTACKSIZE, NULL,
                   tskIDLE_PRIORITY + PRIORITY_SHELL_TASK, NULL) != pdTRUE)
    {
        return(1);
    }

    //
    // Success.
    //
    return(0);
}

