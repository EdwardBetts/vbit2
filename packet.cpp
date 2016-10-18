#include "packet.h"


using namespace vbit;

Packet::Packet() : _isHeader(false), _mag(1)
{
    //ctor
}

Packet::Packet(char *val) : _isHeader(false), _mag(1), _row(99)
{
    //ctor
    strncpy(_packet,val,45+1);
}

Packet::Packet(std::string val) : _isHeader(false), _mag(1), _row(99)
{
    //ctor
    strncpy(_packet,val.c_str(),45+1);
}

Packet::Packet(int mag, int row, std::string val) : _isHeader(false), _mag(mag), _row(row)
{
	SetMRAG(mag, _row);
	SetPacketText(val);
	assert(_row!=0);
}

Packet::~Packet()
{
    //dtor
    strcpy(_packet,"This packet constructed with default txt");
}

void Packet::Set_packet(char *val)
{
    //std::cerr << "[Packet::Set_packet] todo. Implement copy" << std::endl;
    strncpy(&_packet[5],val,40);
}

void Packet::SetPacketText(std::string val)
{
	_isHeader=false; // Because it can't be a header
    strncpy(&_packet[5],val.c_str(),40);
}

// Clear entire packet to 0
void Packet::PacketQuiet()
{
	uint8_t i;
	for (i=0;i<PACKETSIZE;i++)
		_packet[i]=0;
}

// Set CRI and MRAG. Leave the rest of the packet alone
void Packet::SetMRAG(uint8_t mag, uint8_t row)
{
	char *p=_packet; // Remember that the bit order gets reversed later
	*p++=0x55; // clock run in
	*p++=0x55; // clock run in
	*p++=0x27; // framing code
	// add MRAG
	*p++=HamTab[mag%8+((row%2)<<3)]; // mag + bit 3 is the lowest bit of row
	*p++=HamTab[((row>>1)&0x0f)];
	_isHeader=row==0;
	_row=row;
	_mag=mag;
} // SetMRAG


/** get_offset_time. @todo Convert this to c++
 * Given a parameter of say %t+02
 * where str[2] is + or -
 * str[4:3] is a two digit of half hour offsets from local time (GMT/BST in our case)
 * @return Local time plus offset as 5 characters in the form 21:30
 */
bool Packet::get_offset_time(char* str)
{
	char strTime[6];
	// get the time (UTC I think)
	time_t rawtime;
	struct tm *info;
	time( &rawtime );

	// What is our offset in seconds?
	int offset=((str[3]-'0')*10+str[4]-'0')*30*60; // @todo We really ought to validate this
	
	// Is it negative (west of us?)
	if (str[2]=='-')
		offset=-offset;
	else
		if (str[2]!='+') return false; // Must be + or -
		
	// Add the offset to the time value
	rawtime+=offset;

	info = localtime( &rawtime );

	strftime(strTime, 21, "%H:%M", info);
	strncpy(str,strTime,5);
	return true; // @todo
}

/* Ideally we would set _packet[0] for other hardware, or _packet[3] for Alistair Buxton raspi-teletext/
 * but hard code this for now
 * Most of this should be rewritten for c++
 */
std::string Packet::tx(bool debugMode)
{
	// Get local time
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo=localtime(&rawtime);

	// @todo: Get these from the locale!
	const char *dayNames[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	const char *monthNames[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    // @TODO: parity
		if (_isHeader) // We can do header substitutions
		{
			// mpp page number. Use %%#
			char*	ptr2=strstr(_packet,"%%#");
			if (ptr2)
			{
				if (_mag==0)
					ptr2[0]='8';
				else
					ptr2[0]=_mag+'0';
				//std::cerr << "[Packet::tx]page=" << _page << std::endl;
				ptr2[1]=_page/0x10+'0';
				if (ptr2[1]>'9')
					ptr2[1]=ptr2[1]-'0'-10+'A'; 	// Particularly poor hex conversion algorithm
				//std::cerr << "[Packet::tx]ptr[1]=" << ptr2[1] << std::endl;

				ptr2[2]=_page%0x10+'0';
				if (ptr2[2]>'9')
					ptr2[2]=ptr2[2]-'0'-10+'A'; 	// Particularly poor hex conversion algorithm
				//std::cerr << "[Packet::tx]ptr[2]=" << ptr2[2] << std::endl;
			}

			// Day of week. Mon, Tue etc. Use %%a
			ptr2=strstr(_packet,"%%a");
			if (ptr2)
			{
				int day=timeinfo->tm_wday;
				ptr2[0]=dayNames[day][0];
				ptr2[1]=dayNames[day][1];
				ptr2[2]=dayNames[day][2];
			}

			// Date 2 digits, leading 0 %d
			ptr2=strstr(_packet,"%d");
			if (ptr2)
			{
				int date=timeinfo->tm_mday;
				ptr2[0]='0'+date/10;
				ptr2[1]='0'+date%10;
			}

			// Month: Three characters in locale format %%b
			ptr2=strstr(_packet,"%%b");
			if (ptr2)
			{
				int month=timeinfo->tm_mon;
				ptr2[0]=monthNames[month][0];
				ptr2[1]=monthNames[month][1];
				ptr2[2]=monthNames[month][2];
			}

				// @todo See buffer.c for additional codes that have not been implemented


			// Hardcode the time for now
			// std::cerr << "[Packet::tx]" << asctime(timeinfo) << std::endl;
			strncpy(&_packet[37],&asctime(timeinfo)[11],8);
			Parity(13); // redo the parity because substitutions will need processing

		}
		else if (_row<26) // Other text rows
		{
			char *tmpptr;
			// ======= TEMPERATURE ========
			#ifndef WIN32
			char strtemp[]="                    ";
			for (int i=5;i<45;i++) _packet[i]=_packet[i] & 0x7f;
			tmpptr=strstr((char*)_packet,"%%%T");
			if (tmpptr) {
				get_temp(strtemp);
				strncpy(tmpptr,strtemp,4);
			}
			#endif
			// ======= WORLD TIME ========
			// Special case for world time. Put %t<+|-><hh> to get local time HH:MM offset by +/- half hours
			for (;;)
			{
				tmpptr=strstr((char*) _packet,"%t+");
				if (!tmpptr) {
					tmpptr=strstr((char*) _packet,"%t-");
				}
				if (tmpptr) {
					//std::cout << "[test 1]" << _packet << std::endl;
					get_offset_time(tmpptr);
					//exit(4);
				}
				else
					break;
			}		
			// ======= NETWORK ========
			#ifndef WIN32
			// Special case for network address. Put %%%%%%%%%%%%%%n to get network address in form xxx.yyy.zzz.aaa with trailing spaces (15 characters total)
			tmpptr=strstr((char*)_packet,"%%%%%%%%%%%%%%n");
			if (tmpptr) {
				// strncpy(tmpptr,"not yet working",15);
				get_net(strtemp);
				strncpy(tmpptr,strtemp,15);
			}
			#endif		
			// ======= TIME AND DATE ========
			// Special case for system time. Put %%%%%%%%%%%%timedate to get time and date
			tmpptr=strstr((char*) _packet,"%%%%%%%%%%%%timedate");
			if (tmpptr) {
				get_time(strtemp);
				strncpy(tmpptr,strtemp,20);
			}			
			Parity(5); // redo the parity because substitutions will need processing
		}

    if (!debugMode)
    {
        // @todo drop off the first three characters for raspi-teletext
        return &_packet[3]; // For raspi-pi we skip clock run in and framing code
    }
    else
    {
        std::cout << std::setfill('0') <<  std::hex ;
        for (int i=0;i<45;i++)
            std::cout << std::setw(2)  << (int)(_packet[i] & 0xff) << " ";
        std::cout << std::dec << std::endl;
        for (int i=0;i<45;i++)
        {
            char ch=_packet[i] & 0x7f;
            if (ch<' ') ch='.';
            std::cout << " " << ch << " ";
        }
        // std::cout << std::endl;
        return &_packet[3];
    }

}


/** A header has mag, row=0, page, flags, caption and time
 */
void Packet::Header(unsigned char mag, unsigned char page, unsigned int subcode, unsigned int control)
{
	uint8_t cbit;
	SetMRAG(mag,0);
	_packet[5]=HamTab[page%0x10];
	_packet[6]=HamTab[page/0x10];
	_packet[7]=HamTab[(subcode&0x0f)]; // S1
	subcode>>=4;
	// Map the page settings control bits from MiniTED to actual teletext packet.
	// To find the MiniTED settings look at the tti format document.
	// To find the target bit position these are in reverse order to tx and not hammed.
	// So for each bit in ETSI document, just divide the bit number by 2 to find the target location.
	// Where ETSI says bit 8,6,4,2 this maps to 4,3,2,1 (where the bits are numbered 1 to 8)
	cbit=0;
	if (control & 0x4000) cbit=0x08;	// C4 Erase page
	_packet[8]=HamTab[(subcode&0x07) | cbit]; // S2 add C4
	subcode>>=3;
	_packet[9]=HamTab[(subcode&0x0f)]; // S3
	subcode>>=4;
	cbit=0;
	// Not sure if these bits are reversed. C5 and C6 are indistinguishable
	if (control & 0x0002) cbit=0x08;	// C6 Subtitle
	if (control & 0x0001) cbit|=0x04;	// C5 Newsflash

	// cbit|=0x08; // TEMPORARY!
 	_packet[10]=HamTab[(subcode&0x03) | cbit]; // S4 C6, C5
	cbit=0;
	if (control & 0x0004)  cbit=0x01;	// C7 Suppress Header TODO: Check if these should be reverse order
	if (control & 0x0008) cbit|=0x02;	// C8 Update
	if (control & 0x0010) cbit|=0x04;	// C9 Interrupted sequence
	if (control & 0x0020) cbit|=0x08;	// C10 Inhibit display

	_packet[11]=HamTab[cbit]; // C7 to C10
	cbit=(control & 0x0380) >> 6;	// Shift the language bits C12,C13,C14. TODO: Check if C12/C14 need swapping. CHECKED OK.
	// if (control & 0x0040) cbit|=0x01;	// C11 serial/parallel *** We only work in parallel mode, Serial would mean a different packet ordering.
	_packet[12]=HamTab[cbit]; // C11 to C14 (C11=0 is parallel, C12,C13,C14 language)

	_page=page;
} // Header

void Packet::HeaderText(std::string val)
{
	_isHeader=true; // Because it must be a header
  strncpy(&_packet[13],val.c_str(),32);
}

/**
 * @brief Check parity.
 * \param Offset is normally 5 for rows, 13 for header
 */
void Packet::Parity(uint8_t offset)
{
	int i;
	//uint8_t c;
	for (i=offset;i<PACKETSIZE;i++)
	{
		_packet[i]=ParTab[(uint8_t)(_packet[i]&0x7f)];
	}
	/* DO NOT REVERSE for Raspi. Other devices may need byte endian reversed
	for (i=0;i<PACKETSIZE;i++)
	{
		c=(uint8_t)_packet[i];
		c = (c & 0x0F) << 4 | (c & 0xF0) >> 4;
		c = (c & 0x33) << 2 | (c & 0xCC) >> 2;
		c = (c & 0x55) << 1 | (c & 0xAA) >> 1;
		_packet[i]=(char)c;
	}
	*/
} // Parity

void Packet::Fastext(int* links, int mag)
{
	unsigned long nLink;
	// add the designation code
	char *p;
	uint8_t i;
	p=_packet+5;
	*p++=HamTab[0];	// Designation code 0
	mag&=0x07;		// Mask the mag just in case. Keep it valid
	
	// add the link control byte. This will allow row 24 to show.
	_packet[42]=HamTab[0x0f];

	// and the page CRC
	_packet[43]=HamTab[0];	// Don't know how to calculate this.
	_packet[44]=HamTab[0];

	// for each of the six links
	for (i=0; i<6; i++)
	{
		nLink=links[i];
		if (nLink == 0) nLink = 0x8ff; // turn zero into 8FF to be ignored
		
		// calculate the relative magazine
		char cRelMag=(nLink/0x100 ^ mag);
		*p++=HamTab[nLink & 0xF];			// page units
		*p++=HamTab[(nLink & 0xF0) >> 4];	// page tens
		*p++=HamTab[0xF];									// subcode S1
		*p++=HamTab[((cRelMag & 1) << 3) | 7];
		*p++=HamTab[0xF];
		*p++=HamTab[((cRelMag & 6) << 1) | 3];
	}	
}

#ifndef WIN32
/** get_temp
 *  Pinched from raspi-teletext demo.c
 * @return Four character temperature in degrees C eg. "45.7"
 */
bool Packet::get_temp(char* str)
{
    FILE *fp;
    char *pch;
		char tmp[100];

    fp = popen("/usr/bin/vcgencmd measure_temp", "r");
    fgets(tmp, 99, fp);
    pclose(fp);
    pch = strtok (tmp,"=\n");
    pch = strtok (NULL,"=\n");
		strncpy(str,pch,5);
		return true; // @todo
}
#endif


/** get_time
 *  Pinched from raspi-teletext demo.c
 * @return Time as 20 characters
 */
bool Packet::get_time(char* str)
{
    time_t rawtime;
    struct tm *info;

    time( &rawtime );

    info = localtime( &rawtime );

    strftime(str, 21, "\x02%a %d %b\x03%H:%M/%S", info);
		return false; // @todo
}

#ifndef WIN32
/** get_net
 *  Pinched from raspi-teletext demo.c
 * @return network address as 20 characters
 * Sample response
 * 3: wlan0    inet 192.168.1.14/24 brd 192.168.1.255 scope global wlan0\       valid_lft forever preferred_lft forever
 */
bool Packet::get_net(char* str)
{
	FILE *fp;
	char *pch;

	int n;
	char temp[100];
	fp = popen("/sbin/ip -o -f inet addr show scope global", "r");
	fgets(temp, 99, fp);
	pclose(fp);
	pch = strtok (temp," \n/");
	for (n=1; n<4; n++)
	{
			pch = strtok (NULL, " \n/");
	}
	// If we don't have a connection established, try not to crash
	if (pch==NULL)
	{
		strcpy(str,"IP address????");
		return false;
	}
	strncpy(str,pch,15);
	return true; // @todo
}
#endif