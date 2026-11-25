// VectorSink.h - Interface comum para escritores vetoriais (SVG, EPS, PDF, IDML)
// PLANO-MOTORES-PROPRIOS - Motor 1: VizuVector
#pragma once
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include <map>

namespace engines {

// Cor RGBA (premultiplied alpha compatível com D2D)
struct Color {
    float r = 0, g = 0, b = 0, a = 1;
    static Color FromUINT(UINT rgb, float a = 1) {
        return { ((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a };
    }
    UINT ToUINT() const {
        return (UINT)(r * 255) << 16 | (UINT)(g * 255) << 8 | (UINT)(b * 255);
    }
};

// Ponto 2D
struct Point { float x, y; };

// Tamanho
struct Size { float w, h; };

// Retângulo
struct Rect { float x, y, w, h; };

// Transformação 2D (matriz 3x2 estilo D2D)
struct Matrix {
    float m11 = 1, m12 = 0, m21 = 0, m22 = 1, dx = 0, dy = 0;
    static Matrix Identity() { return {}; }
    static Matrix Scale(float sx, float sy) { Matrix m; m.m11 = sx; m.m22 = sy; return m; }
    static Matrix Translate(float x, float y) { Matrix m; m.dx = x; m.dy = y; return m; }
    static Matrix Rotate(float rad) {
        float c = cosf(rad), s = sinf(rad);
        Matrix m; m.m11 = c; m.m12 = -s; m.m21 = s; m.m22 = c; return m;
    }
    Matrix operator*(const Matrix& o) const {
        Matrix r;
        r.m11 = m11 * o.m11 + m12 * o.m21;
        r.m12 = m11 * o.m12 + m12 * o.m22;
        r.m21 = m21 * o.m11 + m22 * o.m21;
        r.m22 = m21 * o.m12 + m22 * o.m22;
        r.dx = m11 * o.dx + m12 * o.dy + dx;
        r.dy = m21 * o.dx + m22 * o.dy + dy;
        return r;
    }
    Point Transform(Point p) const {
        return { m11 * p.x + m12 * p.y + dx, m21 * p.x + m22 * p.y + dy };
    }
};

// Estilo de preenchimento
enum class FillType { None, Solid, LinearGradient, RadialGradient };

struct GradientStop { float offset; Color color; };

struct LinearGradient {
    Point start, end;
    std::vector<GradientStop> stops;
    Matrix transform = Matrix::Identity();
};

struct RadialGradient {
    Point center;
    float radiusX, radiusY;
    Point focalPoint;
    std::vector<GradientStop> stops;
    Matrix transform = Matrix::Identity();
};

// Estilo de traço (stroke)
struct StrokeStyle {
    float width = 1;
    enum class LineCap { Flat, Round, Square } cap = LineCap::Flat;
    enum class LineJoin { Miter, Round, Bevel } join = LineJoin::Miter;
    float miterLimit = 10;
    std::vector<float> dashPattern;
    float dashOffset = 0;
};

// Bitmap embutido
struct EmbeddedBitmap {
    std::vector<uint8_t> data; // PNG/JPEG bytes
    Size size;                 // largura/altura em pixels
    Matrix transform = Matrix::Identity();
    float opacity = 1;
};

// Contexto de desenho atual (push/pop)
struct DrawState {
    Matrix transform = Matrix::Identity();
    FillType fillType = FillType::None;
    Color fillColor;
    LinearGradient linearGrad;
    RadialGradient radialGrad;
    bool hasStroke = false;
    StrokeStyle stroke;
    Color strokeColor;
    float opacity = 1;
    bool clipActive = false;
    Rect clipRect;
};

// Interface principal - VectorSink
struct VectorSink {
    virtual ~VectorSink() = default;

    // Inicia documento
    virtual bool BeginDocument(const Size& pageSize, const wchar_t* title = nullptr) = 0;

    // Finaliza documento
    virtual bool EndDocument() = 0;

    // Salva para arquivo
    virtual bool SaveToFile(const wchar_t* path) = 0;

    // Obtém buffer de memória (para embedding)
    virtual std::vector<uint8_t> GetBuffer() = 0;

    // --- Estados ---
    virtual void PushState() = 0;
    virtual void PopState() = 0;
    virtual void SetTransform(const Matrix& m) = 0;
    virtual void MultiplyTransform(const Matrix& m) = 0;
    virtual void SetOpacity(float opacity) = 0;

    // --- Fill/Stroke ---
    virtual void SetFillNone() = 0;
    virtual void SetFillSolid(const Color& c) = 0;
    virtual void SetFillLinearGradient(const LinearGradient& g) = 0;
    virtual void SetFillRadialGradient(const RadialGradient& g) = 0;
    virtual void SetStroke(const StrokeStyle& style, const Color& color) = 0;
    virtual void SetStrokeNone() = 0;

    // --- Primitivas ---
    virtual void DrawRect(const Rect& r) = 0;
    virtual void DrawRoundedRect(const Rect& r, float rx, float ry) = 0;
    virtual void DrawEllipse(const Point& center, float rx, float ry) = 0;
    virtual void DrawLine(const Point& p1, const Point& p2) = 0;

    // --- Paths (curvas Bézier) ---
    virtual void BeginPath() = 0;
    virtual void MoveTo(const Point& p) = 0;
    virtual void LineTo(const Point& p) = 0;
    virtual void CubicTo(const Point& c1, const Point& c2, const Point& p) = 0;
    virtual void QuadraticTo(const Point& c, const Point& p) = 0;
    virtual void ClosePath() = 0;
    virtual void EndPath(bool fill, bool stroke) = 0;

    // --- Texto (já convertido em curvas) ---
    virtual void DrawGlyphRun(const std::vector<Point>& glyphPositions,
                              const std::vector<uint16_t>& glyphIndices,
                              const std::vector<float>& advances,
                              float fontSize,
                              const wchar_t* fontFamily) = 0;

    // --- Imagens ---
    virtual void DrawBitmap(const EmbeddedBitmap& bmp, const Rect& destRect) = 0;

    // --- Clip ---
    virtual void PushClipRect(const Rect& r) = 0;
    virtual void PopClip() = 0;

    // --- Metadados ---
    virtual void SetTitle(const wchar_t* title) = 0;
    virtual void SetAuthor(const wchar_t* author) = 0;
    virtual void SetCreator(const wchar_t* creator) = 0;
    virtual void SetKeywords(const wchar_t* keywords) = 0;

    // --- CMYK support (para EPS/PDF) ---
    virtual void SetFillCMYK(float c, float m, float y, float k) = 0;
    virtual void SetStrokeCMYK(float c, float m, float y, float k) = 0;

    // --- ICC Profile embedding ---
    virtual void EmbedICCProfile(const uint8_t* data, size_t size) = 0;
};

// Factory para criar writers
struct VectorWriterFactory {
    enum Type { SVG, EPS, PDF, IDML };

    static VectorSink* Create(Type type, const Size& pageSize);
    static void Destroy(VectorSink* sink);
};

// Helper para converter D2D geometries para VectorSink
struct PathConverter {
    // Converte ID2D1PathGeometry para chamadas VectorSink
    static void Convert(ID2D1PathGeometry* geom, VectorSink* sink, const Matrix& transform = Matrix::Identity());

    // Converte ID2D1Geometry (qualquer tipo) recursivamente
    static void ConvertGeometry(ID2D1Geometry* geom, VectorSink* sink, const Matrix& transform = Matrix::Identity());
};

} // namespace engines