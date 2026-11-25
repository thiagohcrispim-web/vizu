// VectorSink.cpp - Implementation of VectorWriterFactory
// PLANO-MOTORES-PROPRIOS - Motor 1: VizuVector
#include "VectorSink.h"
#include "SvgWriter.h"
#include "EpsWriter.h"
#include "PdfWriter.h"
#include "IdmlWriter.h"

namespace engines {

VectorSink* VectorWriterFactory::Create(Type type, const Size& pageSize) {
    switch (type) {
        case SVG: return new SvgWriter(pageSize);
        case EPS: return new EpsWriter(pageSize);
        case PDF: return new PdfWriter(pageSize);
        case IDML: return new idml::IdmlVectorSinkAdapter(*new idml::IdmlDocument(), "");
        default: return nullptr;
    }
}

void VectorWriterFactory::Destroy(VectorSink* sink) {
    delete sink;
}

} // namespace engines