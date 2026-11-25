// Peek.cpp - Implementação do VizuPeek
// PLANO-MOTORES-PROPRIOS - Motor 2: VizuPeek
#include "Peek.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <wincodec.h>
#include <shlwapi.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")

namespace engines {
namespace peek {

// ============================================================================
// PreviewExtractor base
// ============================================================================

bool PreviewExtractor::ReadHeader(const wchar_t* path, std::vector<uint8_t>& header, size_t maxBytes) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    file.seekg(0, std::ios::end);
    size_t fileSize = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);

    size_t toRead = std::min(maxBytes, fileSize);
    header.resize(toRead);
    file.read(reinterpret_cast<char*>(header.data()), toRead);
    return file.gcount() == (std::streamsize)toRead;
}

PreviewExtractorFactory::Format PreviewExtractorFactory::DetectFormat(const wchar_t* filePath) {
    std::vector<uint8_t> header;
    if (!ReadHeader(filePath, header, 512)) return UNKNOWN;

    return DetectFormatFromMemory(header.data(), header.size());
}

PreviewExtractorFactory::Format PreviewExtractorFactory::DetectFormatFromMemory(const uint8_t* data, size_t size) {
    if (size < 4) return UNKNOWN;

    // PSD/PSB: "8BPS"
    if (size >= 4 && data[0] == '8' && data[1] == 'B' && data[2] == 'P' && data[3] == 'S') {
        return PSD;
    }

    // PDF: "%PDF"
    if (size >= 4 && data[0] == '%' && data[1] == 'P' && data[2] == 'D' && data[3] == 'F') {
        // Poderia ser AI (que é PDF-based) - verificar depois
        return PDF;
    }

    // EPS: "%!PS"
    if (size >= 4 && data[0] == '%' && data[1] == '!' && data[2] == 'P' && data[3] == 'S') {
        return EPS;
    }

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (size >= 8 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
        return UNKNOWN; // PNG puro não é formato de origem
    }

    // JPEG: FF D8 FF
    if (size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return UNKNOWN;
    }

    // TIFF: II* (little-endian) ou MM* (big-endian)
    if (size >= 4 && ((data[0] == 'I' && data[1] == 'I' && data[2] == 0x2A && data[3] == 0x00) ||
                      (data[0] == 'M' && data[1] == 'M' && data[2] == 0x00 && data[3] == 0x2A))) {
        return UNKNOWN;
    }

    // Verificar se é AI (PDF com marcador Illustrator)
    if (size >= 100) {
        for (size_t i = 0; i < std::min(size - 20, (size_t)500); ++i) {
            if (data[i] == 'A' && data[i+1] == 'I' && data[i+2] == ' ' && data[i+3] == 'F') {
                return AI;
            }
            if (data[i] == 'I' && data[i+1] == 'l' && data[i+2] == 'l' && data[i+3] == 'u' &&
                data[i+4] == 's' && data[i+5] == 't' && data[i+6] == 'r' && data[i+7] == 'a') {
                return AI;
            }
        }
    }

    // Verificar INDD (InDesign) - geralmente ZIP-based com mimetype
    if (size >= 4 && data[0] == 0x50 && data[1] == 0x4B && data[2] == 0x03 && data[3] == 0x04) {
        // ZIP-based - procurar por "mimetypeapplication/vnd.adobe.indesign"
        for (size_t i = 0; i < std::min(size - 40, (size_t)2000); ++i) {
            if (i + 36 < size && memcmp(data + i, "mimetype", 8) == 0) {
                if (i + 44 < size && memcmp(data + i + 8, "application/vnd.adobe.indesign", 30) == 0) {
                    return INDD;
                }
            }
        }
        // Poderia ser Affinity ou Sketch também
        for (size_t i = 0; i < std::min(size - 20, (size_t)2000); ++i) {
            if (memcmp(data + i, "affinity", 8) == 0) return AFFINITY;
            if (memcmp(data + i, "sketch", 6) == 0) return SKETCH;
        }
    }

    // CDR - verificar via libcdr (formato complexo, deixar para CdrExtractor)
    // CDR files often start with specific signatures

    return UNKNOWN;
}

std::unique_ptr<PreviewExtractor> PreviewExtractorFactory::Create(Format fmt, const Config& cfg) {
    switch (fmt) {
        case PSD: return std::make_unique<PsdExtractor>(cfg);
        case AI: return std::make_unique<AiExtractor>(cfg);
        case INDD: return std::make_unique<InddExtractor>(cfg);
        case EPS: return std::make_unique<EpsExtractor>(cfg);
        case PDF: return std::make_unique<PdfExtractor>(cfg);
        case CDR: return std::make_unique<CdrExtractor>(cfg);
        default: return nullptr;
    }
}

std::unique_ptr<PreviewExtractor> PreviewExtractorFactory::CreateForFile(const wchar_t* filePath, const Config& cfg) {
    Format fmt = DetectFormat(filePath);
    if (fmt == UNKNOWN) return nullptr;
    return Create(fmt, cfg);
}

// ============================================================================
// PsdExtractor
// ============================================================================

PreviewResult PsdExtractor::Extract(const wchar_t* filePath) {
    std::vector<uint8_t> data;
    if (!ReadHeader(filePath, data, config_.maxReadBytes)) {
        return { {}, 0, 0, false, "", "Failed to read file" };
    }
    return ParsePsd(data.data(), data.size());
}

PreviewResult PsdExtractor::ExtractFromMemory(const uint8_t* data, size_t size, const char*) {
    return ParsePsd(data, size);
}

PreviewResult PsdExtractor::ParsePsd(const uint8_t* data, size_t size) {
    PreviewResult result;

    if (size < sizeof(PsdHeader)) {
        result.error = "File too small for PSD header";
        return result;
    }

    const PsdHeader* header = reinterpret_cast<const PsdHeader*>(data);

    // Verificar assinatura
    if (header->signature[0] != '8' || header->signature[1] != 'B' ||
        header->signature[2] != 'P' || header->signature[3] != 'S') {
        result.error = "Invalid PSD signature";
        return result;
    }

    // PSD = version 1, PSB = version 2
    if (header->version != 1 && header->version != 2) {
        result.error = "Unsupported PSD version";
        return result;
    }

    // Pular header (26 bytes) + color mode data + image resources
    const uint8_t* ptr = data + sizeof(PsdHeader);

    // Color mode data section
    if (ptr + 4 > data + size) { result.error = "Truncated color mode data"; return result; }
    uint32_t colorModeDataLen = *(uint32_t*)ptr;
    // Big-endian
    colorModeDataLen = ((colorModeDataLen >> 24) & 0xFF) | ((colorModeDataLen >> 8) & 0xFF00) |
                       ((colorModeDataLen << 8) & 0xFF0000) | ((colorModeDataLen << 24) & 0xFF000000);
    ptr += 4 + colorModeDataLen;
    if (ptr > data + size) { result.error = "Truncated color mode data"; return result; }

    // Image resources section
    if (ptr + 4 > data + size) { result.error = "Truncated image resources"; return result; }
    uint32_t imageResourcesLen = *(uint32_t*)ptr;
    imageResourcesLen = ((imageResourcesLen >> 24) & 0xFF) | ((imageResourcesLen >> 8) & 0xFF00) |
                        ((imageResourcesLen << 8) & 0xFF0000) | ((imageResourcesLen << 24) & 0xFF000000);
    ptr += 4;

    const uint8_t* resEnd = ptr + imageResourcesLen;
    if (resEnd > data + size) { result.error = "Truncated image resources"; return result; }

    // Iterar resources
    while (ptr + 12 <= resEnd) {
        // Signature "8BIM"
        if (ptr[0] != '8' || ptr[1] != 'B' || ptr[2] != 'I' || ptr[3] != 'M') {
            break;
        }

        uint16_t resId = *(uint16_t*)(ptr + 4);
        resId = (resId >> 8) | (resId << 8); // big-endian

        // Pascal string para nome (padded to even)
        uint8_t nameLen = ptr[6];
        size_t nameOffset = 7 + nameLen;
        if (nameOffset % 2) nameOffset++; // pad to even
        ptr += nameOffset;

        if (ptr + 4 > resEnd) break;

        uint32_t dataSize = *(uint32_t*)ptr;
        dataSize = ((dataSize >> 24) & 0xFF) | ((dataSize >> 8) & 0xFF00) |
                   ((dataSize << 8) & 0xFF0000) | ((dataSize << 24) & 0xFF000000);
        ptr += 4;

        if (ptr + dataSize > resEnd) break;

        // Resource 1036 = Thumbnail (JPEG)
        if (resId == 1036) {
            return ExtractThumbnailFromResource(ptr, dataSize);
        }

        // Pular dados + padding para even
        ptr += dataSize;
        if (dataSize % 2) ptr++;
    }

    result.error = "No thumbnail resource (1036) found";
    return result;
}

PreviewResult PsdExtractor::ExtractThumbnailFromResource(const uint8_t* resData, size_t resSize) {
    PreviewResult result;

    // Resource 1036 format:
    // uint16_t format (0=JPEG, 1=PNG? - usually JPEG)
    // uint16_t width
    // uint16_t height
    // uint32_t dataSize (big-endian)
    // data...

    if (resSize < 8) {
        result.error = "Thumbnail resource too small";
        return result;
    }

    uint16_t format = *(uint16_t*)resData;
    format = (format >> 8) | (format << 8);

    uint16_t width = *(uint16_t*)(resData + 2);
    width = (width >> 8) | (width << 8);

    uint16_t height = *(uint16_t*)(resData + 4);
    height = (height >> 8) | (height << 8);

    uint32_t dataSize = *(uint32_t*)(resData + 6);
    dataSize = ((dataSize >> 24) & 0xFF) | ((dataSize >> 8) & 0xFF00) |
               ((dataSize << 8) & 0xFF0000) | ((dataSize << 24) & 0xFF000000);

    const uint8_t* imgData = resData + 10;

    if (imgData + dataSize > resData + resSize) {
        result.error = "Thumbnail data truncated";
        return result;
    }

    // Validar como JPEG
    if (dataSize >= 2 && imgData[0] == 0xFF && imgData[1] == 0xD8) {
        result.data.assign(imgData, imgData + dataSize);
        result.width = width;
        result.height = height;
        result.hasPreview = true;
        result.format = "JPEG";
        return result;
    }

    // Tentar extrair JPEG do buffer
    size_t offset = 0;
    result = ExtractJpegFromBuffer(imgData, dataSize, offset);
    if (result.hasPreview) {
        result.width = width ? width : result.width;
        result.height = height ? height : result.height;
        return result;
    }

    result.error = "Invalid thumbnail data";
    return result;
}

// ============================================================================
// AiExtractor
// ============================================================================

PreviewResult AiExtractor::Extract(const wchar_t* filePath) {
    std::vector<uint8_t> data;
    if (!ReadHeader(filePath, data, config_.maxReadBytes)) {
        return { {}, 0, 0, false, "", "Failed to read file" };
    }
    return ExtractFromMemory(data.data(), data.size(), "AI");
}

PreviewResult AiExtractor::ExtractFromMemory(const uint8_t* data, size_t size, const char*) {
    // Primeiro tentar thumbnail embutido no PDF
    PreviewResult result = TryPdfThumbnail(data, size);
    if (result.hasPreview) return result;

    // Tentar XMP
    return TryXmpThumbnail(data, size);
}

PreviewResult AiExtractor::TryPdfThumbnail(const uint8_t* data, size_t size) {
    // PDFs do Illustrator podem ter thumbnail na página 1
    // Procurar por /Thumb ou /Resources/XObject com imagem pequena
    // Implementação simplificada: procurar JPEG embutido
    PreviewResult result;

    for (size_t i = 0; i + 10 < size; ++i) {
        if (data[i] == 0xFF && data[i+1] == 0xD8) {
            size_t offset = 0;
            result = ExtractJpegFromBuffer(data + i, size - i, offset);
            if (result.hasPreview && result.width <= 512 && result.height <= 512) {
                result.format = "JPEG (PDF thumb)";
                return result;
            }
        }
    }

    result.error = "No PDF thumbnail found";
    return result;
}

PreviewResult AiExtractor::TryXmpThumbnail(const uint8_t* data, size_t size) {
    std::string xmp = FindXmpPacket(data, size);
    if (xmp.empty()) {
        return { {}, 0, 0, false, "", "No XMP packet found" };
    }

    // Procurar xmpGImg:image (base64 encoded JPEG)
    return ExtractBase64Image(xmp, "xmpGImg:image");
}

std::string AiExtractor::FindXmpPacket(const uint8_t* data, size_t size) {
    // XMP packet starts with "<?xpacket begin=" and ends with "<?xpacket end="
    const char* beginMarker = "<?xpacket begin=";
    const char* endMarker = "<?xpacket end=";

    const char* start = nullptr;
    const char* end = nullptr;

    for (size_t i = 0; i + 16 < size; ++i) {
        if (memcmp(data + i, beginMarker, 16) == 0) {
            start = reinterpret_cast<const char*>(data + i);
            break;
        }
    }

    if (!start) return "";

    for (size_t i = 0; i + 14 < size; ++i) {
        if (memcmp(data + i, endMarker, 14) == 0) {
            end = reinterpret_cast<const char*>(data + i) + 14;
            // Procurar o '>' final
            while (end < reinterpret_cast<const char*>(data + size) && *end != '>') end++;
            if (end < reinterpret_cast<const char*>(data + size)) end++;
            break;
        }
    }

    if (!end) return "";

    return std::string(start, end - start);
}

PreviewResult AiExtractor::ExtractBase64Image(const std::string& xmp, const char* propertyName) {
    PreviewResult result;

    // Procurar propertyName="...base64..."
    std::string search = propertyName;
    search += "=\"";

    size_t pos = xmp.find(search);
    if (pos == std::string::npos) {
        result.error = "XMP property not found";
        return result;
    }

    pos += search.length();
    size_t endPos = xmp.find('"', pos);
    if (endPos == std::string::npos) {
        result.error = "Unclosed XMP property";
        return result;
    }

    std::string base64 = xmp.substr(pos, endPos - pos);

    // Decodificar base64
    result.data = Base64Decode(base64);
    if (result.data.empty()) {
        result.error = "Base64 decode failed";
        return result;
    }

    // Validar como JPEG
    if (result.data.size() >= 2 && result.data[0] == 0xFF && result.data[1] == 0xD8) {
        result.hasPreview = true;
        result.format = "JPEG (XMP)";
        // Extrair dimensões do JPEG
        size_t offset = 0;
        PreviewResult dims = ExtractJpegFromBuffer(result.data.data(), result.data.size(), offset);
        result.width = dims.width;
        result.height = dims.height;
        return result;
    }

    result.error = "Decoded data is not JPEG";
    return result;
}

// ============================================================================
// InddExtractor
// ============================================================================

PreviewResult InddExtractor::Extract(const wchar_t* filePath) {
    std::vector<uint8_t> data;
    if (!ReadHeader(filePath, data, config_.maxReadBytes)) {
        return { {}, 0, 0, false, "", "Failed to read file" };
    }
    return ExtractFromMemory(data.data(), data.size(), "INDD");
}

PreviewResult InddExtractor::ExtractFromMemory(const uint8_t* data, size_t size, const char*) {
    return ExtractFromXmp(data, size);
}

PreviewResult InddExtractor::ExtractFromXmp(const uint8_t* data, size_t size) {
    // INDD é ZIP-based, XMP está embutido
    // Procurar pacote XMP
    const char* beginMarker = "<?xpacket begin=";
    const char* endMarker = "<?xpacket end=";

    const char* start = nullptr;
    const char* end = nullptr;

    for (size_t i = 0; i + 16 < size; ++i) {
        if (memcmp(data + i, beginMarker, 16) == 0) {
            start = reinterpret_cast<const char*>(data + i);
            break;
        }
    }

    if (!start) {
        return { {}, 0, 0, false, "", "No XMP packet in INDD" };
    }

    for (size_t i = 0; i + 14 < size; ++i) {
        if (memcmp(data + i, endMarker, 14) == 0) {
            end = reinterpret_cast<const char*>(data + i) + 14;
            while (end < reinterpret_cast<const char*>(data + size) && *end != '>') end++;
            if (end < reinterpret_cast<const char*>(data + size)) end++;
            break;
        }
    }

    if (!end) {
        return { {}, 0, 0, false, "", "Incomplete XMP packet" };
    }

    std::string xmp(start, end - start);

    // xmpGImg:image para thumbnail
    AiExtractor aiExtractor(config_);
    return aiExtractor.ExtractBase64Image(xmp, "xmpGImg:image");
}

// ============================================================================
// EpsExtractor
// ============================================================================

PreviewResult EpsExtractor::Extract(const wchar_t* filePath) {
    std::vector<uint8_t> data;
    // EPS preview pode estar no final do arquivo, ler mais
    if (!ReadHeader(filePath, data, config_.maxReadBytes)) {
        return { {}, 0, 0, false, "", "Failed to read file" };
    }
    return ExtractFromMemory(data.data(), data.size(), "EPS");
}

PreviewResult EpsExtractor::ExtractFromMemory(const uint8_t* data, size_t size, const char*) {
    // Procurar %%BeginPreview / %%EndPreview
    const char* beginMarker = "%%BeginPreview:";
    const char* endMarker = "%%EndPreview";

    const char* previewStart = nullptr;
    const char* previewEnd = nullptr;

    for (size_t i = 0; i + 15 < size; ++i) {
        if (memcmp(data + i, beginMarker, 15) == 0) {
            previewStart = reinterpret_cast<const char*>(data + i);
            // Avançar para depois da linha
            while (previewStart < reinterpret_cast<const char*>(data + size) && *previewStart != '\n') previewStart++;
            if (previewStart < reinterpret_cast<const char*>(data + size)) previewStart++;
            break;
        }
    }

    if (!previewStart) {
        return { {}, 0, 0, false, "", "No %%BeginPreview found" };
    }

    for (size_t i = 0; i + 12 < size; ++i) {
        if (memcmp(data + i, endMarker, 12) == 0) {
            previewEnd = reinterpret_cast<const char*>(data + i);
            break;
        }
    }

    if (!previewEnd || previewEnd <= previewStart) {
        return { {}, 0, 0, false, "", "No %%EndPreview found" };
    }

    size_t previewSize = previewEnd - previewStart;

    // O preview pode ser TIFF (hex encoded) ou WMF (binary)
    // Verificar se começa com TIFF signature (II ou MM)
    if (previewSize >= 4) {
        if ((previewStart[0] == 'I' && previewStart[1] == 'I' && previewStart[2] == '*' && previewStart[3] == '\0') ||
            (previewStart[0] == 'M' && previewStart[1] == 'M' && previewStart[2] == '\0' && previewStart[3] == '*')) {
            return ExtractTiffPreview(reinterpret_cast<const uint8_t*>(previewStart), previewSize);
        }
    }

    // Tentar WMF
    return ExtractWmfPreview(reinterpret_cast<const uint8_t*>(previewStart), previewSize);
}

PreviewResult EpsExtractor::ExtractTiffPreview(const uint8_t* data, size_t size) {
    PreviewResult result;

    // TIFF preview no EPS costuma ser ASCII hex encoded
    // Verificar se é hex (cada byte = 2 chars ASCII)
    bool looksHex = true;
    for (size_t i = 0; i < std::min(size, (size_t)100); ++i) {
        char c = data[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') ||
              c == '\n' || c == '\r' || c == ' ' || c == '\t')) {
            looksHex = false;
            break;
        }
    }

    if (looksHex) {
        // Decodificar hex
        std::vector<uint8_t> decoded;
        decoded.reserve(size / 2);
        for (size_t i = 0; i + 1 < size; i += 2) {
            if (data[i] == '\n' || data[i] == '\r' || data[i] == ' ' || data[i] == '\t') {
                i--; continue;
            }
            char high = data[i];
            char low = data[i+1];
            uint8_t val = 0;
            if (high >= '0' && high <= '9') val = (high - '0') << 4;
            else if (high >= 'A' && high <= 'F') val = (high - 'A' + 10) << 4;
            else if (high >= 'a' && high <= 'f') val = (high - 'a' + 10) << 4;
            else continue;

            if (low >= '0' && low <= '9') val |= (low - '0');
            else if (low >= 'A' && low <= 'F') val |= (low - 'A' + 10);
            else if (low >= 'a' && low <= 'f') val |= (low - 'a' + 10);
            else continue;

            decoded.push_back(val);
        }

        // Validar TIFF
        if (decoded.size() >= 8) {
            result = ExtractPngFromBuffer(decoded.data(), decoded.size(), /*offset*/ *(size_t*)nullptr); // placeholder
            // TIFF parsing seria mais complexo - por ora tentar JPEG dentro
            size_t offset = 0;
            result = ExtractJpegFromBuffer(decoded.data(), decoded.size(), offset);
            if (result.hasPreview) {
                result.format = "TIFF (EPS preview)";
                return result;
            }
        }
    }

    // Tentar como TIFF binário direto
    size_t offset = 0;
    result = ExtractJpegFromBuffer(data, size, offset); // pode ter JPEG embutido
    if (result.hasPreview) {
        result.format = "TIFF (EPS preview)";
        return result;
    }

    result.error = "Failed to parse TIFF preview";
    return result;
}

PreviewResult EpsExtractor::ExtractWmfPreview(const uint8_t* data, size_t size) {
    PreviewResult result;
    // WMF preview - complexo, pular por enquanto
    result.error = "WMF preview not supported yet";
    return result;
}

// ============================================================================
// PdfExtractor
// ============================================================================

PreviewResult PdfExtractor::Extract(const wchar_t* filePath) {
    std::vector<uint8_t> data;
    if (!ReadHeader(filePath, data, config_.maxReadBytes)) {
        return { {}, 0, 0, false, "", "Failed to read file" };
    }
    return ExtractFromMemory(data.data(), data.size(), "PDF");
}

PreviewResult PdfExtractor::ExtractFromMemory(const uint8_t* data, size_t size, const char*) {
    // Procurar thumbnail embutido (/Thumb no page dict)
    return FindEmbeddedThumbnail(data, size);
}

PreviewResult PdfExtractor::FindEmbeddedThumbnail(const uint8_t* data, size_t size) {
    PreviewResult result;

    // Procurar JPEG embutido (comum em PDFs)
    for (size_t i = 0; i + 10 < size; ++i) {
        if (data[i] == 0xFF && data[i+1] == 0xD8) {
            size_t offset = 0;
            result = ExtractJpegFromBuffer(data + i, size - i, offset);
            if (result.hasPreview && result.width <= 512 && result.height <= 512) {
                result.format = "JPEG (PDF embedded)";
                return result;
            }
        }
    }

    result.error = "No embedded thumbnail in PDF";
    return result;
}

// ============================================================================
// CdrExtractor (usa libcdr existente no projeto)
// ============================================================================

PreviewResult CdrExtractor::Extract(const wchar_t* filePath) {
    PreviewResult result;
    // Integração com libcdr existente - chamar função do CdrVector.h
    // Por enquanto, placeholder
    result.error = "CDR extraction via libcdr - integrate with existing CdrVector.cpp";
    return result;
}

PreviewResult CdrExtractor::ExtractFromMemory(const uint8_t* data, size_t size, const char*) {
    PreviewResult result;
    result.error = "CDR extraction via libcdr - integrate with existing CdrVector.cpp";
    return result;
}

// ============================================================================
// Utilitários compartilhados
// ============================================================================

std::vector<uint8_t> Base64Decode(const std::string& input) {
    std::vector<uint8_t> output;
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int val = 0, valb = -8;
    for (char c : input) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;

        const char* p = strchr(alphabet, c);
        if (!p) continue;

        val = (val << 6) + (p - alphabet);
        valb += 6;
        if (valb >= 0) {
            output.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return output;
}

PreviewResult ExtractJpegFromBuffer(const uint8_t* data, size_t size, size_t& outOffset) {
    PreviewResult result;

    // Procurar SOI (FF D8)
    size_t soi = SIZE_MAX;
    for (size_t i = 0; i + 1 < size; ++i) {
        if (data[i] == 0xFF && data[i+1] == 0xD8) {
            soi = i;
            break;
        }
    }

    if (soi == SIZE_MAX) {
        result.error = "No JPEG SOI marker found";
        return result;
    }

    // Procurar EOI (FF D9)
    size_t eoi = SIZE_MAX;
    for (size_t i = soi + 2; i + 1 < size; ++i) {
        if (data[i] == 0xFF && data[i+1] == 0xD9) {
            eoi = i + 2;
            break;
        }
    }

    if (eoi == SIZE_MAX) {
        result.error = "No JPEG EOI marker found";
        return result;
    }

    size_t jpegSize = eoi - soi;
    if (jpegSize < 100) { // muito pequeno
        result.error = "JPEG too small";
        return result;
    }

    // Extrair dados
    result.data.assign(data + soi, data + eoi);
    outOffset = soi;

    // Parse JPEG para obter dimensões (simplificado - procurar SOF marker)
    for (size_t i = soi + 2; i + 8 < eoi; ) {
        if (data[i] == 0xFF) {
            uint8_t marker = data[i+1];
            if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
                // SOF marker - frame header
                if (i + 8 < eoi) {
                    uint16_t h = (data[i+3] << 8) | data[i+4];
                    uint16_t w = (data[i+5] << 8) | data[i+6];
                    result.width = w;
                    result.height = h;
                    break;
                }
            }
            // Skip segment
            if (i + 3 < eoi) {
                uint16_t segLen = (data[i+2] << 8) | data[i+3];
                i += 2 + segLen;
            } else {
                break;
            }
        } else {
            i++;
        }
    }

    if (result.width == 0 || result.height == 0) {
        result.error = "Could not parse JPEG dimensions";
        return result;
    }

    // Validar tamanho
    if (result.width < 16 || result.height < 16 || result.width > 8192 || result.height > 8192) {
        result.error = "Invalid JPEG dimensions";
        return result;
    }

    result.hasPreview = true;
    result.format = "JPEG";
    return result;
}

PreviewResult ExtractPngFromBuffer(const uint8_t* data, size_t size, size_t& outOffset) {
    PreviewResult result;

    // PNG signature
    if (size < 24 || data[0] != 0x89 || data[1] != 0x50 || data[2] != 0x4E || data[3] != 0x47) {
        result.error = "Not a PNG";
        return result;
    }

    // IHDR chunk at offset 8
    uint32_t width = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
    uint32_t height = (data[20] << 24) | (data[21] << 16) | (data[22] << 8) | data[23];

    result.width = width;
    result.height = height;
    result.data.assign(data, data + size);
    result.hasPreview = true;
    result.format = "PNG";
    outOffset = 0;

    return result;
}

bool ResizeImageIfNeeded(std::vector<uint8_t>& data, uint32_t& width, uint32_t& height, uint32_t maxDim) {
    if (width <= maxDim && height <= maxDim) return true;

    // Usar WIC para redimensionar
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)))) {
        return false;
    }

    IWICStream* stream = nullptr;
    if (FAILED(factory->CreateStream(&stream))) {
        factory->Release();
        return false;
    }

    if (FAILED(stream->InitializeFromMemory(data.data(), (DWORD)data.size()))) {
        stream->Release();
        factory->Release();
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) {
        stream->Release();
        factory->Release();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    if (FAILED(decoder->GetFrame(0, &frame))) {
        decoder->Release();
        stream->Release();
        factory->Release();
        return false;
    }

    // Calcular novo tamanho mantendo aspect ratio
    float scale = std::min((float)maxDim / width, (float)maxDim / height);
    uint32_t newW = (uint32_t)(width * scale);
    uint32_t newH = (uint32_t)(height * scale);

    IWICBitmapScaler* scaler = nullptr;
    if (FAILED(factory->CreateBitmapScaler(&scaler))) {
        frame->Release();
        decoder->Release();
        stream->Release();
        factory->Release();
        return false;
    }

    if (FAILED(scaler->Initialize(frame, newW, newH, WICBitmapInterpolationModeFant))) {
        scaler->Release();
        frame->Release();
        decoder->Release();
        stream->Release();
        factory->Release();
        return false;
    }

    // Encoder para JPEG
    IWICStream* outStream = nullptr;
    if (FAILED(factory->CreateStream(&outStream))) {
        scaler->Release();
        frame->Release();
        decoder->Release();
        stream->Release();
        factory->Release();
        return false;
    }

    std::vector<uint8_t> outBuffer;
    outBuffer.reserve(newW * newH * 3);
    // WIC não escreve direto para vector, precisamos de IWICStream em memory
    // Simplificado: omitir implementação completa

    scaler->Release();
    frame->Release();
    decoder->Release();
    stream->Release();
    factory->Release();
    return false; // placeholder
}

bool ValidateImageBuffer(const uint8_t* data, size_t size, const char* expectedFormat) {
    if (!data || size < 2) return false;

    if (strcmp(expectedFormat, "JPEG") == 0 || strcmp(expectedFormat, "JPG") == 0) {
        return data[0] == 0xFF && data[1] == 0xD8;
    }
    if (strcmp(expectedFormat, "PNG") == 0) {
        return size >= 8 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47;
    }
    if (strcmp(expectedFormat, "TIFF") == 0) {
        return size >= 4 && ((data[0] == 'I' && data[1] == 'I' && data[2] == 0x2A && data[3] == 0x00) ||
                             (data[0] == 'M' && data[1] == 'M' && data[2] == 0x00 && data[3] == 0x2A));
    }
    return true;
}

} // namespace peek
} // namespace engines