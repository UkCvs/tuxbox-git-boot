/* dbox2 flash for intel. amd not yet supported, sorry. */

#include <ppcboot.h>
#include <mpc8xx.h>

flash_info_t	flash_info[CFG_MAX_FLASH_BANKS]; /* info for FLASH chips	*/

/*-----------------------------------------------------------------------
 * Protection Flags:
 */
#define FLAG_PROTECT_SET	0x01
#define FLAG_PROTECT_CLEAR	0x02

/*-----------------------------------------------------------------------
 * Functions
 */
static ulong flash_get_size (vu_long *addr, flash_info_t *info);

int flash_write (uchar *, ulong, ulong);
flash_info_t *addr2info (ulong);

static int write_buff (flash_info_t *info, uchar *src, ulong addr, ulong cnt);
static int write_word (flash_info_t *info, ulong dest, ulong data);
static void flash_get_offsets (ulong base, flash_info_t *info);
static int  flash_protect (int flag, ulong from, ulong to, flash_info_t *info);

/*-----------------------------------------------------------------------
 */

unsigned long flash_init (void)
{
	volatile immap_t     *immap  = (immap_t *)CFG_IMMR;
	volatile memctl8xx_t *memctl = &immap->im_memctl;
	unsigned long size_b0;
	int i;

	/* Init: no FLASHes known */
	for (i=0; i<CFG_MAX_FLASH_BANKS; ++i) {
		flash_info[i].flash_id = FLASH_UNKNOWN;
	}

	/* Static FLASH Bank configuration here - FIXME XXX */

	size_b0 = flash_get_size((vu_long *)FLASH_BASE0_PRELIM, &flash_info[0]);

	if (flash_info[0].flash_id == FLASH_UNKNOWN) {
		printf ("## Unknown FLASH on Bank 0 - Size = 0x%08lx = %ld MB\n",
			size_b0, size_b0<<20);
	}

	/* Remap FLASH according to real size */
	memctl->memc_or0 = CFG_OR_TIMING_FLASH | (-size_b0 & 0xFFFF8000);
	memctl->memc_br0 = (CFG_FLASH_BASE & BR_BA_MSK) | BR_MS_GPCM | BR_V;

	/* Re-do sizing to get full correct info */
	size_b0 = flash_get_size((vu_long *)CFG_FLASH_BASE, &flash_info[0]);
	flash_get_offsets (CFG_FLASH_BASE, &flash_info[0]);

	/* monitor protection ON by default */
	(void)flash_protect(FLAG_PROTECT_SET,
			    CFG_FLASH_BASE,
			    CFG_FLASH_BASE+CFG_MONITOR_LEN-1,
			    &flash_info[0]);

        flash_info[1].flash_id = FLASH_UNKNOWN;
	flash_info[1].sector_count = -1;

	flash_info[0].size = size_b0;
	flash_info[1].size = 0;

	return (size_b0);
}

/*-----------------------------------------------------------------------
 * Check or set protection status for monitor sectors
 *
 * The monitor always occupies the _first_ part of the _first_ Flash bank.
 */
static int  flash_protect (int flag, ulong from, ulong to, flash_info_t *info)
{
	ulong b_end = info->start[0] + info->size - 1;	/* bank end address */
	int rc    =  0;
	int first = -1;
	int last  = -1;
	int i;

	if (to < info->start[0]) {
		return (0);
	}

	for (i=0; i<info->sector_count; ++i) {
		ulong end;		/* last address in current sect	*/
		short s_end;

		s_end = info->sector_count - 1;

		end = (i == s_end) ? b_end : info->start[i + 1] - 1;

		if (from > end) {
			continue;
		}
		if (to < info->start[i]) {
			continue;
		}

		if (from == info->start[i]) {
			first = i;
			if (last < 0) {
				last = s_end;
			}
		}
		if (to  == end) {
			last  = i;
			if (first < 0) {
				first = 0;
			}
		}
	}

	for (i=first; i<=last; ++i) {
		if (flag & FLAG_PROTECT_CLEAR) {
			info->protect[i] = 0;
		} else if (flag & FLAG_PROTECT_SET) {
			info->protect[i] = 1;
		}
		if (info->protect[i]) {
			rc = 1;
		}
	}
	return (rc);
}


/*-----------------------------------------------------------------------
 */
static void flash_get_offsets (ulong base, flash_info_t *info)
{
	int i;

	/* set up sector start adress table */
	if (info->flash_id & FLASH_BTYPE) {
		/* set sector offsets for bottom boot block type	*/
		info->start[0] = base + 0x00000000;
		info->start[1] = base + 0x00008000;
		info->start[2] = base + 0x0000C000;
		info->start[3] = base + 0x00010000;
		for (i = 4; i < info->sector_count; i++) {
			info->start[i] = base + (i * 0x00020000) - 0x00060000;
		}
	} else {
		/* set sector offsets for top boot block type		*/
		i = info->sector_count - 1;
		info->start[i--] = base + info->size - 0x00008000;
		info->start[i--] = base + info->size - 0x0000C000;
		info->start[i--] = base + info->size - 0x00010000;
		for (; i >= 0; i--) {
			info->start[i] = base + i * 0x00020000;
		}
	}

}

/*-----------------------------------------------------------------------
 */
void flash_print_info  (flash_info_t *info)
{
	int i;

	if (info->flash_id == FLASH_UNKNOWN) {
		printf ("missing or unknown FLASH type\n");
		return;
	}

	switch (info->flash_id & FLASH_VENDMASK) {
	case FLASH_MAN_AMD:	printf ("AMD ");		break;
	case FLASH_MAN_FUJ:	printf ("FUJITSU ");		break;
	case FLASH_MAN_INTEL:   printf ("INTEL ");              break;
        default:		printf ("Unknown Vendor ");	break;
	}

	switch (info->flash_id & FLASH_TYPEMASK) {
	case FLASH_AM400B:	printf ("AM29LV400B (4 Mbit, bottom boot sect)\n");
				break;
	case FLASH_AM400T:	printf ("AM29LV400T (4 Mbit, top boot sector)\n");
				break;
	case FLASH_AM800B:	printf ("AM29LV800B (8 Mbit, bottom boot sect)\n");
				break;
	case FLASH_AM800T:	printf ("AM29LV800T (8 Mbit, top boot sector)\n");
				break;
	case FLASH_AM160B:	printf ("AM29LV160B (16 Mbit, bottom boot sect)\n");
				break;
	case FLASH_AM160T:	printf ("AM29LV160T (16 Mbit, top boot sector)\n");
				break;
	case FLASH_AM320B:	printf ("AM29LV320B (32 Mbit, bottom boot sect)\n");
				break;
	case FLASH_AM320T:	printf ("AM29LV320T (32 Mbit, top boot sector)\n");
				break;
	case FLASH_INT800B:	printf ("28F800-B   (8 Mbit, bottom boot sect)\n");
				break;
	case FLASH_INT800T:	printf ("28F800-T   (8 Mbit, top boot sector)\n");
				break;
	case FLASH_INT160B:	printf ("28F160-B  (16 Mbit, bottom boot sect)\n");
				break;
	case FLASH_INT160T:	printf ("28F160-T  (16 Mbit, top boot sector)\n");
				break;
	case FLASH_INT320B:	printf ("28F320-B  (32 Mbit, bottom boot sect)\n");
				break;
	case FLASH_INT320T:	printf ("28F320-T  (32 Mbit, top boot sector)\n");
				break;
        default:		printf ("Unknown Chip Type\n");
				break;
	}

	printf ("  Size: %ld MB in %d Sectors\n",
		info->size >> 20, info->sector_count);

	printf ("  Sector Start Addresses:");
	for (i=0; i<info->sector_count; ++i) {
                if ((info->flash_id & FLASH_VENDMASK) == FLASH_MAN_INTEL)
                {
                	volatile unsigned long *addr = (volatile unsigned long *)info->start[i];
                        *addr=0x00900090;               /* read configuration */
                        info->protect[i]=addr[2];
                        *addr=0x00FF00FF;               /* read array */
                }
		if ((i % 5) == 0)
			printf ("\n   ");
		printf (" %08lX%s",
			info->start[i],
			info->protect[i] ? " (RO)" : "     "
		);
	}
	printf ("\n");
}

/*-----------------------------------------------------------------------
 */


/*-----------------------------------------------------------------------
 */

/*
 * The following code cannot be run from FLASH!
 */

static ulong flash_get_size (vu_long *addr, flash_info_t *info)
{
	short i;
	ulong value;
	ulong base = (ulong)addr;


	/* Write auto select command: read Manufacturer ID */
	addr[0x0555] = 0x00AA00AA;
	addr[0x02AA] = 0x00550055;
	addr[0x0555] = 0x00900090;

	value = addr[0];

	switch (value) {
	case AMD_MANUFACT:
		info->flash_id = FLASH_MAN_AMD;
		break;
	case FUJ_MANUFACT:
		info->flash_id = FLASH_MAN_FUJ;
		break;
        case INT_MANUFACT:
                info->flash_id = FLASH_MAN_INTEL;
                break;
	default:
		info->flash_id = FLASH_UNKNOWN;
		info->sector_count = 0;
		info->size = 0;
		return (0);			/* no or unknown flash	*/
	}

	value = addr[1];			/* device ID		*/

	switch (value) {
	case AMD_ID_LV400T:
		info->flash_id += FLASH_AM400T;
		info->sector_count = 11;
		info->size = 0x00100000;
		break;				/* => 1 MB		*/

	case AMD_ID_LV400B:
		info->flash_id += FLASH_AM400B;
		info->sector_count = 11;
		info->size = 0x00100000;
		break;				/* => 1 MB		*/

	case AMD_ID_LV800T:
		info->flash_id += FLASH_AM800T;
		info->sector_count = 19;
		info->size = 0x00200000;
		break;				/* => 2 MB		*/

	case AMD_ID_LV800B:
		info->flash_id += FLASH_AM800B;
		info->sector_count = 19;
		info->size = 0x00200000;
		break;				/* => 2 MB		*/

	case AMD_ID_LV160T:
		info->flash_id += FLASH_AM160T;
		info->sector_count = 35;
		info->size = 0x00400000;
		break;				/* => 4 MB		*/

	case AMD_ID_LV160B:
		info->flash_id += FLASH_AM160B;
		info->sector_count = 35;
		info->size = 0x00400000;
		break;				/* => 4 MB		*/
#if 0
	case AMD_ID_LV320T:
		info->flash_id += FLASH_AM320T;
		info->sector_count = 67;
		info->size = 0x00800000;
		break;				/* => 8 MB		*/

	case AMD_ID_LV320B:
		info->flash_id += FLASH_AM320B;
		info->sector_count = 67;
		info->size = 0x00800000;
		break;				/* => 8 MB		*/
#endif
        case INT_ID_28F800B:
                info->flash_id |= FLASH_BTYPE;
        case INT_ID_28F800T:
                info->flash_id += FLASH_INT800T;
                info->sector_count = 15+4;
                info->size = 0x00200000;
                break;
        case INT_ID_28F160B:
                info->flash_id |= FLASH_BTYPE;
        case INT_ID_28F160T:
                info->flash_id += FLASH_INT160T;
                info->sector_count = 31+4;
                info->size = 0x00400000;
                break;
        case INT_ID_28F320B:
                info->flash_id |= FLASH_BTYPE;
        case INT_ID_28F320T:
                info->flash_id += FLASH_INT320T;
                info->sector_count = 63+4;
                info->size = 0x00800000;
                break;
        case INT_ID_28F640B:
                info->flash_id |= FLASH_BTYPE;
        case INT_ID_28F640T:
                info->flash_id += FLASH_INT640T;
                info->sector_count = 127+4;
                info->size = 0x00800000;
                break;
	default:
		info->flash_id = FLASH_UNKNOWN;
		return (0);			/* => no or unknown flash */

	}

	/* set up sector start adress table */
	if (info->flash_id & FLASH_BTYPE) {
		/* set sector offsets for bottom boot block type	*/
       		info->start[0] = base + 0x00000000;
       		info->start[1] = base + 0x00004000;
       		info->start[2] = base + 0x0000C000;
      		info->start[3] = base + 0x00010000;
		for (i = 4; i < info->sector_count; i++) {
			info->start[i] = base + (i * 0x00020000) - 0x00060000;
		}
	} else {
		/* set sector offsets for top boot block type		*/
		i = info->sector_count - 1;
		info->start[i--] = base + info->size - 0x00008000;
		info->start[i--] = base + info->size - 0x0000C000;
		info->start[i--] = base + info->size - 0x00010000;
		for (; i >= 0; i--) {
			info->start[i] = base + i * 0x00020000;
		}
	}

	/* check for protected sectors */
	for (i = 0; i < info->sector_count; i++) {
		/* read sector protection at sector address, (A7 .. A0) = 0x02 */
		/* D0 = 1 if protected */
		addr = (volatile unsigned long *)(info->start[i]);
		info->protect[i] = addr[2] & 1;
	}

	/*
	 * Prevent writes to uninitialized FLASH.
	 */
	if (info->flash_id != FLASH_UNKNOWN) {
		addr = (volatile unsigned long *)info->start[0];

                if ((info->flash_id & FLASH_VENDMASK)==FLASH_MAN_INTEL)
                	addr[0] = 0x00FF00FF;	/* read array */
                else
        	        addr[0] = 0x00F000F0;	/* reset bank */
	}

	return (info->size);
}


/*-----------------------------------------------------------------------
 */

void	flash_erase (flash_info_t *info, int s_first, int s_last)
{
	vu_long *addr = (vu_long*)(info->start[0]);
	int flag, prot, sect, l_sect;
	ulong start, now, last;

	if ((s_first < 0) || (s_first > s_last)) {
		if (info->flash_id == FLASH_UNKNOWN) {
			printf ("- missing\n");
		} else {
			printf ("- no sectors to erase\n");
		}
		return;
	}

	if ((info->flash_id == FLASH_UNKNOWN) ||
	    ((info->flash_id > FLASH_AMD_COMP) && (info->flash_id & FLASH_VENDMASK)!=FLASH_MAN_INTEL)) {
		printf ("Can't erase unknown flash type %08lx - aborted\n",
			info->flash_id);
		return;
	}

	prot = 0;
	for (sect=s_first; sect<=s_last; ++sect) {
		if (info->protect[sect]) {
			prot++;
		}
	}

	if (prot) {
		printf ("- Warning: %d protected sectors will not be erased!\n",
			prot);
	} else {
		printf ("\n");
	}

	l_sect = -1;

	/* Disable interrupts which might cause a timeout here */

        if ((info->flash_id & FLASH_VENDMASK)!=FLASH_MAN_INTEL)
        {
        	flag = disable_interrupts();
        	addr[0x0555] = 0x00AA00AA;
	        addr[0x02AA] = 0x00550055;
        	addr[0x0555] = 0x00800080;
        	addr[0x0555] = 0x00AA00AA;
        	addr[0x02AA] = 0x00550055;

        	/* Start erase on unprotected sectors */
        	for (sect = s_first; sect<=s_last; sect++) {
        		if (info->protect[sect] == 0) {	/* not protected */
        			addr = (vu_long*)(info->start[sect]);
        			addr[0] = 0x00300030;
        			l_sect = sect;
        		}
        	}
        	/* re-enable interrupts if necessary */
	        if (flag)
		        enable_interrupts();

        } else
        {
                addr[0]=0x00500050;     /* clear status register */
        	/* Start erase on unprotected sectors */
        	for (sect = s_first; sect<=s_last; sect++) 
                {
                        printf("\r sector %d ...", sect);
        		if (info->protect[sect] == 0) 
        	        {
        			addr = (vu_long*)(info->start[sect]);
        			addr[0] = 0x00200020;           /* erase setup */
                                addr[0] = 0x00D000D0;           /* erase confirm */
        			l_sect = sect;
                                addr[0]=0x00700070;     /* read status register */
                                start = get_timer (0);
                        	last  = start;
                          	while ((addr[0] & 0x00800080) != 0x00800080) 
                                {
                       		        if ((now = get_timer(start)) > CFG_FLASH_ERASE_TOUT) 
                       		        {
               	        		        printf ("Timeout\n");
               		        	        return;
               		        	}
               		           		/* show that we're waiting */
               		                if ((now - last) > 1000) 
               		                {	/* every second */
                               			putc ('.');
                               			last = now;
                               		}

               		        }
                        }
        	}
        }


	/* wait at least 80us - let's wait 1 ms */
	udelay (1000);

	/*
	 * We wait for the last triggered sector
	 */
	if (l_sect < 0)
		goto DONE;

	start = get_timer (0);
	last  = start;
	addr = (vu_long*)(info->start[l_sect]);
        if ((info->flash_id & FLASH_VENDMASK)==FLASH_MAN_INTEL)
                addr[0]=0x00700070;     /* read status register */
       	while ((addr[0] & 0x00800080) != 0x00800080) 
        {
       		if ((now = get_timer(start)) > CFG_FLASH_ERASE_TOUT) 
                {
       			printf ("Timeout\n");
       			return;
       		}
       		/* show that we're waiting */
       		if ((now - last) > 1000) {	/* every second */
       			putc ('.');
       			last = now;
       		}
       	}

        if (addr[0] != 0x00800080)
        {
                if ((info->flash_id & FLASH_VENDMASK)==FLASH_MAN_INTEL)
                        printf("erase failed: %02x\n", addr[0]);
        }
DONE:
	/* reset to read mode */
	addr = (volatile unsigned long *)info->start[0];
        if ((info->flash_id & FLASH_VENDMASK)==FLASH_MAN_INTEL)
        	addr[0] = 0x00FF00FF;	/* read array */
        else
	        addr[0] = 0x00F000F0;	/* reset bank */

	printf (" done\n");
}

/*-----------------------------------------------------------------------
 */

flash_info_t *addr2info (ulong addr)
{
	flash_info_t *info;
	int i;

	for (i=0, info=&flash_info[0]; i<CFG_MAX_FLASH_BANKS; ++i, ++info) {
		if ((addr >= info->start[0]) &&
		    (addr < (info->start[0] + info->size)) ) {
			return (info);
		}
	}

	return (NULL);
}

/*-----------------------------------------------------------------------
 * Copy memory to flash.
 * Make sure all target addresses are within Flash bounds,
 * and no protected sectors are hit.
 * Returns:
 * 0 - OK
 * 1 - write timeout
 * 2 - Flash not erased
 * 4 - target range includes protected sectors
 * 8 - target address not in Flash memory
 */
int flash_write (uchar *src, ulong addr, ulong cnt)
{
	int i;
	ulong         end        = addr + cnt - 1;
	flash_info_t *info_first = addr2info (addr);
	flash_info_t *info_last  = addr2info (end );
	flash_info_t *info;

	if (cnt == 0) {
		return (0);
	}

	if (!info_first || !info_last) {
		return (8);
	}

	for (info = info_first; info <= info_last; ++info) {
		ulong b_end = info->start[0] + info->size;	/* bank end addr */
		short s_end = info->sector_count - 1;
		for (i=0; i<info->sector_count; ++i) {
			ulong e_addr = (i == s_end) ? b_end : info->start[i + 1];

			if ((end >= info->start[i]) && (addr < e_addr) &&
			    (info->protect[i] != 0) ) {
				return (4);
			}
		}
	}

	/* finally write data to flash */
	for (info = info_first; info <= info_last && cnt>0; ++info) {
		ulong len;
		
		len = info->start[0] + info->size - addr;
		if (len > cnt)
			len = cnt;
		if ((i = write_buff(info, src, addr, len)) != 0) {
			return (i);
		}
		cnt  -= len;
		addr += len;
		src  += len;
	}
	return (0);
}

/*-----------------------------------------------------------------------
 * Copy memory to flash, returns:
 * 0 - OK
 * 1 - write timeout
 * 2 - Flash not erased
 */

static int write_buff (flash_info_t *info, uchar *src, ulong addr, ulong cnt)
{
	ulong cp, wp, data;
	int i, l, rc;

	wp = (addr & ~3);	/* get lower word aligned address */

	/*
	 * handle unaligned start bytes
	 */
	if ((l = addr - wp) != 0) {
		data = 0;
		for (i=0, cp=wp; i<l; ++i, ++cp) {
			data = (data << 8) | (*(uchar *)cp);
		}
		for (; i<4 && cnt>0; ++i) {
			data = (data << 8) | *src++;
			--cnt;
			++cp;
		}
		for (; cnt==0 && i<4; ++i, ++cp) {
			data = (data << 8) | (*(uchar *)cp);
		}

		if ((rc = write_word(info, wp, data)) != 0) {
			return (rc);
		}
		wp += 4;
	}

	/*
	 * handle word aligned part
	 */
	while (cnt >= 4) {
		data = 0;
		for (i=0; i<4; ++i) {
			data = (data << 8) | *src++;
		}
		if ((rc = write_word(info, wp, data)) != 0) {
			return (rc);
		}
		wp  += 4;
		cnt -= 4;
	}

	if (cnt == 0) {
		return (0);
	}

	/*
	 * handle unaligned tail bytes
	 */
	data = 0;
	for (i=0, cp=wp; i<4 && cnt>0; ++i, ++cp) {
		data = (data << 8) | *src++;
		--cnt;
	}
	for (; i<4; ++i, ++cp) {
		data = (data << 8) | (*(uchar *)cp);
	}
	
	return (write_word(info, wp, data));
}

/*-----------------------------------------------------------------------
 * Write a word to Flash, returns:
 * 0 - OK
 * 1 - write timeout
 * 2 - Flash not erased
 */
static int write_word (flash_info_t *info, ulong dest, ulong data)
{
	vu_long *addr = (vu_long*)(info->start[0]);
	ulong start;
	int flag;

	/* Check if Flash is (sufficiently) erased */
	if ((*((vu_long *)dest) & data) != data) {
		return (2);
	}
	/* Disable interrupts which might cause a timeout here */
	flag = disable_interrupts();

        if ((info->flash_id & FLASH_VENDMASK)!=FLASH_MAN_INTEL)
        {
        	addr[0x0555] = 0x00AA00AA;
        	addr[0x02AA] = 0x00550055;
        	addr[0x0555] = 0x00A000A0;
        	*((vu_long *)dest) = data;
        } else
        {
        	*((vu_long *)dest) = 0x00400040;
        	*((vu_long *)dest) = data;
        }
	/* re-enable interrupts if necessary */
	if (flag)
		enable_interrupts();

        if ((info->flash_id & FLASH_VENDMASK)!=FLASH_MAN_INTEL)
        {
        	/* data polling for D7 */
        	start = get_timer (0);
        	while ((*((vu_long *)dest) & 0x00800080) != (data & 0x00800080)) {
        		if (get_timer(start) > CFG_FLASH_WRITE_TOUT) {
        			return (1);
        		}
        	}
        } else
        {
        	*((vu_long *)dest) = 0x00700070;        /* read status */
                start = get_timer (0);
                while ((addr[0]&0x00800080)!=0x00800080)
                        if (get_timer(start) > (CFG_FLASH_WRITE_TOUT*5))
                        {
                                addr[0]=0x00FF00FF;     /* read array */
                                return (1);
                        }
                if (addr[0]!=0x00800080)
                {
                        printf("flash error: status %x\n", addr[0]);
                        addr[0]=0x00FF00FF;
                        return (1);
                }
        }
        if ((info->flash_id & FLASH_VENDMASK)==FLASH_MAN_INTEL)
        {
                addr[0]=0x00FF00FF;     /* read array */
        }
    	return (0);
}

/*-----------------------------------------------------------------------
 */
