cd..
set PATH=D:\dev\shruthi\WinAVR-20100110\utils\bin;%PATH%
set PATH=D:\dev\shruthi\WinAVR-20100110\bin;%PATH%
set PATH=D:\dev\shruthi\Python-2.7.6;%PATH%


@rem make -f test/makefile clean
@make -f shruthi_drums/makefile syx

@rem @md5sum build\test\test.hex
@rem avr-objcopy -O binary -R .eeprom build\shruthi_drums\shruthi_drums.elf build\shruthi_drums\shruthi_drums.bin
@rem @du -b build\shruthi_drums\shruthi_drums.bin

@rem avr-objdump -h -S build\shruthi_drums\shruthi_drums.elf >build2\out.txt
@rem avr-objdump -D -mavr6 build\shruthi_drums\shruthi_drums.elf >build2\out2.txt

@rem D:\dev\shruthi\arduino-1.0.5-r2\hardware\tools\avr\bin\avrdude -CD:\dev\shruthi\arduino-1.0.5-r2\hardware\tools\avr\etc\avrdude.conf -q -q -patmega328p -carduino -P\\.\COM3 -b57600 -D -U flash:w:build\test\test.hex

pause