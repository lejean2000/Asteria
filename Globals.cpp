#include"Globals.h"
#include<QStandardPaths>
#include<QApplication>

namespace AsteriaGlobals {
bool additionalBodiesEnabled = false;
QString lastGeneratedChartType = "Natal Birth";
QString appDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Asteria";
bool activeModelLoaded = false;

}
