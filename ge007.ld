#define BEGIN_SEG(name, rompos, rampos) \
	_##name##SegmentStart = ADDR(.name); \
	_##name##SegmentRomStart = rompos; \
	.name rampos : AT(rompos)

#define END_SEG(name) \
	_##name##SegmentEnd = ADDR(.name) + SIZEOF(.name); \
	_##name##SegmentRomEnd = _##name##SegmentRomStart + SIZEOF(.name);

/*OUTPUT_FORMAT ("elf32-bigmips")*/
OUTPUT_ARCH (mips)

SECTIONS
{
   
	osTvType = 0x80000300;
	osRomType = 0x80000304;
	osRomBase = 0x80000308;
	osResetType = 0x8000030c;
	osCicId = 0x80000310;
	osVersion = 0x80000314;
	osMemSize = 0x80000318;
	osAppNMIBuffer = 0x8000031c;

	_romSegmentStart = 0;
   /* header */
   BEGIN_SEG(header, _romSegmentStart, 0x04000000)
   {
      build/OUTCODE/src/rom_header.o(.data);
   }
   END_SEG(header)



   /* boot */
   BEGIN_SEG(boot, _headerSegmentRomEnd, 0x04000040)
   {
      build/OUTCODE/src/bootcode.o(.text);
   }
   END_SEG(boot)




   /* 0x80000400, 0x80000450 */
   BEGIN_SEG(start, _bootSegmentRomEnd, 0x80000400)
   {
      build/OUTCODE/src/_start.o (.text);
   }
   END_SEG(start)


   /* All code runs from KSEG0 RAM; nothing is mapped through the TLB. */
   BEGIN_SEG(code, _startSegmentRomEnd, _startSegmentEnd) SUBALIGN(16)
   {
#include "ld/lib.text.ld.inc"
      build/OUTCODE/src/gcc/libgcc_compat.o (.text);
   }
   END_SEG(code)


   BEGIN_SEG(cdata, _codeSegmentRomEnd, _codeSegmentRomEnd)
   {
      build/OUTCODE/src/c_data_filler.o (.data);
   }
   END_SEG(cdata)



   /* Boot-time scratch: init() copies the compressed data and inflate here (see init.c) */
   BEGIN_SEG(inflate, _cdataSegmentRomEnd, 0x80300000)
   {
      build/OUTCODE/src/inflate/inflate.o (.text);
      build/OUTCODE/src/inflate/inflate.o (.data);
      build/OUTCODE/src/inflate/inflate.o (.rodata);
   }
   END_SEG(inflate)



   __csegtempPos = 0xC00000;
   __dataSegmentVaddrStart = _codeSegmentEnd;

   BEGIN_SEG(csegment, __csegtempPos, __dataSegmentVaddrStart)
   {
      build/OUTCODE/src/rspboot.o (.text);
      build/OUTCODE/src/gspboot.o (.text);
      build/OUTCODE/src/aspboot.o (.text);
#include "ld/lib.data.ld.inc"
#include "ld/lib.rodata.ld.inc"
#include "ld/game.data.ld.inc"
#include "ld/game.rodata.ld.inc"
      build/OUTCODE/src/gspboot.o (.data);
      build/OUTCODE/src/aspboot.o (.data);
      /* Small data (-G 8 in the Makefile), reached as an offset from $gp. Uninitialised small
         variables are kept here as loaded zeros too, so all of it is within 32 KB of _gp. */
      . = ALIGN(16);
      _gp = . + 0x7FF0;
      * (.sdata .sdata.* .srodata .srodata.*);
      * (.sbss .sbss.* .scommon);
   }
   END_SEG(csegment)

   __gpCheck = ASSERT(_csegmentSegmentEnd - (_gp - 0x7FF0) <= 0x10000, "small data is bigger than 64 KB, lower -G in the Makefile");

   _dataSegmentVaddrEnd = _csegmentSegmentRomEnd;



   /* Game code sits in RAM right after the data. init() loads it from ROM in one DMA. */
   BEGIN_SEG(game, ALIGN(_inflateSegmentRomEnd, 16), ALIGN(_csegmentSegmentEnd, 16))
   {
#include "ld/game.text.ld.inc"
   }
   END_SEG(game)


   BEGIN_SEG(fontdl, _gameSegmentRomEnd, 0x01000000)
   {
      build/OUTCODE/assets/font_dl.o (.data);
   }
   END_SEG(fontdl)

   BEGIN_SEG(animation_entries, _fontdlSegmentRomEnd, 0x00000000)
   {
      build/OUTCODE/assets/animationtable_entries.o(.data);
   }
   END_SEG(animation_entries)

   BEGIN_SEG(animation_data, _animation_entriesSegmentRomEnd, 0x00000000)
   {
      build/OUTCODE/assets/animationtable_data.o(.data);
   }
   END_SEG(animation_data)

   BEGIN_SEG(Globalimagetable, _animation_dataSegmentRomEnd, 0x02000000)
   {
      build/OUTCODE/assets/oddtextures.o (.data);
   }
   END_SEG(Globalimagetable)

   BEGIN_SEG(rarewarelogo, _GlobalimagetableSegmentRomEnd, 0x02000000)
   {
      build/OUTCODE/assets/rarewarelogo.o (.data);
   }
   END_SEG(rarewarelogo)

   BEGIN_SEG(romfiles2, _rarewarelogoSegmentRomEnd, _romfiles2SegmentRomStart)
   {
      build/OUTCODE/assets/romfiles2.o (.data);
   }
   END_SEG(romfiles2)

   BEGIN_SEG(ramromfiles, _romfiles2SegmentRomEnd, _ramromfilesSegmentRomStart)
   {
      build/OUTCODE/assets/ramrom/ramrom.o (.data);
   }
   END_SEG(ramromfiles)


   BEGIN_SEG(fontbankgothic, _ramromfilesSegmentRomEnd, 0)
   {
      build/OUTCODE/assets/font/fontBankGothic.o (.data);
   }
   END_SEG(fontbankgothic)

   BEGIN_SEG(fontzurichbold, _fontbankgothicSegmentRomEnd, 0)
   {
      build/OUTCODE/assets/font/fontZurichBold.o (.data);
   }
   END_SEG(fontzurichbold)



   BEGIN_SEG(musicfiles, _fontzurichboldSegmentRomEnd, _musicfilesSegmentRomStart)
   {
      build/OUTCODE/assets/music/sfx.ctl.o (.data);
      build/OUTCODE/assets/music/sfx.tbl.o (.data);
      build/OUTCODE/assets/music/instruments.ctl.o (.data);
      build/OUTCODE/assets/music/instruments.tbl.o (.data);
      build/OUTCODE/assets/music/music.o (.music);
      build/OUTCODE/assets/music/music.o (.musiccompressed);
   }
   END_SEG(musicfiles)



   BEGIN_SEG(obseg, _musicfilesSegmentRomEnd, _obsegSegmentRomStart)
   {
      build/OUTCODE/assets/obseg/ob_seg.o (.data);
   }
   END_SEG(obseg)



   BEGIN_SEG(images, _obsegSegmentRomEnd, _imagesSegmentRomStart)
   {
#include "ld/images.ld.inc"
   }
   END_SEG(images)



	_romSegmentEnd = _imagesSegmentRomEnd;



   _bssSegmentStart = ADDR(.bss);
   .bss ALIGN(_gameSegmentEnd, 16) (NOLOAD) :
   {
#include "ld/lib.bss.ld.inc"
#include "ld/game.bss.ld.inc"
   }
   _bssSegmentEnd = ADDR(.bss) + SIZEOF(.bss);
   _bssSegmentSize = SIZEOF(.bss);

   _stacksSegmentStart = ADDR(.stacks);
   .stacks ALIGN(_bssSegmentEnd, 16) (NOLOAD) :
   {
      build/OUTCODE/src/stacks.o (.bss);
   }
   _stacksSegmentEnd = ADDR(.stacks) + SIZEOF(.stacks);

   _cfbSegmentStart = ADDR(.cfb);
   .cfb ALIGN(_stacksSegmentEnd, 64) (NOLOAD) :
   {
      build/OUTCODE/src/cfb.o (.bss);
   }
   _cfbSegmentEnd = ADDR(.cfb) + SIZEOF(.cfb);

   /* The heap runs from here to the end of the 8MB */
   _heapSegmentStart = ALIGN(_cfbSegmentEnd, 16);

   /* init() runs on sp_rmon and copies the compressed data to just below 0x80300000,
      so the boot image (bss and stacks) must stay clear of that area. 0x14000 covers c_data. */
   __bootScratchCheck = ASSERT(_stacksSegmentEnd <= 0x80300000 - 0x14000, "bss/stacks overlap the boot decompression area");

   /*move em to end, out of the way*/
   /DISCARD/ : {
      * (.MIPS.abiflags);
      * (.musicdecompressed);
      * (.options);
      * (.gnu.attributes);
      * (.pdr);
      * (.mdebug);
      * (.gptab.bss);
      * (.gptab.data);
      * (.reginfo);
   }
}
