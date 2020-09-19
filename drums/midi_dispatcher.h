#ifndef MIDI_DISPATCHER_H_
#define MIDI_DISPATCHER_H_

#include "midi.h"
using namespace midi;

enum GM
{
    GM_BD0 = 35,
    GM_BD1 = 36,
    GM_SD = 38,
    GM_CH = 42,
    GM_CP = 39,
    GM_OH = 46
};

const uint8_t DrumMapping[] = {
	GM_BD1, GM_SD, GM_CH, GM_BD0, GM_CP, GM_OH};

struct LowPriorityBufferSpecs
{
    enum
    {
        buffer_size = 128,
        data_size = 8,
    };
    typedef avrlib::DataTypeForSize<data_size>::Type Value;
};

struct HighPriorityBufferSpecs
{
    enum
    {
        buffer_size = 16,
        data_size = 8,
    };
    typedef avrlib::DataTypeForSize<data_size>::Type Value;
};

class MidiDispatcher : public midi::MidiDevice
{
public:
    typedef avrlib::RingBuffer<LowPriorityBufferSpecs> OutputBufferLowPriority;
    typedef avrlib::RingBuffer<HighPriorityBufferSpecs> OutputBufferHighPriority;

    static uint8_t readable_high_priority()
    {
        return OutputBufferHighPriority::readable();
    }

    static uint8_t readable_low_priority()
    {
        return OutputBufferLowPriority::readable();
    }

    static uint8_t ImmediateReadHighPriority()
    {
        return OutputBufferHighPriority::ImmediateRead();
    }

    static uint8_t ImmediateReadLowPriority()
    {
        return OutputBufferLowPriority::ImmediateRead();
    }

    static void Send3(uint8_t status, uint8_t a, uint8_t b)
    {
        OutputBufferLowPriority::Overwrite(status);
        OutputBufferLowPriority::Overwrite(a);
        OutputBufferLowPriority::Overwrite(b);
    }

    static inline void OnDrumNote(uint8_t note, uint8_t velocity)
    {
        if (note != 0xff)
        {
            Send3(0x90, note, velocity);
            Send3(0x80, note, 0);

            uint8_t data[2] = {note, velocity};
            RawMidiData(0x99, data, 2, 0);
        }
    }

    MidiDispatcher() {}

    // ------ MIDI in handling ---------------------------------------------------

    static inline void NoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
    {
    }
    static inline void NoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
    {
    }

    static inline void PitchBend(uint8_t channel, uint16_t pitch_bend)
    {
    }

    static inline void ControlChange(uint8_t channel, uint8_t controller, uint8_t value)
    {
    }

    static void ProgramChange(uint8_t channel, uint8_t program)
    {
    }

    static bool &Playing()
    {
        static bool playing = false;
        return playing;
    }

    static void Clock(bool internal = false)
    {
        if (internal || Playing())
        {
            grids.tick(ticks_granularity[2]);

            uint8_t state = grids.getAllStates();

            if (state & 0x1)
                OnDrumNote(GM_BD1, 127);

            if (state & 0x2)
                OnDrumNote(GM_SD, 127);

            if (state & 0x4)
                OnDrumNote(GM_CH, 127);

            if (state & 0x8)
                OnDrumNote(GM_BD0, 127);

            if (state & 0x16)
                OnDrumNote(GM_CP, 127);

            if (state & 0x32)
                OnDrumNote(GM_OH, 127);
        }
    }

    static void Start()
    {
        grids.reset();
        Playing() = true;
    }

    static void Continue()
    {
        Playing() = true;
    }

    static void Stop()
    {
        Playing() = false;
    }

    static void ActiveSensing() {}
    static void Reset() {}

    static uint8_t CheckChannel(uint8_t channel)
    {
        return 0;
    }

    static void RawMidiData(
        uint8_t status,
        uint8_t *data,
        uint8_t data_size,
        uint8_t accepted_channel)
    {
        //printf("RAW %x %x %x %x\n", status, data[0], data[1], accepted_channel);

        // Manually handles drum sound triggering from MIDI.
        if (status == 0x99)
        {
            uint8_t note = data[0];
            uint8_t velocity = data[1] << 1;
            if (velocity)
            {
                if (note == GM_BD1)
                {
                    drum_synth.Trigger(0, velocity);
                }
                else if (note == GM_SD)
                {
                    drum_synth.Trigger(1, velocity);
                }
                else if (note == GM_CH)
                {
                    drum_synth.Trigger(2, velocity);
                }
            }
        }

        if (status == 0xfa)
        {
            Start();
        }

        if (status == 0xf0)
        {
            Clock();
        }

        // Manually handles CCs for the drum section from MIDI.
        if (status == 0xb9)
        {
            uint8_t cc = data[0];
            uint8_t value = data[1];
            drum_synth.SetParameterCc(cc, value);
        }
    }
};

extern MidiDispatcher midi_dispatcher;

#endif