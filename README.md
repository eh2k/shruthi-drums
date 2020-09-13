# shruthiDRUMS 0.1

shruthiDrums is a Shruthi-1 Firmware, that bases on the Anushri drum synthesizer/sequencer.

## Download
 * build/firmware/firmware.syx

## HowTo jam:

 * Encoder Press : Start/Stop Sequencer
 * Button 1: BD (Basedrum Page)
   * P1 - P4: Pitch, Pitch-Decay/Mod, Crunch, Amp-Decay
 * Button 2: SD (Snare Page)
   * P1 - P4: Pitch, Pitch-Decay/Mod, Crunch, Amp-Decay
 * Button 3: HH1 (HiHat 1 Page)
   * P1 - P4: Pitch, Pitch-Decay/Mod, Crunch, Amp-Decay
 * Button 4: HH2 (HiHat 2 Page)
   * P1 - P4: Pitch, Pitch-Decay/Mod, Crunch, Amp-Decay
 * Button 5: Filter
   * P1 - P2: Cutoff, Resonance
 * Button 6: Sequencer
   * P1: X-Pattern
   * P2: Y-Pattern
   * P3: Pattern Threshold
   * P4: Morph Patches
 * Hold Button 1-4 to Reset

## HowTo compile:

* Get the shruthiDRUMS repository
* Copy all subfolders form https://github.com/pichenettes/shruthi-1 to the shruthiDrums directory
* Execute shruthiDRUMS/shruthi_drums/make.cmd


## Upload Firmware
```sh
amidi -p hw:2,0,0 -s ./build/firmware/firmware.syx -i 200
```