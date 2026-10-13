// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "renderer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <limits>

namespace webassets {
constexpr size_t kMaxFile = 256u * 1024u * 1024u;
struct Chunk { uint32_t first = 0, count = 0; std::vector<std::array<float, 5>> selectors; };
struct Model { std::vector<gfx::Texture> textures; std::vector<float> vertices; std::vector<uint32_t> indices; std::vector<Chunk> chunks; };
struct Reader {
    const std::vector<uint8_t>& bytes; size_t at = 0;
    bool take(void* dst, size_t n) {
        if (n > bytes.size() - at) return false;
        if (n) std::memcpy(dst, bytes.data() + at, n);
        at += n; return true;
    }
    template<class T> bool value(T& v) { return take(&v, sizeof v); }
    size_t remaining() const { return bytes.size() - at; }
};
inline bool model(const std::vector<uint8_t>& raw, Model& out, bool zup, std::string& error) {
    auto fail = [&](const char* s) { error = s; return false; };
    if (raw.size() < 16 || raw.size() > kMaxFile) return fail("Tamaño de modelo no admitido");
    bool v3 = !std::memcmp(raw.data(), "DHM3", 4);
    if (!v3 && std::memcmp(raw.data(), "DHM2", 4)) return fail("Se requiere DHM2/DHM3; convierte otra vez el modelo");
    Reader r{raw, 4}; Model m; uint32_t nt, nv, ni;
    if (!r.value(nt) || !r.value(nv) || !r.value(ni) || nt > 4096 || nv > 4000000 || ni > 12000000 || !nv || !ni || ni % 3) return fail("Conteos de modelo inválidos");
    for (uint32_t i = 0; i < nt; ++i) {
        gfx::Texture t;
        if (!r.value(t.w) || !r.value(t.h) || t.w < 1 || t.h < 1 || t.w > 4096 || t.h > 4096) return fail("Dimensiones de textura inválidas");
        size_t n = size_t(t.w) * size_t(t.h) * 4;
        if (n > r.remaining()) return fail("Textura truncada");
        t.rgba.resize(n); r.take(t.rgba.data(), n); m.textures.push_back(std::move(t));
    }
    if (uint64_t(nv) * 40 + uint64_t(ni) * 4 > r.remaining()) return fail("Geometría truncada");
    m.vertices.resize(size_t(nv) * 10); r.take(m.vertices.data(), m.vertices.size() * 4);
    m.indices.resize(ni); r.take(m.indices.data(), size_t(ni) * 4);
    for (size_t i = 0; i < m.vertices.size(); ++i) {
        float v = m.vertices[i];
        if (!std::isfinite(v) || std::fabs(v) > 1e7f) return fail("Vértice no finito o fuera de límites");
        if (i % 10 == 9 && (v != std::floor(v) || v < -1 || v >= float(nt))) return fail("Índice de textura inválido");
    }
    for (uint32_t i : m.indices) if (i >= nv) return fail("Índice de vértice inválido");
    // Material per triangle, as expected by the original renderer.
    for (size_t i = 0; i < m.indices.size(); i += 3) {
        float t = m.vertices[size_t(m.indices[i]) * 10 + 9];
        if (m.vertices[size_t(m.indices[i+1]) * 10 + 9] != t || m.vertices[size_t(m.indices[i+2]) * 10 + 9] != t) return fail("Material incoherente dentro de un triángulo");
    }
    if (v3) {
        uint32_t nc;
        if (!r.value(nc) || nc > ni / 3 || uint64_t(nc) * 12 > r.remaining()) return fail("Tabla de visibilidad inválida");
        for (uint32_t i = 0; i < nc; ++i) {
            Chunk c; uint32_t ns;
            if (!r.value(c.first) || !r.value(c.count) || !r.value(ns) || ns > 64 || c.first % 3 || c.count % 3 || uint64_t(c.first) + c.count > ni || uint64_t(ns) * 20 > r.remaining()) return fail("Rango de chunk inválido");
            for (uint32_t j = 0; j < ns; ++j) {
                std::array<float, 5> s; r.take(s.data(), 20);
                for (float f : s) if (!std::isfinite(f) || std::fabs(f) > 1e12f) return fail("Selector no finito");
                if (s[3] < 0 || s[4] < 0) return fail("Distancia de visibilidad negativa");
                if (zup) { float y = s[1]; s[1] = s[2]; s[2] = -y; }
                c.selectors.push_back(s);
            }
            m.chunks.push_back(std::move(c));
        }
    }
    if (r.remaining()) return fail("Datos sobrantes: versión incompatible");
    if (zup) for (size_t i = 0; i < m.vertices.size(); i += 10) { float y = m.vertices[i+1]; m.vertices[i+1] = m.vertices[i+2]; m.vertices[i+2] = -y; }
    out = std::move(m); return true;
}
// Validate before Ground builds its grid; bound spatial expansion of hostile triangles.
inline bool collision(const std::vector<uint8_t>& raw, std::string& error) {
    Reader r{raw}; uint32_t n;
    if (raw.size() > kMaxFile || !r.value(n) || !n || uint64_t(n) * 40 != r.remaining()) { error = "Colisión vacía, truncada o incompatible"; return false; }
    uint64_t cells = 0;
    for (uint32_t i = 0; i < n; ++i) {
        float v[9]; uint32_t surface; r.take(v, 36); r.value(surface);
        for (float f : v) if (!std::isfinite(f) || std::fabs(f) > 2e5f) { error = "Coordenada de colisión inválida"; return false; }
        auto span = [&](int k) { float lo = std::min({v[k], v[k+3], v[k+6]}), hi = std::max({v[k], v[k+3], v[k+6]}); return uint64_t(std::floor(hi/100.f) - std::floor(lo/100.f) + 3); };
        cells += span(0) * span(1); // NGP is Z-up: X/Y are horizontal.
        if (cells > 8000000) { error = "Colisión excede el presupuesto de la rejilla web"; return false; }
    }
    return true;
}
inline bool gates(const std::vector<uint8_t>& raw, std::string& error) {
    Reader r{raw}; uint32_t n;
    if (!r.value(n) || !n || n > 4096 || uint64_t(n) * 24 != r.remaining()) { error = "Archivo de puertas inválido"; return false; }
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t kind, index; float v[4]; r.value(kind); r.value(index); r.take(v, 16);
        if (kind < 8050 || kind > 8052 || index > 4096) { error = "Tipo de puerta incompatible"; return false; }
        for (float f : v) if (!std::isfinite(f) || std::fabs(f) > 1e7f) { error = "Plano de puerta inválido"; return false; }
        float l = v[0]*v[0]+v[1]*v[1]+v[2]*v[2];
        if (l < 0.9f || l > 1.1f) { error = "Normal de puerta no unitaria"; return false; }
    }
    return true;
}
inline bool start(const std::vector<uint8_t>& raw, std::array<float, 3>& p, std::string& error) {
    if (raw.size() < 12 || raw.size() % 12 || raw.size() > 4096) { error = "Rejilla de salida inválida"; return false; }
    for (size_t i = 0; i < raw.size(); i += 4) { float f; std::memcpy(&f, raw.data()+i, 4); if (!std::isfinite(f) || std::fabs(f) > 2e5f) { error = "Posición de salida inválida"; return false; } }
    std::memcpy(p.data(), raw.data(), 12); return true;
}
}
