/** PageList
 */
#include "pagelist.h"

using namespace ttx;

PageList::PageList(Configure *configure) :
	_configure(configure)
{
  for (int i=0;i<8;i++)
      _mag[i]=nullptr;
	if (_configure==nullptr)
	{
		std::cerr << "NULL configuration object" << std::endl;
		return;
	}
	LoadPageList(_configure->GetPageDirectory());
}

PageList::~PageList()
{
	// std::cerr << "[PageList] Destructor" << std::endl;
}

int PageList::LoadPageList(std::string filepath)
{
	// std::cerr << "[PageList::LoadPageList] Loading pages from " << filepath << std::endl;
	// Open the directory
	DIR *dp;
	TTXPageStream* q;
	struct dirent *dirp;
	if ((dp = opendir(filepath.c_str())) == NULL)
  {
		std::cerr << "Error(" << errno << ") opening " << filepath << std::endl;
		return errno;
	}
	// Load the filenames into a list
	while ((dirp = readdir(dp)) != NULL)
	{
		//p=new TTXPageStream(filepath+"/"+dirp->d_name);
    if (std::string(dirp->d_name).find(".tti") != std::string::npos)	// Is the file type .tti or ttix?
    {
      q=new TTXPageStream(filepath+"/"+dirp->d_name);
      // If the page loaded, then push it into the appropriate magazine
      if (q->Loaded())
      {
					q->GetPageCount(); // Use for the side effect of renumbering the subcodes

          int mag=(q->GetPageNumber() >> 16) & 0x7;
          _pageList[mag].push_back(*q); // This copies. But we can't copy a mutex
      }
    }
	}
	closedir(dp);
  // std::cerr << "[PageList::LoadPageList]FINISHED LOADING PAGES" << std::endl;

	// How many files did we accept?
	for (int i=0;i<8;i++)
  {
    //std::cerr << "Page list count[" << i << "]=" << _pageList[i].size() << std::endl;
    // Initialise a magazine streamer with a page list
/*
    std::list<TTXPageStream> pageSet;
    pageSet=_pageList[i];
    _mag[i]=new vbit::Mag(pageSet);
*/
    _mag[i]=new vbit::Mag(i, &_pageList[i], _configure);
  }
  // Just for testing
  if (1) for (int i=0;i<8;i++)
  {
    vbit::Mag* m=_mag[i];
    std::list<TTXPageStream>* p=m->Get_pageSet();
    for (std::list<TTXPageStream>::const_iterator it=p->begin();it!=p->end();++it)
    {
      if (&(*it)==NULL)
          std::cerr << "[PageList::LoadPageList] This can't happen unless something is broken" << std::endl;
       // std::cerr << "[PageList::LoadPageList]Dumping :" << std::endl;
       // it->DebugDump();
       std::cerr << "[PageList::LoadPageList] mag["<<i<<"] Filename =" << it->GetSourcePage()  << std::endl;
    }
  }

	return 0;
}

void PageList::AddPage(TTXPageStream* page)
{
		int mag=(page->GetPageNumber() >> 16) & 0x7;
		_pageList[mag].push_back(*page);
}

TTXPageStream* PageList::Locate(std::string filename)
{
  // This is called from the FileMonitor thread
  // std::cerr << "[PageList::Locate] *** TODO *** " << filename << std::endl;
  for (int mag=0;mag<8;mag++)
  {
    //for (auto p : _pageList[mag])
    for (std::list<TTXPageStream>::iterator p=_pageList[mag].begin();p!=_pageList[mag].end();++p)
    {
      TTXPageStream* ptr;
      ptr=&(*p);
      // std::cerr << "[PageList::Locate]scan:" << ptr->GetSourcePage() << std::endl;
      if (filename==ptr->GetSourcePage())
        return ptr;
    }

  }
 return NULL; // @todo placeholder What should we do here?
}

// Detect pages that have been deleted from the drive
// Do this by first clearing all the "exists" flags
// As we scan through the list, set the "exists" flag as we match up the drive to the loaded page

void PageList::ClearFlags()
{
  for (int mag=0;mag<8;mag++)
  {
    for (std::list<TTXPageStream>::iterator p=_pageList[mag].begin();p!=_pageList[mag].end();++p)
    {
      TTXPageStream* ptr;
      ptr=&(*p);
      // Don't unmark a file that was MARKED. Once condemned it won't be pardoned
      if (ptr->GetStatusFlag()==TTXPageStream::FOUND)
      {
        ptr->SetState(TTXPageStream::NOTFOUND);
      }
    }
  }
}

void PageList::DeleteOldPages()
{
  // This is called from the FileMonitor thread
  for (int mag=0;mag<8;mag++)
  {
    for (std::list<TTXPageStream>::iterator p=_pageList[mag].begin();p!=_pageList[mag].end();++p)
    {
      TTXPageStream* ptr;
      ptr=&(*p);
      if (ptr->GetStatusFlag()==TTXPageStream::NOTFOUND)
      {
				// std::cerr << "[PageList::DeleteOldPages] Marked for Delete " << ptr->GetSourcePage() << std::endl;
				// Pages marked here get deleted in the Service thread
        ptr->SetState(TTXPageStream::MARKED);
      }
    }
  }
}


/* Want this to happen in the Service thread.
      // Not the best idea, to check for deletes here
      if (_fileToDelete==ptr->GetSourcePage()) // This works but isn't great. Probably not too efficient
      {
        std::cerr << "[PageList::Locate]ready to delete " << ptr->GetSourcePage() << std::endl;
        _fileToDelete="null"; // Signal that delete has been done.
        //@todo Delete the page object, remove it from pagelist, fixup mag, skip to the next page
        _pageList[mag].remove(*(p++)); // Also post-increment the iterator OR WE CRASH!
        // We can do this safely because this thread is in the Service thread and won't clash WRONG!
      }

*/
