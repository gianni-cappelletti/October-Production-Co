#include "PluginTtl.hpp"

#include <array>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "PortTable.hpp"
#include "Uris.hpp"

namespace octob::lv2
{

namespace
{

constexpr const char* HomepageUri = "https://github.com/gianni-cappelletti/October-Production-Co";
constexpr const char* Brand = "OPC";
constexpr const char* Label = "OctoBASS";
constexpr const char* Comment =
    "Bass amp and tone shaper: graphic EQ, crossover split, compressed low band, two serial NAM "
    "slots and a cabinet IR on the high band. NAM Quality below 50% runs the slim model; 50% and "
    "above runs the full-size model, which is likely too heavy for a MOD Dwarf.";

struct GroupInfo
{
  PortGroup group;
  const char* symbol;
  const char* label;
};

constexpr std::array<GroupInfo, 4> Groups{{
    {PortGroup::Main, "main", "Main"},
    {PortGroup::Equalizer, "eq", "EQ"},
    {PortGroup::Nam1, "nam1", "NAM 1"},
    {PortGroup::Nam2, "nam2", "NAM 2"},
}};

std::string number(float value)
{
  std::array<char, 32> text{};
  std::snprintf(text.data(), text.size(), "%.6g", static_cast<double>(value));
  std::string result(text.data());
  if (result.find_first_of(".e") == std::string::npos)
    result += ".0";
  return result;
}

std::string quoted(const std::string& text)
{
  return "\"" + text + "\"";
}

std::string uriRef(const std::string& uri)
{
  return "<" + uri + ">";
}

std::string groupUri(PortGroup group)
{
  for (const auto& info : Groups)
    if (info.group == group)
      return uriRef(std::string(PluginUri) + "#" + info.symbol);
  return {};
}

std::string customUnit(const std::string& label, const std::string& symbol,
                       const std::string& render)
{
  return "[\n            a units:Unit ;\n            rdfs:label " + quoted(label) +
         " ;\n            units:symbol " + quoted(symbol) + " ;\n            units:render " +
         quoted(render) + "\n        ]";
}

std::string unitTurtle(const PortSpec& spec)
{
  const std::string precision = "%." + std::to_string(spec.displayDecimals) + "f";
  switch (spec.unit)
  {
    case PortUnit::Hertz:
      return customUnit("hertz", "Hz", precision + " Hz");
    case PortUnit::Decibels:
      return customUnit("decibels", "dB", precision + " dB");
    case PortUnit::DecibelsU:
      return customUnit("decibels unloaded", "dBu", precision + " dBu");
    case PortUnit::Percent:
      return customUnit("percent", "%", precision + "%%");
    case PortUnit::Frames:
      return "units:frame";
    case PortUnit::None:
      break;
  }
  return {};
}

std::vector<std::string> portTypes(PortKind kind)
{
  switch (kind)
  {
    case PortKind::AudioInput:
      return {"lv2:InputPort", "lv2:AudioPort"};
    case PortKind::AudioOutput:
      return {"lv2:OutputPort", "lv2:AudioPort"};
    case PortKind::AtomInput:
      return {"lv2:InputPort", "atom:AtomPort"};
    case PortKind::AtomOutput:
      return {"lv2:OutputPort", "atom:AtomPort"};
    case PortKind::ControlInput:
      return {"lv2:InputPort", "lv2:ControlPort"};
    case PortKind::ControlOutput:
      return {"lv2:OutputPort", "lv2:ControlPort"};
  }
  return {};
}

std::string join(const std::vector<std::string>& parts, const std::string& separator)
{
  std::string result;
  for (size_t i = 0; i < parts.size(); ++i)
    result += (i == 0 ? "" : separator) + parts[i];
  return result;
}

std::string scalePointsTurtle(const PortSpec& spec)
{
  std::vector<std::string> points;
  for (const auto& point : spec.scalePoints)
    points.push_back("[\n            rdfs:label " + quoted(point.label) +
                     " ;\n            rdf:value " + number(point.value) + "\n        ]");
  return "lv2:scalePoint " + join(points, " , ");
}

std::string portTurtle(const PortSpec& spec)
{
  std::vector<std::string> statements;
  statements.push_back("a " + join(portTypes(spec.kind), ", "));
  statements.push_back("lv2:index " + std::to_string(spec.index));
  statements.push_back("lv2:symbol " + quoted(spec.symbol));
  statements.push_back("lv2:name " + quoted(spec.name));

  const bool isAtom = spec.kind == PortKind::AtomInput || spec.kind == PortKind::AtomOutput;
  if (isAtom)
  {
    statements.push_back("atom:bufferType atom:Sequence");
    statements.push_back("atom:supports patch:Message");
  }

  switch (spec.designation)
  {
    case PortDesignation::Control:
      statements.push_back("lv2:designation lv2:control");
      break;
    case PortDesignation::Latency:
      statements.push_back("lv2:designation lv2:latency");
      statements.push_back("lv2:portProperty lv2:reportsLatency, lv2:integer, pprop:notOnGUI");
      break;
    case PortDesignation::Enabled:
      statements.push_back("lv2:designation lv2:enabled");
      break;
    case PortDesignation::None:
      break;
  }

  const bool isControl =
      spec.kind == PortKind::ControlInput || spec.kind == PortKind::ControlOutput;
  if (isControl)
  {
    statements.push_back("lv2:default " + number(spec.defaultValue));
    statements.push_back("lv2:minimum " + number(spec.minimum));
    statements.push_back("lv2:maximum " + number(spec.maximum));
  }

  if (const auto unit = unitTurtle(spec); !unit.empty())
    statements.push_back("units:unit " + unit);

  switch (spec.style)
  {
    case PortStyle::Logarithmic:
      statements.push_back("lv2:portProperty pprop:logarithmic");
      break;
    case PortStyle::Toggle:
      statements.push_back("lv2:portProperty lv2:toggled");
      break;
    case PortStyle::Enumeration:
      statements.push_back("lv2:portProperty lv2:integer, lv2:enumeration");
      break;
    case PortStyle::Linear:
      break;
  }

  if (!spec.scalePoints.empty())
    statements.push_back(scalePointsTurtle(spec));

  if (spec.group != PortGroup::None)
    statements.push_back("pg:group " + groupUri(spec.group));

  return "[\n        " + join(statements, " ;\n        ") + "\n    ]";
}

}  // namespace

std::string generatePluginTtl()
{
  std::ostringstream ttl;
  ttl << "@prefix atom:  <http://lv2plug.in/ns/ext/atom#> .\n"
         "@prefix bufsz: <http://lv2plug.in/ns/ext/buf-size#> .\n"
         "@prefix doap:  <http://usefulinc.com/ns/doap#> .\n"
         "@prefix foaf:  <http://xmlns.com/foaf/0.1/> .\n"
         "@prefix log:   <http://lv2plug.in/ns/ext/log#> .\n"
         "@prefix lv2:   <http://lv2plug.in/ns/lv2core#> .\n"
         "@prefix mod:   <http://moddevices.com/ns/mod#> .\n"
         "@prefix opts:  <http://lv2plug.in/ns/ext/options#> .\n"
         "@prefix patch: <http://lv2plug.in/ns/ext/patch#> .\n"
         "@prefix pg:    <http://lv2plug.in/ns/ext/port-groups#> .\n"
         "@prefix pprop: <http://lv2plug.in/ns/ext/port-props#> .\n"
         "@prefix rdf:   <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n"
         "@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#> .\n"
         "@prefix state: <http://lv2plug.in/ns/ext/state#> .\n"
         "@prefix units: <http://lv2plug.in/ns/extensions/units#> .\n"
         "@prefix urid:  <http://lv2plug.in/ns/ext/urid#> .\n"
         "@prefix work:  <http://lv2plug.in/ns/ext/worker#> .\n\n";

  for (const auto& parameter : FileParameters)
    ttl << uriRef(parameter.uri) << "\n    a lv2:Parameter ;\n    rdfs:label "
        << quoted(parameter.label) << " ;\n    rdfs:range atom:Path ;\n    mod:fileTypes "
        << quoted(parameter.modFileTypes) << " .\n\n";

  for (const auto& group : Groups)
    ttl << groupUri(group.group) << "\n    a pg:Group ;\n    lv2:symbol " << quoted(group.symbol)
        << " ;\n    rdfs:label " << quoted(group.label) << " .\n\n";

  std::vector<std::string> writable;
  for (const auto& parameter : FileParameters)
    writable.push_back(uriRef(parameter.uri));

  std::vector<std::string> ports;
  for (const auto& spec : portTable())
    ports.push_back(portTurtle(spec));

  ttl << uriRef(PluginUri) << "\n"
      << "    a lv2:Plugin, lv2:SimulatorPlugin ;\n"
      << "    doap:name " << quoted(Label) << " ;\n"
      << "    doap:license <http://spdx.org/licenses/GPL-3.0-only> ;\n"
      << "    doap:homepage " << uriRef(HomepageUri) << " ;\n"
      << "    doap:maintainer [\n"
      << "        foaf:name \"October Production Co.\" ;\n"
      << "        foaf:homepage " << uriRef(HomepageUri) << "\n"
      << "    ] ;\n"
      << "    mod:brand " << quoted(Brand) << " ;\n"
      << "    mod:label " << quoted(Label) << " ;\n"
      << "    rdfs:comment " << quoted(Comment) << " ;\n"
      << "    lv2:requiredFeature urid:map, work:schedule ;\n"
      << "    lv2:optionalFeature lv2:hardRTCapable, log:log, opts:options, "
         "bufsz:boundedBlockLength, state:threadSafeRestore ;\n"
      << "    lv2:extensionData work:interface, state:interface ;\n"
      << "    patch:writable " << join(writable, " , ") << " ;\n"
      << "    lv2:port " << join(ports, " , ") << " .\n";

  return ttl.str();
}

std::string generatePresetsTtl()
{
  std::vector<std::string> values;
  for (const auto& spec : portTable())
    if (spec.kind == PortKind::ControlInput && spec.designation != PortDesignation::Enabled)
      values.push_back("[\n        lv2:symbol " + quoted(spec.symbol) + " ;\n        pset:value " +
                       number(spec.defaultValue) + "\n    ]");

  std::ostringstream ttl;
  ttl << "@prefix lv2:  <http://lv2plug.in/ns/lv2core#> .\n"
         "@prefix pset: <http://lv2plug.in/ns/ext/presets#> .\n"
         "@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n\n"
      << uriRef(std::string(PluginUri) + "#preset-default") << "\n"
      << "    a pset:Preset ;\n"
      << "    lv2:appliesTo " << uriRef(PluginUri) << " ;\n"
      << "    rdfs:label \"Default\" ;\n"
      << "    lv2:port " << join(values, " , ") << " .\n";
  return ttl.str();
}

}  // namespace octob::lv2
