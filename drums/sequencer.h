
#ifndef ADRUM_SEQUENCER_H_
#define ADRUM_SEQUENCER_H_

#include "avrlib/base.h"

class Sequencer
{
public:
	Sequencer() {}
	~Sequencer() {}
	static void Init();
	static uint8_t Clock();

	static uint8_t X;
	static uint8_t Y;

	static void ReadSequence(char *sequenceString);

	static uint8_t Step;
	static uint8_t Threshold;

	static void Start()
	{
		SubStep = 0;
		Step = 0;
		playing = true;
	}

	static void Continue()
	{
		playing = true;
	}

	static void Stop()
	{
		playing = false;
	}

	static bool IsPaying()
	{
		return playing;
	}

	static bool &InternalClock()
	{
		static bool internal_clock = true;
		return internal_clock;
	}

private:
	static uint8_t SubStep;
	static bool playing;
	DISALLOW_COPY_AND_ASSIGN(Sequencer);
};

extern Sequencer sequencer;

#endif