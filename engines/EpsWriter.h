// EpsWriter.h - PostScript Level 2/3 + EPSF 3.0 Writer implementando VectorSink
// PLANO-MOTORES-PROPRIOS - Motor 1: VizuVector (EPS para impressão profissional)
#pragma once
#include "VectorSink.h"
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>

namespace engines {

class EpsWriter : public VectorSink {
public:
    explicit EpsWriter(const Size& pageSize) : pageSize_(pageSize), inPath_(false), stateDepth_(0),
        bboxSet_(false), minX_(0), minY_(0), maxX_(0), maxY_(0) {
        InitializeDocument();
    }

    ~EpsWriter() override = default;

    bool BeginDocument(const Size& pageSize, const wchar_t* title = nullptr) override {
        pageSize_ = pageSize;
        InitializeDocument();
        if (title) {
            std::string utf8 = WideToUtf8(title);
            doc_ << "%%Title: " << EscapePs(utf8) << "\n";
        }
        return true;
    }

    bool EndDocument() override {
        // Calcular BoundingBox se não foi definido
        if (!bboxSet_) {
            minX_ = 0; minY_ = 0;
            maxX_ = pageSize_.w; maxY_ = pageSize_.h;
        }

        doc_ << content_.str();

        // Escrever BoundingBox
        doc_ << "%%BoundingBox: "
             << (int)floor(minX_) << " " << (int)floor(minY_) << " "
             << (int)ceil(maxX_) << " " << (int)ceil(maxY_) << "\n";

        // HiResBoundingBox para precisão
        doc_ << "%%HiResBoundingBox: "
             << minX_ << " " << minY_ << " " << maxX_ << " " << maxY_ << "\n";

        doc_ << "showpage\n";
        doc_ << "%%EOF\n";
        return true;
    }

    bool SaveToFile(const wchar_t* path) override {
        EndDocument();
        std::wofstream file(path, std::ios::binary);
        if (!file) return false;
        std::string str = doc_.str();
        file.write(str.c_str(), str.size());
        return file.good();
    }

    std::vector<uint8_t> GetBuffer() override {
        EndDocument();
        std::string str = doc_.str();
        return std::vector<uint8_t>(str.begin(), str.end());
    }

    // --- Estados ---
    void PushState() override {
        states_.push_back(currentState_);
        stateDepth_++;
    }

    void PopState() override {
        if (!states_.empty()) {
            currentState_ = states_.back();
            states_.pop_back();
            stateDepth_--;
        }
    }

    void SetTransform(const Matrix& m) override {
        currentState_.transform = m;
        content_ << m.m11 << " " << m.m12 << " " << m.m21 << " " << m.m22 << " " << m.dx << " " << m.dy << " concat\n";
    }

    void MultiplyTransform(const Matrix& m) override {
        currentState_.transform = currentState_.transform * m;
        content_ << m.m11 << " " << m.m12 << " " << m.m21 << " " << m.m22 << " " << m.dx << " " << m.dy << " concat\n";
    }

    void SetOpacity(float opacity) override {
        currentState_.opacity = opacity;
        // PS Level 2: setalpha
        if (opacity >= 1.0f) {
            content_ << "1 setalpha\n";
        } else {
            content_ << opacity << " setalpha\n";
        }
    }

    // --- Fill/Stroke ---
    void SetFillNone() override {
        currentState_.fillType = FillType::None;
    }

    void SetFillSolid(const Color& c) override {
        currentState_.fillType = FillType::Solid;
        currentState_.fillColor = c;
        WritePsColor(c, "setrgbcolor");
    }

    void SetFillLinearGradient(const LinearGradient& g) override {
        currentState_.fillType = FillType::LinearGradient;
        currentState_.linearGrad = g;
        WriteGradientShading(g, "linGrad" + std::to_string(gradId_++));
    }

    void SetFillRadialGradient(const RadialGradient& g) override {
        currentState_.fillType = FillType::RadialGradient;
        currentState_.radialGrad = g;
        WriteGradientShading(g, "radGrad" + std::to_string(gradId_++));
    }

    void SetStroke(const StrokeStyle& style, const Color& color) override {
        currentState_.hasStroke = true;
        currentState_.stroke = style;
        currentState_.strokeColor = color;
        content_ << style.width << " setlinewidth\n";
        WriteLineCap(style.cap);
        WriteLineJoin(style.join);
        if (!style.dashPattern.empty()) {
            content_ << "[";
            for (size_t i = 0; i < style.dashPattern.size(); ++i) {
                if (i) content_ << " ";
                content_ << style.dashPattern[i];
            }
            content_ << "] " << style.dashOffset << " setdash\n";
        } else {
            content_ << "[] 0 setdash\n";
        }
        WritePsColor(color, "setrgbcolor");
    }

    void SetStrokeNone() override {
        currentState_.hasStroke = false;
    }

    // --- Primitivas ---
    void DrawRect(const Rect& r) override {
        content_ << r.x << " " << r.y << " " << r.w << " " << r.h << " rect\n";
        WriteFillStroke();
        UpdateBBox(r.x, r.y, r.x + r.w, r.y + r.h);
    }

    void DrawRoundedRect(const Rect& r, float rx, float ry) override {
        // PostScript não tem rounded rect nativo, construir com curvas
        float x = r.x, y = r.y, w = r.w, h = r.h;
        rx = std::min(rx, w / 2);
        ry = std::min(ry, h / 2);

        content_ << "newpath\n";
        content_ << x + rx << " " << y << " moveto\n";
        content_ << x + w - rx << " " << y << " lineto\n";
        if (rx > 0) {
            content_ << x + w << " " << y << " " << x + w << " " << y + ry << " " << x + w - rx << " " << y + ry << " curveto\n";
        }
        content_ << x + w << " " << y + h - ry << " lineto\n";
        if (ry > 0) {
            content_ << x + w << " " << y + h << " " << x + w - rx << " " << y + h << " " << x + w - rx << " " << y + h - ry << " curveto\n";
        }
        content_ << x + rx << " " << y + h << " lineto\n";
        if (rx > 0) {
            content_ << x << " " << y + h << " " << x << " " << y + h - ry << " " << x + rx << " " << y + h - ry << " curveto\n";
        }
        content_ << x << " " << y + ry << " lineto\n";
        if (ry > 0) {
            content_ << x << " " << y << " " << x + rx << " " << y << " " << x + rx << " " << y + ry << " curveto\n";
        }
        content_ << "closepath\n";
        WriteFillStroke();
        UpdateBBox(x, y, x + w, y + h);
    }

    void DrawEllipse(const Point& center, float rx, float ry) override {
        // Ellipse via scale + circle
        content_ << "newpath\n";
        content_ << center.x << " " << center.y << " translate\n";
        content_ << rx << " " << ry << " scale\n";
        content_ << "0 0 1 0 360 arc\n";
        content_ << (1.0f/ry) << " " << (1.0f/rx) << " scale\n";
        content_ << center.x << " " << center.y << " neg translate\n";
        WriteFillStroke();
        UpdateBBox(center.x - rx, center.y - ry, center.x + rx, center.y + ry);
    }

    void DrawLine(const Point& p1, const Point& p2) override {
        content_ << "newpath\n";
        content_ << p1.x << " " << p1.y << " moveto\n";
        content_ << p2.x << " " << p2.y << " lineto\n";
        if (currentState_.hasStroke) {
            WriteStrokeOnly();
        }
        UpdateBBox(p1.x, p1.y, p2.x, p2.y);
    }

    // --- Paths ---
    void BeginPath() override {
        inPath_ = true;
        content_ << "newpath\n";
    }

    void MoveTo(const Point& p) override {
        if (inPath_) {
            content_ << p.x << " " << p.y << " moveto\n";
        }
    }

    void LineTo(const Point& p) override {
        if (inPath_) {
            content_ << p.x << " " << p.y << " lineto\n";
        }
    }

    void CubicTo(const Point& c1, const Point& c2, const Point& p) override {
        if (inPath_) {
            content_ << c1.x << " " << c1.y << " " << c2.x << " " << c2.y << " " << p.x << " " << p.y << " curveto\n";
        }
    }

    void QuadraticTo(const Point& c, const Point& p) override {
        if (inPath_) {
            // Converter quadrática para cúbica
            // Q c p -> C (2/3*c + 1/3*current) (2/3*c + 1/3*p) p
            // Para simplificar, emitimos como curveto com ponto de controle duplicado
            content_ << c.x << " " << c.y << " " << c.x << " " << c.y << " " << p.x << " " << p.y << " curveto\n";
        }
    }

    void ClosePath() override {
        if (inPath_) {
            content_ << "closepath\n";
        }
    }

    void EndPath(bool fill, bool stroke) override {
        if (!inPath_) return;
        if (fill && currentState_.fillType != FillType::None) {
            if (stroke && currentState_.hasStroke) {
                content_ << "gsave fill grestore stroke\n";
            } else {
                content_ << "fill\n";
            }
        } else if (stroke && currentState_.hasStroke) {
            content_ << "stroke\n";
        } else {
            content_ << "newpath\n"; // descartar
        }
        inPath_ = false;
    }

    // --- Texto (como curvas) ---
    void DrawGlyphRun(const std::vector<Point>& glyphPositions,
                      const std::vector<uint16_t>& glyphIndices,
                      const std::vector<float>& advances,
                      float fontSize,
                      const wchar_t* fontFamily) override {
        // EPS: texto como curvas já convertidas - não implementado aqui
        // O chamador deve converter glifos para caminhos antes
    }

    // --- Imagens ---
    void DrawBitmap(const EmbeddedBitmap& bmp, const Rect& destRect) override {
        // Embutir como imagem PostScript (DCTDecode para JPEG, FlateDecode para PNG)
        // Para simplicidade, usar image operator com dados base85
        std::string encoded = ToAscii85(bmp.data);

        content_ << "gsave\n";
        content_ << destRect.x << " " << destRect.y << " translate\n";
        content_ << destRect.w << " " << destRect.h << " scale\n";
        content_ << "<< /ImageType 1 /Width " << (int)bmp.size.w
                 << " /Height " << (int)bmp.size.h
                 << " /BitsPerComponent 8 /ColorSpace /DeviceRGB /Decode [0 1 0 1 0 1] "
                 << "/DataSource currentfile /ASCII85Decode filter >> image\n";

        // Escrever dados codificados em linhas de até 80 chars
        for (size_t i = 0; i < encoded.size(); i += 80) {
            content_ << encoded.substr(i, 80) << "\n";
        }
        content_ << "~>\n";
        content_ << "grestore\n";

        if (bmp.opacity < 1) {
            // Alpha via transparência Level 2 - complexo, omitido v1
        }

        UpdateBBox(destRect.x, destRect.y, destRect.x + destRect.w, destRect.y + destRect.h);
    }

    // --- Clip ---
    void PushClipRect(const Rect& r) override {
        clipId_++;
        content_ << "gsave\n";
        content_ << "newpath\n";
        content_ << r.x << " " << r.y << " " << r.w << " " << r.h << " rect\n";
        content_ << "clip\n";
        clipStack_.push_back(clipId_);
    }

    void PopClip() override {
        if (!clipStack_.empty()) {
            clipStack_.pop_back();
            content_ << "grestore\n";
        }
    }

    // --- Metadados ---
    void SetTitle(const wchar_t* title) override {
        std::string utf8 = WideToUtf8(title);
        doc_ << "%%Title: " << EscapePs(utf8) << "\n";
    }

    void SetAuthor(const wchar_t* author) override {
        std::string utf8 = WideToUtf8(author);
        doc_ << "%%Creator: " << EscapePs(utf8) << "\n";
    }

    void SetCreator(const wchar_t* creator) override {
        std::string utf8 = WideToUtf8(creator);
        doc_ << "%%Creator: " << EscapePs(utf8) << "\n";
    }

    void SetKeywords(const wchar_t* keywords) override {
        std::string utf8 = WideToUtf8(keywords);
        doc_ << "%%Keywords: " << EscapePs(utf8) << "\n";
    }

    // --- CMYK (EPS suporta nativamente) ---
    void SetFillCMYK(float c, float m, float y, float k) override {
        currentState_.fillType = FillType::Solid;
        content_ << c << " " << m << " " << y << " " << k << " setcmykcolor\n";
        cmykFill_ = {c, m, y, k};
    }

    void SetStrokeCMYK(float c, float m, float y, float k) override {
        content_ << c << " " << m << " " << y << " " << k << " setcmykcolor\n";
        cmykStroke_ = {c, m, y, k};
    }

    void EmbedICCProfile(const uint8_t* data, size_t size) override {
        // EPS: ICC profile como comentário estruturado
        std::string base64 = ToBase64(std::vector<uint8_t>(data, data + size));
        doc_ << "%%ICCProfile: " << base64 << "\n";
    }

private:
    Size pageSize_;
    std::ostringstream doc_;
    std::ostringstream content_;
    std::vector<DrawState> states_;
    DrawState currentState_;
    bool inPath_ = false;
    int stateDepth_ = 0;
    int gradId_ = 0;
    int clipId_ = 0;
    std::vector<int> clipStack_;
    struct { float c, m, y, k; } cmykFill_, cmykStroke_;

    // BoundingBox
    bool bboxSet_ = false;
    float minX_, minY_, maxX_, maxY_;

    void InitializeDocument() {
        doc_.str("");
        content_.str("");
        states_.clear();
        currentState_ = DrawState();
        inPath_ = false;
        stateDepth_ = 0;
        gradId_ = 0;
        clipId_ = 0;
        clipStack_.clear();
        bboxSet_ = false;
        minX_ = minY_ = maxX_ = maxY_ = 0;

        doc_ << "%!PS-Adobe-3.0 EPSF-3.0\n";
        doc_ << "%%Creator: VizuVector EpsWriter\n";
        doc_ << "%%CreationDate: " << GetCurrentDate() << "\n";
        doc_ << "%%Pages: 1\n";
        doc_ << "%%DocumentData: Clean7Bit\n";
        doc_ << "%%LanguageLevel: 3\n";
        doc_ << "%%DocumentNeededResources: font\n";
        doc_ << "%%DocumentSuppliedResources: procset\n";
        doc_ << "%%PageOrder: Ascend\n";
        doc_ << "%%Orientation: Portrait\n";
        doc_ << "%%BoundingBox: (atend)\n";
        doc_ << "%%HiResBoundingBox: (atend)\n";
        doc_ << "%%EndComments\n";
        doc_ << "%%BeginProlog\n";
        WriteProlog();
        doc_ << "%%EndProlog\n";
        doc_ << "%%Page: 1 1\n";
        doc_ << "%%BeginPageSetup\n";
        doc_ << pageSize_.w << " " << pageSize_.h << " scale\n";
        doc_ << "%%EndPageSetup\n";
    }

    void WriteProlog() {
        // Definições de procedimentos úteis
        doc_ << "/rect { 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto neg 0 rlineto closepath } bind def\n";
        doc_ << "/setalpha { /ca exch def /CA exch def } bind def\n"; // Level 2
        doc_ << "/fill { gsave fill grestore } bind def\n"; // wrapper para preservar estado
    }

    void WriteFillStroke() {
        if (currentState_.fillType == FillType::None) {
            if (currentState_.hasStroke) {
                WriteStrokeOnly();
            } else {
                content_ << "newpath\n"; // descartar
            }
        } else if (currentState_.fillType == FillType::Solid) {
            if (currentState_.hasStroke) {
                content_ << "gsave fill grestore stroke\n";
            } else {
                content_ << "fill\n";
            }
        } else if (currentState_.fillType == FillType::LinearGradient ||
                   currentState_.fillType == FillType::RadialGradient) {
            // Shading patterns já definidos em SetFillLinearGradient/RadialGradient
            if (currentState_.hasStroke) {
                content_ << "gsave fill grestore stroke\n";
            } else {
                content_ << "fill\n";
            }
        } else if (currentState_.hasStroke) {
            WriteStrokeOnly();
        } else {
            content_ << "newpath\n";
        }
    }

    void WriteStrokeOnly() {
        if (currentState_.hasStroke) {
            content_ << "stroke\n";
        } else {
            content_ << "newpath\n";
        }
    }

    void WritePsColor(const Color& c, const char* op) {
        content_ << c.r << " " << c.g << " " << c.b << " " << op << "\n";
        if (c.a < 1) {
            content_ << c.a << " setalpha\n";
        }
    }

    void WriteLineCap(StrokeStyle::LineCap cap) {
        switch (cap) {
            case StrokeStyle::LineCap::Round: content_ << "1 setlinecap\n"; break;
            case StrokeStyle::LineCap::Square: content_ << "2 setlinecap\n"; break;
            default: content_ << "0 setlinecap\n"; break;
        }
    }

    void WriteLineJoin(StrokeStyle::LineJoin join) {
        switch (join) {
            case StrokeStyle::LineJoin::Round: content_ << "1 setlinejoin\n"; break;
            case StrokeStyle::LineJoin::Bevel: content_ << "2 setlinejoin\n"; break;
            default: content_ << "0 setlinejoin\n"; break;
        }
    }

    void WriteGradientShading(const LinearGradient& g, const std::string& id) {
        // PostScript Level 3 shading (Type 2 axial)
        content_ << "<< /ShadingType 2 /ColorSpace /DeviceRGB /Domain [0 1] /Coords ["
                 << g.start.x << " " << g.start.y << " " << g.end.x << " " << g.end.y << "] "
                 << "/Function << /FunctionType 2 /Domain [0 1] /C0 [";
        if (!g.stops.empty()) {
            content_ << g.stops.front().color.r << " " << g.stops.front().color.g << " " << g.stops.front().color.b;
        } else {
            content_ << "0 0 0";
        }
        content_ << "] /C1 [";
        if (!g.stops.empty()) {
            content_ << g.stops.back().color.r << " " << g.stops.back().color.g << " " << g.stops.back().color.b;
        } else {
            content_ << "1 1 1";
        }
        content_ << "] /N 1 >> >> " << id << " exch def\n";
        content_ << "/" << id << " shfill\n";
    }

    void WriteGradientShading(const RadialGradient& g, const std::string& id) {
        // PostScript Level 3 shading (Type 3 radial)
        content_ << "<< /ShadingType 3 /ColorSpace /DeviceRGB /Domain [0 1] /Coords ["
                 << g.center.x << " " << g.center.y << " 0 "
                 << g.focalPoint.x << " " << g.focalPoint.y << " " << g.radiusX << "] "
                 << "/Function << /FunctionType 2 /Domain [0 1] /C0 [";
        if (!g.stops.empty()) {
            content_ << g.stops.front().color.r << " " << g.stops.front().color.g << " " << g.stops.front().color.b;
        } else {
            content_ << "0 0 0";
        }
        content_ << "] /C1 [";
        if (!g.stops.empty()) {
            content_ << g.stops.back().color.r << " " << g.stops.back().color.g << " " << g.stops.back().color.b;
        } else {
            content_ << "1 1 1";
        }
        content_ << "] /N 1 >> >> " << id << " exch def\n";
        content_ << "/" << id << " shfill\n";
    }

    void UpdateBBox(float x1, float y1, float x2, float y2) {
        float nx1 = std::min(x1, x2);
        float ny1 = std::min(y1, y2);
        float nx2 = std::max(x1, x2);
        float ny2 = std::max(y1, y2);

        if (!bboxSet_) {
            minX_ = nx1; minY_ = ny1; maxX_ = nx2; maxY_ = ny2;
            bboxSet_ = true;
        } else {
            minX_ = std::min(minX_, nx1);
            minY_ = std::min(minY_, ny1);
            maxX_ = std::max(maxX_, nx2);
            maxY_ = std::max(maxY_, ny2);
        }
    }

    static std::string WideToUtf8(const wchar_t* ws) {
        if (!ws) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
        std::string s(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, ws, -1, &s[0], len, nullptr, nullptr);
        return s;
    }

    static std::string EscapePs(const std::string& s) {
        std::string r;
        r.reserve(s.size());
        for (char c : s) {
            if (c == '(' || c == ')' || c == '\\') r += '\\';
            r += c;
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

    static std::string ToAscii85(const std::vector<uint8_t>& data) {
        std::string r;
        r.reserve((data.size() + 3) / 4 * 5);
        for (size_t i = 0; i < data.size(); i += 4) {
            uint32_t v = 0;
            int n = std::min(4, (int)data.size() - (int)i);
            for (int j = 0; j < n; ++j) {
                v = (v << 8) | data[i + j];
            }
            // Pad com zeros
            v <<= (4 - n) * 8;

            // Converter para 5 chars ASCII85
            for (int j = 4; j >= 0; --j) {
                uint32_t divisor = 1;
                for (int k = 0; k < j; ++k) divisor *= 85;
                char c = (v / divisor) % 85 + 33;
                r += c;
            }
            if (n < 4) {
                // Truncar último grupo
                r.resize(r.size() - (4 - n));
                break;
            }
        }
        return r;
    }

    static std::string GetCurrentDate() {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char buf[64];
        sprintf_s(buf, "%04d-%02d-%02d %02d:%02d:%02d",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        return buf;
    }
};

} // namespace engines