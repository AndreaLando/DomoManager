#include "DMBuffers.h"

Buffer::Buffer(unsigned int items)
    : tracker(items)
{
    this->_items = items;
    this->_buffer = new BufferInfo[items];

    for (int i = 0; i < (int)_items; i++) {
        auto &info = _buffer[i];

        // azzera i dati
        for (int t = 0; t < BufferFlagType_Count; t++) {
            info.exists[t] = false;
            info.Data[t] = { (BufferFlagType)t, 0, 0, 0 };
        }

        info.Reverse       = false;
        info.ReadFromPanel = false;
        info.WriteToPanel  = false;
        info.areaToWrite   = -1;
        info.name          = String();
        info.isVirtual     = false;
        info.wasRead       = false;
        info.wasWritten    = false;
    }
}


void Buffer::SetElement(int area, int areaToWrite, bool WriteToPanel, bool ReadFromPanel, bool Reverse, const char* name) {
  tracker.registerInit(area);

    // --- PATCH SICURA ---
    if (name == nullptr || name[0] == '\0') {
        static char autoName[32];
        snprintf(autoName, sizeof(autoName), "Auto_%d", area);
        this->_buffer[area].name = autoName;
    } else {
        this->_buffer[area].name = name;
    }
    // ---------------------

  this->_buffer[area].WriteToPanel=WriteToPanel;
  this->_buffer[area].ReadFromPanel=ReadFromPanel;
  this->_buffer[area].Reverse=Reverse; //Negato
  this->_buffer[area].areaToWrite=areaToWrite;
}

void Buffer::AddType(int area, long initialValue, BufferFlagType type) {
    auto &info = _buffer[area];

    info.Data[type] = {
        .bufferType = type,
        .time = millis(),
        .value = initialValue,
        .prevValue = 0
    };

    info.exists[type] = true;
    markChanged(area, type);
}


void Buffer::Init() {
  int idxPnl=0;
  for(int i=0; i<this->_items; i++) {
    if(this->_buffer[i].ReadFromPanel)
      idxPnl++;
  }

  this->_toPanelRead.itemsPtr=new int [idxPnl];
  this->_toPanelRead.size=idxPnl;
  
  idxPnl=0;
  for(int i=0; i<this->_items; i++) {
    if(this->_buffer[i].ReadFromPanel) {
      this->_toPanelRead.itemsPtr[idxPnl]=i;
      idxPnl++;
    }
  }
}

int Buffer::Compare(int area, BufferFlagType type, long value) {
    auto &info = _buffer[area];

    if (!info.exists[type]) return 0;
    return (info.Data[type].value == value) ? 2 : 1;
}


bool Buffer::WriteElement(int area, BufferFlagType type, long value, unsigned long now) {
  return WriteElement(area,type,value,false, now);
}

inline void Buffer::markChanged(int area, BufferFlagType type) {
    if (!isValidArea(area)) {
        LOG_WF("Buffer", "markChanged ignorato: area=%d fuori range", area);
        return;
    }

    if (type < 0 || type >= BufferFlagType_Count) {
        LOG_WF("Buffer", "markChanged ignorato: type=%d fuori range", (int)type);
        return;
    }

  int key = area * BufferFlagType_Count + type;  
  // Inserisce o sovrascrive
  _changed[key] = { area, type };

   LOG_DF("Buffer",
               "Area %d markChanged (type=%d)",
               area, type);
}


bool Buffer::WriteElement(int area,
                                BufferFlagType type,
                                long value,
                                bool silent,
                                unsigned long now) {
    // 🔥 Airbag: area non inizializzata
    if (!isValidArea(area)) {
        LOG_WFN("Buffer", "WriteElement ignorato: area %d fuori range", area);
        return false;
    }

    auto &info = _buffer[area];
    // 🔥 Se l’area NON è inizializzata → creiamo una "safe area"
    if (info.name == nullptr) {
        info.name = (char*)"AutoCreated";
        info.ReadFromPanel = false;
        info.WriteToPanel = false;
        info.Reverse = false;
        info.areaToWrite = -1;
        info.isVirtual = false;
        info.wasRead = false;
        info.wasWritten = false;

        LOG_DF("Buffer",
               "Area %d auto‑creata (nome=AutoCreated, type=%d)",
               area, type);
    }
    
    if (!info.exists[type]) {
        // crea nuovo
        info.Data[type] = { type, now, value, 0 };
        info.exists[type] = true;
        if (!silent) markChanged(area, type);
        _buffer[area].wasWritten = true;

        LOG_DF("Buffer",
               "Area %d nuova (nome=AutoCreated, type=%d)",
               area, type);
        return true;
    }

    auto &entry = info.Data[type];
    if (entry.value != value) {
        entry.prevValue = entry.value;
        entry.value = value;
        entry.time = now;
        if (!silent) markChanged(area, type);
        _buffer[area].wasWritten = true;

        LOG_DF("Buffer",
               "Area %d valore aggiornato (nome=AutoCreated, type=%d)",
               area, type);
    }

    return true;
}

char* Buffer::GetName(int area) {
    // Area fuori range → nome sicuro
    if (!isValidArea(area))
        return (char*)"Unknown area";

    auto &info = _buffer[area];

    // Nome non inizializzato → rendilo sicuro
    if (!info.name.length()) {
        info.name = "Undefined area";
    }
    return (char*)info.name.c_str();
}


bool Buffer::GetData(int area, BufferFlagType type, BufferSourceInfo &out) {
    // 🔒 Airbag: area fuori range
    if (!isValidArea(area)) {
        LOG_WF("Buffer", "GetData ignorato: area %d fuori range", area);
        out = { type, 0, 0, 0 };
        return false;
    }

    auto &info = _buffer[area];
    if (!info.exists[type]) {
        out = { type, 0, 0, 0 };
        return true;
    }

    out = info.Data[type];
    _buffer[area].wasRead = true;
    return true;
}


void Buffer::ResetElement(int area, BufferFlagType type) {
    int key = area * BufferFlagType_Count + type;
    _changed.erase(key);
}

void Buffer::ResetArea(int area) {
    if (!isValidArea(area)) return;

    // Rimuovi tutti i changed relativi a quest’area
    for (int t = 0; t < BufferFlagType_Count; t++) {
        int key = area * BufferFlagType_Count + t;
        _changed.erase(key);
    }
}

void Buffer::ResetType(BufferFlagType type) {
    if (type < 0 || type >= BufferFlagType_Count)
        return;

    // Scorri tutte le aree
    for (int area = 0; area < _items; area++) {
        int key = area * BufferFlagType_Count + type;
        _changed.erase(key);
    }
}

void Buffer::ResetAll() {
    _changed.clear();
}

int Buffer::GetAreaToWrite(int area) {
    if (area < 0 || area >= this->_items)
        return -1;
    return this->_buffer[area].areaToWrite;   // può essere -1
}


bool Buffer::IsReverse(int area) {
  return this->_buffer[area].Reverse;
}

bool Buffer::CanReadFromPanel(int area) {
  return this->_buffer[area].ReadFromPanel;
}

bool Buffer::CanWriteToPanel(int area) {
  return this->_buffer[area].WriteToPanel;
}

bool Buffer::HasChanged(int area, BufferFlagType type) {
  int key = area * BufferFlagType_Count + type;
  return _changed.find(key) != _changed.end();
}

BufferArrayInfo Buffer::GetToReadFromPanel() {
  return this->_toPanelRead;
}

std::vector<int> Buffer::getNeverInitialized() {
  return tracker.getNeverInitialized();
}

std::vector<int> Buffer::getInitializedMultipleTimes() {
  return tracker.getInitializedMultipleTimes();
}

void Buffer::Diagnostics::ReportVirtualAreas(Buffer& buf) {
    Serial.println("\n===== AREE VIRTUALI =====");
    for (int area = 0; area < buf.size(); area++) {
        if (buf.IsVirtual(area)) {
            Serial.print("Area ");
            Serial.print(area);
            Serial.print(" → VIRTUAL (");
            Serial.print(buf.GetName(area));
            Serial.println(")");
        }
    }
}

void Buffer::Diagnostics::ReportUnusedBufferAreas(Buffer& buf) {
    Serial.println("\n===== AREE DEFINITE MA MAI USATE =====");

    for (int area = 0; area < buf.size(); area++) {
        if (buf.Exists(area) &&
            !buf.IsVirtual(area) &&
            !buf.WasEverRead(area) &&
            !buf.WasEverWritten(area))
        {
            Serial.print("Area ");
            Serial.print(area);
            Serial.print(" (");
            Serial.print(buf.GetName(area));
            Serial.println(") → MAI letta/scritta");
        }
    }
}

void Buffer::Diagnostics::ReportNeverInitialized(Buffer& buf) {
    auto never = buf.getNeverInitialized();

    Serial.println("\n===== AREE NON INIZIALIZZATE =====");

    if (never.empty()) {
        Serial.println("Nessuna area non inizializzata.");
        return;
    }

    for (int area : never) {
        Serial.print("Area ");
        Serial.print(area);
        Serial.print(" (");
        Serial.print(buf.GetName(area));
        Serial.println(")");
    }
}

void Buffer::Diagnostics::ReportMultipleInitialized(Buffer& buf) {
    auto multi = buf.getInitializedMultipleTimes();

    Serial.println("\n===== AREE INIZIALIZZATE PIÙ VOLTE =====");

    if (multi.empty()) {
        Serial.println("Nessuna area inizializzata più volte.");
        return;
    }

    for (int area : multi) {
        Serial.print("Area ");
        Serial.print(area);
        Serial.print(" (");
        Serial.print(buf.GetName(area));
        Serial.println(")");
    }
}

