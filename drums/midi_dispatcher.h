#ifndef MIDI_DISPATCHER_H_
#define MIDI_DISPATCHER_H_

#include "midi.h"
using namespace midi;

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

	static void Clock()
	{
		Sequencer::Clock();
	}

	static void Start()
	{
		sequencer.Init();
		Sequencer::Start();
	}

	static void Continue()
	{
		Sequencer::Continue();
	}

	static void Stop()
	{
		Sequencer::Stop();
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
				if (note == 36)
				{
					drum_synth.Trigger(0, velocity);
				}
				else if (note == 38)
				{
					drum_synth.Trigger(1, velocity);
				}
				else if (note == 42)
				{
					drum_synth.Trigger(2, velocity);
				}
				else if (note == 44)
				{
					drum_synth.Trigger(3, velocity);
				}
			}
		}

		if (status == 0xfa)
		{
			sequencer.Start();
			sequencer.InternalClock() = false;
		}

		if (status == 0xf0)
		{
			sequencer.Clock();
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