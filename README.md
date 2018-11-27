# other-half-arduino
This project contains the Arduino files for an RFID based simple match game.

The game assumes 3 objects:
1. RFID tags, each encoded with a byte containing their group number which translates to led color.
   The tags also contain information if a "Mission" has been assigned, meaning the tag is valid to play.
2. A Main station where missions are assigned and encoded to RFID tags 
   the Main station also checks tags for a mission complete flag rewarding with a "Prize".
3. Outposts, locations where the tags are checked and the game can be won if two non identical tags of the 
   same group are checked in a close time frame.
   The win is encoded on the last chip checked, it can then be returned to the main station Main station to 
   collect the Prize and get encoded with a different group code.
   
The included Arduino files match the above objects

other-half-arduino-writer - intended for encoding RFID tags with their initial Group

other-half-arduino-main - the code for the Main station, also sending tag information over ethernet

other-half-arduino-outpost - code for an Outpost station

The game assumes 3 bytes of information on the RFID tags according to the following mapping:

byte1: group mask - 1's for bits to be left unchanged, 0's for bits to change, bits 6 and 7 should always be 1's so not change mission validity and win status.
                    This byte is no longer really required as their is only a single group, it is a legacy from the previous game version.
byte2: group - a number from 1-5 signifying the group

bits 6 and 7 are reserved as they are mission validity and win status and should not be set using the group byte.

byte3: mission - bit 6 signifys a valid mission is encoded and bit 7 signifys if the mission has been completed or not.
