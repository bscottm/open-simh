@echo off

rem ----------------------------------
rem CHANGE TO THE DMS SOURCE DIRECTORY
rem ----------------------------------

set SRCDIR=dmsr2v12

set DMS_RELEASE=2.12

set "IBM1130UTILS=%cd%"

cd %SRCDIR%

echo.
echo Assembling DMS source decks (two warnings expected)...

rem Note: in the source directory, we've split file PMONITOR.ASM into two files:
rem EMONITOR.ASM and PMONDEVS.ASM, as the resident monitor and device drivers are
rem loaded in different places in the load deck. Rather than split up the object file,
rem we split the source.

	rem ... SYMBOLS.SYS is the system symbol table. Name is all upper case even on Unix
if exist SYMBOLS.SYS del SYMBOLS.SYS

	rem ... compile all source files to 1130 relocatable binary format, and create listing files

for %%i in (*.asm) do %IBM1130UTILS%\asm1130 -d -r%DMS_RELEASE% -b -l %%i

rem ---------------------------------
rem CHANGE TO MAIN SOFTWARE DIRECTORY
rem ---------------------------------

cd ..

rem Here we produce card images and object decks that are not simply compiled versions of the
rem original DMS source. Several parts of the DMS load deck have to be constructed by massaging
rem the DMS object decks. DMS was maintained by IBM on a S/370 and there were presumably utilities and batch
rem processes to do this, but they are not part of the distribution, so we've had to reconstruct them
rem here. This is what we have to do to finish creating the system load deck:
rem
rem * Assemble the disk formatter ZDCIP into simh "load" format so that we can load and run it without DMS.
rem
rem * Assemble a copy of DBOOTCD (loader boot cards) that has been edited with the correct transfer 
rem   address etc. The copy of DBOOTCD.ASM in the distribution folder has placeholders for these values.
rem 
rem * Extract parts of the DBOOTCD object deck and format them into 1130 and 1800 IPL formats to create
rem   the card images required by the boot loader.
rem
rem * Create the DMS cold start card from the zcldstrt object deck
rem
rem * Sort the phases in the assembler (PTMASMBL) into numerical order. The source code generates
rem   the phases out of order, but the loader won't accept this. Presumably IBM collected ALL of the
rem   load deck's phases and sorted them as a whole, but it turns out that only PTMASMBL produces
rem   an out of order object deck, so we just leave the rest of the objects in separate decks.
rem

echo.
echo Assembling standalone utilities...

	rem ... compile stand-alone disk cartridge utility program ZDCIP to simulator load file format
%IBM1130UTILS%\asm1130 -d -r%DMS_RELEASE%    -l zdcip

	rem ... compile edited DBOOTCD (I added transfer address etc) to 1130 relocatable binary format
%IBM1130UTILS%\asm1130 -d -r%DMS_RELEASE% -b -l dbootcd

echo.
echo Creating loader boot card images (one warning expected)...

	rem ... create dbootcd1.ipl in 1130 IPL format
%IBM1130UTILS%\mkboot dbootcd.bin           dbootcd1.ipl 1130 /00 /47 LDBOTH01

	rem ... create dbootcd2.dat in 1800 IPL format
%IBM1130UTILS%\mkboot dbootcd.bin           dbootcd2.dat 1800 /4F /72 LDBOTH02

	rem ... create dciloadr.dat in 1800 IPL format
%IBM1130UTILS%\mkboot %SRCDIR%\dciloadr.bin dciloadr.dat 1800 /04 /9D DCLA0001

	rem ... create dsysldr1.dat in core image format
%IBM1130UTILS%\mkboot %SRCDIR%\dsysldr1.bin dsysldr1.dat CORE 0   0   DCLB0001

echo.
echo Creating DMS cold start card

	rem Simulator command "boot dsk" has same effect as "attach cr DMSColdStart.crd" followed by "boot cr"
%IBM1130UTILS%\mkboot %SRCDIR%\zcldstrt.bin DMSColdStart.crd 1130

echo.
echo Sorting assembler phases...
	rem ... assembler phases are out of order as assembled, must be physically reordered
%IBM1130UTILS%\bindump -s %SRCDIR%\ptmasmbl.bin >ptmasmbl_12.dat

echo.
echo Binary files are now ready to use with loaddms simulator script
echo (command line: ibm1130 loaddms)
