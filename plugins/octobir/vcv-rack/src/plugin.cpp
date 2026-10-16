#include "plugin.hpp"

#include "IrBrowseFolders.hpp"

Plugin* pluginInstance;

void init(Plugin* p)
{
  pluginInstance = p;
  p->addModel(modelOpcVcvIr);
}

json_t* settingsToJson()
{
  return browseFoldersToJson();
}

void settingsFromJson(json_t* rootJ)
{
  browseFoldersFromJson(rootJ);
}
