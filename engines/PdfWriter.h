// PdfWriter.h - PDF 1.4 Vector Writer implementando VectorSink
// PLANO-MOTORES-PROPRIOS - Motor 1: VizuVector (PDF para intercâmbio/impressão)
#pragma once
#include "VectorSink.h"
#include <sstream>
#include <vector>
#include <map>
#include <cstdint>

namespace engines {

class PdfWriter : public VectorSink {
public:
    explicit PdfWriter(const Size& pageSize) : pageSize_(pageSize), inPath_(false), stateDepth_(0),
        nextObjId_(1), pageObjId_(0), contentObjId_(0), fontObjId_(0) {
        InitializeDocument();
    }

    ~PdfWriter() override = default;

    bool BeginDocument(const Size& pageSize, const wchar_t* title = nullptr) override {
        pageSize_ = pageSize;
        InitializeDocument();
        if (title) {
            info_.title = WideToUtf8(title);
        }
        return true;
    }

    bool EndDocument() override {
        // Finalizar stream de conteúdo da página
        if (contentObjId_) {
            FinalizeContentStream();
        }

        // Escrever objetos
        WriteObjects();
        WriteXRefAndTrailer();

        return true;
    }

    bool SaveToFile(const wchar_t* path) override {
        EndDocument();
        std::wofstream file(path, std::ios::binary);
        if (!file) return false;
        std::string str = output_.str();
        file.write(str.c_str(), str.size());
        return file.good();
    }

    std::vector<uint8_t> GetBuffer() override {
        EndDocument();
        std::string str = output_.str();
        return std::vector<uint8_t>(str.begin(), str.end());
    }

    // --- Estados ---
    void PushState() override {
        states_.push_back(currentState_);
        stateDepth_++;
        content_ << "q\n"; // save graphics state
    }

    void PopState() override {
        if (!states_.empty()) {
            currentState_ = states_.back();
            states_.pop_back();
            stateDepth_--;
            content_ << "Q\n"; // restore graphics state
        }
    }

    void SetTransform(const Matrix& m) override {
        currentState_.transform = m;
        content_ << m.m11 << " " << m.m12 << " " << m.m21 << " " << m.m22 << " " << m.dx << " " << m.dy << " cm\n";
    }

    void MultiplyTransform(const Matrix& m) override {
        currentState_.transform = currentState_.transform * m;
        content_ << m.m11 << " " << m.m12 << " " << m.m21 << " " << m.m22 << " " << m.dx << " " << m.dy << " cm\n";
    }

    void SetOpacity(float opacity) override {
        currentState_.opacity = opacity;
        // PDF 1.4: extended graphics state para opacity
        // Simplificado: usar CA/ca no graphics state
        content_ << "/GS" << gsId_++ << " gs\n";
        extGStates_.push_back({opacity, opacity});
    }

    // --- Fill/Stroke ---
    void SetFillNone() override {
        currentState_.fillType = FillType::None;
    }

    void SetFillSolid(const Color& c) override {
        currentState_.fillType = FillType::Solid;
        currentState_.fillColor = c;
        content_ << c.r << " " << c.g << " " << c.b << " rg\n"; // non-stroking RGB
        if (c.a < 1) {
            // Precisaria de extended graphics state
        }
    }

    void SetFillLinearGradient(const LinearGradient& g) override {
        currentState_.fillType = FillType::LinearGradient;
        currentState_.linearGrad = g;
        int shadingId = CreateShading(g);
        content_ << "/Sh" << shadingId << " sh\n";
    }

    void SetFillRadialGradient(const RadialGradient& g) override {
        currentState_.fillType = FillType::RadialGradient;
        currentState_.radialGrad = g;
        int shadingId = CreateShading(g);
        content_ << "/Sh" << shadingId << " sh\n";
    }

    void SetStroke(const StrokeStyle& style, const Color& color) override {
        currentState_.hasStroke = true;
        currentState_.stroke = style;
        currentState_.strokeColor = color;
        content_ << style.width << " w\n";
        WriteLineCap(style.cap);
        WriteLineJoin(style.join);
        if (!style.dashPattern.empty()) {
            content_ << "[";
            for (size_t i = 0; i < style.dashPattern.size(); ++i) {
                if (i) content_ << " ";
                content_ << style.dashPattern[i];
            }
            content_ << "] " << style.dashOffset << " d\n";
        } else {
            content_ << "[] 0 d\n";
        }
        content_ << color.r << " " << color.g << " " << color.b << " RG\n"; // stroking RGB
    }

    void SetStrokeNone() override {
        currentState_.hasStroke = false;
    }

    // --- Primitivas ---
    void DrawRect(const Rect& r) override {
        content_ << r.x << " " << r.y << " " << r.w << " " << r.h << " re\n";
        WriteFillStroke();
    }

    void DrawRoundedRect(const Rect& r, float rx, float ry) override {
        float x = r.x, y = r.y, w = r.w, h = r.h;
        rx = std::min(rx, w / 2);
        ry = std::min(ry, h / 2);
        float kappa = 0.5522847498f; // 4/3 * (sqrt(2) - 1)

        content_ << x + rx << " " << y << " m\n";
        content_ << x + w - rx << " " << y << " l\n";
        content_ << x + w - rx * (1 - kappa) << " " << y << " " << x + w << " " << y + ry * (1 - kappa) << " " << x + w << " " << y + ry << " c\n";
        content_ << x + w << " " << y + h - ry << " l\n";
        content_ << x + w << " " << y + h - ry * (1 - kappa) << " " << x + w - rx * (1 - kappa) << " " << y + h << " " << x + w - rx << " " << y + h << " c\n";
        content_ << x + rx << " " << y + h << " l\n";
        content_ << x + rx * (1 - kappa) << " " << y + h << " " << x << " " << y + h - ry * (1 - kappa) << " " << x << " " << y + h - ry << " c\n";
        content_ << x << " " << y + ry << " l\n";
        content_ << x << " " << y + ry * (1 - kappa) << " " << x + rx * (1 - kappa) << " " << y << " " << x + rx << " " << y << " c\n";
        content_ << "h\n";
        WriteFillStroke();
    }

    void DrawEllipse(const Point& center, float rx, float ry) override {
        float kappa = 0.5522847498f;
        float x = center.x, y = center.y;

        content_ << x << " " << y - ry << " m\n";
        content_ << x + rx * kappa << " " << y - ry << " " << x + rx << " " << y - ry * kappa << " " << x + rx << " " << y << " c\n";
        content_ << x + rx << " " << y + ry * kappa << " " << x + rx * kappa << " " << y + ry << " " << x << " " << y + ry << " c\n";
        content_ << x - rx * kappa << " " << y + ry << " " << x - rx << " " << y + ry * kappa << " " << x - rx << " " << y << " c\n";
        content_ << x - rx << " " << y - ry * kappa << " " << x - rx * kappa << " " << y - ry << " " << x << " " << y - ry << " c\n";
        content_ << "h\n";
        WriteFillStroke();
    }

    void DrawLine(const Point& p1, const Point& p2) override {
        content_ << p1.x << " " << p1.y << " m\n";
        content_ << p2.x << " " << p2.y << " l\n";
        if (currentState_.hasStroke) {
            content_ << "S\n";
        }
    }

    // --- Paths ---
    void BeginPath() override {
        inPath_ = true;
    }

    void MoveTo(const Point& p) override {
        if (inPath_) content_ << p.x << " " << p.y << " m\n";
    }

    void LineTo(const Point& p) override {
        if (inPath_) content_ << p.x << " " << p.y << " l\n";
    }

    void CubicTo(const Point& c1, const Point& c2, const Point& p) override {
        if (inPath_) content_ << c1.x << " " << c1.y << " " << c2.x << " " << c2.y << " " << p.x << " " << p.y << " c\n";
    }

    void QuadraticTo(const Point& c, const Point& p) override {
        if (inPath_) {
            // Converter quadrática para cúbica
            // Não temos o ponto atual, simplificação
            content_ << c.x << " " << c.y << " " << c.x << " " << c.y << " " << p.x << " " << p.y << " c\n";
        }
    }

    void ClosePath() override {
        if (inPath_) content_ << "h\n";
    }

    void EndPath(bool fill, bool stroke) override {
        if (!inPath_) return;
        if (fill && currentState_.fillType != FillType::None) {
            if (stroke && currentState_.hasStroke) {
                content_ << "B\n"; // fill and stroke
            } else {
                content_ << "f\n"; // fill only (non-zero winding)
            }
        } else if (stroke && currentState_.hasStroke) {
            content_ << "S\n"; // stroke only
        } else {
            content_ << "n\n"; // end path without painting
        }
        inPath_ = false;
    }

    // --- Texto (como curvas) ---
    void DrawGlyphRun(const std::vector<Point>& glyphPositions,
                      const std::vector<uint16_t>& glyphIndices,
                      const std::vector<float>& advances,
                      float fontSize,
                      const wchar_t* fontFamily) override {
        // PDF: texto como curvas já convertidas - não implementado aqui
        // O chamador deve converter glifos para caminhos antes
    }

    // --- Imagens ---
    void DrawBitmap(const EmbeddedBitmap& bmp, const Rect& destRect) override {
        int imgId = CreateImageXObject(bmp);
        content_ << "q\n";
        content_ << destRect.w << " 0 0 " << destRect.h << " " << destRect.x << " " << destRect.y << " cm\n";
        content_ << "/Im" << imgId << " Do\n";
        content_ << "Q\n";
    }

    // --- Clip ---
    void PushClipRect(const Rect& r) override {
        content_ << r.x << " " << r.y << " " << r.w << " " << r.h << " re\n";
        content_ << "W\n";
        content_ << "n\n";
        clipStack_.push_back(clipId_++);
    }

    void PopClip() override {
        if (!clipStack_.empty()) {
            clipStack_.pop_back();
            content_ << "Q\nq\n"; // restore e save novamente (PDF clip é aninhado via q/Q)
        }
    }

    // --- Metadados ---
    void SetTitle(const wchar_t* title) override {
        info_.title = WideToUtf8(title);
    }

    void SetAuthor(const wchar_t* author) override {
        info_.author = WideToUtf8(author);
    }

    void SetCreator(const wchar_t* creator) override {
        info_.creator = WideToUtf8(creator);
    }

    void SetKeywords(const wchar_t* keywords) override {
        info_.keywords = WideToUtf8(keywords);
    }

    // --- CMYK (PDF suporta nativamente) ---
    void SetFillCMYK(float c, float m, float y, float k) override {
        currentState_.fillType = FillType::Solid;
        content_ << c << " " << m << " " << y << " " << k << " k\n"; // non-stroking CMYK
        cmykFill_ = {c, m, y, k};
    }

    void SetStrokeCMYK(float c, float m, float y, float k) override {
        content_ << c << " " << m << " " << y << " " << k << " K\n"; // stroking CMYK
        cmykStroke_ = {c, m, y, k};
    }

    void EmbedICCProfile(const uint8_t* data, size_t size) override {
        int iccId = nextObjId_++;
        std::string base64 = ToBase64(std::vector<uint8_t>(data, data + size));
        // Decodificar base64 para binário no objeto
        objects_[iccId] = "<< /N 4 /Alternate /DeviceCMYK /Length " + std::to_string(size) + " >>\nstream\n" + base64 + "\nendstream";
        info_.iccProfileId = iccId;
    }

private:
    struct DocInfo {
        std::string title, author, creator, keywords;
        int iccProfileId = 0;
    } info_;

    struct ExtGState {
        float ca, CA; // alpha for non-stroking and stroking
    };

    struct PdfObject {
        std::string content;
        bool isStream = false;
    };

    Size pageSize_;
    std::ostringstream output_;
    std::ostringstream content_;
    std::map<int, PdfObject> objects_;
    std::vector<DrawState> states_;
    DrawState currentState_;
    bool inPath_ = false;
    int stateDepth_ = 0;
    int nextObjId_ = 1;
    int pageObjId_ = 0;
    int contentObjId_ = 0;
    int fontObjId_ = 0;
    int gsId_ = 1;
    int clipId_ = 0;
    std::vector<int> clipStack_;
    std::vector<ExtGState> extGStates_;
    std::vector<std::pair<int, std::string>> shadings_; // id, type
    std::vector<int> images_; // image object IDs
    struct { float c, m, y, k; } cmykFill_, cmykStroke_;

    void InitializeDocument() {
        output_.str("");
        content_.str("");
        objects_.clear();
        states_.clear();
        currentState_ = DrawState();
        inPath_ = false;
        stateDepth_ = 0;
        nextObjId_ = 1;
        pageObjId_ = 0;
        contentObjId_ = 0;
        fontObjId_ = 0;
        gsId_ = 1;
        clipId_ = 0;
        clipStack_.clear();
        extGStates_.clear();
        shadings_.clear();
        images_.clear();
        info_ = DocInfo();

        // Header
        output_ << "%PDF-1.4\n";
        output_ << "%\xE2\xE3\xCF\xD3\n"; // binary marker

        // Catalog (obj 1)
        pageObjId_ = nextObjId_++;
        contentObjId_ = nextObjId_++;
    }

    void FinalizeContentStream() {
        objects_[contentObjId_].content = content_.str();
        objects_[contentObjId_].isStream = true;
    }

    void WriteObjects() {
        // Page object
        std::string pageContent = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + std::to_string(pageSize_.w) + " " + std::to_string(pageSize_.h) + "] ";
        pageContent += "/Resources << ";
        if (!extGStates_.empty()) pageContent += "/ExtGState << " + BuildExtGStateDict() + " >> ";
        if (!shadings_.empty()) pageContent += "/Shading << " + BuildShadingDict() + " >> ";
        if (!images_.empty()) pageContent += "/XObject << " + BuildXObjectDict() + " >> ";
        pageContent += ">> ";
        pageContent += "/Contents " + std::to_string(contentObjId_) + " 0 R >>";
        objects_[pageObjId_].content = pageContent;

        // Pages (obj 2)
        objects_[2].content = "<< /Type /Pages /Kids [" + std::to_string(pageObjId_) + " 0 R] /Count 1 >>";

        // Catalog (obj 1)
        objects_[1].content = "<< /Type /Catalog /Pages 2 0 R >>";

        // Info
        int infoId = nextObjId_++;
        std::string infoContent = "<<";
        if (!info_.title.empty()) infoContent += " /Title (" + EscapePdfString(info_.title) + ")";
        if (!info_.author.empty()) infoContent += " /Author (" + EscapePdfString(info_.author) + ")";
        if (!info_.creator.empty()) infoContent += " /Creator (" + EscapePdfString(info_.creator) + ")";
        if (!info_.keywords.empty()) infoContent += " /Keywords (" + EscapePdfString(info_.keywords) + ")";
        infoContent += " /Producer (VizuVector PdfWriter) >>";
        objects_[infoId].content = infoContent;

        // Extended Graphics States
        for (size_t i = 0; i < extGStates_.size(); ++i) {
            int gsId = nextObjId_++;
            objects_[gsId].content = "<< /Type /ExtGState /CA " + std::to_string(extGStates_[i].CA)
                + " /ca " + std::to_string(extGStates_[i].ca) + " >>";
        }

        // Shadings
        for (auto& [shId, type] : shadings_) {
            int objId = nextObjId_++;
            // Simplificado - shading dicts seriam complexos
            objects_[objId].content = "<< /ShadingType " + (type == "axial" ? "2" : "3") + " /ColorSpace /DeviceRGB /Coords [0 0 1 1] /Function << /FunctionType 2 /Domain [0 1] /C0 [0 0 0] /C1 [1 1 1] /N 1 >> >>";
            objects_[objId].isStream = false;
        }

        // Images
        for (size_t i = 0; i < images_.size(); ++i) {
            int imgId = images_[i];
            // Já criado em CreateImageXObject
        }

        // Escrever todos os objetos em ordem
        for (int i = 1; i < nextObjId_; ++i) {
            if (objects_.find(i) != objects_.end()) {
                WriteObject(i, objects_[i]);
            }
        }
    }

    std::string BuildExtGStateDict() {
        std::string r;
        for (size_t i = 0; i < extGStates_.size(); ++i) {
            int objId = 3 + (int)i; // após catalog, pages, page
            if (!r.empty()) r += " ";
            r += "/GS" + std::to_string(i+1) + " " + std::to_string(objId) + " 0 R";
        }
        return r;
    }

    std::string BuildShadingDict() {
        std::string r;
        for (size_t i = 0; i < shadings_.size(); ++i) {
            int objId = 3 + extGStates_.size() + (int)i;
            if (!r.empty()) r += " ";
            r += "/Sh" + std::to_string(i+1) + " " + std::to_string(objId) + " 0 R";
        }
        return r;
    }

    std::string BuildXObjectDict() {
        std::string r;
        for (size_t i = 0; i < images_.size(); ++i) {
            if (!r.empty()) r += " ";
            r += "/Im" + std::to_string(i+1) + " " + std::to_string(images_[i]) + " 0 R";
        }
        return r;
    }

    void WriteObject(int id, const PdfObject& obj) {
        output_ << id << " 0 obj\n";
        if (obj.isStream) {
            output_ << "<< /Length " << obj.content.size() << " >>\nstream\n";
            output_ << obj.content;
            output_ << "\nendstream\n";
        } else {
            output_ << obj.content << "\n";
        }
        output_ << "endobj\n";
    }

    void WriteXRefAndTrailer() {
        size_t startXRef = output_.tellp();
        output_ << "xref\n";
        output_ << "0 " << nextObjId_ << "\n";
        output_ << "0000000000 65535 f \n";
        // Simplificado - offsets reais precisariam ser calculados
        for (int i = 1; i < nextObjId_; ++i) {
            output_ << "0000000000 00000 n \n";
        }
        output_ << "trailer\n";
        output_ << "<< /Size " << nextObjId_ << " /Root 1 0 R /Info " << (nextObjId_ - 1) << " 0 R >>\n";
        output_ << "startxref\n";
        output_ << startXRef << "\n";
        output_ << "%%EOF\n";
    }

    int CreateShading(const LinearGradient& g) {
        int id = (int)shadings_.size() + 1;
        shadings_.push_back({id, "axial"});
        return id;
    }

    int CreateShading(const RadialGradient& g) {
        int id = (int)shadings_.size() + 1;
        shadings_.push_back({id, "radial"});
        return id;
    }

    int CreateImageXObject(const EmbeddedBitmap& bmp) {
        int imgId = nextObjId_++;
        images_.push_back(imgId);

        // Determinar filtro baseado nos dados (assumir PNG/JPEG)
        std::string filter = "/FlateDecode"; // PNG
        // Para JPEG, detectar SOI marker
        if (bmp.data.size() >= 2 && bmp.data[0] == 0xFF && bmp.data[1] == 0xD8) {
            filter = "/DCTDecode";
        }

        std::string stream;
        stream.reserve(bmp.data.size() * 2);
        // Para FlateDecode, precisaríamos comprimir - simplificado: raw
        // Em produção, usar zlib

        objects_[imgId].content = "<< /Type /XObject /Subtype /Image /Width " + std::to_string((int)bmp.size.w)
            + " /Height " + std::to_string((int)bmp.size.h)
            + " /BitsPerComponent 8 /ColorSpace /DeviceRGB /Length " + std::to_string(bmp.data.size())
            + " " + filter + " >>";
        objects_[imgId].isStream = true;
        // Stream data seria escrito em WriteObject

        return (int)images_.size(); // 1-based para /Im1, /Im2...
    }

    void WriteFillStroke() {
        if (currentState_.fillType == FillType::None) {
            if (currentState_.hasStroke) {
                content_ << "S\n";
            } else {
                content_ << "n\n";
            }
        } else if (currentState_.fillType == FillType::Solid) {
            if (currentState_.hasStroke) {
                content_ << "B\n";
            } else {
                content_ << "f\n";
            }
        } else if (currentState_.fillType == FillType::LinearGradient ||
                   currentState_.fillType == FillType::RadialGradient) {
            // Shading já aplicado em SetFillLinearGradient/RadialGradient
            if (currentState_.hasStroke) {
                content_ << "B\n";
            } else {
                content_ << "f\n";
            }
        } else if (currentState_.hasStroke) {
            content_ << "S\n";
        } else {
            content_ << "n\n";
        }
    }

    void WriteLineCap(StrokeStyle::LineCap cap) {
        switch (cap) {
            case StrokeStyle::LineCap::Round: content_ << "1 J\n"; break;
            case StrokeStyle::LineCap::Square: content_ << "2 J\n"; break;
            default: content_ << "0 J\n"; break;
        }
    }

    void WriteLineJoin(StrokeStyle::LineJoin join) {
        switch (join) {
            case StrokeStyle::LineJoin::Round: content_ << "1 j\n"; break;
            case StrokeStyle::LineJoin::Bevel: content_ << "2 j\n"; break;
            default: content_ << "0 j\n"; break;
        }
    }

    static std::string WideToUtf8(const wchar_t* ws) {
        if (!ws) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
        std::string s(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, ws, -1, &s[0], len, nullptr, nullptr);
        return s;
    }

    static std::string EscapePdfString(const std::string& s) {
        std::string r;
        r.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '(': r += "\\("; break;
                case ')': r += "\\)"; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                case '\b': r += "\\b"; break;
                case '\f': r += "\\f"; break;
                default:
                    if (c < 32 || c > 126) {
                        char buf[8];
                        sprintf_s(buf, "\\%03o", (unsigned char)c);
                        r += buf;
                    } else {
                        r += c;
                    }
            }
        }
        return r;
    }

    static std::string ToBase64(const std::vector<uint8_t>& data) {
        static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string r;
        r.reserve((data.size() + 2) / 3 * 4);
        for (size_t i = 0; i < data.size(); i += 3) {
            uint32_t v = data[i] << 16;
            if (i + 1 < data.size()) v |= data[i + 1] << 8;
            if (i + 2 < data.size()) v |= data[i + 2];
            r += alphabet[(v >> 18) & 0x3F];
            r += alphabet[(v >> 12) & 0x3F];
            r += (i + 1 < data.size()) ? alphabet[(v >> 6) & 0x3F] : '=';
            r += (i + 2 < data.size()) ? alphabet[v & 0x3F] : '=';
        }
        return r;
    }
};

} // namespace engines