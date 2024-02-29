#!/bin/sh

SRCDIR=dmsr2v12
DMS_RELEASE=2.12
HERE="$(pwd)"
IBM1130UTILS="$(pwd)"

for i in "${HERE}" "${HERE}/${SRCDIR}"; do
  if [ -f ${i}/asm1130 -a -x ${i}/asm1130 ]; then
    IBM1130UTILS="${i}"
    break
  fi
done

if [ "x${IBM1130UTILS}" = x ]; then
  echo "Cannot find asm1130 (and other utilities.) Exiting."
fi

## On MSYS64, MING64 and family, slash-prefixed arguments need
## special handling (a lone "/" will insert the local directory's
## name, but "//" is elided to a single "/".)
SLASHARG="/"
if [ "x${MSYSTEM}" != x ]; then
  SLASHARG="//"
fi

(
  # -------------------------------------------------
  # Do all assembler work in the DMS source directory
  # -------------------------------------------------

  cd ${SRCDIR}
  echo
  echo "Assembling DMS source decks (two warnings expected)..."

  # Note: in the source directory, we've split file PMONITOR.ASM into two files:
  # EMONITOR.ASM and PMONDEVS.ASM, as the resident monitor and device drivers are
  # loaded in different places in the load deck. Rather than split up the object file,
  # we split the source.

  # ... SYMBOLS.SYS is the system symbol table. Name is all upper case even on Unix

  if [ -f SYMBOLS.SYS ]; then rm -f SYMBOLS.SYS; fi

  # ... compile all source files to 1130 relocatable binary format, and create listing files

  for i in *.asm; do ${IBM1130UTILS}/asm1130 -d -r${DMS_RELEASE} -b -l $i; done
)

# Here we produce card images and object decks that are not simply compiled versions of the
# original DMS source. Several parts of the DMS load deck have to be constructed by massaging
# the DMS object decks. DMS was maintained by IBM on a S/370 and there were presumably utilities and batch
# processes to do this, but they are not part of the distribution, so we've had to reconstruct them
# here. This is what we have to do to finish creating the system load deck:
#
# * Assemble the disk formatter ZDCIP into simh "load" format so that we can load and run it without DMS.
#
# * Assemble a copy of DBOOTCD (loader boot cards) that has been edited with the correct transfer 
#   address etc. The copy of DBOOTCD.ASM in the distribution folder has placeholders for these values.
# 
# * Extract parts of the DBOOTCD object deck and format them into 1130 and 1800 IPL formats to create
#   the card images required by the boot loader.
#
# * Create the DMS cold start card from the zcldstrt object deck
#
# * Sort the phases in the assembler (PTMASMBL) into numerical order. The source code generates
#   the phases out of order, but the loader won't accept this. Presumably IBM collected ALL of the
#   load deck's phases and sorted them as a whole, but it turns out that only PTMASMBL produces
#   an out of order object deck, so we just leave the rest of the objects in separate decks.
#

echo
echo "Assembling standalone utilities..."

# ... compile stand-alone disk cartridge utility program ZDCIP to simulator load file format
${IBM1130UTILS}/asm1130 -d -r${DMS_RELEASE} -l zdcip

# ... compile edited DBOOTCD (I added transfer address etc) to 1130 relocatable binary format
${IBM1130UTILS}/asm1130 -d -r${DMS_RELEASE} -b -l dbootcd

echo
echo "Creating loader boot card images (one warning expected)..."

set -x
# ... create dbootcd1.ipl in 1130 IPL format
${IBM1130UTILS}/mkboot dbootcd.bin           dbootcd1.ipl 1130 ${SLASHARG}00 ${SLASHARG}47 LDBOTH01

# ... create dbootcd2.dat in 1800 IPL format
${IBM1130UTILS}/mkboot dbootcd.bin           dbootcd2.dat 1800 ${SLASHARG}4F ${SLASHARG}72 LDBOTH02

# ... create dciloadr.dat in 1800 IPL format
${IBM1130UTILS}/mkboot ${SRCDIR}/dciloadr.bin dciloadr.dat 1800 ${SLASHARG}04 ${SLASHARG}9D DCLA0001

# ... create dsysldr1.dat in core image format
${IBM1130UTILS}/mkboot ${SRCDIR}/dsysldr1.bin dsysldr1.dat CORE 0   0   DCLB0001
set +x

echo
echo "Creating DMS cold start card"

# Simulator command "boot dsk" has same effect as "attach cr DMSColdStart.crd" followed by "boot cr"
${IBM1130UTILS}/mkboot ${SRCDIR}/zcldstrt.bin DMSColdStart.crd 1130

echo
echo "Sorting assembler phases..."
# ... assembler phases are out of order as assembled, must be physically reordered
${IBM1130UTILS}/bindump -s ${SRCDIR}/ptmasmbl.bin >ptmasmbl_12.dat

echo
echo "Binary files are now ready to use with loaddms simulator script"
echo "(command line: ibm1130 loaddms)"
