// IdmlWriter.h - PDF → IDML (InDesign Markup Language) Writer
// PLANO-MOTORES-PROPRIOS - Motor 3: VizuIDML (v2 - prioridade média)
// IDML é um pacote ZIP contendo XML (DesignMap.xml, Spreads/, Stories/, Resources/)
// Este header define a interface; implementação completa em IdmlWriter.cpp
#pragma once
#include "VectorSink.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace engines {
namespace idml {

// Configuração de conversão PDF→IDML
struct Config {
    bool preserveTextAsText = true;      // Tentar manter texto editável
    bool rasterizeComplex = true;        // Rasterizar elementos complexos (gradients, transparência)
    float rasterDpi = 300.0f;            // DPI para rasterização
    bool embedFonts = true;              // Embutir fontes (se licença permitir)
    bool createLayers = true;            // Criar camadas baseadas em estrutura PDF
    std::string jobName = "VizuIDML";    // Nome do job
};

// Estrutura de página IDML
struct IdmlPage {
    std::string id;                      // "Page_1"
    std::string name;                    // "Page 1"
    Size size;                           // Tamanho em points
    std::vector<std::string> spreadRefs; // Referências a spreads
};

// Spread (páginas lado a lado)
struct IdmlSpread {
    std::string id;                      // "Spread_1"
    std::vector<std::string> pageIds;    // Páginas neste spread
    Size size;                           // Tamanho total do spread
};

// Story (fluxo de texto)
struct IdmlStory {
    std::string id;                      // "Story_1"
    std::string content;                 // Conteúdo XML do texto
    std::map<std::string, std::string> styles; // Estilos de parágrafo/caractere
};

// Recurso (imagem, cor, estilo)
struct IdmlResource {
    std::string id;
    std::string type;                    // "Image", "Color", "ParagraphStyle", etc.
    std::string xmlContent;              // XML serializado
    std::vector<uint8_t> binaryData;     // Para imagens
    std::string mimeType;                // Para imagens
};

// Documento IDML completo
struct IdmlDocument {
    std::string version = "8.0";         // IDML version (InDesign CC 2020+)
    std::string creator = "VizuIDML";
    Config config;
    std::vector<IdmlPage> pages;
    std::vector<IdmlSpread> spreads;
    std::vector<IdmlStory> stories;
    std::vector<IdmlResource> resources;
    std::map<std::string, std::string> metadata; // title, author, etc.
};

// Writer principal
class IdmlWriter {
public:
    explicit IdmlWriter(const Config& cfg = Config()) : config_(cfg) {}
    ~IdmlWriter() = default;

    // Converte PDF para IDML
    // Retorna true se sucesso, preenche document
    bool ConvertPdfToIdml(const wchar_t* pdfPath, IdmlDocument& document);

    // Converte PDF de buffer de memória
    bool ConvertPdfToIdmlFromMemory(const uint8_t* pdfData, size_t pdfSize, IdmlDocument& document);

    // Salva documento IDML como arquivo .idml (ZIP)
    bool SaveToIdmlFile(const IdmlDocument& document, const wchar_t* outputPath);

    // Salva como diretório descompactado (para debug)
    bool SaveToIdmlDirectory(const IdmlDocument& document, const wchar_t* outputDir);

    // Obtém buffer ZIP em memória
    std::vector<uint8_t> GetIdmlBuffer(const IdmlDocument& document);

private:
    Config config_;

    // PDF parsing (usa pdfium ou parsing simplificado)
    bool ParsePdfStructure(const uint8_t* data, size_t size, IdmlDocument& doc);

    // Extrai texto e posições do PDF
    bool ExtractPdfText(const uint8_t* data, size_t size, std::vector<IdmlStory>& stories);

    // Extrai imagens do PDF
    bool ExtractPdfImages(const uint8_t* data, size_t size, std::vector<IdmlResource>& resources);

    // Extrai caminhos vetoriais do PDF
    bool ExtractPdfPaths(const uint8_t* data, size_t size, IdmlDocument& doc);

    // Gera XML do DesignMap.xml
    std::string GenerateDesignMap(const IdmlDocument& doc);

    // Gera XML de Spread
    std::string GenerateSpreadXml(const IdmlSpread& spread, const IdmlDocument& doc);

    // Gera XML de Page
    std::string GeneratePageXml(const IdmlPage& page, const IdmlDocument& doc);

    // Gera XML de Story
    std::string GenerateStoryXml(const IdmlStory& story);

    // Gera XML de Resources
    std::string GenerateResourcesXml(const IdmlDocument& doc);

    // Gera XML de Preferences
    std::string GeneratePreferencesXml();

    // Gera XML de META-INF/container.xml
    std::string GenerateContainerXml();

    // Cria ZIP em memória
    std::vector<uint8_t> CreateZipArchive(const IdmlDocument& doc);

    // Helpers XML
    std::string EscapeXml(const std::string& s);
    std::string FloatToStr(float v);
    std::string PointToStr(float x, float y);
    std::string RectToStr(float x, float y, float w, float h);
    std::string MatrixToStr(const VectorSink::Matrix& m);
    std::string ColorToStr(const VectorSink::Color& c);
    std::string ColorToCmykStr(const VectorSink::Color& c); // CMYK aproximado
};

// ============================================================================
// PDF Parser Simplificado (para extrair estrutura básica)
// ============================================================================

struct PdfObject {
    enum Type { Null, Boolean, Integer, Real, Name, String, Array, Dictionary, Stream } type;
    bool boolVal = false;
    int intVal = 0;
    float realVal = 0;
    std::string strVal;
    std::string nameVal;
    std::vector<PdfObject> arrayVal;
    std::map<std::string, PdfObject> dictVal;
    std::vector<uint8_t> streamData;
    std::map<std::string, PdfObject> streamDict;
};

class PdfParser {
public:
    explicit PdfParser(const uint8_t* data, size_t size) : data_(data), size_(size), pos_(0) {}

    bool Parse();

    // Encontra objetos por tipo
    std::vector<PdfObject*> FindObjectsByType(PdfObject::Type type);
    std::vector<PdfObject*> FindPages();
    PdfObject* FindCatalog();
    PdfObject* FindPageTree(PdfObject* catalog);

    // Extrai informações de página
    struct PageInfo {
        Size mediaBox;
        Size cropBox;
        int rotate = 0;
        std::vector<PdfObject*> resources;
        std::vector<uint8_t> contentStream;
        std::map<std::string, PdfObject*> xObjects; // imagens, forms
        std::map<std::string, PdfObject*> fonts;
        std::map<std::string, PdfObject*> extGStates;
    };
    bool ExtractPageInfo(PdfObject* pageObj, PageInfo& info);

    // Extrai texto de content stream
    struct TextRun {
        std::string text;
        float x, y;
        float fontSize;
        std::string fontName;
        VectorSink::Color color;
        VectorSink::Matrix transform;
    };
    std::vector<TextRun> ExtractTextFromStream(const uint8_t* stream, size_t size,
                                                const std::map<std::string, PdfObject*>& fonts,
                                                const std::map<std::string, PdfObject*>& resources);

    // Extrai imagens (XObjects)
    struct ImageInfo {
        std::string name;
        uint32_t width, height;
        int bitsPerComponent;
        std::string colorSpace; // DeviceRGB, DeviceCMYK, DeviceGray
        std::string filter;     // DCTDecode, FlateDecode, JPXDecode
        std::vector<uint8_t> data;
        bool hasMask = false;
        std::vector<uint8_t> maskData;
    };
    std::vector<ImageInfo> ExtractImages(PdfObject* resourcesDict);

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_;
    std::vector<std::unique_ptr<PdfObject>> objects_;
    std::map<size_t, PdfObject*> objMap_; // offset -> object

    void SkipWhitespace();
    bool ParseObject(PdfObject& obj);
    bool ParseDictionary(PdfObject& obj);
    bool ParseArray(PdfObject& obj);
    bool ParseStream(PdfObject& obj, size_t streamStart);
    std::string ParseName();
    std::string ParseString();
    int ParseInteger();
    float ParseReal();
    bool MatchKeyword(const char* keyword);

    // Decodificadores de stream
    std::vector<uint8_t> DecodeFlate(const uint8_t* data, size_t size);
    std::vector<uint8_t> DecodeDct(const uint8_t* data, size_t size); // JPEG - retorna como está
    std::vector<uint8_t> DecodePredictor(const uint8_t* data, size_t size, int predictor, int columns, int colors, int bpc);
};

// ============================================================================
// ZIP Creator (usa Windows API ou miniz)
// ============================================================================

class ZipArchive {
public:
    ZipArchive() = default;
    ~ZipArchive() = default;

    void AddFile(const std::string& zipPath, const std::string& content);
    void AddFileBinary(const std::string& zipPath, const uint8_t* data, size_t size);
    void AddDirectory(const std::string& zipPath);
    std::vector<uint8_t> Finalize();
    bool SaveToFile(const wchar_t* path);

private:
    struct Entry {
        std::string name;
        std::vector<uint8_t> data;
        bool isDirectory = false;
        uint32_t crc32 = 0;
        uint64_t compressedSize = 0;
        uint64_t uncompressedSize = 0;
        uint32_t offset = 0;
    };
    std::vector<Entry> entries_;

    uint32_t ComputeCrc32(const uint8_t* data, size_t size);
    std::vector<uint8_t> CompressDeflate(const uint8_t* data, size_t size);
    void WriteLocalFileHeader(std::vector<uint8_t>& out, const Entry& e);
    void WriteCentralDirectory(std::vector<uint8_t>& out);
    void WriteEndOfCentralDirectory(std::vector<uint8_t>& out);
    void WriteUInt16(std::vector<uint8_t>& out, uint16_t v);
    void WriteUInt32(std::vector<uint8_t>& out, uint32_t v);
    void WriteUInt64(std::vector<uint8_t>& out, uint64_t v);
    std::string GetCurrentDosDateTime();
};

// ============================================================================
// Integração com VectorSink (para reutilizar código de paths)
// ============================================================================

// Adapter para usar VectorSink como target de saída IDML
class IdmlVectorSinkAdapter : public VectorSink {
public:
    explicit IdmlVectorSinkAdapter(IdmlDocument& doc, const std::string& pageId) : doc_(doc), pageId_(pageId) {}
    ~IdmlVectorSink() override = default;

    // Document
    bool BeginDocument(const Size& pageSize, const wchar_t* title = nullptr) override { return true; }
    bool EndDocument() override { return true; }
    bool SaveToFile(const wchar_t*) override { return true; }
    std::vector<uint8_t> GetBuffer() override { return {}; }

    // State
    void PushState() override { states_.push_back(currentState_); }
    void PopState() override { if (!states_.empty()) { currentState_ = states_.back(); states_.pop_back(); } }
    void SetTransform(const Matrix& m) override { currentState_.transform = m; }
    void MultiplyTransform(const Matrix& m) override { currentState_.transform = currentState_.transform * m; }
    void SetOpacity(float opacity) override { currentState_.opacity = opacity; }

    // Fill/Stroke
    void SetFillNone() override { currentState_.fillType = FillType::None; }
    void SetFillSolid(const Color& c) override { currentState_.fillType = FillType::Solid; currentState_.fillColor = c; }
    void SetFillLinearGradient(const LinearGradient& g) override { currentState_.fillType = FillType::LinearGradient; currentState_.linearGrad = g; }
    void SetFillRadialGradient(const RadialGradient& g) override { currentState_.fillType = FillType::RadialGradient; currentState_.radialGrad = g; }
    void SetStroke(const StrokeStyle& style, const Color& color) override { currentState_.hasStroke = true; currentState_.stroke = style; currentState_.strokeColor = color; }
    void SetStrokeNone() override { currentState_.hasStroke = false; }

    // Primitivas - converter para XML IDML
    void DrawRect(const Rect& r) override { AddPathXml(GenerateRectPath(r)); }
    void DrawRoundedRect(const Rect& r, float rx, float ry) override { AddPathXml(GenerateRoundedRectPath(r, rx, ry)); }
    void DrawEllipse(const Point& center, float rx, float ry) override { AddPathXml(GenerateEllipsePath(center, rx, ry)); }
    void DrawLine(const Point& p1, const Point& p2) override { AddPathXml(GenerateLinePath(p1, p2)); }

    // Paths
    void BeginPath() override { pathBuilder_.clear(); inPath_ = true; }
    void MoveTo(const Point& p) override { if (inPath_) pathBuilder_ += "M " + FloatToStr(p.x) + " " + FloatToStr(p.y) + " "; }
    void LineTo(const Point& p) override { if (inPath_) pathBuilder_ += "L " + FloatToStr(p.x) + " " + FloatToStr(p.y) + " "; }
    void CubicTo(const Point& c1, const Point& c2, const Point& p) override { if (inPath_) pathBuilder_ += "C " + FloatToStr(c1.x) + " " + FloatToStr(c1.y) + " " + FloatToStr(c2.x) + " " + FloatToStr(c2.y) + " " + FloatToStr(p.x) + " " + FloatToStr(p.y) + " "; }
    void QuadraticTo(const Point& c, const Point& p) override { if (inPath_) pathBuilder_ += "Q " + FloatToStr(c.x) + " " + FloatToStr(c.y) + " " + FloatToStr(p.x) + " " + FloatToStr(p.y) + " "; }
    void ClosePath() override { if (inPath_) pathBuilder_ += "Z "; }
    void EndPath(bool fill, bool stroke) override {
        if (!inPath_) return;
        std::string pathXml = "<PathGeometry>" + pathBuilder_ + "</PathGeometry>";
        if (fill || stroke) {
            pathXml = "<Path " + (fill ? "Fill=\"" + ColorToStr(currentState_.fillColor) + "\"" : "Fill=\"None\"")
                       + (stroke ? " Stroke=\"" + ColorToStr(currentState_.strokeColor) + "\" StrokeThickness=\"" + FloatToStr(currentState_.stroke.width) + "\"" : " Stroke=\"None\"")
                       + ">" + pathBuilder_ + "</Path>";
        }
        AddPathXml(pathXml);
        inPath_ = false;
    }

    // Text
    void DrawGlyphRun(const std::vector<Point>&, const std::vector<uint16_t>&, const std::vector<float>&, float, const wchar_t*) override {}

    // Images
    void DrawBitmap(const EmbeddedBitmap& bmp, const Rect& destRect) override {
        // Adicionar imagem aos resources e referenciar
    }

    // Clip
    void PushClipRect(const Rect&) override {}
    void PopClip() override {}

    // Metadata
    void SetTitle(const wchar_t*) override {}
    void SetAuthor(const wchar_t*) override {}
    void SetCreator(const wchar_t*) override {}
    void SetKeywords(const wchar_t*) override {}

    // CMYK
    void SetFillCMYK(float, float, float, float) override {}
    void SetStrokeCMYK(float, float, float, float) override {}
    void EmbedICCProfile(const uint8_t*, size_t) override {}

private:
    IdmlDocument& doc_;
    std::string pageId_;
    std::vector<DrawState> states_;
    DrawState currentState_;
    bool inPath_ = false;
    std::string pathBuilder_;

    void AddPathXml(const std::string& pathXml) {
        // Adicionar ao spread/page correspondente
    }

    std::string GenerateRectPath(const Rect& r) {
        return "M " + FloatToStr(r.x) + " " + FloatToStr(r.y) + " "
             + "L " + FloatToStr(r.x + r.w) + " " + FloatToStr(r.y) + " "
             + "L " + FloatToStr(r.x + r.w) + " " + FloatToStr(r.y + r.h) + " "
             + "L " + FloatToStr(r.x) + " " + FloatToStr(r.y + r.h) + " Z ";
    }

    std::string GenerateRoundedRectPath(const Rect& r, float rx, float ry) {
        // Simplificado
        return GenerateRectPath(r);
    }

    std::string GenerateEllipsePath(const Point& c, float rx, float ry) {
        // Aproximação com curvas Bézier
        float kappa = 0.5522847498f;
        return "M " + FloatToStr(c.x) + " " + FloatToStr(c.y - ry) + " "
             + "C " + FloatToStr(c.x + rx * kappa) + " " + FloatToStr(c.y - ry) + " "
               + FloatToStr(c.x + rx) + " " + FloatToStr(c.y - ry * kappa) + " "
               + FloatToStr(c.x + rx) + " " + FloatToStr(c.y) + " "
             + "C " + FloatToStr(c.x + rx) + " " + FloatToStr(c.y + ry * kappa) + " "
               + FloatToStr(c.x + rx * kappa) + " " + FloatToStr(c.y + ry) + " "
               + FloatToStr(c.x) + " " + FloatToStr(c.y + ry) + " "
             + "C " + FloatToStr(c.x - rx * kappa) + " " + FloatToStr(c.y + ry) + " "
               + FloatToStr(c.x - rx) + " " + FloatToStr(c.y + ry * kappa) + " "
               + FloatToStr(c.x - rx) + " " + FloatToStr(c.y) + " "
             + "C " + FloatToStr(c.x - rx) + " " + FloatToStr(c.y - ry * kappa) + " "
               + FloatToStr(c.x - rx * kappa) + " " + FloatToStr(c.y - ry) + " "
               + FloatToStr(c.x) + " " + FloatToStr(c.y - ry) + " Z ";
    }

    std::string GenerateLinePath(const Point& p1, const Point& p2) {
        return "M " + FloatToStr(p1.x) + " " + FloatToStr(p1.y) + " L " + FloatToStr(p2.x) + " " + FloatToStr(p2.y) + " ";
    }

    static std::string FloatToStr(float v) {
        char buf[32];
        sprintf_s(buf, "%.4f", v);
        std::string s(buf);
        // Remover zeros trailing
        size_t pos = s.find_last_not_of('0');
        if (pos != std::string::npos && s[pos] == '.') pos--;
        return s.substr(0, pos + 1);
    }

    static std::string ColorToStr(const Color& c) {
        return "rgb(" + FloatToStr(c.r) + "," + FloatToStr(c.g) + "," + FloatToStr(c.b) + ")";
    }
};

} // namespace idml
} // namespace engines