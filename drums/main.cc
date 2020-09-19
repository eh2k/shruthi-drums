// Copyright 2016 E.Heidt
//
// Author: E.Heidt
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#define F_CPU 20000000UL //#define F_CPU 16000000UL

#include <stdio.h>
#include <util/delay.h>

#include "avrlib/gpio.h"
#include "avrlib/spi.h"
#include "avrlib/devices/external_eeprom.h"
#include "avrlib/devices/shift_register.h"
#include "avrlib/debug_output.h"
#include "avrlib/boot.h"
#include "avrlib/random.h"
#include "avrlib/parallel_io.h"
#include "avrlib/devices/buffered_display.h"
#include "avrlib/devices/hd44780_lcd.h"
#include "avrlib/devices/rotary_encoder.h"

#include "drum_synth.h"
#include "TopographPatternGenerator.h"

PatternGenerator grids;

#include "midi_dispatcher.h"

using namespace avrlib;
using avrlib::BufferedDisplay;
using avrlib::Hd44780Lcd;

const uint16_t kBankSize = 8192;
const uint16_t kMaxNumBanks = 2; // Tested for 128kb eeprom
ExternalEeprom<kMaxNumBanks * kBankSize> external_eeprom;

// PWM/audio output.
const uint8_t kPinVcoOut = 12;
const uint8_t kPinVcaOut = 13;
const uint8_t kPinVcfCutoffOut = 14;
const uint8_t kPinVcfResonanceOut = 15;
const uint8_t kPinCv1Out = 3;
const uint8_t kPinCv2Out = 4;

typedef Gpio<PortC, 2> LcdRsLine;
typedef Gpio<PortC, 3> LcdEnableLine;
typedef ParallelPort<PortC, PARALLEL_NIBBLE_HIGH> LcdDataBus;

typedef Hd44780Lcd<LcdRsLine, LcdEnableLine, LcdDataBus> Lcd;
Lcd lcd;
BufferedDisplay<Lcd> display;

const prog_uint8_t sequencer_characters[] PROGMEM =
    {
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,

        0b00110,
        0b00110,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,

        0b00000,
        0b00000,
        0b00000,
        0b00110,
        0b00110,
        0b00000,
        0b00000,
        0b00000,

        0b00110,
        0b00110,
        0b00000,
        0b00110,
        0b00110,
        0b00000,
        0b00000,
        0b00000,

        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00110,
        0b00110,

        0b00110,
        0b00110,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00110,
        0b00110,

        0b00000,
        0b00000,
        0b00000,
        0b00110,
        0b00110,
        0b00000,
        0b00110,
        0b00110,

        0b00110,
        0b00110,
        0b00000,
        0b00110,
        0b00110,
        0b00000,
        0b00110,
        0b00110,
};

const prog_uint8_t *const character_table[] =
    {
        sequencer_characters,
};

char *linebuffer = display.line_buffer(0);

PwmOutput<kPinVcoOut> audio_out;
PwmOutput<kPinVcfResonanceOut> fr_out;
PwmOutput<kPinVcfCutoffOut> fc_out;
PwmOutput<kPinVcaOut> vca_out;

static Adc adc_;

ShiftRegisterInput<NumberedGpio<10>, NumberedGpio<7>, NumberedGpio<6>, 8, LSB_FIRST> switches;
ShiftRegisterOutput<NumberedGpio<10>, NumberedGpio<7>, NumberedGpio<5>, 8, MSB_FIRST> status_leds;

typedef Gpio<PortB, 1> EncoderALine;
typedef Gpio<PortB, 0> EncoderBLine;
typedef Gpio<PortB, 2> EncoderClickLine;

typedef RotaryEncoder<EncoderALine, EncoderBLine, EncoderClickLine> Encoder;
Encoder encoder;

uint8_t poti[8];
uint8_t last_poti[8];

int8_t enc_inc = 0;
uint8_t enc_click = 0;
uint8_t button_pressed = 0;
uint8_t page = 0;

//DebugOutput<Serial<SerialPort0, 9600, DISABLED, POLLED>> dbg;

typedef SerialPort0 MidiPort;
typedef avrlib::Serial<
    MidiPort,
    31250,
    avrlib::POLLED,
    avrlib::POLLED>
    MidiIO;

MidiIO midi_io;
typedef SerialInput<MidiPort> Midi;
RingBuffer<Midi> midi_buffer;
MidiStreamParser<MidiDispatcher> midi_parser;

inline void ProcessMidi()
{
    while (midi_buffer.readable())
    {
        midi_parser.PushByte(midi_buffer.ImmediateRead());
    }
}

inline void PollMidiIn()
{
    static uint8_t clock_divider_midi = 0;

    --clock_divider_midi;
    if (!clock_divider_midi)
    {
        if (midi_io.readable())
        {
            midi_buffer.NonBlockingWrite(midi_io.ImmediateRead());
        }

        clock_divider_midi = 4;
    }
}

inline void FlushMidiOut()
{
    // Try to flush the high priority buffer first.
    if (midi_dispatcher.readable_high_priority())
    {
        if (midi_io.writable())
        {
            midi_io.Overwrite(midi_dispatcher.ImmediateReadHighPriority());
        }
    }
    else
    {
        if (midi_dispatcher.readable_low_priority())
        {
            if (midi_io.writable())
            {
                midi_io.Overwrite(midi_dispatcher.ImmediateReadLowPriority());
            }
        }
    }
}

static bool playing = false;
static bool internalClock = true;

inline void TickInternalClock()
{
    static uint8_t cycle = 0;
    if (playing && internalClock)
    {
        if (cycle == 0)
            MidiDispatcher::Clock(true);

        if (++cycle >= 32)
            cycle = 0;
    }
    else
    {
        cycle = 0;
    }
}

inline void FillAudioBuffer()
{
    if (drum_synth.AudioBuffer.readable())
        audio_out.Write(drum_synth.AudioBuffer.ImmediateRead());
    else
        audio_out.Write(0);

    fc_out.Write(drum_synth.CFBuffer.ImmediateRead());
}

// 39kHz clock used for the tempo counter.
ISR(TIMER2_OVF_vect, ISR_NOBLOCK)
{
    FillAudioBuffer();

    PollMidiIn();
    FlushMidiOut();

    static uint8_t cycle = 0;
    if (++cycle % 32 == 0)
    {
        Timer<1>::Start();

        lcd.Tick();

        TickInternalClock();
    }
}

ISR(TIMER1_OVF_vect, ISR_NOBLOCK)
{
    Timer<1>::Stop();

    //TickInternalClock();
    TickSystemClock();

    static uint8_t pots_multiplexer_address_ = 0;
    last_poti[pots_multiplexer_address_] = poti[pots_multiplexer_address_];
    poti[pots_multiplexer_address_] = adc_.ReadOut() >> 2;
    pots_multiplexer_address_ = (pots_multiplexer_address_ + 1) & 7;
    adc_.StartConversion(pots_multiplexer_address_);

    enc_inc += encoder.ReadEncoder();

    static uint8_t last_v = 1;
    uint8_t v = encoder.immediate_value();
    if (v == 0 && last_v != v)
        enc_click++;

    last_v = v;
}

inline void potiChanged(bool *poti_changed)
{
    poti_changed[0] = (poti[0] >> 2) != (last_poti[0] >> 2);
    poti_changed[1] = (poti[1] >> 2) != (last_poti[1] >> 2);
    poti_changed[2] = (poti[2] >> 2) != (last_poti[2] >> 2);
    poti_changed[3] = (poti[3] >> 2) != (last_poti[3] >> 2);
}

inline void itoa(uint8_t i, char *destination)
{
    if (i == 0)
    {
        *destination++ = ' ';
        *destination++ = ' ';
        *destination++ = '0';
    }

    static unsigned char digits[3];

    uint8_t digit = 0;
    while (i > 0)
    {
        digits[digit++] = i % 10;
        i /= 10;
    }

    for (int i = 3; i > digit; --i)
        *destination++ = ' ';

    while (digit)
        *destination++ = 48 + digits[--digit];
}

uint8_t cutoff = 255;

bool poti_changed[4];
uint8_t patches[kNumDrumInstruments] = {0, 8, 14};

static uint8_t currentCharMap = -1;
inline void setCharacters(uint8_t index)
{
    static uint8_t c = 0;
    if (currentCharMap != index)
    {
        currentCharMap = index;
        lcd.SetCustomCharMapRes(character_table[currentCharMap], 8, 0);
    }
}

ISR(TIMER0_OVF_vect, ISR_BLOCK)
{
}

static void ReadSequence(char *sequenceString)
{
    for (int i = 0; i < 32; ++i)
    {
        sequenceString[i] = 0; //grids.getPattern()[i] & 0x7;

        uint8_t X = grids.getX();
        uint8_t Y = grids.getY();

        if (grids.readDrumMatrix(i, 0, X, Y))
            sequenceString[i] |= 1;

        if (grids.readDrumMatrix(i, 1, X, Y))
            sequenceString[i] |= 2;

        if (grids.readDrumMatrix(i, 2, X, Y))
            sequenceString[i] |= 4;

        // if (sequenceString[i] == 0)
        //     sequenceString[i] = ' ';
    }
}

int main(void)
{
    sei();

    //dbg.Init();
    status_leds.Init();
    switches.Init();
    midi_io.Init();
    drum_synth.Init();
    audio_out.Init();

    adc_.Init();
    adc_.set_reference(ADC_DEFAULT);
    adc_.set_alignment(ADC_RIGHT_ALIGNED);

    fr_out.Init();
    fc_out.Init();
    vca_out.Init();

    fr_out.Write(0);
    fc_out.Write(255);
    vca_out.Write(255);

    lcd.Init();
    setCharacters(0);
    display.Init();
    encoder.Init();
    page = 1;

    for (int i = 0; i < kNumDrumInstruments; ++i)
        drum_synth.LoadPatch(i, patches[i]);

    Timer<0>::set_prescaler(1);
    Timer<0>::set_mode(TIMER_PWM_PHASE_CORRECT);
    //Timer<0>::Start();

    Timer<1>::set_prescaler(1);
    Timer<1>::set_mode(TIMER_PWM_PHASE_CORRECT);

    Timer<2>::set_prescaler(1);
    Timer<2>::set_mode(TIMER_PWM_PHASE_CORRECT);
    Timer<2>::Start();

    sprintf(linebuffer, "Shruthi-DRUM 0.1________________");
    for (int i = 0; i < 32; i++)
    {
        display.Tick();
        Delay(42);
    }

    grids.setPatternMode(PATTERN_ORIGINAL);
    grids.setAccentAltMode(false);

    grids.setBDDensity(127);
    grids.setSDDensity(127);
    grids.setHHDensity(127);

    uint32_t ticks = 0;
    const int n = 1000;
    int8_t last_page = -1;
    int8_t last_step = -1;
    char last_char = 0;

    //AA__BB__CC__DD__
    //  2   6   A   E
    while (1)
    {
        ProcessMidi();
        drum_synth.Render(cutoff);
        display.Tick();
        potiChanged(poti_changed);

        if (poti_changed[0] || poti_changed[1] || poti_changed[2] || poti_changed[3])
        {
            ticks = 0;
            last_step = -1;
        }
        else if (++ticks > n)
            ticks = n + 1;

        if (ticks > n)
        {
            last_page = -1;

            if (last_step == -1)
            {
                ReadSequence(linebuffer);
                //sprintf(&linebuffer[16], "                ");
            }
            else
            {
                linebuffer[last_step] = (grids.getPattern()[last_step] & 0x7);
            }

            last_step = grids.getStep();
            last_char = linebuffer[last_step];
            linebuffer[last_step] = 0xff;  
        }
        else
        {
            if (last_page != page)
            {
                last_page = page;
                display.Clear();

                if (page == 1)
                {
                    sprintf(&linebuffer[00], "DB  SD  HH  RND ");
                    sprintf(&linebuffer[16], "                ");
                }
                else if (page == 2)
                {
                    sprintf(&linebuffer[00], "X   Y   LEN RND ");
                    sprintf(&linebuffer[16], "                ");
                }
                else if (page == 3)
                {
                    sprintf(&linebuffer[00], "BaseDrum        ");
                    sprintf(&linebuffer[16], "                ");
                }
                else if (page == 4)
                {
                    sprintf(&linebuffer[00], "SnareDrum        ");
                    sprintf(&linebuffer[16], "                ");
                }
                else if (page == 5)
                {
                    sprintf(&linebuffer[00], "HiHat          ");
                    sprintf(&linebuffer[16], "                ");
                }
                else if (page == 6)
                {
                    sprintf(&linebuffer[00], "FILTER          ");
                    sprintf(&linebuffer[16], "                ");
                }
            }

            itoa(poti[0], linebuffer + 16);
            itoa(poti[1], linebuffer + 20);
            itoa(poti[2], linebuffer + 24);
            itoa(poti[3], linebuffer + 28);
        }

        if (enc_click)
        {
            if (!playing)
            {
                playing = true;
                internalClock = true;
            }
            else
                playing = false;

            --enc_click;

            //display.Clear();
            grids.reset();
            ticks = n + 1;
        }

        static uint8_t last_input = 0;
        uint8_t input = ~switches.Read();

        if ((input & (32 | 16 | 8 | 4))) //Button Pressed
        {
            if (button_pressed < 255)
                button_pressed++;

            ticks = 0;
        }
        else
            button_pressed = 0;

        if (input & 1)
            page = 6;

        if (input & 2)
            page = 5;

        if (input & 4)
            page = 4;

        if (input & 8)
            page = 3;

        if (input & 16)
            page = 2;

        if (input & 32)
            page = 1;

        last_input = input;
        status_leds.Write(1 << (page - 1));

        if (page == 1)
        {
            if (poti_changed[0])
            {
                grids.setBDDensity(poti[0]);
            }

            if (poti_changed[1])
            {
                grids.setSDDensity(poti[1]);
            }

            if (poti_changed[2])
            {
                grids.setHHDensity(poti[2]);
            }

            if (poti_changed[3])
            {
                grids.setRandomness(poti[3]);
            }
        }
        else if (page == 2)
        {
            if (poti_changed[0])
            {
                grids.setMapX(poti[0]);
                grids.setEuclideanLength(0, poti[0]);
            }

            if (poti_changed[1])
            {
                grids.setMapY(poti[1]);
                grids.setEuclideanLength(1, poti[1]);
            }

            //if (poti_changed[2])
            {
                if (grids.getStep() * 4 > poti[2])
                    grids.reset();
            }

            if (poti_changed[3])
            {
                grids.setRandomness(poti[3]);
                grids.setEuclideanLength(2, poti[3]);
            }
        }
        else if (page == 6)
        {

            if (poti_changed[0])
                cutoff = poti[0];

            if (poti_changed[1])
                fr_out.Write(poti[1]);
        }
        else if (page - 3 <= kNumDrumInstruments)
        {
            if (last_input != input)
            {
                midi_dispatcher.OnDrumNote(DrumMapping[page - 3], 127);
            }

            uint8_t synth = drum_synth.Patch(page - 3).synth;
            DrumPatch &patch = drum_synth.Patch(page - 3);

            if (button_pressed > 128) //Long Pressed
            {
                drum_synth.LoadPatch(page - 1, patches[page - 1]);
            }
            else
            {
                if (poti_changed[0])
                    patch.pitch = poti[0];

                if (poti_changed[1])
                {
                    patch.pitch_decay = poti[1];
                    patch.pitch_mod = 255 - poti[1];
                }

                if (poti_changed[2])
                    patch.crunchiness = poti[2];

                if (poti_changed[3])
                    patch.amp_decay = poti[3];
            }
        }
    }
}

#if 0

int read_eeprom()
{
	external_eeprom.Init();

	uint16_t address = 0;

	uint8_t data[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
	external_eeprom.Write(address, data, 8);

	while (1)
	{
		Delay(100);

		const int len = 16;
		uint8_t data[len];
		uint8_t read = external_eeprom.Read(address, len, data);

		uint8_t a = address >> 8;
		D2::set_value(a & 0b00000001);
		D3::set_value(a & 0b00000010);
		D4::set_value(a & 0b00000100);
		D5::set_value(a & 0b00001000);
		D6::set_value(a & 0b00010000);
		D7::set_value(a & 0b00100000);
		D8::set_value(a & 0b01000000);
		D9::set_value(a & 0b10000000);

		//D9.Write(pwm);
		//pwm += 32;

		//printf("%04x : ", address);

		//for(int i = 0; i < read; i++)
		//	printf("%02x ", data[i]);

		//printf("\n");

		address += read;
	}
}

#endif