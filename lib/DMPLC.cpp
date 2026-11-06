#include "DMPLC.h"

///////////////// GenericPrgDeviceManager

uint64_t MakeKey(const DMIPAddress& ip, uint8_t prio)
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

void GenericPrgDeviceManager::DebugPriorityCache(
    std::vector<GenericPrgDevice>& devices)
{
    LOG_IF(
        __FUNCTION__,
        "===== PRIORITY CACHE DEBUG ====="
    );

    for (const auto& entry : _cachePriorityIndex)
    {
        const uint64_t key = entry.first;
        const auto& indices = entry.second;

        const uint32_t ipRaw =
            static_cast<uint32_t>(key >> 8);

        const uint8_t priority =
            static_cast<uint8_t>(key & 0xFF);

        const DMIPAddress ip(
            (ipRaw >> 24) & 0xFF,
            (ipRaw >> 16) & 0xFF,
            (ipRaw >> 8) & 0xFF,
            ipRaw & 0xFF
        );

        LOG_IF(
            __FUNCTION__,
            "Key=%llu | IP=%u.%u.%u.%u | Priority=%u | Devices=%u",
            static_cast<unsigned long long>(key),
            static_cast<unsigned>(ip[0]),
            static_cast<unsigned>(ip[1]),
            static_cast<unsigned>(ip[2]),
            static_cast<unsigned>(ip[3]),
            static_cast<unsigned>(priority),
            static_cast<unsigned>(indices.size())
        );

        for (const int index : indices)
        {
            if (index < 0 ||
                index >= static_cast<int>(devices.size()))
            {
                LOG_WF(
                    __FUNCTION__,
                    "Indice device non valido: %d",
                    index
                );

                continue;
            }

            const GenericPrgDevice& device =
                devices[static_cast<size_t>(index)];

            LOG_IF(
                __FUNCTION__,
                "Device[%d] | %s | Zona=%s | Installazione=%s | "
                "IP=%u.%u.%u.%u | Addr=%u | Priority=%u",
                index,
                device.GetName(),
                device.GetZone(),
                device.GetInstallationLocation(),
                static_cast<unsigned>(device.GetIp()[0]),
                static_cast<unsigned>(device.GetIp()[1]),
                static_cast<unsigned>(device.GetIp()[2]),
                static_cast<unsigned>(device.GetIp()[3]),
                static_cast<unsigned>(device.GetDeviceAddress()),
                static_cast<unsigned>(device.GetPriority())
            );
        }
    }

    LOG_IF(
        __FUNCTION__,
        "===== END PRIORITY CACHE DEBUG ====="
    );
}

const std::vector<int>& GenericPrgDeviceManager::GetDevicesByPriority(
    Priority priority,
    const DMIPAddress& ip) const
{
    static const std::vector<int> empty;

    const uint64_t key = MakeKey(
        ip,
        static_cast<uint8_t>(priority)
    );

    LOG_DF("CACHE::LOOKUP",
        "LOOKUP ip=%d.%d.%d.%d prio=%d key=%llu",
        ip[0], ip[1], ip[2], ip[3],
        static_cast<int>(priority),
        static_cast<unsigned long long>(key));

    const auto it = _cachePriorityIndex.find(key);

    if (it == _cachePriorityIndex.end())
        return empty;

    LOG_DF("CACHE::FOUND",
        "FOUND %d devices for key=%llu",
        static_cast<int>(it->second.size()),
        static_cast<unsigned long long>(key));

    for (int idx : it->second) {
        LOG_DF("CACHE::DEV",
            "  devIdx=%d", idx);
    }

    return it->second;
}



////////////////////////////////////////////////////////// GenericDevice
GenericPrgDevice::GenericPrgDevice(
    const char* name,
    DMIPAddress ip,
    unsigned int deviceAddress,
    std::vector<GenericPrgDeviceChannel> channels,
    std::vector<int> ioAreas,
    short errorCount,
    Priority priority,
    const char* zone,
    const char* installationLocation)
    : _ioAreas(
          std::move(
              ioAreas
          )
      ),
      _priority(
          priority
      ),
      _name(
          name
      ),
      _zone(
          zone
      ),
      _installationLocation(
          installationLocation
      ),
      _deviceAddress(
          deviceAddress
      ),
      _ip(
          ip
      ),
      _channels(
          std::move(
              channels
          )
      ),
      Error(
          errorCount,
          30000
      ),
      bank(
          0
      )
{
}

int GenericPrgDevice::GetArea(
    int channel,
    int address)
{
    if (_ioAreas.empty())
    {
        LOG_EF(
            __FUNCTION__,
            "EMPTY IO AREA: ch=%d addr=%d",
            channel,
            address
        );

        return -1;
    }


    if (channel < 0 ||
        channel >=
            static_cast<int>(
                _channels.size()
            ))
    {
        LOG_EF(
            __FUNCTION__,
            "Canale fuori range: "
            "device=%s channel=%d channels=%u",
            GetName(),
            channel,
            static_cast<unsigned>(
                _channels.size()
            )
        );

        return -1;
    }


    if (address < 0 ||
        address >=
            _channels[channel].items)
    {
        LOG_EF(
            __FUNCTION__,
            "Address fuori range: "
            "device=%s channel=%d "
            "address=%d items=%d",
            GetName(),
            channel,
            address,
            _channels[channel].items
        );

        return -1;
    }


    int index =
        address;


    for (int currentChannel = 0;
         currentChannel < channel;
         ++currentChannel)
    {
        index +=
            _channels[currentChannel].items;
    }


    if (index < 0 ||
        index >=
            static_cast<int>(
                _ioAreas.size()
            ))
    {
        LOG_EF(
            __FUNCTION__,
            "Indice area fuori range: "
            "device=%s channel=%d "
            "address=%d index=%d size=%u",
            GetName(),
            channel,
            address,
            index,
            static_cast<unsigned>(
                _ioAreas.size()
            )
        );

        return -1;
    }


    return _ioAreas[index];
}

const GenericPrgDevice::GenericPrgDeviceChannel& GenericPrgDevice::GetChannelInfo(int channel) const
{
    static const GenericPrgDeviceChannel invalidChannel = {
        NOTSET,
        Coil,
        0,
        0,
        0
    };

    if (channel >= 0 && channel < static_cast<int>(_channels.size()))
        return _channels[channel];

    return invalidChannel;
}


size_t GenericPrgDevice::GetChannelsSize() const
{
    return _channels.size();
}

Priority GenericPrgDevice::GetPriority() const
{
    return _priority;
}

const DMIPAddress& GenericPrgDevice::GetIp() const
{
    return _ip;
}

unsigned int GenericPrgDevice::GetDeviceAddress() const
{
    return _deviceAddress;
}

const char* GenericPrgDevice::GetName() const
{
    if (!_name ||
        _name[0] == '\0')
    {
        return "NO name Device";
    }

    return _name;
}

const char* GenericPrgDevice::GetZone() const
{
    if (!_zone ||
        _zone[0] == '\0')
    {
        return "Not set";
    }

    return _zone;
}

const char*
GenericPrgDevice::GetInstallationLocation() const
{
    if (!_installationLocation ||
        _installationLocation[0] == '\0')
    {
        return "Not set";
    }

    return _installationLocation;
}

bool GenericPrgDeviceManager::HasDevicesByPriority(
    Priority priority,
    const DMIPAddress& ip) const
{
    const uint64_t key =
        MakeKey(ip, static_cast<uint8_t>(priority));

    auto it = _cachePriorityIndex.find(key);

    return it != _cachePriorityIndex.end()
        && !it->second.empty();
}

bool GenericPrgDevice::FindChannelByArea(
    int area,
    int& channel,
    int& item)
{
    const int channelCount =
        static_cast<int>(
            GetChannelsSize()
        );


    for (int channelIndex = 0;
         channelIndex < channelCount;
         ++channelIndex)
    {
        const GenericPrgDeviceChannel& channelInfo =
            GetChannelInfo(
                channelIndex
            );


        for (int itemIndex = 0;
             itemIndex < channelInfo.items;
             ++itemIndex)
        {
            if (GetArea(
                    channelIndex,
                    itemIndex) != area)
            {
                continue;
            }


            channel =
                channelIndex;

            item =
                itemIndex;


            return true;
        }
    }


    return false;
}

GenericPrgDevice::structRead GenericPrgDevice::Read(
    DMPlatform::Modbus::TCPClient& cli,
    int channel,
    uint16_t* outBuffer,
    unsigned long now)
{
    structRead result{};
    result.ok = false;

    if (!outBuffer)
    {
        LOG_EF(__FUNCTION__,
            "outBuffer NULL | Device=%s | Zona=%s | Installazione=%s",
            GetName(),
            GetZone(),
            GetInstallationLocation());

        Error.Loop(true, now);
        return result;
    }

    if (channel < 0 ||
        channel >= static_cast<int>(_channels.size()))
    {
        LOG_EF(__FUNCTION__,
            "Canale fuori range | Device=%s | Zona=%s | Installazione=%s "
            "| IP=%u.%u.%u.%u | Addr=%u | Channel=%d | Channels=%u",
            GetName(),
            GetZone(),
            GetInstallationLocation(),
            static_cast<unsigned>(_ip[0]),
            static_cast<unsigned>(_ip[1]),
            static_cast<unsigned>(_ip[2]),
            static_cast<unsigned>(_ip[3]),
            static_cast<unsigned>(_deviceAddress),
            channel,
            static_cast<unsigned>(_channels.size()));

        Error.Loop(true, now);
        return result;
    }

    const GenericPrgDeviceChannel& channelInfo =
        _channels[channel];

    result.itemsPerCall =
        channelInfo.ItemsPerCall;

    if (channelInfo.items <= 0 ||
        channelInfo.ItemsPerCall <= 0)
    {
        LOG_EF(__FUNCTION__,
            "Configurazione canale non valida | Device=%s "
            "| Channel=%d | Items=%d | ItemsPerCall=%d",
            GetName(),
            channel,
            channelInfo.items,
            channelInfo.ItemsPerCall);

        Error.Loop(true, now);
        return result;
    }

    const int bankCount =
        (channelInfo.items + MAX_CALLS - 1) /
        MAX_CALLS;

    bank %= bankCount;

    const int jump =
        bank * MAX_CALLS;

    int startingAddr =
        channelInfo.startingAddr + jump;

    const int remaining =
        channelInfo.items - jump;

    result.startIndex =
        jump;

    result.items =
        remaining < MAX_CALLS
            ? remaining
            : MAX_CALLS;

    ++bank;

    int requestedItems =
        result.items;

    int readCount =
        0;

    switch (channelInfo.hwType)
    {
        case Hold:
        {
            if (Error.IsInError())
            {
                Error.Loop(true, now);
                return result;
            }

            requestedItems *=
                channelInfo.ItemsPerCall;

            readCount = cli.requestFrom(
                _deviceAddress,
                HOLDING_REGISTERS,
                startingAddr,
                requestedItems
            );

            if (readCount <= 0)
            {
                LOG_DF(__FUNCTION__,
                    "READ HOLD fallita | Device=%s | Zona=%s "
                    "| Installazione=%s | Errore=%s",
                    GetName(),
                    GetZone(),
                    GetInstallationLocation(),
                    cli.lastError());

                Error.Loop(true, now);
                return result;
            }

            for (int i = 0; i < readCount; ++i)
            {
                const long value =
                    cli.read();

                if (value < 0)
                {
                    LOG_DF(__FUNCTION__,
                        "READ HOLD valore non valido | Device=%s | Index=%d",
                        GetName(),
                        i);

                    Error.Loop(true, now);
                    return result;
                }

                outBuffer[i] =
                    static_cast<uint16_t>(value);
            }

            Error.Loop(false, now);
            result.ok = true;
            break;
        }

        case Input:
        {
            if (Error.IsInError())
            {
                Error.Loop(true, now);
                return result;
            }

            requestedItems *=
                channelInfo.ItemsPerCall;

            readCount = cli.requestFrom(
                _deviceAddress,
                INPUT_REGISTERS,
                startingAddr,
                requestedItems
            );

            if (readCount <= 0)
            {
                if (!Error.Loop(true, now))
                {
                    LOG_DF(__FUNCTION__,
                        "READ INPUT fallita | Device=%s | Zona=%s "
                        "| Installazione=%s | IP=%u.%u.%u.%u "
                        "| Addr=%u | Errore=%s",
                        GetName(),
                        GetZone(),
                        GetInstallationLocation(),
                        static_cast<unsigned>(_ip[0]),
                        static_cast<unsigned>(_ip[1]),
                        static_cast<unsigned>(_ip[2]),
                        static_cast<unsigned>(_ip[3]),
                        static_cast<unsigned>(_deviceAddress),
                        cli.lastError());
                }

                return result;
            }

            for (int i = 0; i < readCount; ++i)
            {
                const long value =
                    cli.read();

                if (value < 0)
                {
                    Error.Loop(true, now);
                    return result;
                }

                outBuffer[i] =
                    static_cast<uint16_t>(value);
            }

            Error.Loop(false, now);
            result.ok = true;
            break;
        }

        case Discrete:
        {
            if (Error.IsInError())
            {
                Error.Loop(true, now);
                return result;
            }

            readCount = cli.requestFrom(
                _deviceAddress,
                DISCRETE_INPUTS,
                startingAddr,
                requestedItems
            );

            if (readCount <= 0)
            {
                if (!Error.Loop(true, now))
                {
                    LOG_DF(__FUNCTION__,
                        "READ DISCRETE fallita | Device=%s | Zona=%s "
                        "| Installazione=%s | IP=%u.%u.%u.%u "
                        "| Addr=%u | Errore=%s",
                        GetName(),
                        GetZone(),
                        GetInstallationLocation(),
                        static_cast<unsigned>(_ip[0]),
                        static_cast<unsigned>(_ip[1]),
                        static_cast<unsigned>(_ip[2]),
                        static_cast<unsigned>(_ip[3]),
                        static_cast<unsigned>(_deviceAddress),
                        cli.lastError());
                }

                return result;
            }

            for (int i = 0; i < readCount; ++i)
            {
                const long value =
                    cli.read();

                if (value < 0)
                {
                    Error.Loop(true, now);
                    return result;
                }

                outBuffer[i] =
                    static_cast<uint16_t>(value);
            }

            Error.Loop(false, now);
            result.ok = true;
            break;
        }

        default:
        {
            LOG_EF(__FUNCTION__,
                "Funzione READ sconosciuta | Device=%s "
                "| Zona=%s | Installazione=%s | HwType=%d",
                GetName(),
                GetZone(),
                GetInstallationLocation(),
                static_cast<int>(channelInfo.hwType));

            Error.Loop(true, now);
            break;
        }
    }

    return result;
}

bool GenericPrgDevice::Write(DMPlatform::Modbus::TCPClient &mb, int channel, int address, int value, unsigned long now) {
  if(this->_channels[channel].type==DO || this->_channels[channel].type==AO) {
    int tmpResult=0;

    /*LOG_DF("TIMING", "[WRITE_OK] dev=%s addr=%d area=%d time=%lu",
       this->_name, this->_deviceAddress, address, now);*/

    switch(this->_channels[channel].hwType) {
      case Hold: //Hold
        tmpResult=mb.holdingRegisterWrite(this->_deviceAddress, address, value);
        
        if(!this->Error.Loop(tmpResult==0, now)) {
          //LOG_EF("GenericPrgDevice", "ERROR (Write Holding): %s", this->_name); 
          return false;
        }

        return tmpResult!=0;
      break;

      case Coil:
        tmpResult=mb.coilWrite(this->_deviceAddress, address, value);
                
        if(!this->Error.Loop(tmpResult==0, now)) {
          //LOG_EF(__FUNCTION__, "ERROR (Write coil): name=%s DevAddr=%d Addr=%d Value=%d", this->_name, this->_deviceAddress, address, value); 
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

