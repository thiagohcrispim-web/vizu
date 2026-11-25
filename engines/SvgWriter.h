// SvgWriter.h - SVG 1.1 Writer implementando VectorSink
// PLANO-MOTORES-PROPRIOS - Motor 1: VizuVector (SVG primeiro para validar interface)
#pragma once
#include "VectorSink.h"
#include <sstream>
#include <fstream>
#include <memory>

namespace engines {

class SvgWriter : public VectorSink {
public:
    explicit SvgWriter(const Size& pageSize) : pageSize_(pageSize), inPath_(false), stateDepth_(0) {
        doc_ << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        doc_ << "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" ";
        doc_ << "width=\"" << pageSize.w << "\" height=\"" << pageSize.h << "\" viewBox=\"0 0 " << pageSize.w << " " << pageSize.h << "\">\n";
        doc_ << "<defs>\n</defs>\n";
        content_.str("");
    }

    ~SvgWriter() override = default;

    bool BeginDocument(const Size& pageSize, const wchar_t* title = nullptr) override {
        pageSize_ = pageSize;
        if (title) {
            std::string utf8 = WideToUtf8(title);
            doc_ << "<title>" << EscapeXml(utf8) << "</title>\n";
        }
        return true;
    }

    bool EndDocument() override {
        doc_ << content_.str();
        doc_ << "</svg>\n";
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
        WriteTransform();
    }

    void MultiplyTransform(const Matrix& m) override {
        currentState_.transform = currentState_.transform * m;
        WriteTransform();
    }

    void SetOpacity(float opacity) override {
        currentState_.opacity = opacity;
    }

    // --- Fill/Stroke ---
    void SetFillNone() override {
        currentState_.fillType = FillType::None;
    }

    void SetFillSolid(const Color& c) override {
        currentState_.fillType = FillType::Solid;
        currentState_.fillColor = c;
    }

    void SetFillLinearGradient(const LinearGradient& g) override {
        currentState_.fillType = FillType::LinearGradient;
        currentState_.linearGrad = g;
        WriteGradientDef(g, "linGrad" + std::to_string(gradId_++));
    }

    void SetFillRadialGradient(const RadialGradient& g) override {
        currentState_.fillType = FillType::RadialGradient;
        currentState_.radialGrad = g;
        WriteGradientDef(g, "radGrad" + std::to_string(gradId_++));
    }

    void SetStroke(const StrokeStyle& style, const Color& color) override {
        currentState_.hasStroke = true;
        currentState_.stroke = style;
        currentState_.strokeColor = color;
    }

    void SetStrokeNone() override {
        currentState_.hasStroke = false;
    }

    // --- Primitivas ---
    void DrawRect(const Rect& r) override {
        ApplyTransform();
        content_ << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.w << "\" height=\"" << r.h << "\" ";
        WriteFillStroke();
        content_ << "/>\n";
        ResetTransform();
    }

    void DrawRoundedRect(const Rect& r, float rx, float ry) override {
        ApplyTransform();
        content_ << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\"" << r.w << "\" height=\"" << r.h << "\" rx=\"" << rx << "\" ry=\"" << ry << "\" ";
        WriteFillStroke();
        content_ << "/>\n";
        ResetTransform();
    }

    void DrawEllipse(const Point& center, float rx, float ry) override {
        ApplyTransform();
        content_ << "<ellipse cx=\"" << center.x << "\" cy=\"" << center.y << "\" rx=\"" << rx << "\" ry=\"" << ry << "\" ";
        WriteFillStroke();
        content_ << "/>\n";
        ResetTransform();
    }

    void DrawLine(const Point& p1, const Point& p2) override {
        ApplyTransform();
        content_ << "<line x1=\"" << p1.x << "\" y1=\"" << p1.y << "\" x2=\"" << p2.x << "\" y2=\"" << p2.y << "\" ";
        WriteFillStroke();
        content_ << "/>\n";
        ResetTransform();
    }

    // --- Paths ---
    void BeginPath() override {
        inPath_ = true;
        pathData_.str("");
    }

    void MoveTo(const Point& p) override {
        if (inPath_) pathData_ << "M " << p.x << " " << p.y << " ";
    }

    void LineTo(const Point& p) override {
        if (inPath_) pathData_ << "L " << p.x << " " << p.y << " ";
    }

    void CubicTo(const Point& c1, const Point& c2, const Point& p) override {
        if (inPath_) pathData_ << "C " << c1.x << " " << c1.y << " " << c2.x << " " << c2.y << " " << p.x << " " << p.y << " ";
    }

    void QuadraticTo(const Point& c, const Point& p) override {
        if (inPath_) pathData_ << "Q " << c.x << " " << c.y << " " << p.x << " " << p.y << " ";
    }

    void ClosePath() override {
        if (inPath_) pathData_ << "Z ";
    }

    void EndPath(bool fill, bool stroke) override {
        if (!inPath_) return;
        ApplyTransform();
        content_ << "<path d=\"" << pathData_.str() << "\" ";
        if (fill) {
            // fill handled by WriteFillStroke
        } else {
            content_ << "fill=\"none\" ";
        }
        if (stroke) {
            WriteStrokeAttrs();
        } else {
            content_ << "stroke=\"none\" ";
        }
        content_ << "/>\n";
        ResetTransform();
        inPath_ = false;
    }

    // --- Texto (como curvas) ---
    void DrawGlyphRun(const std::vector<Point>& glyphPositions,
                      const std::vector<uint16_t>& glyphIndices,
                      const std::vector<float>& advances,
                      float fontSize,
                      const wchar_t* fontFamily) override {
        // Para SVG, texto como curvas - não implementado ainda, seria necessário outline
        // Placeholder: desenha como texto simples (não curvas)
        ApplyTransform();
        content_ << "<text font-size=\"" << fontSize << "\" font-family=\"" << WideToUtf8(fontFamily) << "\" ";
        WriteFillStroke();
        content_ << ">";
        // Não temos o texto original, só glifos - limitação
        content_ << "</text>\n";
        ResetTransform();
    }

    // --- Imagens ---
    void DrawBitmap(const EmbeddedBitmap& bmp, const Rect& destRect) override {
        ApplyTransform();
        // Embutir como base64 data URI
        std::string base64 = ToBase64(bmp.data);
        content_ << "<image x=\"" << destRect.x << "\" y=\"" << destRect.y
                 << "\" width=\"" << destRect.w << "\" height=\"" << destRect.h
                 << "\" xlink:href=\"data:image/png;base64," << base64 << "\" ";
        if (bmp.opacity < 1) {
            content_ << "opacity=\"" << bmp.opacity << "\" ";
        }
        content_ << "/>\n";
        ResetTransform();
    }

    // --- Clip ---
    void PushClipRect(const Rect& r) override {
        clipId_++;
        content_ << "<clipPath id=\"clip" << clipId_ << "\"><rect x=\"" << r.x << "\" y=\"" << r.y
                 << "\" width=\"" << r.w << "\" height=\"" << r.h << "\"/></clipPath>\n";
        clipStack_.push_back(clipId_);
        content_ << "<g clip-path=\"url(#clip" << clipId_ << ")\">\n";
    }

    void PopClip() override {
        if (!clipStack_.empty()) {
            clipStack_.pop_back();
            content_ << "</g>\n";
        }
    }

    // --- Metadados ---
    void SetTitle(const wchar_t* title) override {
        std::string utf8 = WideToUtf8(title);
        doc_ << "<title>" << EscapeXml(utf8) << "</title>\n";
    }

    void SetAuthor(const wchar_t* author) override {
        std::string utf8 = WideToUtf8(author);
        doc_ << "<desc>Author: " << EscapeXml(utf8) << "</desc>\n";
    }

    void SetCreator(const wchar_t* creator) override {
        std::string utf8 = WideToUtf8(creator);
        doc_ << "<metadata>Created by: " << EscapeXml(utf8) << "</metadata>\n";
    }

    void SetKeywords(const wchar_t* keywords) override {
        std::string utf8 = WideToUtf8(keywords);
        doc_ << "<metadata>Keywords: " << EscapeXml(utf8) << "</metadata>\n";
    }

    // --- CMYK (SVG não suporta nativamente, gravar como data attribute) ---
    void SetFillCMYK(float c, float m, float y, float k) override {
        currentState_.fillType = FillType::Solid;
        // Converter CMYK para RGB aproximado
        float r = (1 - c) * (1 - k);
        float g = (1 - m) * (1 - k);
        float b = (1 - y) * (1 - k);
        currentState_.fillColor = {r, g, b, 1};
        // Guardar CMYK original como data attribute
        cmykFill_ = {c, m, y, k};
    }

    void SetStrokeCMYK(float c, float m, float y, float k) override {
        cmykStroke_ = {c, m, y, k};
    }

    void EmbedICCProfile(const uint8_t* data, size_t size) override {
        // SVG: embutir como base64 em metadata
        std::string base64 = ToBase64(std::vector<uint8_t>(data, data + size));
        doc_ << "<metadata>ICC Profile: " << base64 << "</metadata>\n";
    }

private:
    Size pageSize_;
    std::ostringstream doc_;
    std::ostringstream content_;
    std::vector<DrawState> states_;
    DrawState currentState_;
    bool inPath_ = false;
    std::ostringstream pathData_;
    int stateDepth_ = 0;
    int gradId_ = 0;
    int clipId_ = 0;
    std::vector<int> clipStack_;
    struct { float c, m, y, k; } cmykFill_, cmykStroke_;

    void ApplyTransform() {
        const Matrix& m = currentState_.transform;
        if (m.m11 != 1 || m.m12 != 0 || m.m21 != 0 || m.m22 != 1 || m.dx != 0 || m.dy != 0) {
            content_ << "transform=\"matrix(" << m.m11 << " " << m.m12 << " " << m.m21 << " " << m.m22 << " " << m.dx << " " << m.dy << ")\" ";
        }
    }

    void ResetTransform() {
        // Transform already applied inline
    }

    void WriteTransform() {
        // Handled in ApplyTransform
    }

    void WriteFillStroke() {
        if (currentState_.fillType == FillType::None) {
            content_ << "fill=\"none\" ";
        } else if (currentState_.fillType == FillType::Solid) {
            content_ << "fill=\"rgb(" << (int)(currentState_.fillColor.r * 255) << ","
                     << (int)(currentState_.fillColor.g * 255) << ","
                     << (int)(currentState_.fillColor.b * 255) << ")\" ";
            if (currentState_.fillColor.a < 1) {
                content_ << "fill-opacity=\"" << currentState_.fillColor.a << "\" ";
            }
        } else if (currentState_.fillType == FillType::LinearGradient) {
            content_ << "fill=\"url(#linGrad" << (gradId_ - 1) << ")\" ";
        } else if (currentState_.fillType == FillType::RadialGradient) {
            content_ << "fill=\"url(#radGrad" << (gradId_ - 1) << ")\" ";
        }

        if (currentState_.hasStroke) {
            WriteStrokeAttrs();
        } else {
            content_ << "stroke=\"none\" ";
        }

        if (currentState_.opacity < 1) {
            content_ << "opacity=\"" << currentState_.opacity << "\" ";
        }
    }

    void WriteStrokeAttrs() {
        const auto& s = currentState_.stroke;
        content_ << "stroke=\"rgb(" << (int)(currentState_.strokeColor.r * 255) << ","
                 << (int)(currentState_.strokeColor.g * 255) << ","
                 << (int)(currentState_.strokeColor.b * 255) << ")\" ";
        content_ << "stroke-width=\"" << s.width << "\" ";
        if (s.cap == StrokeStyle::LineCap::Round) content_ << "stroke-linecap=\"round\" ";
        else if (s.cap == StrokeStyle::LineCap::Square) content_ << "stroke-linecap=\"square\" ";
        if (s.join == StrokeStyle::LineJoin::Round) content_ << "stroke-linejoin=\"round\" ";
        else if (s.join == StrokeStyle::LineJoin::Bevel) content_ << "stroke-linejoin=\"bevel\" ";
        if (!s.dashPattern.empty()) {
            content_ << "stroke-dasharray=\"";
            for (size_t i = 0; i < s.dashPattern.size(); ++i) {
                if (i) content_ << ",";
                content_ << s.dashPattern[i];
            }
            content_ << "\" ";
            if (s.dashOffset != 0) content_ << "stroke-dashoffset=\"" << s.dashOffset << "\" ";
        }
        if (currentState_.strokeColor.a < 1) {
            content_ << "stroke-opacity=\"" << currentState_.strokeColor.a << "\" ";
        }
    }

    void WriteGradientDef(const LinearGradient& g, const std::string& id) {
        content_ << "<linearGradient id=\"" << id << "\" gradientUnits=\"userSpaceOnUse\" ";
        content_ << "x1=\"" << g.start.x << "\" y1=\"" << g.start.y << "\" x2=\"" << g.end.x << "\" y2=\"" << g.end.y << "\">\n";
        for (const auto& stop : g.stops) {
            content_ << "<stop offset=\"" << (stop.offset * 100) << "%\" stop-color=\"rgb("
                     << (int)(stop.color.r * 255) << "," << (int)(stop.color.g * 255) << "," << (int)(stop.color.b * 255) << ")\"";
            if (stop.color.a < 1) content_ << " stop-opacity=\"" << stop.color.a << "\"";
            content_ << "/>\n";
        }
        content_ << "</linearGradient>\n";
    }

    void WriteGradientDef(const RadialGradient& g, const std::string& id) {
        content_ << "<radialGradient id=\"" << id << "\" gradientUnits=\"userSpaceOnUse\" ";
        content_ << "cx=\"" << g.center.x << "\" cy=\"" << g.center.y << "\" r=\"" << g.radiusX << "\" ";
        content_ << "fx=\"" << g.focalPoint.x << "\" fy=\"" << g.focalPoint.y << "\">\n";
        for (const auto& stop : g.stops) {
            content_ << "<stop offset=\"" << (stop.offset * 100) << "%\" stop-color=\"rgb("
                     << (int)(stop.color.r * 255) << "," << (int)(stop.color.g * 255) << "," << (int)(stop.color.b * 255) << ")\"";
            if (stop.color.a < 1) content_ << " stop-opacity=\"" << stop.color.a << "\"";
            content_ << "/>\n";
        }
        content_ << "</radialGradient>\n";
    }

    static std::string WideToUtf8(const wchar_t* ws) {
        if (!ws) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
        std::string s(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, ws, -1, &s[0], len, nullptr, nullptr);
        return s;
    }

    static std::string EscapeXml(const std::string& s) {
        std::string r;
        r.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '&': r += "&"; break;
                case '<': r += "<"; break;
                case '>': r += ">"; break;
                case '"': r += """; break;
                case '\'': r += "&apos;"; break;
                default: r += c;
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