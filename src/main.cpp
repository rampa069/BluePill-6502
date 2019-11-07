/*
  Small 6502 emulator for the blue pill
  (c) 2019 rampa@encomix.org
  using:
    jeeH (from jeelabs)
    fake6502 (Mike Chambers)
    Some (ACIA code) from satoshinm
*/

#include <jee.h>
#include <string.h>

extern "C" {
#include "fake6502.h"
}


UartBufDev< PinA<9>, PinA<10> > console;

#include "rom.h"
#define ACIAControl 0
#define ACIAStatus 0
#define ACIAData 1

// "MC6850 Data Register (R/W) Data can be read when Status.Bit0=1, and written when Status.Bit1=1."
#define RDRF (1 << 0)
#define TDRE (1 << 1)


static char input[256];
static int input_index = 0;
static int input_processed_index = 0;

void process_serial_input_byte(char b) {
    input[input_index++] = b;
    input_index %= sizeof(input);
}


uint8_t read6850(uint16_t address) {
	switch(address & 1) {
		case ACIAStatus: {
            // Always writable
            uint8_t flags = TDRE;

            // Readable if there is pending user input data which wasn't read
            if (input_processed_index < input_index) flags |= RDRF;

            return flags;
			break;
        }
		case ACIAData: {
            char data = input[input_processed_index++];
            //char data=console.getc();
            input_processed_index %= sizeof(input);
            return data;
			break;
        }
		default:
            break;
	}

	return 0xff;
}

void write6850(uint16_t address, uint8_t value) {
  //printf("ACIA address: %x  value %x",address,value);
	switch(address & 1) {
		case ACIAControl:
            // TODO: decode baudrate, mode, break control, interrupt
			break;
		case ACIAData: {
            //static char buf[1];
            //buf[0] = value;
            //cdcacm_send_chunked_blocking(buf, sizeof(buf), usbd_dev);
            //printf("%c",value);
            console.putc(value);
			break;
        }
		default:
            break;
	}
}




int printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); veprintf(console.putc, fmt, ap); va_end(ap);
    return 0;
}


PinC<13> led;

uint8_t ram[0x4000];
uint8_t read6502(uint16_t address) {
        // RAM
        if (address < sizeof(ram)) {
                return ram[address];
        }

        // ROM
        if (address >= 0xc000) {
                //const uint8_t *rom = &_binary____osi_bas_ROM_o_bin_start;

                return rom[address - 0xc000];
        }

        // ACIA
        if (address >= 0xa000 && address <= 0xbfff) {
                return read6850(address);

                //return console.getc();
        }

        return 0xff;
}

void write6502(uint16_t address, uint8_t value) {
        // RAM
        if (address < sizeof(ram)) {
                ram[address] = value;
        }

        // ACIA
        if (address >= 0xa000 && address <= 0xbfff) {
                write6850(address, value);
        }

}

int main() {
    console.baud(115200, fullSpeedClock());
    led.mode(Pinmode::out);
    console.init();
#if BLUEPILL
    int usartBusHz = fullSpeedClock();
#else
    int usartBusHz = fullSpeedClock() / 2; // usart bus runs at 84 iso 168 MHz
#endif
    console.baud(115200, usartBusHz);
    led.mode(Pinmode::out);


    printf("6502 reset,,,...\n");
    reset6502();
    printf("6502 Starting...\n");
    uint32_t start = ticks;
    do {
        if (console.readable())
          process_serial_input_byte(console.getc());

        step6502();
        led.toggle();
        //printf("Ticks: %d\n",ticks);
    } while (1);
}
