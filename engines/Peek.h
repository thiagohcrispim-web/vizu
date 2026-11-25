// Peek.h - VizuPeek: Extrator de prévias embutidas para miniaturas instantâneas
// PLANO-MOTORES-PROPRIOS - Motor 2: VizuPeek (prioridade alta)
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace engines {
namespace peek {

// Resultado da extração de prévia
struct PreviewResult {
    std::vector<uint8_t> data;     // Dados da imagem (JPEG/PNG)
    uint32_t width = 0;            // Largura em pixels
    uint32_t height = 0;           // Altura em pixels
    bool hasPreview = false;       // Se encontrou prévia válida
    std::string format;            // "JPEG", "PNG", "TIFF", etc.
    std::string error;             // Mensagem de erro se falhou
};

// Configuração de extração
struct Config {
    size_t maxReadBytes = 8 * 1024 * 1024; // Máximo 8MB iniciais
    bool fallbackToFullDecode = true;      // Fallback para decodificação completa
    uint32_t minPreviewSize = 128;         // Tamanho mínimo aceitável
    uint32_t maxPreviewSize = 4096;        // Tamanho máximo (redimensiona se maior)
};

// Interface principal
class PreviewExtractor {
public:
    explicit PreviewExtractor(const Config& cfg = Config()) : config_(cfg) {}
    virtual ~PreviewExtractor() = default;

    // Extrai prévia de arquivo
    virtual PreviewResult Extract(const wchar_t* filePath) = 0;

    // Extrai prévia de buffer de memória
    virtual PreviewResult ExtractFromMemory(const uint8_t* data, size_t size, const char* formatHint = nullptr) = 0;

protected:
    Config config_;

    // Utilitário: ler cabeçalho de arquivo
    bool ReadHeader(const wchar_t* path, std::vector<uint8_t>& header, size_t maxBytes);
};

// Factory para criar extratores por formato
class PreviewExtractorFactory {
public:
    enum Format {
        PSD,      // Photoshop PSD/PSB
        AI,       // Adobe Illustrator
        INDD,     // InDesign
        EPS,      // Encapsulated PostScript
        PDF,      // PDF (página 1 thumbnail)
        CDR,      // CorelDRAW (via libcdr)
        AFFINITY, // Affinity Photo/Designer
        SKETCH,   // Sketch
        UNKNOWN
    };

    static Format DetectFormat(const wchar_t* filePath);
    static Format DetectFormatFromMemory(const uint8_t* data, size_t size);
    static std::unique_ptr<PreviewExtractor> Create(Format fmt, const Config& cfg = Config());
    static std::unique_ptr<PreviewExtractor> CreateForFile(const wchar_t* filePath, const Config& cfg = Config());
};

// ============================================================================
// PSD/PSB Extractor - Image Resource 1036 (JPEG thumbnail)
// Spec: Adobe Photoshop File Formats Specification
// ============================================================================
class PsdExtractor : public PreviewExtractor {
public:
    using PreviewExtractor::PreviewExtractor;

    PreviewResult Extract(const wchar_t* filePath) override;
    PreviewResult ExtractFromMemory(const uint8_t* data, size_t size, const char* formatHint = nullptr) override;

private:
    struct PsdHeader {
        char signature[4];        // "8BPS"
        uint16_t version;         // 1 = PSD, 2 = PSB
        uint8_t reserved[6];
        uint16_t channels;
        uint32_t height;
        uint32_t width;
        uint16_t depth;
        uint16_t colorMode;
    };

    struct ImageResource {
        char signature[4];        // "8BIM"
        uint16_t id;              // 1036 = thumbnail
        // name (pascal string)
        uint32_t size;            // tamanho dos dados
        // data (padded to even)
    };

    PreviewResult ParsePsd(const uint8_t* data, size_t size);
    PreviewResult ExtractThumbnailFromResource(const uint8_t* resData, size_t resSize);
};

// ============================================================================
// AI Extractor - PDF thumbnail ou XMP xmpGImg:image
// Spec: PDF 1.7 + XMP Specification
// ============================================================================
class AiExtractor : public PreviewExtractor {
public:
    using PreviewExtractor::PreviewExtractor;

    PreviewResult Extract(const wchar_t* filePath) override;
    PreviewResult ExtractFromMemory(const uint8_t* data, size_t size, const char* formatHint = nullptr) override;

private:
    PreviewResult TryPdfThumbnail(const uint8_t* data, size_t size);
    PreviewResult TryXmpThumbnail(const uint8_t* data, size_t size);
    std::string FindXmpPacket(const uint8_t* data, size_t size);
    PreviewResult ExtractBase64Image(const std::string& xmp, const char* propertyName);
};

// ============================================================================
// INDD Extractor - XMP xmpGImg:image (JPEG base64)
// Spec: XMP Specification
// ============================================================================
class InddExtractor : public PreviewExtractor {
public:
    using PreviewExtractor::PreviewExtractor;

    PreviewResult Extract(const wchar_t* filePath) override;
    PreviewResult ExtractFromMemory(const uint8_t* data, size_t size, const char* formatHint = nullptr) override;

private:
    PreviewResult ExtractFromXmp(const uint8_t* data, size_t size);
};

// ============================================================================
// EPS Extractor - TIFF/WMF preview no cabeçalho DOS EPS
// Spec: EPSF 3.0
// ============================================================================
class EpsExtractor : public PreviewExtractor {
public:
    using PreviewExtractor::PreviewExtractor;

    PreviewResult Extract(const wchar_t* filePath) override;
    PreviewResult ExtractFromMemory(const uint8_t* data, size_t size, const char* formatHint = nullptr) override;

private:
    struct EpsHeader {
        char signature[4];        // "%!PS"
        // %%BeginPreview / %%EndPreview
    };

    PreviewResult ExtractTiffPreview(const uint8_t* data, size_t size);
    PreviewResult ExtractWmfPreview(const uint8_t* data, size_t size);
};

// ============================================================================
// PDF Extractor - Primeira página como thumbnail (via pdfium ou parsing simples)
// ============================================================================
class PdfExtractor : public PreviewExtractor {
public:
    using PreviewExtractor::PreviewExtractor;

    PreviewResult Extract(const wchar_t* filePath) override;
    PreviewResult ExtractFromMemory(const uint8_t* data, size_t size, const char* formatHint = nullptr) override;

private:
    PreviewResult FindEmbeddedThumbnail(const uint8_t* data, size_t size);
    // Nota: Para renderizar página, precisaria pdfium - fallback para thumbnail embutido
};

// ============================================================================
// CDR Extractor - Via libcdr (já existe no projeto)
// ============================================================================
class CdrExtractor : public PreviewExtractor {
public:
    using PreviewExtractor::PreviewExtractor;

    PreviewResult Extract(const wchar_t* filePath) override;
    PreviewResult ExtractFromMemory(const uint8_t* data, size_t size, const char* formatHint = nullptr) override;
};

// ============================================================================
// Utilitários compartilhados
// ============================================================================

// Decodificar Base64 para binário
std::vector<uint8_t> Base64Decode(const std::string& input);

// Extrair JPEG de buffer (procura SOI/EOI markers)
PreviewResult ExtractJpegFromBuffer(const uint8_t* data, size_t size, size_t& outOffset);

// Extrair PNG de buffer (procura assinatura PNG)
PreviewResult ExtractPngFromBuffer(const uint8_t* data, size_t size, size_t& outOffset);

// Redimensionar imagem se necessário (usa WIC)
bool ResizeImageIfNeeded(std::vector<uint8_t>& data, uint32_t& width, uint32_t& height, uint32_t maxDim);

// Validar se buffer é imagem válida
bool ValidateImageBuffer(const uint8_t* data, size_t size, const char* expectedFormat);

} // namespace peek
} // namespace engines