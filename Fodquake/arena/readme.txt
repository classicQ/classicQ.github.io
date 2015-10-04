Title    : Final Arena
Filename : farena12.zip
Version  : 1.20
Date     : 12-8-97
Author   : David 'crt' Wright
Email    : wrightd@stanford.edu
Webpage  : http://www.planetquake.com/servers/arena

Intro
-----
Rocket Arena is an interesting new twist to traditional deathmatch.
It is a strictly one-on-one game with "winner stays, loser goes" type rotation.
It is designed both to improve deathmatch skills and be a kick butt patch to boot.

Installation Instructions
-------------------------

READ THESE THOROUGHLY BEFORE INSTALLING

To install: simply extract the pak0.pak file into an "arena" subdirectory under
quake.
e.g. c:\quake\arena

The file quake\arena\pak0.pak should now exist. If it does not, you will not
be able to play with all the maps/sounds.

To connect to a QuakeWorld server, use GameSpy (formerly QuakeSpy, www.gamespy.com)
to find a server that is running Rocket Arena.
Click, and play. Your game directory will be set to "arena" automatically.

To connect to a Normal Quake server, remember to use "-game arena"

If you get an error about a sound or map not being found, you installed the
pak incorrectly.

To run a server you must download the SEPERATE fasrv12.zip

How to Play
-----------

Once connected to a Rocket Arena server, the gameplay is totally automated.
You are put at the end of a line, and wait for your turn to fight in the arena.
Your position in line is the ammo count on your status bar.
Alternatively, you can use the "position" impulse (69) to find out how many people
are ahead of you.

You can take the time in line to watch other matches, and learn valuble tactics.
Your "health" and "armor" status bars now show the health of the two competitors
(look for their names above the status bar to figure out which is which).
Currently, two impulses control the placement of the statusbar names:
Impulse 71 : the default, use for 320x200 fullscreen
Impulse 72 : use for 320x240 windowed, or 640x480 windowed

For higher resolutions, use impulse 72. DO NOT switch to a lower resolution
without first going back to impulse 71. Doing so will crash Quake.
If none of the above made sense, ignore it.

Should you need to take a short break (grab some food, relieve your self, get an icq
message) simply type "impulse 70"
You will be given up to a 5 minute break. Type "impulse 70" again when you are ready to
return. If you do not return within two minutes, you will be disconnected. You will
be warned as the time drains down. You can NOT use impulse 70 if you are already
in the Arena.

When it is your turn to fight, you will be put in the Arena and given 200 armor,
100 health, and a full weapon complement. A 10 second timer will start. When it
says FIGHT! kill or be killed.

"TWO MEN ENTER, ONE MAN LEAVES"

That is the only rule (other than that, it works mostly like normal Quake)

If you win.. you will stay to fight again.. if you lose.. the back of the line for you.

Impulse Quick Reference
-----------------------

IMPULSE 68 - shows simple statistics for the current level. Skill = wins / (wins + loses) * 100
IMPULSE 69 - shows your "position" in line
IMPULSE 70 - step out of line for up to two minutes
IMPULSE 71 - set status bar text for 320x200
IMPULSE 72 - set status bar text for 320x240 or higher


The Maps
--------
We have decided to include the Telefragged Arena maps in this pak.
Because the TF client pak did not include credits or addresses for
the authors, we were unable to contact all of them. We hope that
they feel it is better for their maps to be out there being played
than sitting around unused.

|Status->  R: Previous RA, T: Telefragged Arena, F: Final Arena
|                                                                                                                                           
S File     Name                    Author                                                                                                  
                                                                                                                                           
R arenax   ArenaX                  Brian 'Plucky' Ploeckelman <plucky@planetquake.com>                                                     
                                   http://www.planetquake.com/edit                                                                         
R terrain2 Terrain                 Chris 'Drastic_Man' Sykes <dmp@dc.jones.com>                                                            
                                   http://www.planetquake.com/keygrip                                                                      
R xarena3  X-Arena                 Eric 'Xanthen' Braun <eb9@evansville.edu>                                                               
                                   http://www.planetquake.com/tdk                                                                          
R arenazap Zaphods Arena           'Zaphod' <quake@wvinter.net>                                                                            
                                   http://www.planetquake.com/qref                                                                         
R arenarg2 Rocket Jump This        Ralph Gustavsen a.k.a. tesh[BoB] <clg@coastlinegraphics.com>                                            
                                   http://www.coastlinegraphics.com                                                                        
R arenrg3a Edge of Insanity        Ralph Gustavsen a.k.a. tesh[BoB] <clg@coastlinegraphics.com>                                            
                                   http://www.coastlinegraphics.com                                                                        
R mayan1   Mayan Showdown          Joshua Beardsley a.k.a. NiOXiN <nioxin@mindspring.com>                                                  
                                   http://terra219.telefragged.com                                                                         
R 2pyramid Two Pyramids                                                                                                                    
                                                                                                                                           
R 2towers  Twin Towers Arena       Scooby (Brian Glines) <bglines@apeleon.net>                                                             
                                                                                                                                           
R arenarg4 Spongebath              Ralph Gustavsen a.k.a. tesh[BoB] <clg@coastlinegraphics.com>                                            
                                   http://www.coastlinegraphics.com                                                                        
R arenarg5 The Wicked Empty Base   Ralph Gustavsen a.k.a. tesh[BoB] <clg@coastlinegraphics.com>                                            
                                   http://www.coastlinegraphics.com                                                                        
R arenarg6 ThE PiT                 Ralph Gustavsen a.k.a. tesh[BoB] <clg@coastlinegraphics.com>                                            
                                   http://www.coastlinegraphics.com                                                                        
R arma4    Armageddon 4            Matthias Worch <langsuyar@ocrana.de>                                                                    
                                                                                                                                           
R crandome Crandome                Matt Lutjen a.k.a cranman <cranman@bendnet.com>                                                         
                                                                                                                                           
R egyptra  Egyptra                 Eric Konno <konno@lava.net>                                                                             
                                                                                                                                           
R hill20   Hill 20                 Harvey "Papa Control" Morris <thefargi@erols.com>                                                       
                                                                                                                                           
R iarena2  2 Castles               iNiT <init@hem1.passagen.se>                                                                            
                                                                                                                                           
R pitarena The Pit Arena           Scooby (Brian Glines) <bglines@apeleon.net>                                                             
                                                                                                                                           
R ra_funkf Funkadooda's Revenge    Steve Fukuda <stevefu@unixg.ubc.ca>                                                                     
                                                                                                                                           
R rgarden  Rock Garden             ObserveR <borg@planetquake.com>                                                                         
                                   http://www.planetquake.com/future                                                                       
T barena1  Bowl Arena              nacho <nacho@telefragged.com>                                                                           
                                                                                                                                           
T bbarena2 Blood Bath              halogen <halogen@telefragged.com>                                                                       
                                                                                                                                           
T dm2arena Claustarena             Zoid <zoid@idsoftware.com>                                                                              
                                                                                                                                           
T marena2  Arena by Mungo          Mungo <target@radix.net>                                                                                
                                                                                                                                           
T marena3  Arena3 by Mungo         Mungo <target@radix.net>                                                                                
                                                                                                                                           
T marena4  Arena4 by Mungo         Mungo <target@radix.net>                                                                                
                                                                                                                                           
T rarena3  Rage Arena              Nacho <nacho@telefragged.com>                                                                           
                                                                                                                                           
T soyarena Flesh Arena             Soylent                                                                                                 
                                                                                                                                           
T uarena1  Unknown Arena           Nacho <nacho@telefragged.com>                                                                           
                                                                                                                                           
F 23ar-a   P.L.A.T.Y.P.U.S Arena   AgEnT_xxiii (Brian Gebbia) <agentxxiii@geocities.com>                                                   
                                                                                                                                           
F arendm1a Place To Die                                                                                                                    
                                                                                                                                           
F basarena Base Arena              Cokeman                                                                                                 
                                                                                                                                           
F bunmoo3  First Bunny on the Moon Tika <tika@leland.stanford.edu>                                                                         
                                                                                                                                           
F bunski   Ski Bunny               Tika <tika@leland.stanford.edu>                                                                         
                                                                                                                                           
F chamber1 The Chamber of Pain     Shadow [iC/JL/N] <SHADOW@CYBERZONE.CO.UK>                                                               
                                                                                                                                           
F dom2_1ra Doom2 Map01 - Entryway  Peter Lomax <pcl@provider.co.uk>                                                                        
                                                                                                                                           
F football Rocket Football Arena   Aegis                                                                                                   
                                                                                                                                           
F gear8    CENSORED                Marc Roussel <gear@videotron.ca>                                                                        
                                   http://pages.infinit.net/gear                                                                           
F gnurena  Seek and Hide           Greebo <snoer@image.dk>                                                                                 
                                                                                                                                           
F id3      DM6 Arena               Greg "Q-Man" Olson <qman@mad.scientist.com>                                                             
                                                                                                                                           
F lowgrav  Reduced Gravity         Lance "CornJob" Kett <kett@home.com/kett@cts.com>                                                       
                                                                                                                                           
F nilsrar3 Nils Rocket Arena 3     Mikael [Nils] Nilsson <sandog@hem.passagen.se>                                                          
                                                                                                                                           
F pen2     The Pen                 Casey Hawley - A.K.A WarPig <warpig@sirius.com>                                                         
                                                                                                                                           
F ptucket  Pawtucket Palladium     Andy Alberg (Greblaja) <greblaja@ids.net>                                                               
                                                                                                                                           
F unholy   Unholy Chapel           Spanky <spanky@znet.com>                                                                                
                                                                                                                                           
F yard1    The Backyard            Espen Solberg (KillerBee) <esso@powertech.no>                                                           
                                   
Totals:
20 Maps from Previous RA
 9 Maps from Telefragged Arena
17 Maps new to Final Arena
--
46 Total Final Arena maps


Other contributors
------------------
Greg 'TerMy' Wiles : For starting this whole thing!
                     He kept the servers up and running too.
                     http://www.planetquake.com/servers

Joshua 'NiOXiN' Beardsley : For sorting through the hundreds of maps that came in and
				helping to pick the winners.

Andrew 'Kolinahr' Wu : For the great sounds! Thanks man!
		       http://www.bluesnews.com/guide/qe/who/index.htm

Matt 'WhiteFang' Ayres: For the Regular Quake Rocket Arena port

Copyright and Distribution Permissions
--------------------------------------

This patch is freely distributable provided that this readme is distributed
as well and is unchanged.

All code is copyright PlanetQuake 1997.
Commercial code licensing is available by contacting wrightd@stanford.edu

DISCLAIMER:  If this patch ever formats your hard drive, TerMy takes full legal
responsibility. Uh.. just kidding.. this patch is 100% safe, tested, and FDA approved,
so you should have no trouble with it. But if you do.. its not our fault.

Availability
------------

This modification is available from the following places:

WWW   : http://www.planetquake.com/servers/arena
