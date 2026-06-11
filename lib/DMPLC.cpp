#include "DMPLC.h"

///////////////// GenericPrgDeviceManager

uint64_t MakeKey(const arduino::IPAddress& ip, uint8_t prio)
{
    uint32_t ipKey =
        ((uint32_t)ip[0] << 24) |
        ((uint32_t)ip[1] << 16) |
        ((uint32_t)ip[2] << 8)  |
        ((uint32_t)ip[3]);

    return ((uint64_t)ipKey << 8) | prio;
}


void GenericPrgDeviceManager::BuildPriorityCache(std::vector<GenericPrgDevice>& devices) {
    if (_cacheBuilt) return;

    _cachePriorityIndex.clear();

    for (size_t i = 0; i < devices.size(); i++) {
        auto& dev = devices[i];

        uint64_t key = MakeKey(dev.GetIp(), (uint8_t)dev.GetPriority());
        _cachePriorityIndex[key].push_back(i);
    }

    _cacheBuilt = true;

    //DebugPriorityCache(devices);
}

void GenericPrgDeviceManager::DebugPriorityCache(std::vector<GenericPrgDevice>& devices)
{
    LOG_DF(__FUNCTION__, "=== Priority Cache Debug ===");

    for (auto& entry : _cachePriorityIndex) {
        uint64_t key = entry.first;
        auto& indices = entry.second;

        uint32_t ipRaw = (uint32_t)(key >> 8);
        uint8_t prio   = (uint8_t)(key & 0xFF);

        arduino::IPAddress ip(
            (ipRaw >> 24) & 0xFF,
            (ipRaw >> 16) & 0xFF,
            (ipRaw >> 8)  & 0xFF,
            ipRaw & 0xFF
        );

        LOG_DF(__FUNCTION__,
               "Key=%llu IP=%d.%d.%d.%d Priority=%d",
               (unsigned long long)key,
               ip[0], ip[1], ip[2], ip[3],
               prio);

        String idxList;
        for (int idx : indices) idxList += String(idx) + " ";
        LOG_DF(__FUNCTION__, "  Indici: %s", idxList.c_str());

        for (int idx : indices) {
            auto& dev = devices[idx];
            LOG_DF(__FUNCTION__,
                   "    -> Device[%d] IP=%d.%d.%d.%d Priority=%d",
                   idx,
                   dev.GetIp()[0], dev.GetIp()[1], dev.GetIp()[2], dev.GetIp()[3],
                   (uint8_t)dev.GetPriority());
        }
    }

    LOG_DF(__FUNCTION__, "=== Fine Debug ===");
}


int GenericPrgDeviceManager::GetDevicesByPriority(GenericPrgDevicePriority priority,
                                                  arduino::IPAddress ip,
                                                  std::vector<int>& out) {
    uint64_t key = MakeKey(ip, (uint8_t)priority);

    auto it = _cachePriorityIndex.find(key);
    if (it == _cachePriorityIndex.end())
        return 0;

    out = it->second;
    return out.size();
}



////////////////////////////////////////////////////////// GenericDevice
GenericPrgDevice::GenericPrgDevice(const char* name, arduino::IPAddress ip, unsigned int deviceAddress, std::vector<GenericPrgDeviceChannel> channels, std::vector<int> ioAreas, short ErrorCnt, GenericPrgDevicePriority priority): Error(ErrorCnt, 30000)
{ 
 this->_channels=channels;
 
  this->_ioAreas=ioAreas;

  this->_deviceAddress=deviceAddress;
  this->_name=name;
  
  this->_ip=ip;
  this->_priority=priority;
  this->bank=0; // nel caso di calls ripetute
}

int GenericPrgDevice::GetArea(int channel, int address) { 
    if (this->_ioAreas.empty()) { 
        LOG_EF(__FUNCTION__, "EMPTY IO AREA: ch=%d addr=%d", channel, address);
        return -1;
    }

    int size = this->_channels[0].items * channel;
    int index = size + address;

    if (index < 0 || index >= this->_ioAreas.size()) {
        LOG_EF(__FUNCTION__, ">>> ERRORE: index fuori range! <<<");
        return 0;
    }

    return this->_ioAreas[index];
}



GenericPrgDevice::GenericPrgDeviceChannel GenericPrgDevice::GetChannelInfo(int channel) { 
  if(channel<this->_channels.size())
    return this->_channels[channel];
  else { 
    GenericPrgDeviceChannel tmp;
    tmp.type=NOTSET;
    return tmp;
  }
}

bool GenericPrgDevice::IsInError()
{ 
  return this->Error.IsInError(); //this->_inError;
}

size_t GenericPrgDevice::GetChannelsSize()
{ 
  return this->_channels.size();
}

GenericPrgDevicePriority GenericPrgDevice::GetPriority()
{ 
  return this->_priority;
}

arduino::IPAddress GenericPrgDevice::GetIp()
{ 
  return this->_ip;
}

unsigned int GenericPrgDevice::GetDeviceAddress()
{ 
  return this->_deviceAddress;
}

const char* GenericPrgDevice::GetName()
{ 
  return this->_name;
}

GenericPrgDevice::structRead GenericPrgDevice::Read(ModbusTCPClient &cli,
                                                    int channel,
                                                    uint16_t* outBuffer,
                                                    unsigned long now) {
    structRead retVal;
    retVal.ok = false;
    retVal.itemsPerCall=this->_channels[channel].ItemsPerCall;

    if (channel >= this->_channels.size()) {
        LOG_EF(__FUNCTION__, "READ ERROR size<channel name=%s IP=%d.%d.%d.%d Addr=%d Size=%d Channel=%d", this->_name, this->_ip[0], this->_ip[1], this->_ip[2], this->_ip[3], this->GetDeviceAddress(), this->_channels.size(), channel);

        this->Error.Loop(true, now);
        return retVal;
    }

    //Gestione back di lettura
    int startingAddr = this->_channels[channel].startingAddr;

    // Calcolo numero di bank validi
    int bankCount = (this->_channels[channel].items + this->MAX_CALLS - 1) / this->MAX_CALLS;

    // Normalizza il bank (evita overflow e start fuori range)
    this->bank = this->bank % bankCount;

    // Calcola jump in base al bank
    int jump = this->bank * this->MAX_CALLS;

    // Imposta startIndex e indirizzo reale
    retVal.startIndex = jump;
    startingAddr = this->_channels[channel].startingAddr + jump;

    // Calcola quanti registri leggere in questo bank
    int remaining = this->_channels[channel].items - jump;
    retVal.items = (remaining < this->MAX_CALLS) ? remaining : this->MAX_CALLS;

    // Avanza il bank per la prossima chiamata
    this->bank++;


    int tmpRead = 0;
    switch (this->_channels[channel].hwType)
    {
        case Hold:
          if (!this->Error.IsInError()) {
            int toRead = retVal.items * this->_channels[channel].ItemsPerCall;
            LOG_DF("READ::Read",
              "dev=%s addr=%d func=%d start=%d toRead=%d bank=%d items=%d itemsPerCall=%d",
              this->_name,
              this->_deviceAddress,
              this->_channels[channel].hwType,
              startingAddr,
              retVal.items * this->_channels[channel].ItemsPerCall,
              this->bank,
              retVal.items,
              this->_channels[channel].ItemsPerCall);

            tmpRead = cli.requestFrom(this->_deviceAddress,
                                          HOLDING_REGISTERS,
                                          startingAddr,
                                          toRead);
            LOG_DF("READ", "start=%d toRead=%d tmpRead=%d", startingAddr, toRead, tmpRead);

            if (tmpRead > 0) {
                for (int i = 0; i < tmpRead; i++) {
                    long v = cli.read();
                    if (v < 0) {
                        retVal.ok = false;
                        this->Error.Loop(true, now);
                        return retVal;
                    }
                    outBuffer[i] = (uint16_t)v;
                }

                this->Error.Loop(false, now);
                retVal.ok = true;
            } 
            else  {
                this->Error.Loop(true, now);
                retVal.ok = false;
            }
          } 
          else {
              this->Error.Loop(true, now);
              retVal.ok = false;
          }

          break;

        case Input:
            if (!this->Error.IsInError()) {
              tmpRead = cli.requestFrom(this->_deviceAddress,
                                        INPUT_REGISTERS,
                                        startingAddr,
                                        retVal.items * this->_channels[channel].ItemsPerCall);

              if (tmpRead != 0) {
                  for (int i = 0; i < tmpRead; i++)
                      outBuffer[i] = cli.read();

                  this->Error.Loop(false, now);
                  retVal.ok = true;
              } else if(!this->Error.Loop(true, now)) {
                LOG_DF(__FUNCTION__, "ERROR (Input): name=%s IP=%d.%d.%d.%d Addr=%d", this->_name, this->_ip[0], this->_ip[1], this->_ip[2], this->_ip[3], this->_deviceAddress);
              }
            } else this->Error.Loop(true, now);
            break;

        case Discrete:
            if (!this->Error.IsInError()) {
                tmpRead = cli.requestFrom(this->_deviceAddress,
                                         DISCRETE_INPUTS,
                                         startingAddr,
                                         retVal.items);

                if (tmpRead != 0) {
                    for (int i = 0; i < tmpRead; i++)
                        outBuffer[i] = cli.read();

                    this->Error.Loop(false, now);
                    retVal.ok = true;
                } else if(!this->Error.Loop(true, now)) {
                  LOG_DF(__FUNCTION__, "ERROR (Discrete): name=%s IP=%d.%d.%d.%d Addr=%d", this->_name, this->_ip[0], this->_ip[1], this->_ip[2], this->_ip[3], this->_deviceAddress);
                }
            } else this->Error.Loop(true, now);
            break;

        default:
            LOG_EF(__FUNCTION__, "ERROR (Unknown read function): %s", this->_name);
            this->Error.Loop(true, now);
            break;
    }

    return retVal;
}


bool GenericPrgDevice::FindChannelByArea(int area, int &channel, int &item) {
    for (int ch = 0; ch < GetChannelsSize(); ch++) {
      auto items=GetChannelInfo(ch).items;
        for (int j = 0; j < items; j++) {
            if (GetArea(ch, j) == area) {
                channel = ch;
                item = j;
                return true;
            }
        }
    }
    return false;
}

bool GenericPrgDevice::Write(ModbusClient &mb, int channel, int address, int value, unsigned long now) {
  if(this->_channels[channel].type==DO || this->_channels[channel].type==AO) {
    int tmpResult=0;

    switch(this->_channels[channel].hwType) {
      case Hold: //Hold
        tmpResult=mb.holdingRegisterWrite(this->_deviceAddress, address, value);
        
        if(!this->Error.Loop(tmpResult==0, now)) {
          LOG_DF("GenericPrgDevice", "ERROR (Write Holding): %s", this->_name); 
          return false;
        }
        
        return tmpResult!=0;
      break;

      case Coil:
        tmpResult=mb.coilWrite(this->_deviceAddress, address, value);
                
        if(!this->Error.Loop(tmpResult==0, now)) {
          LOG_DF(__FUNCTION__, "ERROR (Write coil): name=%s DevAddr=%d Addr=%d Value=%d", this->_name, this->_deviceAddress, address, value); 
          return false;
        }
   
        return tmpResult!=0;
      break;

      default:
        LOG_EF("GenericPrgDevice", "ERROR (Unknown Write): name=%s IP=%d.%d.%d.%d Addr=%d", this->_name, this->_ip[0], this->_ip[1], this->_ip[2], this->_ip[3], this->GetDeviceAddress()); 
        return false;
      break;
    }
  }
  else {
    LOG_EF("GenericPrgDevice", "Write definition error: DevAddr=%d StartAddr=%d Value=%d Type=%d", this->_deviceAddress, this->_channels[channel].startingAddr, value, this->_channels[channel].type);

    return false;
  }
}


int GetJump(GenericPrgDevicePriority priority) {
  switch (priority)   {
      case Low:
        return 1;
        break;

      case Medium:
        return 2;
      break;
      
      case Normal:
        return 3;
      break;

      default:
        return 0;
    }

    return 0;
}


