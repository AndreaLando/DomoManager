\# ============================================================

\# DIAGRAMMA ASCII DELLE DIPENDENZE DEI FILE

\# Partenza: domomanager.ino

\# ============================================================



domomanager.ino

│

├── #include "DMUsb.hpp"

│   ├── DMLogger.hpp

│   ├── DMFrontend.hpp

│   ├── DMDeclares.h

│   └── (dipendenze minori interne)

│

├── #include "DMFrontend.hpp"

│   ├── DMFrontendEngines.hpp

│   │   ├── DMHVAC.hpp

│   │   │   ├── DMHVAC.cpp

│   │   │   ├── DMDeclares.h

│   │   │   ├── DMLogger.hpp

│   │   │   └── DMFncs.hpp

│   │   │

│   │   ├── DMPower.hpp

│   │   │   ├── DMDeclares.h

│   │   │   ├── DMLogger.hpp

│   │   │   └── DMFncs.hpp

│   │   │

│   │   ├── DMWeather.hpp

│   │   │   ├── DMDeclares.h

│   │   │   ├── DMLogger.hpp

│   │   │   └── DMFncs.hpp

│   │   │

│   │   ├── DMWiredSensors.hpp

│   │   │   ├── DMDeclares.h

│   │   │   ├── DMLogger.hpp

│   │   │   └── DMFncs.hpp

│   │   │

│   │   ├── DMMQTTEngine.hpp

│   │   │   ├── DMDeclares.h

│   │   │   ├── DMLogger.hpp

│   │   │   └── DMFncs.hpp

│   │   │

│   │   ├── DMWebAPI.hpp

│   │   │   ├── DMWebAPIDefs.hpp

│   │   │   ├── DMDeclares.h

│   │   │   ├── DMLogger.hpp

│   │   │   └── DMFncs.hpp

│   │   │

│   │   ├── DMRS485Node.hpp

│   │   │   ├── DMDeclares.h

│   │   │   ├── DMLogger.hpp

│   │   │   └── DMFncs.hpp

│   │   │

│   │   └── DMAdapters.hpp

│   │       ├── DMDeclares.h

│   │       ├── DMLogger.hpp

│   │       └── DMFncs.hpp

│   │

│   ├── DMAutomation.hpp

│   │   ├── DMAutomationBuilder.hpp

│   │   │   ├── DMDeclares.h

│   │   │   ├── DMLogger.hpp

│   │   │   └── DMFncs.hpp

│   │   │

│   │   ├── DMDeclares.h

│   │   ├── DMLogger.hpp

│   │   └── DMFncs.hpp

│   │

│   ├── DMBridge.hpp

│   │   ├── DMLogger.hpp

│   │   ├── DMDeclares.h

│   │   └── DMFncs.hpp

│   │

│   ├── DMPLC.hpp

│   │   ├── MgsModbus.hpp

│   │   │   ├── Ethernet.h

│   │   │   ├── Arduino.h

│   │   │   └── DMLogger.hpp

│   │   │

│   │   ├── DMDeclares.h

│   │   ├── DMLogger.hpp

│   │   └── DMFncs.hpp

│   │

│   ├── DMEquipment.hpp

│   │   ├── DMDeclares.h

│   │   ├── DMLogger.hpp

│   │   └── DMFncs.hpp

│   │

│   ├── DMBuffers.hpp

│   │   ├── DMBuffers.cpp

│   │   ├── DMDeclares.h

│   │   ├── DMLogger.hpp

│   │   └── DMFncs.hpp

│   │

│   ├── DMWeather.hpp

│   ├── DMSetup.hpp

│   ├── DMOptaRTC.hpp

│   ├── DMDiagnostic.hpp

│   ├── DMFncs.hpp

│   ├── DMLogger.hpp

│   └── DMDeclares.h

│

├── #include "DMLogger.hpp"

│   ├── DMDeclares.h

│   └── Arduino.h

│

└── (Altre dipendenze implicite)

&#x20;   ├── Arduino core

&#x20;   ├── Ethernet

&#x20;   ├── Wire

&#x20;   ├── SPI

&#x20;   └── librerie Opta/Arduino



