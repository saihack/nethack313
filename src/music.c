/*	SCCS Id: @(#)music.c	3.1	93/05/25	*/
/* 	Copyright (c) 1989 by Jean-Christophe Collet */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * This file contains the different functions designed to manipulate the
 * musical instruments and their various effects.
 *
 * Actually the list of instruments / effects is :
 *
 * (wooden) flute	may calm snakes if player has enough dexterity
 * magic flute		may put monsters to sleep:  area of effect depends
 *			on player level.
 * (tooled) horn	Will awaken monsters:  area of effect depends on player
 *			level.  May also scare monsters.
 * fire horn		Acts like a wand of fire.
 * frost horn		Acts like a wand of cold.
 * bugle		Will awaken soldiers (if any):  area of effect depends
 *			on player level.
 * (wooden) harp	May calm nymph if player has enough dexterity.
 * magic harp		Charm monsters:  area of effect depends on player
 *			level.
 * (leather) drum	Will awaken monsters like the horn.
 * drum of earthquake	Will initiate an earthquake whose intensity depends
 *			on player level.  That is, it creates random pits
 *			called here chasms.
 */

#include "hack.h"

static void FDECL(awaken_monsters,(int));
static void FDECL(put_monsters_to_sleep,(int));
static void FDECL(charm_snakes,(int));
static void FDECL(calm_nymphs,(int));
static void FDECL(charm_monsters,(int));
static void FDECL(do_earthquake,(int));
static int FDECL(do_improvisation,(struct obj *));

#ifdef UNIX386MUSIC
static int NDECL(atconsole);
static void FDECL(speaker,(struct obj *,char *));
#endif
#ifdef VPIX_MUSIC
extern int sco_flag_console;	/* will need changing if not _M_UNIX */
static void NDECL(playinit);
static void FDECL(playstring, (char *,size_t));
static void FDECL(speaker,(struct obj *,char *));
#endif

#ifdef AMIGA
void FDECL( amii_speaker, ( struct obj *, char *, int ) );
#endif

/*
 * Wake every monster in range...
 */

static void
awaken_monsters(distance)
int distance;
{
	register struct monst *mtmp = fmon;
	register int distm;

	while(mtmp) {
		distm = distu(mtmp->mx, mtmp->my);
		if (distm < distance) {
		    mtmp->msleep = 0;
		    mtmp->mcanmove = 1;
		    mtmp->mfrozen = 0;
		    /* May scare some monsters */
		    if (distm < distance/3 &&
			    !resist(mtmp, SCROLL_CLASS, 0, NOTELL))
			mtmp->mflee = 1;
		}
		mtmp = mtmp->nmon;
	}
}

/*
 * Make monsters fall asleep.  Note that they may resist the spell.
 */

static void
put_monsters_to_sleep(distance)
int distance;
{
	register struct monst *mtmp = fmon;

	while(mtmp) {
		if (distu(mtmp->mx, mtmp->my) < distance &&
			!resist(mtmp, WAND_CLASS, 0, NOTELL)) {
		    register int min_sleep = d(10,10);
		    /* 10d10 turns + wake_nearby() to rouse */
		    mtmp->msleep = 1;
		    mtmp->mcanmove = 0;
		    if ((int)mtmp->mfrozen < min_sleep)
			mtmp->mfrozen = min_sleep;
		}
		mtmp = mtmp->nmon;
	}
}

/*
 * Charm snakes in range.  Note that the snakes are NOT tamed.
 */

static void
charm_snakes(distance)
int distance;
{
	register struct monst *mtmp = fmon;

	while (mtmp) {
		if (mtmp->data->mlet == S_SNAKE &&
		    distu(mtmp->mx, mtmp->my) < distance) {
			mtmp->mpeaceful = 1;
			if (cansee(mtmp->mx, mtmp->my))
				pline(
 "%s freezes and sways with the music, then seems quieter.", Monnam(mtmp));
		}
		mtmp = mtmp->nmon;
	}
}

/*
 * Calm nymphs in range.
 */

static void
calm_nymphs(distance)
int distance;
{
	register struct monst *mtmp = fmon;

	while (mtmp) {
		if (mtmp->data->mlet == S_NYMPH &&
		    distu(mtmp->mx, mtmp->my) < distance) {
			mtmp->mpeaceful = 1;
			if (cansee(mtmp->mx, mtmp->my))
				pline(
 "%s listens cheerfully to the music, then seems quieter.", Monnam(mtmp));
		}
		mtmp = mtmp->nmon;
	}
}

/* Awake only soldiers of the level. */

void
awaken_soldiers()
{
#ifdef ARMY
	register struct monst *mtmp = fmon;

	while(mtmp) {
	    if (is_mercenary(mtmp->data) && mtmp->data != &mons[PM_GUARD]) {
		mtmp->mpeaceful = mtmp->msleep = mtmp->mfrozen = 0;
		mtmp->mcanmove = 1;
		if (canseemon(mtmp))
		    pline("%s is now ready for battle!", Monnam(mtmp));
		else
		    Norep("You hear the rattle of battle gear being readied.");
	    }
	    mtmp = mtmp->nmon;
	}
#endif /* ARMY */
}

/* Charm monsters in range.  Note that they may resist the spell. */

static void
charm_monsters(distance)
int distance;
{
	register struct monst *mtmp = fmon, *mtmp2;

	while(mtmp) {
		mtmp2 = mtmp->nmon;
		if (distu(mtmp->mx, mtmp->my) <= distance)
		    if(!resist(mtmp, SCROLL_CLASS, 0, NOTELL))
			(void) tamedog(mtmp, (struct obj *) 0);
		mtmp = mtmp2;
	}

}

/* Generate earthquake :-) of desired force.
 * That is:  create random chasms (pits).
 */

static void
do_earthquake(force)
int force;
{
	register int x,y;
	struct monst *mtmp;
	struct obj *otmp;
	struct trap *chasm;
	int start_x, start_y, end_x, end_y;

	start_x = u.ux - (force * 2);
	start_y = u.uy - (force * 2);
	end_x = u.ux + (force * 2);
	end_y = u.uy + (force * 2);
	if (start_x < 1) start_x = 1;
	if (start_y < 1) start_y = 1;
	if (end_x >= COLNO) end_x = COLNO - 1;
	if (end_y >= ROWNO) end_y = ROWNO - 1;
	for (x=start_x; x<=end_x; x++) for (y=start_y; y<=end_y; y++) {
	    if ((mtmp = m_at(x,y)) != 0) {
		wakeup(mtmp);	/* peaceful monster will become hostile */
		if (mtmp->mundetected && is_hider(mtmp->data)) {
		    mtmp->mundetected = 0;
		    if (cansee(x,y))
			pline("%s is shaken loose from the ceiling!",
							    Amonnam(mtmp));
		    else
			You("hear a thumping sound.");
		    if (x==u.ux && y==u.uy)
			You("easily dodge the falling %s.",
							    mon_nam(mtmp));
		    newsym(x,y);
		}
	    }
	    if (!rn2(14 - force)) switch (levl[x][y].typ) {
		  case FOUNTAIN : /* Make the fountain disappear */
			if (cansee(x,y))
				pline("The fountain falls into a chasm.");
			goto do_pit;
#ifdef SINKS
		  case SINK :
			if (cansee(x,y))
				pline("The kitchen sink falls into a chasm.");
			goto do_pit;
#endif
		  case ALTAR :
			if (cansee(x,y))
				pline("The altar falls into a chasm.");
			goto do_pit;
		  case THRONE :
			if (cansee(x,y))
				pline("The throne falls into a chasm.");
			/* Falls into next case */
		  case ROOM :
		  case CORR : /* Try to make a pit */
do_pit:		    chasm = maketrap(x,y,PIT);
		    if (!chasm) break;	/* no pit if portal at that location */
		    chasm->tseen = 1;

		    levl[x][y].doormask = 0;

		    mtmp = m_at(x,y);

		    if ((otmp = sobj_at(BOULDER, x, y)) != 0) {
			if (cansee(x, y))
			   pline("KADOOM! The boulder falls into a chasm%s!",
			      ((x == u.ux) && (y == u.uy)) ? " below you" : "");
			if (mtmp)
				mtmp->mtrapped = 0;
			freeobj(otmp);
			(void) flooreffects(otmp, x, y, "");
			break;
		    }

		    /* We have to check whether monsters or player
		       falls in a chasm... */

		    if (mtmp) {
			if(!is_flyer(mtmp->data) && !is_clinger(mtmp->data)) {
			    mtmp->mtrapped = 1;
			    if(cansee(x,y))
				pline("%s falls into a chasm!", Monnam(mtmp));
			    else if (flags.soundok && humanoid(mtmp->data))
				You("hear a scream!");
#ifdef MUSE
			    mselftouch(mtmp, "Falling, ", TRUE);
			    if (mtmp->mhp > 0)
#endif
			    if ((mtmp->mhp -= rnd(6)) <= 0) {
				if(!cansee(x,y))
				    pline("It is destroyed!");
				else {
				    You("destroy %s!", mtmp->mtame ?
					x_monnam(mtmp, 0, "poor", 0) :
					mon_nam(mtmp));
				}
				xkilled(mtmp,0);
			    }
			}
		    } else if (x == u.ux && y == u.uy) {
			    if (Levitation
#ifdef POLYSELF
				|| is_flyer(uasmon) || is_clinger(uasmon)
#endif
				) {
				    pline("A chasm opens up under you!");
				    You("don't fall in!");
			    } else {
				    You("fall into a chasm!");
				    u.utrap = rn1(6,2);
				    u.utraptype = TT_PIT;
				    losehp(rnd(6),"fell into a chasm",
					NO_KILLER_PREFIX);
				    selftouch("Falling, you");
			    }
		    } else newsym(x,y);
		    break;
		  case DOOR : /* Make the door collapse */
		    if (levl[x][y].doormask == D_NODOOR) break;
		    if (cansee(x,y))
			pline("The door collapses.");
		    levl[x][y].doormask = D_NODOOR;
		    newsym(x,y);
		    break;
	    }
	}
}

/*
 * The player is trying to extract something from his/her instrument.
 */

static int
do_improvisation(instr)
struct obj *instr;
{
	int damage;

#ifdef MAC
	mac_speaker ( instr , "C" ) ;
#endif
#ifdef AMIGA
	amii_speaker ( instr , "Cw", AMII_OKAY_VOLUME ) ;
#endif
#ifdef VPIX_MUSIC
	if (sco_flag_console)
		speaker(instr, "C");
#endif
	if (Confusion)
	  pline("What you produce is quite far from music...");
	else
	  You("start playing %s.", the(xname(instr)));
	switch (instr->otyp) {
	      case WOODEN_FLUTE:	/* May charm snakes */
		if (rn2(ACURR(A_DEX)) + u.ulevel > 25)
		  charm_snakes((int)u.ulevel*3);
		exercise(A_DEX, TRUE);
		break;
	      case MAGIC_FLUTE: /* Make monster fall asleep */
		if (instr->spe > 0) {
			instr->spe--;
			check_unpaid(instr);
			You("produce soft music.");
			put_monsters_to_sleep((int)u.ulevel*5);
		}
		exercise(A_DEX, TRUE);
		break;
	      case TOOLED_HORN:	/* Awaken monsters or scare monsters */
		You("produce a frightful, grave sound.");
		awaken_monsters((int)u.ulevel*30);
		exercise(A_WIS, FALSE);
		break;
	      case FROST_HORN:	/* Idem wand of cold */
	      case FIRE_HORN:	/* Idem wand of fire */
		if (instr->spe > 0) {
			instr->spe--;
			check_unpaid(instr);
			if (!getdir(NULL)) {
				if (!Blind)
				    pline("%s glows then fades.",
					  The(xname(instr)));
			} else {
				if (!u.dx && !u.dy && !u.dz) {
					if((damage = zapyourself(instr)))
					  losehp(damage,
		self_pronoun("using a magical horn on %sself", "him"),
					  NO_KILLER_PREFIX);
					makeknown(instr->otyp);
					return(2);
				}
				buzz((instr->otyp == FROST_HORN) ? AD_COLD-1 : AD_FIRE-1, rn1(6,6), u.ux, u.uy, u.dx, u.dy);
				makeknown(instr->otyp);
				return(2);
			}
		}
		break;
	      case BUGLE:	/* Awaken & attract soldiers */
		You("extract a loud noise from %s.", the(xname(instr)));
		awaken_soldiers();
		exercise(A_WIS, FALSE);
		break;
	      case WOODEN_HARP:	/* May calm Nymph */
		if (rn2(ACURR(A_DEX)) + u.ulevel > 25)
		  calm_nymphs((int)u.ulevel*3);
		exercise(A_DEX, TRUE);
		break;
	      case MAGIC_HARP:	/* Charm monsters */
		if (instr->spe > 0) {
			instr->spe--;
			check_unpaid(instr);
			pline("%s produces very attractive music.",
			      The(xname(instr)));
			charm_monsters(((int)u.ulevel - 1) / 3 + 1);
		}
		exercise(A_DEX, TRUE);
		break;
	      case LEATHER_DRUM:	/* Awaken monsters */
		You("beat a deafening row!");
		awaken_monsters((int)u.ulevel * 40);
		exercise(A_WIS, FALSE);
		break;
	      case DRUM_OF_EARTHQUAKE:	/* create several pits */
		if (instr->spe > 0) {
			instr->spe--;
			check_unpaid(instr);
			You("produce a heavy, thunderous rolling!");
			pline("The entire dungeon is shaking around you!");
			do_earthquake(((int)u.ulevel - 1) / 3 + 1);
			/* shake up monsters in a much larger radius... */
			awaken_monsters(ROWNO * COLNO);
			makeknown(DRUM_OF_EARTHQUAKE);
		}
		break;
	      default:
		impossible("What a weird instrument (%d)!",instr->otyp);
		break;
	}
	return (2);		/* That takes time */
}

/*
 * So you want music...
 */

int
do_play_instrument(instr)
struct obj *instr;
{
    char buf[BUFSZ], c = 'y';
#ifndef	AMIGA
	char *s;
#endif
    int x,y;
    boolean ok;

    if (Underwater) {
	You("can't play music underwater!");
	return(0);
    }
    if (instr->otyp != LEATHER_DRUM && instr->otyp != DRUM_OF_EARTHQUAKE) {
	c = yn("Improvise?");
    }
    if (c == 'n') {
	if (u.uevent.uheard_tune == 2 && yn("Play the passtune?") == 'y')
		Strcpy(buf, tune);
	else
		getlin("What tune are you playing? [what 5 notes]", buf);
#ifndef	AMIGA
	/* The AMIGA supports two octaves of notes */
	for (s=buf; *s; s++) *s = highc(*s);
#endif
	You("extract a strange sound from %s!", the(xname(instr)));
#ifdef UNIX386MUSIC
	/* if user is at the console, play through the console speaker */
	if (atconsole())
	    speaker(instr, buf);
#endif
#ifdef VPIX_MUSIC
	if (sco_flag_console)
	    speaker(instr, buf);
#endif
#ifdef MAC
	mac_speaker ( instr , buf ) ;
#endif
#ifdef AMIGA
	{
		char nbuf[ 20 ];
		int i;
		for( i = 0; buf[i] && i < 5; ++i )
		{
			nbuf[ i*2 ] = buf[ i ];
			nbuf[ (i*2)+1 ] = 'h';
		}
		nbuf[ i*2 ] = 0;
		amii_speaker ( instr , nbuf, AMII_OKAY_VOLUME ) ;
	}
#endif
	/* Check if there was the Stronghold drawbridge near
	 * and if the tune conforms to what we're waiting for.
	 */
	if(Is_stronghold(&u.uz)) {
	    exercise(A_WIS, TRUE);		/* just for trying */
	    if(!strcmp(buf,tune)) {
		/* Search for the drawbridge */
		for(y=u.uy-1; y<=u.uy+1; y++)
		    for(x=u.ux-1;x<=u.ux+1;x++)
			if(isok(x,y))
			if(find_drawbridge(&x,&y)) {
			    if(levl[x][y].typ == DRAWBRIDGE_DOWN)
				close_drawbridge(x,y);
			    else
				open_drawbridge(x,y);
			    return 0;
			}
	    } else if(flags.soundok) {
		if (u.uevent.uheard_tune < 1) u.uevent.uheard_tune = 1;
		/* Okay, it wasn't the right tune, but perhaps
		 * we can give the player some hints like in the
		 * Mastermind game */
		ok = FALSE;
		for(y = u.uy-1; y <= u.uy+1 && !ok; y++)
		    for(x = u.ux-1; x <= u.ux+1 && !ok; x++)
			if(isok(x,y))
			if(IS_DRAWBRIDGE(levl[x][y].typ) ||
			   is_drawbridge_wall(x,y) >= 0)
				ok = TRUE;
		if(ok) { /* There is a drawbridge near */
		    int tumblers, gears;
		    boolean matched[5];

		    tumblers = gears = 0;
		    for(x=0; x < 5; x++)
			matched[x] = FALSE;

		    for(x=0; x < (int)strlen(buf); x++)
			if(x < 5) {
			    if(buf[x] == tune[x]) {
				gears++;
				matched[x] = TRUE;
			    } else
				for(y=0; y < 5; y++)
				    if(!matched[y] &&
				       buf[x] == tune[y] &&
				       buf[y] != tune[y]) {
					tumblers++;
					matched[y] = TRUE;
					break;
				    }
			}
		    if(tumblers)
			if(gears)
			    You("hear %d tumbler%s click and %d gear%s turn.",
				tumblers, plur(tumblers), gears, plur(gears));
			else
			    You("hear %d tumbler%s click.",
				tumblers, plur(tumblers));
		    else if(gears) {
			You("hear %d gear%s turn.", gears, plur(gears));
			if (gears == 5) u.uevent.uheard_tune = 2;
		    }
		}
	    }
	  }
	return 1;
    } else
	    return do_improvisation(instr);
}

#ifdef UNIX386MUSIC
/*
 * Play audible music on the machine's speaker if appropriate.
 */

static int
atconsole()
{
    /*
     * Kluge alert: This code assumes that your [34]86 has no X terminals
     * attached and that the console tty type is AT386 (this is always true
     * under AT&T UNIX for these boxen). The theory here is that your remote
     * ttys will have terminal type `ansi' or something else other than
     * `AT386' or `xterm'. We'd like to do better than this, but testing
     * to see if we're running on the console physical terminal is quite
     * difficult given the presence of virtual consoles and other modern
     * UNIX impedimenta...
     */
    char	*termtype = getenv("TERM");

     return(!strcmp(termtype, "AT386") || !strcmp(termtype, "xterm"));
}

static void
speaker(instr, buf)
struct obj *instr;
char	*buf;
{
    /*
     * For this to work, you need to have installed the PD speaker-control
     * driver for PC-compatible UNIX boxes that I (esr@snark.thyrsus.com)
     * posted to comp.sources.unix in Feb 1990.  A copy should be included
     * with your nethack distribution.
     */
    int	fd;

    if ((fd = open("/dev/speaker", 1)) != -1)
    {
	/* send a prefix to modify instrumental `timbre' */
	switch (instr->otyp)
	{
	case WOODEN_FLUTE:
	case MAGIC_FLUTE:
	    (void) write(fd, ">ol", 1); /* up one octave & lock */
	    break;
	case TOOLED_HORN:
	case FROST_HORN:
	case FIRE_HORN:
	    (void) write(fd, "<<ol", 2); /* drop two octaves & lock */
	    break;
	case BUGLE:
	    (void) write(fd, "ol", 2); /* octave lock */
	    break;
	case WOODEN_HARP:
	case MAGIC_HARP:
	    (void) write(fd, "l8mlol", 4); /* fast, legato, octave lock */
	    break;
	}
	(void) write(fd, buf, strlen(buf));
	(void) close(fd);
    }
}
#endif /* UNIX386MUSIC */

#ifdef VPIX_MUSIC

# if 0
#include <sys/types.h>
#include <sys/console.h>
#include <sys/vtkd.h>
# else
#define KIOC ('K' << 8)
#define KDMKTONE (KIOC | 8)
# endif

#define noDEBUG

#include "interp.c"	/* from snd86unx.shr */

static void tone(hz, ticks)
/* emit tone of frequency hz for given number of ticks */
unsigned int hz, ticks;
{
    ioctl(0,KDMKTONE,hz|((ticks*10)<<16));
# ifdef DEBUG
    printf("TONE: %6d %6d\n",hz,ticks * 10);
# endif
    nap(ticks * 10);
}


static void rest(ticks)
/* rest for given number of ticks */
int	ticks;
{
    nap(ticks * 10);
# ifdef DEBUG
    printf("REST:        %6d\n",ticks * 10);
# endif
}


static void
speaker(instr, buf)
struct obj *instr;
char	*buf;
{
    /* emit a prefix to modify instrumental `timbre' */
    playinit();
    switch (instr->otyp)
    {
	case WOODEN_FLUTE:
	case MAGIC_FLUTE:
	    playstring(">ol", 1); /* up one octave & lock */
	    break;
	case TOOLED_HORN:
	case FROST_HORN:
	case FIRE_HORN:
	    playstring("<<ol", 2); /* drop two octaves & lock */
	    break;
	case BUGLE:
	    playstring("ol", 2); /* octave lock */
	    break;
	case WOODEN_HARP:
	case MAGIC_HARP:
	    playstring("l8mlol", 4); /* fast, legato, octave lock */
	    break;
    }
    playstring( buf, strlen(buf));
}

# ifdef DEBUG
main(argc,argv)
char *argv[];
{
    if (argc == 2) {
	playinit();
	playstring(argv[1], strlen(argv[1]));
    }
}
# endif
#endif	/* VPIX_MUSIC */

/*music.c*/
