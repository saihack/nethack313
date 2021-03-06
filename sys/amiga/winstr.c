/*    SCCS Id: @(#)winstr.c    3.1    93/04/02 */
/* Copyright (c) Gregg Wonderly, Naperville, Illinois,  1991,1992,1993. */
/* NetHack may be freely redistributed.  See license for details. */

#include "amiga:windefs.h"
#include "amiga:winext.h"
#include "amiga:winproto.h"

/* Put a string into the indicated window using the indicated attribute */

void
amii_putstr(window,attr,str)
    winid window;
    int attr;
    const char *str;
{
    struct Window *w;
    register struct amii_WinDesc *cw;
    char *ob;
    int i, j, n0, bottom, totalvis, wheight;

#ifdef	VIEWWINDOW
    /* Never write text in MAP it would be too small to read */
    if( window == WIN_MAP )
	window = WIN_MESSAGE;
#endif
    /* Always try to avoid a panic when there is no window */
    if( window == WIN_ERR )
    {
	window = WIN_BASE;
	if( window == WIN_ERR )
	    window = WIN_BASE = amii_create_nhwindow( NHW_BASE );
    }

    if( window == WIN_ERR || ( cw = amii_wins[window] ) == NULL )
    {
	flags.window_inited=0;
	panic(winpanicstr,window, "putstr");
    }

    w = cw->win;

    if(!str) return;
    amiIDisplay->lastwin = window;    /* do we care??? */

    /* NHW_MENU windows are not opened immediately, so check if we
     * have the window pointer yet
     */

    if( w )
    {
	/* Force the drawing mode and pen colors */

	SetDrMd( w->RPort, JAM2 );
	if( cw->type == NHW_STATUS )
	    SetAPen( w->RPort, attr ? C_WHITE : C_RED );
	else if( cw->type == NHW_MESSAGE )
	    SetAPen( w->RPort, attr ? C_RED : C_WHITE );
	else
	    SetAPen( w->RPort, attr ? C_RED : C_WHITE );
	SetBPen( w->RPort, C_BLACK );
    }
    else if( cw->type != NHW_MENU && cw->type != NHW_TEXT )
    {
	panic( "NULL window pointer in putstr 2: %d", window );
    }

    /* Okay now do the work for each type */

    switch(cw->type)
    {
#define MORE_FUDGE  10  /* 8 for --more--, 1 for preceeding sp, 1 for */
		/* putstr pad */
    case NHW_MESSAGE:
    	/* Calculate the bottom line */
    	bottom = amii_msgborder( w );

	wheight = ( w->Height - w->BorderTop -
			    w->BorderBottom - 3 ) / w->RPort->TxHeight;
	
	if( attr != -1 && scrollmsg )
	{
	    if( ++cw->disprows > wheight )
	    {
		outmore( cw );
		cw->disprows = 1; /* count this line... */
	    }
	    else
	    {
		ScrollRaster( w->RPort, 0, w->RPort->TxHeight,
			w->BorderLeft-1, w->BorderTop+1,
			w->Width - w->BorderRight-1,
			w->Height - w->BorderBottom - 1 );
	    }
	    amii_curs( WIN_MESSAGE, 1, bottom );
	}

	strncpy( toplines, str, BUFSZ );
	toplines[ BUFSZ - 1 ] = 0;

	/* For initial message to be visible, we need to explicitly position the
	 * cursor.  This flag, cw->curx == -1 is used elsewhere to force the
	 * cursor to be repositioned to the "bottom".
	 */
	if( cw->curx == -1 )
	{
	    amii_curs( WIN_MESSAGE, 1, bottom );
	    cw->curx = 0;
	}

	/* If used all of history lines, move them down */

	if( cw->maxrow >= flags.msg_history )
	{
	    if( cw->data[ 0 ] )
		free( cw->data[ 0 ] );
	    memcpy( cw->data, &cw->data[ 1 ],
		( flags.msg_history - 1 ) * sizeof( char * ) );
	    cw->data[ flags.msg_history - 1 ] =
			    (char *) alloc( strlen( toplines ) + 5 );
	    strcpy( cw->data[ i = flags.msg_history - 1 ] +
				SOFF + (scrollmsg!=0), toplines );
	}
	else
	{
	    /* Otherwise, allocate a new one and copy the line in */
	    cw->data[ cw->maxrow ] = (char *)
					alloc( strlen( toplines ) + 5 );
	    strcpy( cw->data[ i = cw->maxrow++ ] +
				SOFF + (scrollmsg!=0), toplines );
	}
	cw->data[ i ][0] = 1;
	cw->data[ i ][1] = attr;

	if( scrollmsg )
	    cw->data[ i ][2] = (cw->wflags & FLMSG_FIRST ) ? '>' : ' ';

	str = cw->data[i] + SOFF;
	if( strlen(str) >= (cw->cols-MORE_FUDGE) )
	{
	    int i;
	    char *p;

	    while( strlen( str ) >= (cw->cols-MORE_FUDGE) )
	    {
		for(p=(&str[ cw->cols ])-MORE_FUDGE; !isspace(*p) && p != str;)
		{
		    --p;
		}
		if( p == str )
		    p = &str[ cw->cols ];
		outsubstr( cw, str, i = (long)p-(long)str );
		cw->curx += i;
		amii_cl_end( cw, cw->curx );
		str = p+1;
	    }

	    if( *str )
	    {
		outsubstr( cw, str, i = strlen( str ) );
		cw->curx += i;
		amii_cl_end( cw, cw->curx );
	    }
	}
	else
	{
	    outsubstr( cw, str, i = strlen( str ) );
	    cw->curx += i;
	    amii_cl_end( cw, cw->curx );
	}
	cw->wflags &= ~FLMSG_FIRST;
	for( totalvis = i = 0; i < cw->maxrow; ++i )
	{
	    if( cw->data[i][1] == 0 )
		++totalvis;
	}
	if( scrollmsg )
	{
	    SetPropInfo( w, &MsgScroll,
	      ( w->Height-w->BorderTop-w->BorderBottom ) / w->RPort->TxHeight,
	      totalvis, totalvis );
	}
	i = strlen( toplines + SOFF );
	cw->maxcol = max( cw->maxcol, i );
	cw->vwy = cw->maxrow;
	break;

    case NHW_STATUS:
	if( cw->data[ cw->cury ] == NULL )
	    panic( "NULL pointer for status window" );
	ob = &cw->data[cw->cury][j = cw->curx];
	if(flags.botlx) *ob = 0;

	    /* Display when beam at top to avoid flicker... */
	WaitTOF();
	Text(w->RPort,str,strlen(str));
	if( cw->cols > strlen( str ) )
	    TextSpaces( w->RPort, cw->cols - strlen( str ) );

	(void) strncpy(cw->data[cw->cury], str, cw->cols );
	cw->data[cw->cury][cw->cols-1] = '\0'; /* null terminate */
	cw->cury = (cw->cury+1) % 2;
	cw->curx = 0;
	break;

    case NHW_VIEWBOX:
    case NHW_VIEW:
    case NHW_MAP:
    case NHW_BASE:
	amii_curs(window, cw->curx+1, cw->cury);
	Text(w->RPort,str,strlen(str));
	cw->curx = 0;
	    /* CR-LF is automatic in these windows */
	cw->cury++;
	break;

    case NHW_MENU:
    case NHW_TEXT:

	/* always grows one at a time, but alloc 12 at a time */

	if( cw->cury >= cw->rows || !cw->data ) {
	    char **tmp;

		/* Allocate 12 more rows */
	    cw->rows += 12;
	    tmp = (char**) alloc(sizeof(char*) * cw->rows);

		/* Copy the old lines */
	    for(i=0; i<cw->cury; i++)
		tmp[i] = cw->data[i];

	    if( cw->data )
		free( cw->data );

	    cw->data = tmp;

		/* Null out the unused entries. */
	    for(i=cw->cury; i<cw->rows; i++)
		cw->data[i] = 0;
	}

	if( !cw->data )
	    panic("no data storage");

	    /* Shouldn't need to do this, but... */

	if( cw->data && cw->data[cw->cury] )
	    free( cw->data[cw->cury] );

	n0 = strlen(str)+1;
	cw->data[cw->cury] = (char*) alloc(n0+SOFF);

	    /* avoid nuls, for convenience */
	cw->data[cw->cury][VATTR] = attr+1;
	cw->data[cw->cury][SEL_ITEM] = 0;
	Strcpy( cw->data[cw->cury] + SOFF, str);

	if(n0 > cw->maxcol) cw->maxcol = n0;
	if(++cw->cury > cw->maxrow) cw->maxrow = cw->cury;
	break;

    default:
	panic("Invalid or unset window type in putstr()");
    }
}

int
amii_msgborder( w )
    struct Window *w;
{
    register int bottom;

    /* There is a one pixel border at the borders, so subtract two */
    bottom = w->Height - w->BorderTop - w->BorderBottom - 2;
    bottom /= w->RPort->TxHeight;
    if( bottom > 0 )
	--bottom;
    return( bottom );
}

void
outmore( cw )
    register struct amii_WinDesc *cw;
{
    struct Window *w = cw->win;

    if((cw->wflags & FLMAP_SKIP) == 0)
    {
	if( scrollmsg )
	{
	    int bottom;

	    bottom = amii_msgborder( w );

	    ScrollRaster( w->RPort, 0, w->RPort->TxHeight,
			w->BorderLeft-1, w->BorderTop+1,
			w->Width - w->BorderRight-1,
			w->Height - w->BorderBottom - 1 );
	    amii_curs( WIN_MESSAGE, 1, bottom );
	    Text( w->RPort, "--more--", 8 );
	}
	else
	    Text( w->RPort, " --more--", 9 );

	/* Make sure there are no events in the queue */
	flushIDCMP( HackPort );

	/* Allow mouse clicks to clear --more-- */
	WindowGetchar();
	if( lastevent.type == WEKEY && lastevent.un.key == '\33' )
	    cw->wflags |= FLMAP_SKIP;
    }
    if( !scrollmsg )
    {
	amii_curs( WIN_MESSAGE, 1, 0 );
	amii_cl_end( cw, cw->curx );
    }
}

void
outsubstr( cw, str, len )
    register struct amii_WinDesc *cw;
    char *str;
    int len;
{
    struct Window *w = cw->win;

    if( cw->curx )
    {
	/* Check if this string and --more-- fit, if not,
	 * then put out --more-- and wait for a key.
	 */
	if( (len + MORE_FUDGE ) + cw->curx >= cw->cols )
	{
	    if( !scrollmsg )
		outmore( cw );
	}
	else if(topl_addspace)
	{
	    /* Otherwise, move and put out a blank separator */
	    Text( w->RPort, spaces, 1 );
	    cw->curx += 1;
	}
    }

    Text(w->RPort,str,len);
}

/* Put a graphics character onto the screen */

void
amii_putsym( st, i, y, c )
    winid st;
    int i, y;
    CHAR_P c;
{
    char buf[ 2 ];
    amii_curs( st, i, y );
    buf[ 0 ] = c;
    buf[ 1 ] = 0;
    amii_putstr( st, 0, buf );
}

/* Add a line in the message window */

void
amii_addtopl(s)
    const char *s;
{
    amii_putstr(WIN_MESSAGE,0,s);   /* is this right? */
}

void
TextSpaces( rp, nr )
    struct RastPort *rp;
    int nr;
{
    if( nr < 1 )
	return;

    while (nr > sizeof(spaces) - 1)
    {
	Text(rp, spaces, (long)sizeof(spaces) - 1);
	nr -= sizeof(spaces) - 1;
    }
    if (nr > 0)
	Text(rp, spaces, (long)nr);
}

void
amii_remember_topl()
{
    /* ignore for now.  I think this will be done automatically by
     * the code writing to the message window, but I could be wrong.
     */
}

int
amii_doprev_message()
{
    struct amii_WinDesc *cw;
    struct Window *w;
    char *str;

    if( WIN_MESSAGE == WIN_ERR ||
	( cw = amii_wins[ WIN_MESSAGE ] ) == NULL || ( w = cw->win ) == NULL )
    {
	panic(winpanicstr,WIN_MESSAGE, "doprev_message");
    }

    /* When an interlaced/tall screen is in use, the scroll bar will be there */
    if( scrollmsg )
    {
	struct Gadget *gd;
	struct PropInfo *pip;
	int hidden, topidx, i, total, wheight;

	for( gd = w->FirstGadget; gd && gd->GadgetID != 1; )
	    gd = gd->NextGadget;

	if( gd )
	{
	    pip = (struct PropInfo *)gd->SpecialInfo;
	    wheight = ( w->Height - w->BorderTop -
			    w->BorderBottom - 2 ) / w->RPort->TxHeight;
	    hidden = max( cw->maxrow - wheight, 0 );
	    topidx = (((ULONG)hidden * pip->VertPot) + (MAXPOT/2)) >> 16;
	    for( total = i = 0; i < cw->maxrow; ++i )
	    {
		if( cw->data[i][1] == 0 )
		    ++total;
	    }

	    i = 0;
	    while( (--topidx) >= 0 && i < wheight/2 )
	    {
		SetPropInfo( w, &MsgScroll, wheight, total, topidx );
		DisplayData( WIN_MESSAGE, topidx, -1 );
		++i;
	    }
	}
	return(0);
    }

    if( --cw->vwy < 0 )
    {
	cw->maxcol = 0;
	DisplayBeep( NULL );
	str = "\0\0No more history saved...";
    }
    else
	str = cw->data[ cw->vwy ];

    amii_cl_end(cw, 0);
    amii_curs( WIN_MESSAGE, 1, 0 );
    Text(w->RPort,str+SOFF,strlen(str+SOFF));
    cw->curx = cw->cols + 1;

    return( 0 );
}
