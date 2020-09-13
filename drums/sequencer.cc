
#include "sequencer.h"
#include "drum_synth.h"
#include <avr/pgmspace.h>

#include "avrlib/random.h"
#include "avrlib/op.h"
#include "midi.h"
#include "midi_dispatcher.h"
#include "machinedrum.h"

using namespace avrlib;

uint8_t Sequencer::Step = 0;
uint8_t Sequencer::SubStep = 0;
uint8_t Sequencer::Threshold = 128;

uint8_t Sequencer_perturbation_[3];

const prog_uint8_t wav_res_drum_map_node_0[] PROGMEM =
{
	236,      0,      0,    138,      0,      0,    208,      0,
	58,     28,    174,      0,    104,      0,     58,      0,
	10,     66,      0,      8,    232,      0,      0,     38,
	0,    148,      0,     14,    198,      0,    114,      0,
	154,     98,    244,     34,    160,    108,    192,     24,
	160,     98,    228,     20,    160,     92,    194,     44,
};
const prog_uint8_t wav_res_drum_map_node_1[] PROGMEM =
{
	246,     10,     88,     14,    214,     10,     62,      8,
	250,      8,     40,     14,    198,     14,    160,    120,
	16,    186,     44,     52,    230,     12,    116,     18,
	22,    154,     10,     18,    246,     88,     72,     58,
	136,    130,    220,     64,    130,    120,    156,     32,
	128,    112,    220,     32,    126,    106,    184,     88,
};
const prog_uint8_t wav_res_drum_map_node_2[] PROGMEM =
{
	224,      0,     98,      0,      0,     68,      0,    198,
	0,    136,    174,      0,     46,     28,    116,     12,
	0,     94,      0,      0,    224,    160,     20,     34,
	0,     52,      0,      0,    194,      0,     16,    118,
	228,    104,    138,     90,    122,    102,    108,     76,
	196,    160,    182,    160,     96,     36,    202,     22,
};
const prog_uint8_t wav_res_drum_map_node_3[] PROGMEM =
{
	240,    204,     42,      0,     86,    108,     66,    104,
	190,     22,    224,      0,     14,    148,      0,     36,
	0,      0,    112,     62,    232,    180,      0,     34,
	0,     48,     26,     18,    214,     18,    138,     38,
	232,    186,    224,    182,    108,     60,     80,     62,
	142,     42,     24,     34,    136,     14,    170,     26,
};
const prog_uint8_t wav_res_drum_map_node_4[] PROGMEM =
{
	228,     14,     36,     24,     74,     54,    122,     26,
	186,     14,     96,     34,     18,     30,     48,     12,
	2,      0,     46,     38,    226,      0,     68,      0,
	2,      0,     92,     30,    232,    166,    116,     22,
	64,     12,    236,    128,    160,     30,    202,     74,
	68,     28,    228,    120,    160,     28,    188,     82,
};
const prog_uint8_t wav_res_drum_map_node_5[] PROGMEM =
{
	236,     24,     14,     54,      0,      0,    106,      0,
	202,    220,      0,    178,      0,    160,    140,      8,
	134,     82,    114,    160,    224,      0,     22,     44,
	66,     40,      0,      0,    192,     22,     14,    158,
	174,     86,    230,     58,    124,     64,    210,     58,
	160,     76,    224,     22,    124,     34,    194,     26,
};
const prog_uint8_t wav_res_drum_map_node_6[] PROGMEM =
{
	236,      0,    226,      0,      0,      0,    160,      0,
	0,      0,    188,      0,      0,      0,    210,      0,
	26,    188,      0,     62,    242,    102,      8,    160,
	22,    216,      0,     48,    200,    112,     30,     22,
	230,    212,    222,    228,    180,     14,    114,     32,
	160,     38,     66,     12,    154,     22,     88,     36,
};
const prog_uint8_t wav_res_drum_map_node_7[] PROGMEM =
{
	226,      0,     42,      0,     66,      0,    226,     14,
	238,      0,    126,      0,     84,     10,    170,     22,
	0,      0,     54,      0,    182,      0,    128,     36,
	6,     10,     84,     10,    238,      8,    158,     26,
	240,     46,    218,     24,    232,      0,     96,      0,
	240,     28,    204,     30,    214,      0,     64,      0,
};
const prog_uint8_t wav_res_drum_map_node_8[] PROGMEM =
{
	228,      0,    212,      0,     14,      0,    214,      0,
	160,     52,    218,      0,      0,      0,    134,     32,
	104,      0,     22,     84,    230,     22,      0,     58,
	6,      0,    138,     20,    220,     18,    176,     34,
	230,     26,     52,     24,     82,     28,     52,    118,
	154,     26,     52,     24,    202,    212,    186,    196,
};

static const prog_uint8_t *drum_map[3][3] =
	{
		{wav_res_drum_map_node_8, wav_res_drum_map_node_3, wav_res_drum_map_node_6},
		{wav_res_drum_map_node_2, wav_res_drum_map_node_4, wav_res_drum_map_node_0},
		{wav_res_drum_map_node_7, wav_res_drum_map_node_1, wav_res_drum_map_node_5},
};

/* static */
uint8_t ReadDrumMap(uint8_t step, uint8_t instrument, uint8_t x, uint8_t y)
{
	uint8_t i = x >> 7;
	uint8_t j = y >> 7;
	const prog_uint8_t *a_map = drum_map[i][j];
	const prog_uint8_t *b_map = drum_map[i + 1][j];
	const prog_uint8_t *c_map = drum_map[i][j + 1];
	const prog_uint8_t *d_map = drum_map[i + 1][j + 1];
	uint8_t offset = U8ShiftLeft4(instrument) + step;
	uint8_t a = pgm_read_byte(a_map + offset);
	uint8_t b = pgm_read_byte(b_map + offset);
	uint8_t c = pgm_read_byte(c_map + offset);
	uint8_t d = pgm_read_byte(d_map + offset);
	return U8Mix(U8Mix(a, b, x << 1), U8Mix(c, d, x << 1), y << 1);
}

uint8_t Sequencer::X = 1;
uint8_t Sequencer::Y = 1;

void Sequencer::Init()
{
	Step = 0;
}

void Sequencer::ReadSequence(char *sequenceString)
{
	for (int i = 0; i < 16; ++i)
	{
		sequenceString[i] = 0;

		if (ReadDrumMap(i, 0, X, Y) > Threshold)
			sequenceString[i] |= 1;

		if (ReadDrumMap(i, 1, X, Y) > Threshold)
			sequenceString[i] |= 2;

		if (ReadDrumMap(i, 2, X, Y) > Threshold)
			sequenceString[i] |= 4;

		if (sequenceString[i] == 0)
			sequenceString[i] = ' ';
	}
}

/* static */
uint8_t Sequencer::Clock()
{
	if (playing == false)
		return 0;

	uint8_t trig = 0;

	if ((SubStep % 6) == 0)
	{
		uint8_t x = X;
		uint8_t y = Y;
		for (uint8_t i = 0; i < kNumDrumInstruments; ++i)
		{
			uint8_t threshold = Threshold; //~seq_settings_.drums_density[i];

			uint8_t level = ReadDrumMap(Step, i, x, y);

			if (level < 255 - Sequencer_perturbation_[i])
			{
				level += Sequencer_perturbation_[i];
			}

			if (level > threshold)
			{
				//uint8_t level = 128 + (level >> 1);
				drum_synth.Trigger(i, level);
				trig |= 1 << i;

				midi_dispatcher.OnDrumNote(machinedrumMapping[12 + i], level >> 1);
			}
		}

		++Step;
		if (Step >= 16)
		{
			Step = 0;
			for (uint8_t i = 0; i < kNumDrumInstruments; ++i)
			{
			   Sequencer_perturbation_[i] = Random::GetByte() >> 3;
			}
		}
	}

	++SubStep;

	if (SubStep >= 16 * 6)
		SubStep = 0;

	return trig;
}

bool Sequencer::playing = false;