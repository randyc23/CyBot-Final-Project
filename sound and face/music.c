#include "open_interface.h"
#include "music.h"

/// Load three songs onto the iRobot
/**
 * Before calling this method, be sure to initialize the open interface by calling:
 *
 *   oi_t* sensor = oi_alloc();
 *   oi_init(sensor); 
 *
 */


void load_songs(int i) {

	// Notes: oi_load_song takes four arguments
	// arg1 - an integer from 0 to 16 identifying this song
	// arg2 - an integer that indicates the number of notes in the song (if greater than 16, it will consume the next song index's storage space)
	// arg3 - an array of integers representing the midi note values (example: 60 = C, 61 = C sharp)
	// arg4 - an array of integers representing the duration of each note (in 1/64ths of a second)
	// 
	// For a note sheet, see page 12 of the iRobot Creat Open Interface datasheet

   if (i==0) {
       unsigned char stopNumNotes = 1;
       unsigned char stopNotes[1] = {0};

       unsigned char stopDuration[1] = {0};

       oi_loadSong(STOP, stopNumNotes, stopNotes, stopDuration);
       oi_play_song(STOP);
   }

   else if (i==1) {
      unsigned char happyNumNotes = 12;
      unsigned char happyNotes[12] = {
          84, 88, 91,   // rising chirp
          88, 84,       // quick drop
          91, 95,       // excited jump
          88, 84,       // bounce back
          79, 84, 88    // ending trill
      };

      unsigned char happyDuration[12] = {
          4, 6, 10,
          4, 4,
          6, 10,
          4, 4,
          6, 6, 8
      };
              oi_loadSong(HAPPY, happyNumNotes, happyNotes, happyDuration);
              oi_play_song(HAPPY);
   }

   else if (i==2) {
       unsigned char alarmNumNotes = 6;
              unsigned char alarmNotes[6] = {90, 0, 90, 0, 90, 0};

              unsigned char alarmDuration[6] = {12, 0, 12, 0, 12, 0};

              oi_loadSong(ALARM, alarmNumNotes, alarmNotes, alarmDuration);
              oi_play_song(ALARM);
   }

   else if (i==3) {
      unsigned char backNumNotes = 6;
      unsigned char backNotes[6] = {85, 0, 85, 0, 85, 0};

      unsigned char backDuration[6] = {24, 24, 24, 24, 24, 24};

      oi_loadSong(BACK, backNumNotes, backNotes, backDuration);
      oi_play_song(BACK);
   }

}
