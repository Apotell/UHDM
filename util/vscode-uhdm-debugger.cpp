/**
 * vscode-uhdm-debugger: UHDM Debugger Information Generator
 *
 * Reads a UHDM database and writes a JSON file containing
 * per-file coverage ranges for use by the VS Code extension.
 *
 * Usage: uhdm-locmap <path-to-uhdm-file> <output-json-file>
 *
 * Output: JSON to the specified output file in schema v1 format
 */

#include <uhdm/uhdm.h>
#include <uhdm/vpi_uhdm.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

struct Range {
  int startLine;
  int startCol;
  int endLine;
  int endCol;

  Range(int sl, int sc, int el, int ec) : startLine(sl), startCol(sc), endLine(el), endCol(ec) {}

  bool operator<(const Range& other) const {
    if (startLine != other.startLine) return startLine < other.startLine;
    if (startCol != other.startCol) return startCol < other.startCol;
    if (endLine != other.endLine) return endLine < other.endLine;
    return endCol < other.endCol;
  }
};

static std::string jsonEscape(const std::string& s) {
  std::string result;
  result.reserve(s.size() + 10);
  for (char c : s) {
    switch (c) {
      case '"': result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default: result += c;
    }
  }
  return result;
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <path-to-uhdm-file> <output-json-file>\n";
    return 1;
  }

  const std::string uhdmPath = argv[1];
  const std::string outputPath = argv[2];

  uhdm::Serializer serializer;
  std::vector<vpiHandle> designs;

  try {
    designs = serializer.restore(uhdmPath);
  } catch (const std::exception& e) {
    std::cerr << "Error restoring UHDM database: " << e.what() << "\n";
    return 1;
  }

  if (designs.empty()) {
    std::cerr << "No designs found in UHDM database\n";
    return 1;
  }

  std::map<std::string, std::vector<Range>> fileRanges;
  int objectsTotal = 0;

  const auto idMap = serializer.getAllObjects();
  for (const auto& entry : idMap) {
    objectsTotal++;
    const auto* obj = entry.first;

    const uhdm::BaseClass* baseObj = static_cast<const uhdm::BaseClass*>(obj);
    if (!baseObj) continue;

    std::string_view fileNameView = baseObj->getFile();
    if (fileNameView.empty()) continue;
    std::string fileName(fileNameView);

    int startLine = baseObj->getStartLine();
    int startCol = baseObj->getStartColumn();
    int endLine = baseObj->getEndLine();
    int endCol = baseObj->getEndColumn();

    if (startLine <= 0 || startCol <= 0 || endLine <= 0 || endCol <= 0) continue;
    if (startLine != endLine) continue;
    if (endCol < startCol) continue;

    fileRanges[fileName].emplace_back(startLine, startCol, endLine, endCol);
  }

  std::ofstream out(outputPath, std::ios::binary);
  if (!out.is_open()) {
    std::cerr << "Failed to open output file for writing: " << outputPath << "\n";
    return 1;
  }

  out << "{\n";
  out << "  \"schemaVersion\": 1,\n";
  out << "  \"uhdmPath\": \"" << jsonEscape(uhdmPath) << "\",\n";
  out << "  \"stats\": {\n";
  out << "    \"objectsTotal\": " << objectsTotal << ",\n";
  out << "    \"filesTotal\": " << fileRanges.size() << "\n";
  out << "  },\n";
  out << "  \"files\": [\n";

  bool firstFile = true;
  for (auto& [fileName, ranges] : fileRanges) {
    if (!firstFile) out << ",\n";
    firstFile = false;

    std::sort(ranges.begin(), ranges.end());

    out << "    {\n";
    out << "      \"path\": \"" << jsonEscape(fileName) << "\",\n";
    out << "      \"ranges\": [";

    bool firstRange = true;
    for (const auto& r : ranges) {
      if (!firstRange) out << ", ";
      firstRange = false;
      out << "[" << r.startLine << ", " << r.startCol << ", " << r.endLine << ", " << r.endCol << "]";
    }

    out << "]\n";
    out << "    }";
  }

  out << "\n  ]\n";
  out << "}\n";

  out.flush();
  if (!out) {
    std::cerr << "Failed while writing output file: " << outputPath << "\n";
    return 1;
  }

  return 0;
}
