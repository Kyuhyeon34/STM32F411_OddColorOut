#include "device_driver.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>

char * _sbrk(int inc)
{
	extern unsigned char __ZI_LIMIT__;
	static char * heap = (char *)0;

	char * prevHeap;
	char * nextHeap;

	if(heap == (char *)0) heap = (char *)HEAP_BASE;

	prevHeap = heap;
	nextHeap = (char *)((((unsigned int)heap + inc) + 0x7) & ~0x7);

	if((unsigned int)nextHeap >= HEAP_LIMIT) return (char *)0;

	heap = nextHeap;
	return prevHeap;
}

static void _Uart2_Send_Byte(char data)
{
  if(data == '\n')
  {
    while(!Macro_Check_Bit_Set(USART2->SR, 7));
    USART2->DR = 0x0d;
  }

  while(!Macro_Check_Bit_Set(USART2->SR, 7));
  USART2->DR = data;
}

int _write(int file, char *ptr, int len) 
{
    for (int i = 0; i < len; i++) 
	{
        _Uart2_Send_Byte(*ptr++);
    }
    
	return len;
}

static int _Uart2_Get_Char(void)
{
  while(!Macro_Check_Bit_Set(USART2->SR, 5));
  return USART2->DR;
}

int _read(int file, char *ptr, int len) 
{ 
    int count = 0;

    while(count < len) 
    {
        char ch = _Uart2_Get_Char();
        
		if(ch == '\r' || ch == '\n') 
        {
            _Uart2_Send_Byte('\r');
            _Uart2_Send_Byte('\n');
            *ptr++ = ch; 
            count++;
            break;
        } 
        else 
        {
            _Uart2_Send_Byte(ch);
            *ptr++ = ch;
            count++;
        }
    }
    
    return count;
}

int _lseek(int file, int ptr, int dir)  
{ 
	return 0; 
}

int _close(int file) 
{ 
	return -1; 
}

int _fstat(int file, struct stat *st) 
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) 
{ 
	return 1; 
}

int _getpid(void) 
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    
    errno = EINVAL;
    return -1;
}