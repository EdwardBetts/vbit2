#ifndef _SERVICE_H_
#define _SERVICE_H_

#include <iostream>
#include <iomanip>
#include <thread>
#include <ctime>
#include <list>

#include "configure.h"
#include "pagelist.h"
#include "packet.h"
#include "mag.h" // @todo THIS WILL BE REDUNDANT
#include "packetsource.h"
#include "packetmag.h"

/// Eight magazines and subtitles (maybe other packets too)
#define STREAMS 9
/// Strictly these should ODD/EVEN VBI line counts as in general they would be different.
#define LINESPERFIELD 16

namespace ttx
{

/** A Service creates a teletext stream from packet sources.
 *  Packet sources are magazines, subtitles, Packet 830 and databroadcast.
 *  Service:
 *    Instances the packet sources
 *    Sends them timing events (cues for field timing etc.)
 *    Polls the packet sources for packets to send
 *    Sends the packets.
 *
 */
class Service
{
public:
	Service();
	/**
	 * @param configure A Configure object with all the settings
	 * @param pageList A pageList object already loaded with pages
	 */
	Service(Configure* configure, PageList* pageList);
	~Service();
	/**
	 * Creates a worker thread and does not terminate (at least for now)
	 * @return Nothing useful yet. Perhaps return an error status if something goes wrong
	 */
	int run();
private:
  // Member variables that define the service
	Configure* _configure; /// Member reference to the configuration settings
	PageList* _pageList; /// Member reference to the pages list

	// Member variables for event management
	uint8_t _lineCounter; // Which VBI line are we on? Used to signal a new field.
	uint8_t _fieldCounter; // Which field? Used to time packet 8/30
	std::list<vbit::PacketSource*> _Sources; /// A list of packet sources

	// Member functions
	void _register(vbit::PacketSource *src); /// Register packet sources

  /**
   * @brief Check if anything changed, and if so signal the event to the packet sources.
   * Must be called once per transmitted row so that it can maintain a field count
   */
	void _updateEvents();

};

}

#endif
