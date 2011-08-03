#include "PLTEvent.h"


PLTEvent::PLTEvent ()
{
  // Default constructor
  fRun = 0;
}


PLTEvent::PLTEvent (std::string const DataFileName)
{
  // Constructor, but you won't have the gaincal data..
  fBinFile.Open(DataFileName);
  fRun = 0;
}


PLTEvent::PLTEvent (std::string const DataFileName, std::string const GainCalFileName)
{
  // Constructor, which will also give you access to the gaincal values
  fBinFile.Open(DataFileName);
  fGainCal.ReadGainCalFile(GainCalFileName);
  
  fRun = 0;
}


PLTEvent::PLTEvent (std::string const DataFileName, std::string const GainCalFileName, std::string const AlignmentFileName)
{
  // Constructor, which will also give you access to the gaincal values
  fBinFile.Open(DataFileName);
  fGainCal.ReadGainCalFile(GainCalFileName);
  fAlignment.ReadAlignmentFile(AlignmentFileName);
  
  fRun = 0;
}


PLTEvent::~PLTEvent ()
{
  // Destructor!!
  Clear();
};





size_t PLTEvent::NPlanes ()
{
  // Number of planes in the event
  return fPlanes.size();
}



PLTPlane* PLTEvent::Plane(size_t i)
{
  // Get a specific plane
  return fPlanes[i];
}



size_t PLTEvent::NTelescopes ()
{
  // Number of telescopes with at least one hit
  return fTelescopes.size();
}



PLTTelescope* PLTEvent::Telescope (size_t i)
{
  // get specific telescope
  return fTelescopes[i];
}



void PLTEvent::Clear ()
{
  // clear up.  All memory is owned by the maps and fHits, so
  // hopefully it is self managed and you don't have to delete anything.
  fTelescopeMap.clear();
  fPlaneMap.clear();

  // Clear vectors
  // You own the hits and need to delete them!!
  for (std::vector<PLTHit*>::iterator i = fHits.begin(); i != fHits.end(); ++i) {
    delete *i;
  }


  fHits.clear();
  fPlanes.clear();
  fTelescopes.clear();
}



void PLTEvent::AddHit (PLTHit& Hit)
{
  // This method DOES do a copy, so if you want speed for very large number of
  // hits this isn't your best choice.

  PLTHit* NewHit = new PLTHit(Hit);

  // If we have the GC object let's fill the charge
  if (fGainCal.IsGood()) {
    fGainCal.SetCharge(*NewHit);
  }

  // add the hit
  fHits.push_back(NewHit);
  return;
}



void PLTEvent::AddHit (PLTHit* Hit)
{
  // If you give it to me the I OWN it and I will delete it!!


  // If we have the GC object let's fill the charge
  if (fGainCal.IsGood()) {
    fGainCal.SetCharge(*Hit);
  }

  // add the hit
  fHits.push_back(Hit);
  return;
}



void PLTEvent::MakeEvent ()
{
  // This function organizes the "hits" into Planes, clusters, and telescopes

  // Add hits to planes according to their channel-roc
  for (std::vector<PLTHit*>::iterator it = fHits.begin(); it != fHits.end(); ++it) {
    std::pair<int, int> ChannelRoc = std::make_pair<int, int>((*it)->Channel(), (*it)->ROC());
    fPlaneMap[ChannelRoc].AddHit( *it );
  }

  for (std::map< std::pair<int, int>, PLTPlane>::iterator it = fPlaneMap.begin(); it != fPlaneMap.end(); ++it) {
    int const Channel = it->first.first;
    for (int i = 0; i != 3; ++i) {
      std::pair<int, int> ChROC = std::make_pair<int, int>(Channel, i);
      if (!fPlaneMap.count(ChROC)) {
        fPlaneMap[ ChROC ].SetChannel(Channel);
        fPlaneMap[ ChROC ].SetROC(i);
      }
    }
  }
  // Loop over all planes and clusterize each one, then add each plane to the correct telescope (by channel number
  for (std::map< std::pair<int, int>, PLTPlane>::iterator it = fPlaneMap.begin(); it != fPlaneMap.end(); ++it) {
    it->second.Clusterize(fGainCal.IsGood());
    fTelescopeMap[it->second.Channel()].AddPlane( &(it->second) );
  }

  // Just to make it easier.. put them in a vector..
  for (std::map<int, PLTTelescope>::iterator it = fTelescopeMap.begin(); it != fTelescopeMap.end(); ++it) {
    it->second.FillAndOrderTelescope();

    for (size_t i = 0; i != it->second.NPlanes(); ++i) {
      fPlanes.push_back( it->second.Plane(i));
    }
    fTelescopes.push_back( &(it->second) );
  }

  return;
}



int PLTEvent::GetNextEvent ()
{
  // First clear the event
  Clear();

  // The number we'll return.. number of hits, or -1 for end
  int ret = fBinFile.ReadEventHits(fHits, fEvent);
  if (ret < 0) {
    return ret;
  }

  static bool const DoLoop = fGainCal.IsGood() || fAlignment.IsGood();
  static bool const DoAlignment = fAlignment.IsGood();
  static bool const DoGainCal = fGainCal.IsGood();

  // If the GC is good let's compute the charge
  if (DoLoop) {
    for (std::vector<PLTHit*>::iterator it = fHits.begin(); it != fHits.end(); ++it) {
      if (DoGainCal) {
        fGainCal.SetCharge(**it);
        //std::cout << (*it)->Charge() << std::endl;
      }
      if (DoAlignment) {
        fAlignment.AlignHit(**it);
      }
    }
  }

  // Now make the event into some useful form
  MakeEvent();

  return ret;
}
  

